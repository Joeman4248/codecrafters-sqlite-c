#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/lib-sqlite/execute.h"
#include "include/lib-sqlite/btree_cursor.h"
#include "include/lib-sqlite/record.h"

// a pass over every row of the plan's table
typedef struct {
    const SelectPlan* plan;
    TableCursor       cursor;
    RecordColumn*     storage;      // room for plan->column_count entries, reused for every row
} TableScan;

// one row as read by next_row. Its record points into the cursor's page and
// scan->storage, so it is valid until the next next_row.
typedef struct {
    int64_t rowid;
    Record  record;
} Row;

// reads the cursor's next row into *row, with its record parsed into
// scan->storage. Returns CURSOR_ROW, CURSOR_DONE once there are no more rows,
// or CURSOR_ERROR.
static enum CursorStatus next_row(TableScan* scan, Row* row) {
    ByteView payload;
    const enum CursorStatus status = cursor_next(&scan->cursor, &payload, &row->rowid);
    if (status != CURSOR_ROW) return status;

    if (parse_record(payload, scan->storage, scan->plan->column_count, &row->record)) {
        return CURSOR_ERROR;
    }
    return CURSOR_ROW;
}

static bool is_rowid_alias(const SelectPlan* plan, const uint32_t index) {
    return plan->has_rowid_alias && index == plan->rowid_column;
}

// true if column index of the row is NULL, including when a short record
// doesn't have that column at all. The rowid alias is never NULL.
static bool is_null(const SelectPlan* plan, const Row* row, const uint32_t index) {
    if (is_rowid_alias(plan, index)) return false;
    return record_is_null(&row->record, index);
}

// sets *matches to whether the row passes the plan's WHERE condition. Every
// row passes when there is no WHERE.
static int row_matches(const SelectPlan* plan, const Row* row, bool* matches) {
    *matches = false;

    if (!plan->has_where) {
        *matches = true;
        return 0;
    }

    // NULL never equals anything, and only text is compared so far
    if (!record_is_text(&row->record, plan->where_column)) return 0;

    StringView text;
    if (record_text_view(&row->record, plan->where_column, &text)) return 1;
    *matches = sv_equal(text, plan->where_value);
    return 0;
}

// prints column index of the row without a newline. NULL prints nothing.
static int print_value(const SelectPlan* plan, const Row* row, const uint32_t index) {
    // the record holds NULL in place of the rowid alias
    if (is_rowid_alias(plan, index)) {
        printf("%lld", (long long) row->rowid);
        return 0;
    }
    if (is_null(plan, row, index)) return 0;

    // printed straight from the page, without copying
    if (record_is_text(&row->record, index)) {
        StringView text;
        if (record_text_view(&row->record, index, &text)) return 1;
        printf(SV_FMT, SV_ARG(text));
        return 0;
    }

    int64_t value;
    if (record_get_int(&row->record, index, &value)) {
        fprintf(stderr, "only text and integer values can be printed so far\n");
        return 1;
    }

    printf("%lld", (long long) value);
    return 0;
}

// prints the selected columns of every matching row, one row per line with |
// between values
static int print_rows(TableScan* scan) {
    const SelectPlan* plan = scan->plan;
    Row row;
    enum CursorStatus status;
    while ((status = next_row(scan, &row)) == CURSOR_ROW) {
        bool matches;
        if (row_matches(plan, &row, &matches)) return 1;
        if (!matches) continue;

        for (uint32_t j = 0; j < plan->selected_count; j++) {
            if (j > 0) printf("|");
            if (print_value(plan, &row, plan->selected[j])) return 1;
        }
        printf("\n");
    }
    return status == CURSOR_ERROR;
}

// prints how many matching rows there are for COUNT(*), or how many of them
// have a non-NULL count_column for COUNT(column)
static int count_values(TableScan* scan) {
    const SelectPlan* plan = scan->plan;
    uint32_t count = 0;
    Row row;
    enum CursorStatus status;
    while ((status = next_row(scan, &row)) == CURSOR_ROW) {
        bool matches;
        if (row_matches(plan, &row, &matches)) return 1;
        if (!matches) continue;

        if (plan->count_rows || !is_null(plan, &row, plan->count_column)) count++;
    }
    if (status == CURSOR_ERROR) return 1;

    printf("%u\n", count);
    return 0;
}

int execute_select(const Database* db, const SelectPlan* plan) {
    // the cursor is zeroed so cursor_close is safe even if cursor_open is never reached
    TableScan scan = {
        .plan = plan,
        .cursor = { 0 },
        .storage = malloc(plan->column_count * sizeof(RecordColumn)),
    };

    int result = (scan.storage == NULL) ? 1 : 0;
    if (result == 0) result = cursor_open(&scan.cursor, db, plan->table_root);
    if (result == 0) result = plan->is_count ? count_values(&scan) : print_rows(&scan);

    cursor_close(&scan.cursor);
    free(scan.storage);
    return result;
}

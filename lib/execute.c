#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    TableLeafCell cell;
    const enum CursorStatus status = cursor_next(&scan->cursor, &cell);
    if (status != CURSOR_ROW) return status;

    row->rowid = cell.rowid;
    row->record.header = (RecordHeader) { .columns = scan->storage, .capacity = scan->plan->column_count };
    if (parse_record(cell.payload, &row->record)) return CURSOR_ERROR;
    return CURSOR_ROW;
}

// column index of the row as a Value. The record holds NULL in place of the
// rowid alias, so that column comes from the cell's rowid instead.
static int row_get_value(const SelectPlan* plan, const Row* row, const uint32_t index, Value* out) {
    if (plan->has_rowid_alias && index == plan->rowid_column) {
        *out = (Value) { .type = VALUE_INTEGER, .integer = row->rowid };
        return 0;
    }
    return record_get_value(&row->record, index, out);
}

// sets *matches to whether the row passes the plan's WHERE condition. Every
// row passes when there is no WHERE.
static int row_matches(const SelectPlan* plan, const Row* row, bool* matches) {
    *matches = false;

    if (!plan->has_where) {
        *matches = true;
        return 0;
    }

    Value value;
    if (row_get_value(plan, row, plan->where_column, &value)) return 1;

    *matches = value_equal(value, plan->where_value);
    return 0;
}

// prints a REAL the way sqlite3 does: up to 15 significant digits, and always
// with a decimal point, which %g leaves out of whole numbers ("3", "1e+100"
// rather than "3.0", "1.0e+100")
static void print_real(const double real) {
    char text[32];
    snprintf(text, sizeof(text), "%.15g", real);

    // already has a point, or is inf or nan
    if (strchr(text, '.') != NULL || strchr(text, 'n') != NULL) {
        printf("%s", text);
        return;
    }

    const char* exponent = strchr(text, 'e');
    if (exponent == NULL) printf("%s.0", text);
    else printf("%.*s.0%s", (int) (exponent - text), text, exponent);
}

// prints column index of the row without a newline. NULL prints nothing.
static int print_value(const SelectPlan* plan, const Row* row, const uint32_t index) {
    Value value;
    if (row_get_value(plan, row, index, &value)) return 1;

    // text and blobs are printed straight from the page, without copying
    switch (value.type) {
        case VALUE_NULL:    break;
        case VALUE_INTEGER: printf("%lld", (long long) value.integer); break;
        case VALUE_REAL:    print_real(value.real); break;
        case VALUE_TEXT:    printf(SV_FMT, SV_ARG(value.text)); break;
        case VALUE_BLOB:    fwrite(value.blob.data, 1, value.blob.len, stdout); break;
    }
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

        if (plan->count_rows) {
            count++;
            continue;
        }

        Value value;
        if (row_get_value(plan, &row, plan->count_column, &value)) return 1;
        if (value.type != VALUE_NULL) count++;
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

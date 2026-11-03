#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "query/execute.h"
#include "btree/btree_cursor.h"
#include "record/record.h"

// a pass over every row of the plan's table
typedef struct TableScan {
    const SelectPlan* plan;
    TableCursor       cursor;
    RecordColumn*     storage;      // room for plan->column_count entries, reused for every row
} TableScan;

// one row as read by next_row. Its record points into the cursor's page and
// scan->storage, so it is valid until the next next_row.
typedef struct Row {
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

// the value of column index in the row. The rowid alias column is stored as
// NULL in the record, so its value is the row's rowid instead.
static Value row_get_value(const SelectPlan* plan, const Row* row, const uint32_t index) {
    if (plan->has_rowid_alias && index == plan->rowid_column) {
        return (Value) { .type = VALUE_INTEGER, .integer = row->rowid };
    }
    return record_get_value(&row->record, index);
}

// whether the row passes the plan's WHERE condition. Every row passes when
// there is no WHERE.
static bool row_matches(const SelectPlan* plan, const Row* row) {
    if (!plan->has_where) return true;
    return value_equal(row_get_value(plan, row, plan->where_column), plan->where_value);
}

// prints column index of the row without a newline
static void print_column(const SelectPlan* plan, const Row* row, const uint32_t index) {
    value_print(row_get_value(plan, row, index), stdout);
}

// prints the selected columns of every matching row, one row per line with |
// between values
static int print_rows(TableScan* scan) {
    const SelectPlan* plan = scan->plan;
    Row row;
    enum CursorStatus status;
    while ((status = next_row(scan, &row)) == CURSOR_ROW) {
        if (!row_matches(plan, &row)) continue;

        for (uint32_t j = 0; j < plan->selected_count; j++) {
            if (j > 0) printf("|");
            print_column(plan, &row, plan->selected[j]);
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
        if (!row_matches(plan, &row)) continue;

        if (plan->count_rows) {
            count++;
            continue;
        }

        if (row_get_value(plan, &row, plan->count_column).type != VALUE_NULL) count++;
    }
    if (status == CURSOR_ERROR) return 1;

    printf("%u\n", count);
    return 0;
}

int execute_select(Database* db, const SelectPlan* plan) {
    // the cursor is zeroed so cursor_close is safe even if cursor_open is never reached
    TableScan scan = {
        .plan = plan,
        .cursor = { 0 },
        .storage = malloc(plan->column_count * sizeof(RecordColumn)),
    };

    int result = (scan.storage == NULL) ? 1 : 0;
    if (result == 0) result = cursor_open(&scan.cursor, &db->pager, plan->table_root);
    if (result == 0) result = plan->is_count ? count_values(&scan) : print_rows(&scan);

    cursor_close(&scan.cursor);
    free(scan.storage);
    return result;
}

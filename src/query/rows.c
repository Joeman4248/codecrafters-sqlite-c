#include <stdlib.h>

#include "query/rows.h"

void scan_open(TableScan* scan, Pager* pager, const PlanTable* table) {
    // the cursor is zeroed so scan_close is safe even if cursor_open is never reached
    *scan = (TableScan) {
        .table = table,
        .cursor = { 0 },
        .storage = malloc(table->column_count * sizeof(RecordColumn)),
    };
    if (scan->storage == NULL) {
        cursor_fail(&scan->cursor);
        return;
    }
    cursor_open(&scan->cursor, pager, table->root_page);
}

bool scan_next(TableScan* scan, Row* row) {
    if (!cursor_next(&scan->cursor)) return false;

    const TableLeafCell cell = scan->cursor.cell;
    row->rowid = cell.rowid;
    Reader reader = reader_at(cell.payload, 0);
    row->record = read_record(&reader, (RecordHeader) { .columns = scan->storage,
                                                        .capacity = scan->table->column_count });
    if (reader.failed) cursor_fail(&scan->cursor);
    return !reader.failed;
}

void scan_close(TableScan* scan) {
    cursor_close(&scan->cursor);
    free(scan->storage);
    scan->storage = NULL;
}

Value row_get_value(const PlanTable* table, const Row* row, const uint32_t index) {
    if (table->has_rowid_alias && index == table->rowid_column) {
        return (Value) { .type = VALUE_INTEGER, .integer = row->rowid };
    }
    return record_get_value(&row->record, index);
}

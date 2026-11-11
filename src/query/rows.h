#ifndef CODECRAFTERS_SQLITE_ROWS_H
#define CODECRAFTERS_SQLITE_ROWS_H

#include <stdbool.h>
#include <stdint.h>

#include "btree/btree_cursor.h"
#include "query/plan.h"
#include "record/record.h"

// a pass over every row of the plan's table, in rowid order. Like its cursor
// it fails stickily: once failed, scan_next returns false, so a loop over the
// rows checks scan_failed once after it ends.
typedef struct TableScan {
    const SelectPlan* plan;
    TableCursor       cursor;
    RecordColumn*     storage;      // room for plan->column_count entries, reused for every row
} TableScan;

// one row as read by scan_next. Its record points into the cursor's page and
// the scan's storage, so it is valid until the next scan_next or scan_close.
typedef struct Row {
    int64_t rowid;
    Record  record;
} Row;

// starts a scan at the first row of the plan's table. Fails the scan if its
// storage can't be allocated or the root page can't be read. Either way the
// caller releases it with scan_close.
void scan_open(TableScan* scan, Pager* pager, const SelectPlan* plan);

// reads the next row into *row, with its record parsed. Returns false once
// every row has been read or the scan has failed; a record that can't be read
// fails the scan.
bool scan_next(TableScan* scan, Row* row);

static inline bool scan_failed(const TableScan* scan) {
    return scan->cursor.failed;
}

// frees the scan's storage and unpins its pages
void scan_close(TableScan* scan);

// the value of column index in the row. The rowid alias column is stored as
// NULL in the record, so its value is the row's rowid instead.
Value row_get_value(const SelectPlan* plan, const Row* row, uint32_t index);

#endif // CODECRAFTERS_SQLITE_ROWS_H

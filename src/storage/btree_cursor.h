#ifndef CODECRAFTERS_SQLITE_BTREE_CURSOR_H
#define CODECRAFTERS_SQLITE_BTREE_CURSOR_H

#include <stdint.h>

#include "storage/btree_page.h"
#include "storage/database.h"
#include "util/view.h"

// real table b-trees are 3-4 levels deep. The cap also stops a corrupt file
// whose pages point back at an ancestor from being followed forever.
#define BTREE_MAX_DEPTH 20

// one level of the path from the root to the current leaf
typedef struct {
    Page       page;            // this level's page, owned by the frame
    PageHeader header;
    uint16_t   next_entry;      // next row (leaf) or child (interior) to visit; the
                                // page is finished when it reaches the frame's entry count
} CursorFrame;

// walks the rows of a table b-tree in rowid order, one row per cursor_next
typedef struct {
    const Database* db;
    CursorFrame     frames[BTREE_MAX_DEPTH];
    uint32_t        depth;      // frames in use; 0 once every row has been visited
} TableCursor;

// what cursor_next found
enum CursorStatus {
    CURSOR_ROW,                 // *cell holds the next row
    CURSOR_DONE,                // every row has been visited
    CURSOR_ERROR,               // the file is malformed or a read failed
};

// starts a cursor at the first row of the table b-tree rooted at rootpage.
// On success the caller releases it with cursor_close, even if it stops
// before the last row. On failure there is nothing to close.
int cursor_open(TableCursor* cursor, const Database* db, uint32_t rootpage);

// moves to the next row. On CURSOR_ROW, *cell holds the row's rowid and a view
// of its record. The record points into the cursor's current page, so it is
// only valid until the next cursor_next or cursor_close.
enum CursorStatus cursor_next(TableCursor* cursor, TableLeafCell* cell);

// frees the cursor's pages. Safe to call more than once.
void cursor_close(TableCursor* cursor);

#endif // CODECRAFTERS_SQLITE_BTREE_CURSOR_H

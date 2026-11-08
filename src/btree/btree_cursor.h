#ifndef CODECRAFTERS_SQLITE_BTREE_CURSOR_H
#define CODECRAFTERS_SQLITE_BTREE_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "btree/btree_page.h"
#include "pager/pager.h"
#include "util/view.h"

// real table b-trees are 3-4 levels deep. The cap also stops a corrupt file
// whose pages point back at an ancestor from being followed forever.
#define BTREE_MAX_DEPTH 20

// one level of the path from the root to the current leaf
typedef struct CursorFrame {
    Page*      page;            // this level's page, pinned while the frame is in use
    PageHeader header;
    uint16_t   next_entry;      // next row (leaf) or child (interior) to visit; the
                                // page is finished when it reaches the frame's entry count
} CursorFrame;

// walks the rows of a table b-tree in rowid order, one row per cursor_next.
// Like a Reader, it fails stickily: once failed is set, cursor_next returns
// false, so a loop over the rows checks failed once after it ends.
typedef struct TableCursor {
    Pager*        pager;
    CursorFrame   frames[BTREE_MAX_DEPTH];
    uint32_t      depth;        // frames in use; 0 once every row has been visited
    TableLeafCell cell;         // the current row, set when cursor_next returns true
    bool          failed;       // the file is malformed or a read failed
} TableCursor;

// starts a cursor at the first row of the table b-tree rooted at rootpage.
// Fails the cursor if the root page can't be read. Either way the caller
// releases it with cursor_close, even if it stops before the last row.
void cursor_open(TableCursor* cursor, Pager* pager, uint32_t rootpage);

// moves to the next row. Returns true with cell holding the row's rowid and a
// view of its record, or false once every row has been visited or the cursor
// has failed. The record points into the cursor's current page, so it is only
// valid until the next cursor_next or cursor_close.
bool cursor_next(TableCursor* cursor);

// marks the cursor as failed, for a caller that finds the current row is bad
static inline void cursor_fail(TableCursor* cursor) {
    cursor->failed = true;
}

// unpins the cursor's pages. Safe to call more than once.
void cursor_close(TableCursor* cursor);

#endif // CODECRAFTERS_SQLITE_BTREE_CURSOR_H

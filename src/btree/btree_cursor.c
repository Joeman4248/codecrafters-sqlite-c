#include "btree/btree_cursor.h"

// decodes the header of the frame's page. Returns nonzero if it can't be read
// or the page isn't a table b-tree page.
static int read_frame_header(CursorFrame* frame) {
    Reader reader = btree_header_reader(frame->page);
    frame->header = read_btree_header(&reader);
    return reader.failed || !is_table_page(frame->header.page_type);
}

// reads page number into a new frame on top of the stack. Does nothing if the
// cursor has already failed, so a page number read by a failed step is never
// followed. Fails the cursor, leaving the stack as it was, if the page number
// is out of range, the tree is deeper than BTREE_MAX_DEPTH, or the page isn't
// a table b-tree page.
static void push_page(TableCursor* cursor, const uint32_t page_number) {
    if (cursor->failed) return;

    // the pager checks the page number
    if (cursor->depth == BTREE_MAX_DEPTH) {
        cursor_fail(cursor);
        return;
    }

    CursorFrame* frame = &cursor->frames[cursor->depth];
    frame->next_entry = 0;
    frame->page = pager_get(cursor->pager, page_number);
    if (frame->page == NULL) {
        cursor_fail(cursor);
        return;
    }

    // unpinned here on failure, so the stack is left as it was
    if (read_frame_header(frame)) {
        pager_unref(cursor->pager, frame->page);
        cursor_fail(cursor);
        return;
    }

    cursor->depth++;
}

// unpins the top frame's page and removes it from the stack
static void pop_page(TableCursor* cursor) {
    pager_unref(cursor->pager, cursor->frames[--cursor->depth].page);
}

// cursor->depth is assumed to be > 0
static inline CursorFrame* cursor_top(TableCursor* cursor) {
    return &cursor->frames[cursor->depth - 1];
}

// how many entries a frame visits: its rows on a leaf, or its children on an
// interior page, which has one more child (rightmost_page) than it has cells
static uint32_t frame_entries(const CursorFrame* frame) {
    const uint32_t cells = frame->header.cell_count;
    return frame->header.page_type == PAGE_TYPE_LEAF_TABLE ? cells : cells + 1;
}

// the page number of the child page to visit for entry index of the top
// frame, an interior page: cell index's left_child_page, or the header's
// rightmost_page for the entry after the last cell. Fails the cursor if the
// cell can't be read.
static uint32_t frame_child_page(TableCursor* cursor, const uint16_t index) {
    const CursorFrame* frame = cursor_top(cursor);
    if (index == frame->header.cell_count) return frame->header.rightmost_page;

    // the cell's key separates the children; only lookups need it, not full scans
    Reader reader = btree_header_reader(frame->page);
    seek_cell(&reader, &frame->header, index);
    const uint32_t child_page = read_interior_table_cell(&reader).left_child_page;
    if (reader.failed) cursor_fail(cursor);
    return child_page;
}

// the row in entry index of the top frame, a leaf page. Fails the cursor if
// the cell can't be read.
static TableLeafCell frame_row(TableCursor* cursor, const uint16_t index) {
    const CursorFrame* frame = cursor_top(cursor);
    Reader reader = btree_header_reader(frame->page);
    seek_cell(&reader, &frame->header, index);
    const TableLeafCell cell = read_leaf_table_cell(&reader);
    if (reader.failed) cursor_fail(cursor);
    return cell;
}

void cursor_open(TableCursor* cursor, Pager* pager, const uint32_t rootpage) {
    *cursor = (TableCursor) { .pager = pager };
    push_page(cursor, rootpage);
}

bool cursor_next(TableCursor* cursor) {
    while (!cursor->failed && cursor->depth > 0) {
        CursorFrame* frame = cursor_top(cursor);
        
        // finished with this page, go back up to its parent
        if (frame->next_entry == frame_entries(frame)) {
            pop_page(cursor);
            continue;
        }
        
        const uint16_t index = frame->next_entry++;
        
        if (frame->header.page_type == PAGE_TYPE_LEAF_TABLE) {
            cursor->cell = frame_row(cursor, index);
            return !cursor->failed;
        }

        // push_page does nothing if frame_child_page failed
        push_page(cursor, frame_child_page(cursor, index));
    }
    return false;
}

void cursor_close(TableCursor* cursor) {
    while (cursor->depth > 0) pop_page(cursor);
}

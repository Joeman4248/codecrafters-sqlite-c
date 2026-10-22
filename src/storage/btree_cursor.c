#include <stdlib.h>

#include "storage/btree_cursor.h"

// the frame's page as a view, for the read functions
static ByteView frame_page(const TableCursor* cursor, const CursorFrame* frame) {
    return (ByteView) { frame->buf, cursor->db->file_header.page_size };
}

// reads page_number into the frame's buffer and decodes its header. Returns
// nonzero if the read fails or the page isn't a table b-tree page; the
// caller frees the buffer either way.
static int load_frame(const TableCursor* cursor, CursorFrame* frame, const uint32_t page_number) {
    if (read_page(cursor->db, page_number, frame->buf)) return 1;
    if (read_btree_header(frame->page, page_number, &frame->header)) return 1;

    switch (frame->header.page_type) {
        case PAGE_TYPE_INTERIOR_TABLE:
        case PAGE_TYPE_LEAF_TABLE:
            return 0;
        default:
            return 1;
    }
}

// reads page_number into a new frame on top of the stack. Fails, leaving the
// stack as it was, if the page number is out of range, the tree is deeper
// than BTREE_MAX_DEPTH, or the page isn't a table b-tree page.
static int push_page(TableCursor* cursor, const uint32_t page_number) {
    // pages are numbered from 1; 0 means "no page"
    const uint32_t max_page = cursor->db->file_header.db_size_in_pages;
    if (page_number == 0 || page_number > max_page || cursor->depth == BTREE_MAX_DEPTH) {
        return 1;
    }

    CursorFrame* frame = &cursor->frames[cursor->depth];
    frame->buf = malloc(cursor->db->file_header.page_size);
    if (frame->buf == NULL) return 1;

    frame->page = frame_page(cursor, frame);
    frame->page_number = page_number;
    frame->next_entry = 0;

    // the buffer is freed here on any failure, so the stack is left as it was
    if (load_frame(cursor, frame, page_number)) {
        free(frame->buf);
        frame->buf = NULL;
        return 1;
    }

    cursor->depth++;
    return 0;
}

// frees the top frame's page and removes it from the stack
static void pop_page(TableCursor* cursor) {
    free(cursor->frames[--cursor->depth].buf);
    cursor->frames[cursor->depth].buf = NULL;
}

// cursor->depth is assumed to be > 0
static inline CursorFrame* cursor_top(TableCursor* cursor) {
    return &cursor->frames[cursor->depth - 1];
}

// how many entries a frame visits: its rows on a leaf, or its children on an
// interior page, which has one more child (rightmost_ptr) than it has cells
static uint32_t frame_entries(const CursorFrame* frame) {
    const uint32_t cells = frame->header.cell_count;
    return frame->header.page_type == PAGE_TYPE_LEAF_TABLE ? cells : cells + 1;
}

// the child page for entry index of an interior frame: cell index's left
// child, or rightmost_ptr for the entry after the last cell
static int frame_child(const CursorFrame* frame, const uint16_t index, uint32_t* child) {
    if (index == frame->header.cell_count) {
        *child = frame->header.rightmost_ptr;
        return 0;
    }

    // the cell's key separates the children; only lookups need it, not full scans
    uint16_t offset;
    TableInteriorCell cell;
    if (read_cell_pointer(frame->page, frame->page_number, &frame->header, index, &offset) ||
        read_interior_table_cell(frame->page, offset, &cell)) {
        return 1;
    }
    *child = cell.left_child;
    return 0;
}

// reads the row in entry index of a leaf frame
static int frame_row(const TableCursor* cursor, const CursorFrame* frame, const uint16_t index,
        TableLeafCell* cell) {
    // cells never reach into the reserved bytes at the end of the page
    const ByteView usable = bv_prefix(frame->page, cursor->db->file_header.usable_size);

    uint16_t offset;
    return read_cell_pointer(frame->page, frame->page_number, &frame->header, index, &offset) ||
           read_leaf_table_cell(usable, offset, cell);
}

int cursor_open(TableCursor* cursor, const Database* db, const uint32_t rootpage) {
    *cursor = (TableCursor) { .db = db };
    return push_page(cursor, rootpage);
}

enum CursorStatus cursor_next(TableCursor* cursor, TableLeafCell* cell) {
    while (cursor->depth > 0) {
        CursorFrame* frame = cursor_top(cursor);
        
        // finished with this page, go back up to its parent
        if (frame->next_entry == frame_entries(frame)) {
            pop_page(cursor);
            continue;
        }
        
        const uint16_t index = frame->next_entry++;
        
        if (frame->header.page_type == PAGE_TYPE_LEAF_TABLE) {
            return frame_row(cursor, frame, index, cell) ? CURSOR_ERROR : CURSOR_ROW;
        }

        uint32_t child;
        if (frame_child(frame, index, &child) || push_page(cursor, child)) return CURSOR_ERROR;
    }
    return CURSOR_DONE;
}

void cursor_close(TableCursor* cursor) {
    while (cursor->depth > 0) pop_page(cursor);
}

#ifndef CODECRAFTERS_SQLITE_BTREE_PAGE_H
#define CODECRAFTERS_SQLITE_BTREE_PAGE_H

#include <stdint.h>

#include "storage/database.h"
#include "storage/reader.h"

#define SCHEMA_ROOT_PAGE 1            // root page of the sqlite_schema b-tree

enum PageType {
    PAGE_TYPE_INTERIOR_INDEX = 0x02,
    PAGE_TYPE_INTERIOR_TABLE = 0x05,
    PAGE_TYPE_LEAF_INDEX = 0x0a,
    PAGE_TYPE_LEAF_TABLE = 0x0d
};

// a b-tree page header, in the order its fields appear in the page
typedef struct {
    uint8_t  page_type;         // offset 0
    uint16_t freeblock_offset;  // offset 1
    uint16_t cell_count;        // offset 3
    uint16_t cell_area_start;   // offset 5
    uint8_t  num_free_bytes;    // offset 7
    uint32_t rightmost_ptr;     // offset 8, only on interior pages (0 on leaves)

    uint32_t cell_pointers;     // not stored: where in the page the cell pointer
                                // array starts, right after this header
} PageHeader;

// a table b-tree interior cell: one child page and the key that bounds it
typedef struct {
    uint32_t left_child;    // the page holding every rowid up to and including key
    int64_t  key;
} TableInteriorCell;

// a table b-tree leaf cell: one row
typedef struct {
    int64_t  rowid;
    ByteView payload;       // the row's record, viewed inside the page (not copied)
} TableLeafCell;

// The functions below read through a Reader over the whole page, as
// btree_header_reader gives, and report errors through its sticky failed flag:
// a read past the end of the page, or bytes that make no sense, both fail it.
// So a caller can make several of these calls and check reader.failed once.

// a reader over the page, positioned at its b-tree header, which on page 1
// comes after the 100-byte file header
Reader btree_header_reader(const Page* page);

// reads the b-tree page header at the reader's position, leaving the reader at
// the cell pointer array that follows it. Fails the reader if the page type
// isn't one of the four b-tree page types.
PageHeader read_btree_header(Reader* reader);

// moves the reader to the start of cell index of the page. Fails the reader if
// there is no such cell.
void seek_cell(Reader* reader, const PageHeader* header, uint16_t index);

// reads a table b-tree interior cell at the reader's position
TableInteriorCell read_interior_table_cell(Reader* reader);

// reads a table b-tree leaf cell at the reader's position. Fails the reader if
// the payload is too big for one page, meaning the rest of it is on overflow
// pages, which aren't supported.
TableLeafCell read_leaf_table_cell(Reader* reader);

#endif // CODECRAFTERS_SQLITE_BTREE_PAGE_H

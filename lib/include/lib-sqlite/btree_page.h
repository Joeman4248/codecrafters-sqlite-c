#ifndef CODECRAFTERS_SQLITE_BTREE_PAGE_H
#define CODECRAFTERS_SQLITE_BTREE_PAGE_H

#include <stdint.h>

#include "database.h"
#include "decode.h"

#define SCHEMA_ROOT_PAGE 1            // root page of the sqlite_schema b-tree

enum PageType {
    PAGE_TYPE_INTERIOR_INDEX = 0x02,
    PAGE_TYPE_INTERIOR_TABLE = 0x05,
    PAGE_TYPE_LEAF_INDEX = 0x0a,
    PAGE_TYPE_LEAF_TABLE = 0x0d
};

enum PageHeaderSize {
    PAGE_HEADER_SZ_LEAF = 8,          // 8 bytes on leaf pages
    PAGE_HEADER_SZ_INTERIOR = 12,     // 12 bytes on interior pages
};

typedef struct {
    uint8_t page_type;          // offset 0
    uint8_t num_free_bytes;     // offset 7

    uint16_t freeblock_offset;  // offset 1
    uint16_t cell_count;        // offset 3
    uint16_t cell_area_start;   // offset 5
    uint32_t rightmost_ptr;     // offset 8, only exists on interior pages

    uint8_t header_size;    // 8 bytes in size for leaf pages and 12 bytes for interior pages
} PageHeader;

// decodes the b-tree page header of an in-memory page
int read_btree_header(ByteView page, uint32_t page_number, PageHeader* header);

// decodes the array of K 2-byte cell offsets (which immediately follows the
// b-tree page header) into cell_offsets, which must hold at least
// header->cell_count entries
int read_cell_pointers(ByteView page, uint32_t page_number,
        const PageHeader* header, uint16_t* cell_offsets);

// decodes entry index of the cell pointer array into *cell_offset, without
// reading the rest of the array
int read_cell_pointer(ByteView page, uint32_t page_number,
        const PageHeader* header, uint16_t index, uint16_t* cell_offset);

// decodes a table b-tree interior cell: left_child is the page holding every
// rowid up to and including key
int read_interior_table_cell(ByteView page, uint16_t cell_offset,
        uint32_t* left_child, int64_t* key);

// decodes a table b-tree leaf cell. usable is the page cut to its usable size
// (see bv_prefix), since cells never extend into the reserved bytes at the
// end. payload is set to view the payload inside the page (no copy), and
// rowid receives the cell's signed rowid. Returns nonzero if the payload
// overflows onto overflow pages or on error.
int read_leaf_table_cell(ByteView usable, uint16_t cell_offset,
        ByteView* payload, int64_t* rowid);

#endif // CODECRAFTERS_SQLITE_BTREE_PAGE_H
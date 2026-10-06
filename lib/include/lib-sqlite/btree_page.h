#ifndef CODECRAFTERS_SQLITE_BTREE_PAGE_H
#define CODECRAFTERS_SQLITE_BTREE_PAGE_H

#include <stdint.h>

#include "database.h"
#include "decode.h"

#define SCHEMA_ROOT_PAGE 1            // root page of the sqlite_schema b-tree

enum PageType {
    PAGE_TYPE_INTERIOR_TABLE = 0x02,
    PAGE_TYPE_INTERIOR_INDEX = 0x05,
    PAGE_TYPE_LEAF_TABLE = 0x0a,
    PAGE_TYPE_LEAF_INDEX = 0x0d,
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

// decodes the b-tree page header of an in-memory page of page_size bytes
int read_btree_header(const uint8_t* page, uint32_t page_size,
        uint32_t page_number, PageHeader* header);

// decodes the array of K 2-byte cell offsets (which immediately follows the
// b-tree page header) into cell_offsets, which must hold at least
// header->cell_count entries
int read_cell_pointers(const uint8_t* page, uint32_t page_size,
        uint32_t page_number, const PageHeader* header, uint16_t* cell_offsets);

// decodes a table b-tree leaf cell. payload is set to point at the payload
// inside page (no copy), with payload_size and rowid receiving the cell's
// payload length and integer key. Returns nonzero if the payload overflows
// onto overflow pages or on error.
int read_leaf_table_cell(const uint8_t* page, uint32_t usable_size,
        uint16_t cell_offset, const uint8_t** payload,
        uint64_t* payload_size, uint64_t* rowid);

#endif // CODECRAFTERS_SQLITE_BTREE_PAGE_H
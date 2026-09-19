#ifndef CODECRAFTERS_SQLITE_BTREE_PAGE_H
#define CODECRAFTERS_SQLITE_BTREE_PAGE_H

#include <stdint.h>

#include "database.h"
#include "file_helpers.h"

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

int read_btree_header(const Database* db, PageHeader* header,
        const uint32_t page_number);

// reads the array of K 2-byte cell offsets (which immediately follows the
// b-tree page header) into cell_offsets, which must hold at least
// header->cell_count entries
int read_cell_pointers(const Database* db,
        const PageHeader* header, const uint32_t page_number, uint16_t* cell_offsets);

// reads the payload of a table b-tree leaf cell. The payload is copied into
// payload_buf (which must hold at least payload_buf_capacity bytes), with
// payload_size and rowid receiving the cell's payload length and integer key.
// Returns nonzero if the payload overflows onto overflow pages or on error.
int read_leaf_table_cell(const Database* db,
        const uint32_t page_number, const uint16_t cell_offset,
        uint8_t* payload_buf, const uint32_t payload_buf_capacity,
        uint64_t* payload_size, uint64_t* rowid);

#endif // CODECRAFTERS_SQLITE_BTREE_PAGE_H
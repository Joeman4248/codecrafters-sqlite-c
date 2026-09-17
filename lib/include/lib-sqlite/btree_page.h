#ifndef CODECRAFTERS_SQLITE_BTREE_PAGE_H
#define CODECRAFTERS_SQLITE_BTREE_PAGE_H

#include <stdint.h>
#include <stdio.h>

#include "file_header.h"
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

int read_btree_header(FILE* file, PageHeader* header,
        const uint16_t page_size, const uint32_t page_number);

#endif // CODECRAFTERS_SQLITE_BTREE_PAGE_H
#include "include/lib-sqlite/btree_page.h"

int read_btree_header(FILE* file, PageHeader* header,
        const uint16_t page_size, const uint32_t page_number) {
    const long offset = FILE_HEADER_SZ + (page_number - 1) * page_size;
    if (fseek(file, (long) offset, SEEK_SET) != 0) {
        // fprintf(stderr, "Error seeking to start of header");
        return 1;
    }

    if (read_uint8(file, &header->page_type)) {
        // fprintf(stderr, "Error reading page type");
        return 1;
    }

    switch (header->page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            header->header_size = PAGE_HEADER_SZ_INTERIOR;
            break;

        case PAGE_TYPE_LEAF_INDEX:
        case PAGE_TYPE_LEAF_TABLE:
            header->header_size = PAGE_HEADER_SZ_LEAF;
            break;

        default:
            // fprintf(stderr, "Invalid page header type flag\n");
            return 1;
    }

    read_uint16(file, &header->freeblock_offset);
    read_uint16(file, &header->cell_count);
    read_uint16(file, &header->cell_area_start);
    read_uint8(file, &header->num_free_bytes);
    header->rightmost_ptr = 0;

    switch (header->page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            read_uint32(file, &header->rightmost_ptr);
    }

    return 0;
}
#include <string.h>

#include "include/lib-sqlite/btree_page.h"

// returns the offset within the page where the b-tree page header begins:
// page 1 starts after the 100-byte database header, all other pages start at
// the beginning of the page
static uint32_t page_header_start(const uint32_t page_number) {
    return page_number == SCHEMA_ROOT_PAGE ? FILE_HEADER_SZ : 0;
}

int read_btree_header(const uint8_t* page, const uint32_t page_size,
        const uint32_t page_number, PageHeader* header) {
    uint32_t offset = page_header_start(page_number);

    if (decode_uint8(page, page_size, &offset, &header->page_type) ||
        decode_uint16(page, page_size, &offset, &header->freeblock_offset) ||
        decode_uint16(page, page_size, &offset, &header->cell_count) ||
        decode_uint16(page, page_size, &offset, &header->cell_area_start) ||
        decode_uint8(page, page_size, &offset, &header->num_free_bytes)) {
        return 1;
    }

    switch (header->page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            if (decode_uint32(page, page_size, &offset, &header->rightmost_ptr)) {
                return 1;
            }
            header->header_size = PAGE_HEADER_SZ_INTERIOR;
            break;

        case PAGE_TYPE_LEAF_INDEX:
        case PAGE_TYPE_LEAF_TABLE:
            header->rightmost_ptr = 0;
            header->header_size = PAGE_HEADER_SZ_LEAF;
            break;

        default:
            // fprintf(stderr, "Invalid page header type flag\n");
            return 1;
    }

    return 0;
}

int read_cell_pointers(const uint8_t* page, const uint32_t page_size,
        const uint32_t page_number, const PageHeader* header, uint16_t* cell_offsets) {
    // the cell pointer array immediately follows the b-tree page header,
    // not the end of the page
    uint32_t offset = page_header_start(page_number) + header->header_size;

    for (uint32_t i = 0; i < header->cell_count; i++) {
        if (decode_uint16(page, page_size, &offset, &cell_offsets[i])) {
            return 1;
        }
    }

    return 0;
}

int read_leaf_table_cell(const uint8_t* page, const uint32_t usable_size,
        const uint16_t cell_offset, const uint8_t** payload,
        uint32_t* payload_size, int64_t* rowid) {
    // cell offsets are relative to the start of the physical page; page 1's
    // 100-byte database header only shifts the b-tree page header, not cell-offset
    uint32_t offset = cell_offset;

    uint64_t raw;
    if (decode_varint(page, usable_size, &offset, &raw)) {
        return 1;
    }
    *payload_size = (uint32_t) raw;

    if (decode_varint(page, usable_size, &offset, &raw)) {
        return 1;
    }
    // the varint holds the rowid's two's-complement bits; memcpy copies them
    // without an implementation-defined unsigned-to-signed conversion
    memcpy(rowid, &raw, sizeof(*rowid));

    // max payload that can be stored inline on a table b-tree leaf page is
    // U - 35, where U is the usable page size. The schema records are far
    // below that, so larger payloads (which spill onto overflow pages) are
    // reported as unsupported rather than read incorrectly.
    if (*payload_size > usable_size - 35 ||
        *payload_size > usable_size - offset) {
        return 1;
    }

    *payload = page + offset;
    return 0;
}

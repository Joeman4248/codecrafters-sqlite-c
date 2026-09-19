#include <string.h>

#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/varint.h"

// returns the file offset where the b-tree content of the given page begins:
// page 1 starts after the 100-byte database header, all other pages start at
// a page boundary
static long page_content_offset(const uint16_t page_size, const uint32_t page_number) {
    const uint32_t size = page_size_bytes(page_size);
    return (long) (page_number - 1) * size + (page_number == 1 ? FILE_HEADER_SZ : 0);
}

int read_btree_header(const Database* db, PageHeader* header,
        const uint32_t page_number) {
    const uint32_t page_size = page_size_bytes(db->file_header.page_size);

    if (fseek(db->file, page_content_offset(page_size, page_number), SEEK_SET) != 0) {
        // fprintf(stderr, "Error seeking to start of header");
        return 1;
    }

    if (read_uint8(db->file, &header->page_type)) {
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

    read_uint16(db->file, &header->freeblock_offset);
    read_uint16(db->file, &header->cell_count);
    read_uint16(db->file, &header->cell_area_start);
    read_uint8(db->file, &header->num_free_bytes);
    header->rightmost_ptr = 0;

    switch (header->page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            read_uint32(db->file, &header->rightmost_ptr);
    }

    return 0;
}

int read_cell_pointers(const Database* db,
        const PageHeader* header, const uint32_t page_number, uint16_t* cell_offsets) {
    // the cell pointer array immediately follows the b-tree page header,
    // not the end of the page
    const uint32_t page_size = page_size_bytes(db->file_header.page_size);
    const long array_start = page_content_offset(page_size, page_number)
        + header->header_size;

    if (fseek(db->file, array_start, SEEK_SET) != 0) {
        return 1;
    }

    for (uint32_t i = 0; i < header->cell_count; i++) {
        if (read_uint16(db->file, &cell_offsets[i])) {
            return 1;
        }
    }

    return 0;
}

int read_leaf_table_cell(const Database* db,
        const uint32_t page_number, const uint16_t cell_offset,
        uint8_t* payload_buf, const uint32_t payload_buf_capacity,
        uint64_t* payload_size, uint64_t* rowid) {
    // cell offsets are relative to the start of the physical page; page 1's
    // 100-byte database header only shifts the b-tree page header, not cell-offs
    const uint32_t page_size = page_size_bytes(db->file_header.page_size);
    const long cell_start = (long) (page_number - 1) * page_size
        + cell_offset;

    if (fseek(db->file, cell_start, SEEK_SET) != 0) {
        return 1;
    }

    // two varints precede the payload: the payload length and the rowid.
    // Read them into a buffer (sized for two max-length varints) and decode,
    // then continue reading the payload from the file just past the consumed
    // bytes.
    uint8_t cell_header[2 * VARINT_MAX_SZ];
    const uint32_t header_bytes = (uint32_t) fread(cell_header, 1, sizeof(cell_header), db->file);

    uint32_t offset = 0;
    if (decode_varint(cell_header, header_bytes, &offset, payload_size)) {
        return 1;
    }
    if (decode_varint(cell_header, header_bytes, &offset, rowid)) {
        return 1;
    }

    if (fseek(db->file, cell_start + offset, SEEK_SET) != 0) {
        return 1;
    }

    const uint32_t usable_page_size = page_size - db->file_header.reserved_space;

    // max payload that can be stored inline on a table b-tree leaf page is
    // U - 35, where U is the usable page size. The schema records are far
    // below that, so larger payloads (which spill onto overflow pages) are
    // reported as unsupported rather than read incorrectly.
    if (*payload_size > usable_page_size - 35 ||
        *payload_size > payload_buf_capacity) {
        return 1;
    }

    if (fread(payload_buf, 1, (size_t) *payload_size, db->file) != *payload_size) {
        return 1;
    }

    return 0;
}
#include <string.h>

#include "include/lib-sqlite/btree_page.h"

// returns the offset within the page where the b-tree page header begins:
// page 1 starts after the 100-byte database header, all other pages start at
// the beginning of the page
static uint32_t page_header_start(const uint32_t page_number) {
    return page_number == SCHEMA_ROOT_PAGE ? FILE_HEADER_SZ : 0;
}

int read_btree_header(const ByteView page, const uint32_t page_number, PageHeader* header) {
    Reader reader = reader_at(page, page_header_start(page_number));

    read_u8(&reader, &header->page_type);
    read_u16(&reader, &header->freeblock_offset);
    read_u16(&reader, &header->cell_count);
    read_u16(&reader, &header->cell_area_start);
    read_u8(&reader, &header->num_free_bytes);

    // checked before the switch, which decides from page_type what to read next
    if (reader.failed) return 1;

    // so cell_count + 1 still fits in 16 bits
    if (header->cell_count == UINT16_MAX) return 1;

    switch (header->page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            if (read_u32(&reader, &header->rightmost_ptr)) return 1;
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

int read_cell_pointers(const ByteView page, const uint32_t page_number,
        const PageHeader* header, uint16_t* cell_offsets) {
    // the cell pointer array immediately follows the b-tree page header,
    // not the end of the page
    Reader reader = reader_at(page, page_header_start(page_number) + header->header_size);
    for (uint32_t i = 0; i < header->cell_count; i++) {
        if (read_u16(&reader, &cell_offsets[i])) return 1;
    }
    return 0;
}

int read_cell_pointer(const ByteView page, const uint32_t page_number,
        const PageHeader* header, const uint16_t index, uint16_t* cell_offset) {
    if (index >= header->cell_count) return 1;
    Reader reader = reader_at(page, page_header_start(page_number) + 
                                    header->header_size + (index * 2u));
    if (read_u16(&reader, cell_offset)) return 1;
    return 0;
}

int read_interior_table_cell(const ByteView page, const uint16_t cell_offset,
        uint32_t* left_child, int64_t* key) {
    Reader reader = reader_at(page, cell_offset);

    uint64_t raw_key;
    read_u32(&reader, left_child);
    read_varint(&reader, &raw_key);
    if (reader.failed) return 1;

    // the varint holds the rowid's two's-complement bits; memcpy copies them
    // without an implementation-defined unsigned-to-signed conversion
    memcpy(key, &raw_key, sizeof(*key));
    return 0;
}

int read_leaf_table_cell(const ByteView usable, const uint16_t cell_offset,
        ByteView* payload, int64_t* rowid) {
    Reader reader = reader_at(usable, cell_offset);

    // SQLite caps a value's length at 2^31 - 1 bytes, so a payload size always
    // fits in 32 bits
    uint32_t payload_size;
    uint64_t raw_rowid;
    read_varint32(&reader, &payload_size);
    read_varint(&reader, &raw_rowid);
    if (reader.failed) return 1;

    // the varint holds the rowid's two's-complement bits; memcpy copies them
    // without an implementation-defined unsigned-to-signed conversion
    memcpy(rowid, &raw_rowid, sizeof(*rowid));

    // max payload that can be stored inline on a table b-tree leaf page is
    // U - 35, where U is the usable page size. Larger payloads spill onto
    // overflow pages, so they are reported as unsupported rather than read
    // incorrectly. Usable sizes are at least 480, so this can't wrap.
    if (payload_size > usable.len - 35) return 1;

    return reader_take(&reader, payload_size, payload);
}

// reads the payload size and payload that every index cell ends with. As for
// table leaves, a payload over the size an index cell keeps inline,
// ((U - 12) * 64 / 255) - 23, spills onto overflow pages and is reported as
// unsupported. Usable sizes are at least 480, so this can't wrap.
static int read_index_payload(Reader* reader, const uint32_t usable_size, ByteView* payload) {
    uint32_t payload_size;
    if (read_varint32(reader, &payload_size)) return 1;
    if (payload_size > (usable_size - 12) * 64 / 255 - 23) return 1;
    return reader_take(reader, payload_size, payload);
}

int read_interior_index_cell(const ByteView usable, const uint16_t cell_offset,
        uint32_t* left_child, ByteView* payload) {
    Reader reader = reader_at(usable, cell_offset);
    if (read_u32(&reader, left_child)) return 1;
    return read_index_payload(&reader, usable.len, payload);
}

int read_leaf_index_cell(const ByteView usable, const uint16_t cell_offset, ByteView* payload) {
    Reader reader = reader_at(usable, cell_offset);
    return read_index_payload(&reader, usable.len, payload);
}

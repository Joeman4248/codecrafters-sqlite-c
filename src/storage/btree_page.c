#include "storage/btree_page.h"

// Each read is its own statement: in an initializer such as
// { .a = read_u32(r), .b = read_u16(r) } the order the calls run in isn't
// specified, so the bytes could be read in the wrong order.

Reader btree_header_reader(const Page* page) {
    // page 1 starts with the file header; every other page with its b-tree header
    const uint32_t start = (page->number == SCHEMA_ROOT_PAGE) ? FILE_HEADER_SZ : 0;
    return reader_at(page_bytes(page), start);
}

PageHeader read_btree_header(Reader* reader) {
    PageHeader header = { 0 };
    header.page_type        = read_u8(reader);
    header.freeblock_offset = read_u16(reader);
    header.cell_count       = read_u16(reader);
    header.cell_area_start  = read_u16(reader);
    header.num_free_bytes   = read_u8(reader);

    // checked before the switch, which decides from page_type what to read next
    if (reader->failed) return header;

    // so cell_count + 1 still fits in 16 bits
    if (header.cell_count == UINT16_MAX) reader_fail(reader);

    switch (header.page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            header.rightmost_ptr = read_u32(reader);
            break;

        case PAGE_TYPE_LEAF_INDEX:
        case PAGE_TYPE_LEAF_TABLE:
            break;

        default:
            reader_fail(reader);
            break;
    }

    header.cell_pointers = reader->pos;
    return header;
}

void seek_cell(Reader* reader, const PageHeader* header, const uint16_t index) {
    if (index >= header->cell_count) {
        reader_fail(reader);
        return;
    }

    // each pointer is the 2-byte offset of its cell from the start of the page
    reader_seek(reader, header->cell_pointers + index * 2u);
    reader_seek(reader, read_u16(reader));
}

TableInteriorCell read_interior_table_cell(Reader* reader) {
    TableInteriorCell cell;
    cell.left_child = read_u32(reader);
    cell.key        = read_varint_i64(reader);
    return cell;
}

TableLeafCell read_leaf_table_cell(Reader* reader) {
    TableLeafCell cell;

    // SQLite caps a value's length at 2^31 - 1 bytes, so a payload size always
    // fits in 32 bits
    const uint32_t payload_size = read_varint_u32(reader);
    cell.rowid = read_varint_i64(reader);

    // max payload that can be stored inline on a table b-tree leaf page is
    // U - 35, where U is the usable page size: the length of the bytes the
    // reader reads (see page_bytes). Larger payloads spill onto overflow pages,
    // so they are reported as unsupported rather than read incorrectly. Usable
    // sizes are at least 480, so this can't wrap.
    if (payload_size > reader->bytes.len - 35) reader_fail(reader);

    cell.payload = reader_take(reader, payload_size);
    return cell;
}

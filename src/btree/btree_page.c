#include "btree/btree_page.h"

// Each read is its own statement: in an initializer such as
// { .a = read_u32(r), .b = read_u16(r) } the order the calls run in isn't
// specified, so the bytes could be read in the wrong order.

Reader btree_header_reader(const Page* page) {
    // page 1 starts with the file header; every other page with its b-tree header
    const uint32_t start = (page->page_number == SCHEMA_ROOT_PAGE) ? FILE_HEADER_SZ : 0;
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
            header.rightmost_page = read_u32(reader);
            break;

        case PAGE_TYPE_LEAF_INDEX:
        case PAGE_TYPE_LEAF_TABLE:
            break;

        // the byte from the file isn't a page type
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

    // the cell pointer array right after the header has one 2-byte entry per
    // cell, in key order. First go to entry index of that array...
    reader_seek(reader, header->cell_pointers + index * 2u);

    // ...then read the entry, which is where the cell starts, counted from the
    // start of the page (even on page 1, behind the file header), and go there.
    // The cells themselves sit in no particular order at the end of the page.
    reader_seek(reader, read_u16(reader));
}

TableInteriorCell read_interior_table_cell(Reader* reader) {
    TableInteriorCell cell;
    cell.left_child_page = read_u32(reader);
    cell.key        = read_varint_i64(reader);
    return cell;
}

// how many bytes of a table leaf record of payload_size bytes stay in the
// cell, on a page whose usable size is usable_size; the rest spill onto
// overflow pages. The formula is SQLite's, from its file format spec. Usable
// sizes are at least 480, so none of it can wrap.
static uint32_t local_payload_size(const uint32_t usable_size, const uint32_t payload_size) {
    // the most a cell holds itself; a record this size or smaller doesn't spill
    const uint32_t max_local = usable_size - 35;
    if (payload_size <= max_local) return payload_size;

    // a spilling record keeps at least min_local bytes, plus whatever makes the
    // rest fill its last overflow page (each holds usable_size - 4), if that
    // still fits in the cell
    const uint32_t min_local = (usable_size - 12) * 32 / 255 - 23;
    const uint32_t local = min_local + (payload_size - min_local) % (usable_size - 4);
    return (local <= max_local) ? local : min_local;
}

TableLeafCell read_leaf_table_cell(Reader* reader) {
    TableLeafCell cell;

    // SQLite caps a value's length at 2^31 - 1 bytes, so a payload size always
    // fits in 32 bits
    cell.payload_size = read_varint_u32(reader);
    cell.rowid        = read_varint_i64(reader);

    // the reader's bytes are the page's usable part (see page_bytes)
    const uint32_t local_size = local_payload_size(reader->bytes.len, cell.payload_size);
    cell.payload = reader_take(reader, local_size);

    // the first overflow page's number follows the part in the cell
    const bool spills = local_size < cell.payload_size;
    cell.overflow_page = spills ? read_u32(reader) : 0;
    return cell;
}

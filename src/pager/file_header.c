#include <string.h>

#include "pager/file_header.h"
#include "util/reader.h"

// resolves the page size field into the actual page size in bytes,
// translating the marker value 1 into 65536
static uint32_t page_size_bytes(const uint16_t page_size) {
    return (page_size == PAGE_SIZE_MAX_MARKER) ? PAGE_SIZE_MAX : page_size;
}

// checks if a uint32 is a valid power of 2
static bool is_powerof2(const uint32_t val) {
    return (val & (val - 1)) == 0;
}

// whether a file with this read version can be read: 1 (legacy) or 2 (WAL).
// A file with a read version > 2 cannot be read at all.
static bool is_readable_version(const enum FileFormatVersion version) {
    switch (version) {
        case FILE_FORMAT_VERSION_LEGACY:
        case FILE_FORMAT_VERSION_WAL:
            return true;
    }
    return false;
}

// whether this is a supported schema format, 0-4 (0 means an empty database)
static bool is_known_schema_format(const enum SchemaFormat format) {
    switch (format) {
        case SCHEMA_FORMAT_NONE:
        case SCHEMA_FORMAT_1:
        case SCHEMA_FORMAT_2:
        case SCHEMA_FORMAT_3:
        case SCHEMA_FORMAT_4:
            return true;
    }
    return false;
}

// whether this is a text encoding: 1 (UTF-8), 2 (UTF-16le), or 3 (UTF-16be)
static bool is_known_text_encoding(const enum TextEncoding encoding) {
    switch (encoding) {
        case TEXT_ENCODING_UTF8:
        case TEXT_ENCODING_UTF16LE:
        case TEXT_ENCODING_UTF16BE:
            return true;
    }
    return false;
}

// validates the field values once the header has been fully read,
// per the SQLite file format spec
static int validate_file_header(const FileHeader* header) {
    if (memcmp(header->header_string, VALID_HEADER_STR, HEADER_STR_SZ) != 0) {
        // fprintf(stderr, "Invalid header string");
        return 1;
    }

    // page size must be a power of two between 512 and 65536 (the marker
    // value 1 was already resolved to 65536 when reading)
    if (header->page_size < PAGE_SIZE_MIN ||
        header->page_size > PAGE_SIZE_MAX ||
        !is_powerof2(header->page_size)) {
        return 1;
    }

    if (!is_readable_version(header->read_version)) return 1;

    // usable page size = page_size - reserved_space, and must be >= 480
    if (header->page_size < header->reserved_space + MIN_USABLE_PAGE_SIZE) {
        return 1;
    }

    if (header->max_embedded_payload_fraction != MAX_EMBEDDED_PAYLOAD_FRACTION ||
        header->min_embedded_payload_fraction != MIN_EMBEDDED_PAYLOAD_FRACTION ||
        header->leaf_payload_fraction != LEAF_PAYLOAD_FRACTION) {
        return 1;
    }

    if (!is_known_schema_format(header->schema_format)) return 1;
    if (!is_known_text_encoding(header->text_encoding)) return 1;

    // if there is no largest root b-tree page (no auto-vacuum), the
    // incremental-vacuum flag (offset 64) must also be zero
    if (header->largest_root_btree_page == 0 && header->incremental_vacuum_enabled != 0) {
        return 1;
    }

    return 0;
}

// decodes and validates the header. Returns a complete, valid header, or
// fails the reader and returns all zeros, so the caller never sees a partly
// filled or invalid one.
static FileHeader decode_file_header(Reader* reader) {
    FileHeader header = { 0 };

    const ByteView header_string = reader_take(reader, sizeof(header.header_string));
    if (!reader->failed) memcpy(header.header_string, header_string.data, header_string.len);

    header.page_size = page_size_bytes(read_u16(reader));

    header.write_version                 = read_u8(reader);
    header.read_version                  = read_u8(reader);
    header.reserved_space                = read_u8(reader);
    header.max_embedded_payload_fraction = read_u8(reader);
    header.min_embedded_payload_fraction = read_u8(reader);
    header.leaf_payload_fraction         = read_u8(reader);

    header.file_change_counter        = read_u32(reader);
    header.db_size_in_pages           = read_u32(reader);
    header.first_freelist_trunk_page  = read_u32(reader);
    header.freelist_page_count        = read_u32(reader);
    header.schema_cookie              = read_u32(reader);
    header.schema_format              = read_u32(reader);
    header.default_page_cache_size    = read_u32(reader);
    header.largest_root_btree_page    = read_u32(reader);
    header.text_encoding              = read_u32(reader);
    header.user_version               = read_u32(reader);
    header.incremental_vacuum_enabled = read_u32(reader);
    header.application_id             = read_u32(reader);

    reader_skip(reader, FILE_HEADER_RESV_SZ);     // reserved, always zero
    header.version_valid_for     = read_u32(reader);
    header.sqlite_version_number = read_u32(reader);

    // one check covers every read above: none of the values is used until here
    if (reader->failed || validate_file_header(&header)) {
        reader_fail(reader);
        return (FileHeader) { 0 };
    }

    // computed only after validation guarantees page_size >= reserved_space + 480,
    // so the subtraction can't wrap
    header.usable_size = header.page_size - header.reserved_space;
    return header;
}

FileHeader read_file_header(FILE* file) {
    uint8_t buf[FILE_HEADER_SZ];
    if (fseek(file, 0, SEEK_SET)) return (FileHeader) { 0 };
    if (fread(buf, 1, sizeof(buf), file) != sizeof(buf)) return (FileHeader) { 0 };

    Reader reader = reader_at((ByteView) { buf, sizeof(buf) }, 0);
    return decode_file_header(&reader);
}
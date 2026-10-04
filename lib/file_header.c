#include <string.h>

#include "include/lib-sqlite/file_header.h"
#include "include/lib-sqlite/file_helpers.h"

uint32_t page_size_bytes(const uint16_t page_size) {
    return page_size == PAGE_SIZE_65536_MARKER ? PAGE_SIZE_MAX : page_size;
}

// checks if a uint16 is a valid power of 2
static bool is_powerof2(const uint16_t val) {
    return (val & (val - 1)) == 0;
}

// validates the field values once the header has been fully read,
// per the SQLite file format spec
static int validate_file_header(const FileHeader* header) {
    if (memcmp(header->header_string, VALID_HEADER_STR, HEADER_STR_SZ) != 0) {
        // fprintf(stderr, "Invalid header string");
        return 1;
    }

    // page size must be a power of two between 512 and 32768, or the
    // marker value 1 which means a 65536-byte page
    if (header->page_size != PAGE_SIZE_65536_MARKER &&
        (header->page_size < PAGE_SIZE_MIN ||
         header->page_size > PAGE_SIZE_2_BYTE_MAX ||
         !is_powerof2(header->page_size))) {
        return 1;
    }

    // read version must be 1 (legacy) or 2 (WAL); a file with a
    // read version > 2 cannot be read at all
    switch (header->read_version) {
        case FILE_FORMAT_VERSION_LEGACY:
        case FILE_FORMAT_VERSION_WAL:
            break;
        default:
            return 1;
    }

    // usable page size = page_size - reserved_space, and must be >= 480
    // (always satisfied for a 65536-byte page, so skip the marker)
    if (header->page_size != PAGE_SIZE_65536_MARKER &&
        header->page_size < header->reserved_space + MIN_USABLE_PAGE_SIZE) {
        return 1;
    }

    if (header->max_embedded_payload_fraction != MAX_EMBEDDED_PAYLOAD_FRACTION ||
        header->min_embedded_payload_fraction != MIN_EMBEDDED_PAYLOAD_FRACTION ||
        header->leaf_payload_fraction != LEAF_PAYLOAD_FRACTION) {
        return 1;
    }

    // supported schema formats are 0-4 (0 means an empty database)
    switch (header->schema_format) {
        case SCHEMA_FORMAT_NONE:
        case SCHEMA_FORMAT_1:
        case SCHEMA_FORMAT_2:
        case SCHEMA_FORMAT_3:
        case SCHEMA_FORMAT_4:
            break;
        default:
            return 1;
    }

    // text encoding must be 1 (UTF-8), 2 (UTF-16le), or 3 (UTF-16be)
    switch (header->text_encoding) {
        case TEXT_ENCODING_UTF8:
        case TEXT_ENCODING_UTF16LE:
        case TEXT_ENCODING_UTF16BE:
            break;
        default:
            return 1;
    }

    // if there is no largest root b-tree page (no auto-vacuum), the
    // incremental-vacuum flag (offset 64) must also be zero
    if (header->largest_root_btree_page == 0 && header->incremental_vacuum_enabled != 0) {
        return 1;
    }

    return 0;
}

int read_file_header(FILE* file, FileHeader* header) {
    if (fseek(file, 0, SEEK_SET) != 0) {
        // fprintf(stderr, "Error seeking to beginning of file");
        return 1;
    }
    
    if (fread(header->header_string, 1, HEADER_STR_SZ, file) != HEADER_STR_SZ) {
        // fprintf(stderr, "Error reading header string");
        return 1;
    }

    if (read_uint16(file, &header->page_size)) {
        return 1;
    }

    if (read_uint8(file, &header->write_version) ||
        read_uint8(file, &header->read_version) ||
        read_uint8(file, &header->reserved_space) ||
        read_uint8(file, &header->max_embedded_payload_fraction) ||
        read_uint8(file, &header->min_embedded_payload_fraction) ||
        read_uint8(file, &header->leaf_payload_fraction)) {
        return 1;
    }

    if (read_uint32(file, &header->file_change_counter) ||
        read_uint32(file, &header->db_size_in_pages) ||
        read_uint32(file, &header->first_freelist_trunk_page) ||
        read_uint32(file, &header->freelist_page_count) ||
        read_uint32(file, &header->schema_cookie) ||
        read_uint32(file, &header->schema_format) ||
        read_uint32(file, &header->default_page_cache_size) ||
        read_uint32(file, &header->largest_root_btree_page) ||
        read_uint32(file, &header->text_encoding) ||
        read_uint32(file, &header->user_version) ||
        read_uint32(file, &header->incremental_vacuum_enabled) ||
        read_uint32(file, &header->application_id)) {
        return 1;
    }

    // skip empty reserved bytes in the header
    if (fseek(file, FILE_HEADER_RESERVED_SZ, SEEK_CUR) != 0) {
        return 1;
    }

    if (read_uint32(file, &header->version_valid_for) ||
        read_uint32(file, &header->sqlite_version_number)) {
        return 1;
    }

    return validate_file_header(header);
}
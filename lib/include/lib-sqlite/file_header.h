#ifndef CODECRAFTERS_SQLITE_FILE_HEADER_H
#define CODECRAFTERS_SQLITE_FILE_HEADER_H

#include <stdint.h>
#include <stdio.h>

#define FILE_HEADER_SZ 100            // first 100 bytes of page 1
#define HEADER_STR_SZ 16
#define VALID_HEADER_STR "SQLite format 3"
#define FILE_HEADER_RESERVED_SZ 20    // header bytes reserved for expansion (offset 72)

// page size field (offset 16): stores an ordinary page size directly, as a
// power of two between PAGE_SIZE_MIN and PAGE_SIZE_2_BYTE_MAX, or the

#define PAGE_SIZE_MIN (1 << 9)          // smallest valid page size
#define PAGE_SIZE_2_BYTE_MAX (1 << 15)  // largest size the 4-byte page size field stores directly

#define PAGE_SIZE_MAX (1 << 16)         // largest possible page size, does not fit directly into the 4-byte page size field
#define PAGE_SIZE_65536_MARKER 1        // field value encoding a PAGE_SIZE_MAX page

#define MIN_USABLE_PAGE_SIZE 480        // usable = page_size - reserved_space

enum FileFormatVersion {
    FILE_FORMAT_VERSION_LEGACY = 1,   // rollback journal mode
    FILE_FORMAT_VERSION_WAL = 2,      // write-ahead log mode
};

enum PayloadFraction {
    MAX_EMBEDDED_PAYLOAD_FRACTION = 64, // Maximum embedded payload fraction. Must be 64.
    MIN_EMBEDDED_PAYLOAD_FRACTION = 32, // Minimum embedded payload fraction. Must be 32.
    LEAF_PAYLOAD_FRACTION = 32,         // Leaf payload fraction. Must be 32.
};

enum SchemaFormat {
    SCHEMA_FORMAT_NONE = 0,     // no schema, an empty database
    SCHEMA_FORMAT_1 = 1,        // understood by all versions since 3.0.0
    SCHEMA_FORMAT_2 = 2,        // varying column counts (ALTER TABLE ADD COLUMN)
    SCHEMA_FORMAT_3 = 3,        // non-NULL defaults on added columns
    SCHEMA_FORMAT_4 = 4,        // DESC index ordering, serial types 8 and 9
};

enum TextEncoding {
    TEXT_ENCODING_UTF8 = 1,
    TEXT_ENCODING_UTF16LE = 2,
    TEXT_ENCODING_UTF16BE = 3,
};

typedef struct {
    uint8_t  header_string[HEADER_STR_SZ]; // offset 0
    uint16_t page_size;                    // offset 16
    uint8_t  write_version;                // offset 18
    uint8_t  read_version;                 // offset 19
    uint8_t  reserved_space;               // offset 20
    uint8_t  max_embedded_payload_fraction;    // offset 21
    uint8_t  min_embedded_payload_fraction;    // offset 22
    uint8_t  leaf_payload_fraction;        // offset 23
    uint32_t file_change_counter;          // offset 24
    uint32_t db_size_in_pages;             // offset 28
    uint32_t first_freelist_trunk_page;    // offset 32
    uint32_t freelist_page_count;          // offset 36
    uint32_t schema_cookie;                // offset 40
    uint32_t schema_format;                // offset 44
    uint32_t default_page_cache_size;      // offset 48
    uint32_t largest_root_btree_page;      // offset 52
    uint32_t text_encoding;                // offset 56
    uint32_t user_version;                 // offset 60
    uint32_t incremental_vacuum_enabled;   // offset 64
    uint32_t application_id;               // offset 68
    // uint8_t  reserved[20];                 // offset 72
    uint32_t version_valid_for;            // offset 92
    uint32_t sqlite_version_number;        // offset 96
} FileHeader;

// resolves the page size field into the actual page size in bytes,
// translating the marker value 1 into 65536
uint32_t page_size_bytes(uint16_t page_size);

int read_file_header(FILE* file, FileHeader* header);

#endif // CODECRAFTERS_SQLITE_FILE_HEADER_H
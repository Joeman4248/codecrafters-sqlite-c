#include <stdlib.h>
#include <string.h>

#include "include/lib-sqlite/schema.h"
#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/record.h"

// type strings, indexed by their SchemaType value; SCHEMA_TYPE_COUNT keeps
// this list in sync with the enum
static const char* const TYPE_NAMES[SCHEMA_TYPE_COUNT] = {
    [SCHEMA_TYPE_TABLE]   = "table",
    [SCHEMA_TYPE_INDEX]   = "index",
    [SCHEMA_TYPE_VIEW]    = "view",
    [SCHEMA_TYPE_TRIGGER] = "trigger",
};

// maps a sqlite_schema type string to its enum value
static enum SchemaType type_from_text(const char* text) {
    for (int i = 0; i < SCHEMA_TYPE_COUNT; i++) {
        if (strcmp(text, TYPE_NAMES[i]) == 0) {
            return (enum SchemaType) i;
        }
    }
    return SCHEMA_TYPE_UNKNOWN;
}

// fills db->schema from the in-memory schema root page
static int parse_schema_page(Database* db, const uint8_t* page, const uint32_t page_size) {
    PageHeader schema_page;
    if (read_btree_header(page, page_size, SCHEMA_ROOT_PAGE, &schema_page) != 0) {
        return 1;
    }

    uint16_t cell_offsets[schema_page.cell_count];
    if (read_cell_pointers(page, page_size, SCHEMA_ROOT_PAGE, &schema_page, cell_offsets) != 0) {
        return 1;
    }

    if (schema_page.cell_count > 0) {
        db->schema = calloc(schema_page.cell_count, sizeof(SchemaEntry));
        if (db->schema == NULL) return 1;
    }
    db->schema_count = schema_page.cell_count;

    const uint32_t usable_size = page_size - db->file_header.reserved_space;
    const uint8_t* payload = NULL;
    uint64_t payload_size = 0;
    uint64_t rowid = 0;

    for (uint32_t i = 0; i < schema_page.cell_count; i++) {

        if (read_leaf_table_cell(page, usable_size, cell_offsets[i],
                &payload, &payload_size, &rowid) != 0) {
            return 1;
        }

        RecordColumn columns[16];
        uint32_t column_count = 0;
        if (parse_record(payload, (uint32_t) payload_size,
                columns, sizeof(columns) / sizeof(columns[0]), &column_count) != 0) {
            return 1;
        }

        if (column_count < 2) {
            return 1;
        }

        SchemaEntry* entry = &db->schema[i];

        // sqlite_schema columns: type, name, tbl_name, rootpage, sql
        char type[SCHEMA_TYPE_MAX];
        if (record_get_text(payload, (uint32_t) payload_size,
                columns, column_count, SCHEMA_COL_TYPE, type, sizeof(type)) != 0) {
            return 1;
        }
        entry->type = type_from_text(type);

        if (record_get_text(payload, (uint32_t) payload_size,
                columns, column_count, SCHEMA_COL_NAME, entry->name, sizeof(entry->name)) != 0) {
            return 1;
        }
    }

    return 0;
}

int read_schema(Database* db) {
    // pages can be up to 64 KiB, so keep the buffer off the stack
    const uint32_t page_size = page_size_bytes(db->file_header.page_size);
    uint8_t* page = malloc(page_size);
    if (page == NULL) return 1;

    int result = read_page(db, SCHEMA_ROOT_PAGE, page);
    if (result == 0) {
        result = parse_schema_page(db, page, page_size);
    }

    free(page);
    return result;
}
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
static enum SchemaType schema_type_from_text(const char* text) {
    for (int i = 0; i < SCHEMA_TYPE_COUNT; i++) {
        if (strcmp(text, TYPE_NAMES[i]) == 0) {
            return (enum SchemaType) i;
        }
    }
    return SCHEMA_TYPE_UNKNOWN;
}

// allocates *out to fit the column and copies the text into it; leaves *out
// NULL for a NULL column (sql is NULL for automatic indexes)
static int alloc_column_text(const uint8_t* payload, uint32_t size,
        const RecordColumn* columns, uint32_t column_count,
        const uint32_t index, char** out) {
    if (columns[index].serial_type == SERIAL_NULL) return 0;

    const uint32_t len = columns[index].value_size;
    *out = malloc(len + 1);
    if (*out == NULL) return 1;
    return record_get_text(payload, size, columns, column_count, index, *out, len + 1);
}

// fills db->schema from the in-memory schema root page
static int parse_schema_page(Database* db, const uint8_t* page) {
    const uint32_t page_size = db->file_header.page_size;
    PageHeader schema_page;
    if (read_btree_header(page, page_size, SCHEMA_ROOT_PAGE, &schema_page) != 0) {
        return 1;
    }

    if (schema_page.page_type != PAGE_TYPE_LEAF_TABLE) {
        fprintf(stderr, "multi-page schema not supported yet\n");
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

    const uint32_t usable_size = db->file_header.usable_size;
    const uint8_t* payload = NULL;
    uint32_t payload_size = 0;
    int64_t rowid = 0;

    for (uint32_t i = 0; i < schema_page.cell_count; i++) {

        if (read_leaf_table_cell(page, usable_size, cell_offsets[i],
                &payload, &payload_size, &rowid) != 0) {
            return 1;
        }

        RecordColumn columns[SCHEMA_COL_COUNT];
        uint32_t column_count = 0;
        if (parse_record(payload, payload_size,
                columns, SCHEMA_COL_COUNT, &column_count) != 0) {
            return 1;
        }

        if (column_count != SCHEMA_COL_COUNT) {
            return 1;
        }

        // sqlite_schema columns: type, name, tbl_name, rootpage, sql
        SchemaEntry* entry = &db->schema[i];

        char type[SCHEMA_TYPE_MAX];
        if (record_get_text(payload, payload_size,
                columns, column_count, SCHEMA_COL_TYPE, type, sizeof(type)) != 0) {
            return 1;
        }
        entry->type = schema_type_from_text(type);
        
        // page numbers are positive and fit in 32 bits; views and triggers use 0
        switch (entry->type) {
            case SCHEMA_TYPE_TABLE:
            case SCHEMA_TYPE_INDEX:
                int64_t rootpage;
                if (record_get_int(payload, payload_size,
                        columns, SCHEMA_COL_COUNT, SCHEMA_COL_ROOTPAGE, &rootpage) != 0 ||
                    rootpage < 0 || rootpage > UINT32_MAX) {
                    return 1;
                }
                entry->rootpage = (uint32_t) rootpage;
                break;

            case SCHEMA_TYPE_VIEW:
            case SCHEMA_TYPE_TRIGGER:
                entry->rootpage = 0;
                break;

            case SCHEMA_TYPE_UNKNOWN:
                return 1;
        }
        
        if (alloc_column_text(payload, payload_size,
                columns, SCHEMA_COL_COUNT, SCHEMA_COL_NAME, &entry->name) ||
            alloc_column_text(payload, payload_size,
                columns, SCHEMA_COL_COUNT, SCHEMA_COL_TBL_NAME, &entry->tbl_name) ||
            alloc_column_text(payload, payload_size,
                columns, SCHEMA_COL_COUNT, SCHEMA_COL_SQL, &entry->sql)) {
            return 1;
        }
    }

    return 0;
}

int read_schema(Database* db) {
    // pages can be up to 64 KiB, so keep the buffer off the stack
    const uint32_t page_size = db->file_header.page_size;
    uint8_t* page = malloc(page_size);
    if (page == NULL) return 1;

    int result = read_page(db, SCHEMA_ROOT_PAGE, page);
    if (result == 0) {
        result = parse_schema_page(db, page);
    }

    free(page);
    return result;
}

const SchemaEntry* find_table(const Database* db, const char* name) {
    for (uint32_t i = 0; i < db->schema_count; i++) {
        const SchemaEntry* entry = &db->schema[i];
        if (entry->type == SCHEMA_TYPE_TABLE && strcmp(entry->name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}
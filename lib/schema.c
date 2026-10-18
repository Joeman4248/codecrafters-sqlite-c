#include <stdlib.h>
#include <string.h>

#include "include/lib-sqlite/schema.h"
#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/record.h"

// type strings, indexed by their SchemaType value; SCHEMA_TYPE_COUNT keeps
// this list in sync with the enum
static const StringView TYPE_NAMES[SCHEMA_TYPE_COUNT] = {
    [SCHEMA_TYPE_TABLE]   = SV_LIT("table"),
    [SCHEMA_TYPE_INDEX]   = SV_LIT("index"),
    [SCHEMA_TYPE_VIEW]    = SV_LIT("view"),
    [SCHEMA_TYPE_TRIGGER] = SV_LIT("trigger"),
};

// maps a sqlite_schema type string to its enum value
static enum SchemaType schema_type_from_text(const StringView text) {
    const int32_t i = sv_find(TYPE_NAMES, SCHEMA_TYPE_COUNT, text);
    return (i < 0) ? SCHEMA_TYPE_UNKNOWN : (enum SchemaType) i;
}

// copies a text column into a new heap string in *out, since schema entries
// outlive the page they're read from. Leaves *out NULL for a NULL column (sql
// is NULL for automatic indexes).
static int alloc_column_text(const ByteView payload,
        const RecordColumn* columns, uint32_t column_count,
        const uint32_t index, char** out) {
    if (columns[index].serial_type == SERIAL_NULL) return 0;

    StringView text;
    if (record_text_view(payload, columns, column_count, index, &text)) return 1;

    *out = sv_to_cstr(text);
    return *out == NULL;
}

// fills db->schema from the in-memory schema root page
static int parse_schema_page(Database* db, const ByteView page) {
    PageHeader schema_page;
    if (read_btree_header(page, SCHEMA_ROOT_PAGE, &schema_page) != 0) {
        return 1;
    }

    if (schema_page.page_type != PAGE_TYPE_LEAF_TABLE) {
        fprintf(stderr, "multi-page schema not supported yet\n");
        return 1;
    }

    // a new database has an empty schema. Returning here leaves db->schema
    // NULL and db->schema_count 0, and keeps the arrays below from having a
    // length of 0, which is undefined for a variable-length array.
    const uint16_t schema_count = schema_page.cell_count;
    if (schema_count == 0) return 0;

    uint16_t cell_offsets[schema_count];
    if (read_cell_pointers(page, SCHEMA_ROOT_PAGE, &schema_page, cell_offsets) != 0) {
        return 1;
    }

    db->schema = calloc(schema_count, sizeof(SchemaEntry));
    if (db->schema == NULL) return 1;
    db->schema_count = schema_count;

    const ByteView usable = bv_prefix(page, db->file_header.usable_size);
    ByteView payload;
    int64_t rowid = 0;

    for (uint32_t i = 0; i < schema_count; i++) {
        if (read_leaf_table_cell(usable, cell_offsets[i], &payload, &rowid) != 0) {
            return 1;
        }

        RecordColumn columns[SCHEMA_COL_COUNT];
        uint32_t column_count = 0;
        if (parse_record(payload, columns, SCHEMA_COL_COUNT, &column_count) != 0 ||
            column_count != SCHEMA_COL_COUNT) {
            return 1;
        }

        // sqlite_schema columns: type, name, tbl_name, rootpage, sql
        SchemaEntry* entry = &db->schema[i];

        // compared where it sits in the page. A type that isn't text stays
        // empty, and so unknown, rather than failing the whole schema.
        StringView type = { 0 };
        if (serial_type_is_text(columns[SCHEMA_COL_TYPE].serial_type) &&
            record_text_view(payload,
                columns, column_count, SCHEMA_COL_TYPE, &type) != 0) {
            return 1;
        }
        entry->type = schema_type_from_text(type);
        
        // page numbers are positive and fit in 32 bits; views and triggers use 0
        int64_t rootpage = 0;
        if (record_get_int(payload,
                columns, SCHEMA_COL_COUNT, SCHEMA_COL_ROOTPAGE, &rootpage) != 0 ||
            rootpage < 0 || rootpage > UINT32_MAX) {
            return 1;
        }
        entry->rootpage = (uint32_t) rootpage;
        
        if (alloc_column_text(payload,
                columns, SCHEMA_COL_COUNT, SCHEMA_COL_NAME, &entry->name) ||
            alloc_column_text(payload,
                columns, SCHEMA_COL_COUNT, SCHEMA_COL_TBL_NAME, &entry->tbl_name) ||
            alloc_column_text(payload,
                columns, SCHEMA_COL_COUNT, SCHEMA_COL_SQL, &entry->sql)) {
            return 1;
        }
    }

    return 0;
}

int read_schema(Database* db) {
    const uint32_t page_size = db->file_header.page_size;
    uint8_t* page = malloc(page_size);
    if (page == NULL) return 1;

    int result = read_page(db, SCHEMA_ROOT_PAGE, page);
    if (result == 0) {
        result = parse_schema_page(db, (ByteView) { page, page_size });
    }

    free(page);
    return result;
}

const SchemaEntry* find_table(const Database* db, const StringView name) {
    for (uint32_t i = 0; i < db->schema_count; i++) {
        const SchemaEntry* entry = &db->schema[i];
        if (entry->type == SCHEMA_TYPE_TABLE &&
            sv_equal_nocase(sv_from_cstr(entry->name), name)) {
            return entry;
        }
    }
    return NULL;
}
#include <stdlib.h>
#include <string.h>

#include "storage/schema.h"
#include "storage/btree_page.h"
#include "storage/record.h"

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
    uint32_t i;
    if (sv_find(TYPE_NAMES, SCHEMA_TYPE_COUNT, text, &i)) return (enum SchemaType) i;
    return SCHEMA_TYPE_UNKNOWN;
}

// copies a text column into a new heap string in *out, since schema entries
// outlive the page they're read from. Leaves *out NULL for a NULL column (sql
// is NULL for automatic indexes).
static int alloc_column_text(const Record* record, const uint32_t index, char** out) {
    Value value;
    if (record_get_value(record, index, &value)) return 1;
    if (value.type == VALUE_NULL) return 0;
    if (value.type != VALUE_TEXT) return 1;

    *out = sv_to_cstr(value.text);
    return *out == NULL;
}

// fills db->schema from the in-memory schema root page
static int parse_schema_page(Database* db, const Page* page) {
    PageHeader schema_page;
    if (read_btree_header(page, &schema_page)) {
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
    if (read_cell_pointers(page, &schema_page, cell_offsets)) {
        return 1;
    }

    db->schema = calloc(schema_count, sizeof(SchemaEntry));
    if (db->schema == NULL) return 1;
    db->schema_count = schema_count;

    for (uint32_t i = 0; i < schema_count; i++) {
        TableLeafCell cell;
        if (read_leaf_table_cell(page, cell_offsets[i], &cell)) return 1;

        RecordColumn columns[SCHEMA_COL_COUNT];
        Record record = { .header = { .columns = columns, .capacity = SCHEMA_COL_COUNT } };
        if (parse_record(cell.payload, &record) || record.header.count != SCHEMA_COL_COUNT) {
            return 1;
        }

        // sqlite_schema columns: type, name, tbl_name, rootpage, sql
        SchemaEntry* entry = &db->schema[i];

        // compared where it sits in the page. A type that isn't text stays
        // empty, and so unknown, rather than failing the whole schema.
        Value type;
        if (record_get_value(&record, SCHEMA_COL_TYPE, &type)) return 1;
        entry->type = (type.type == VALUE_TEXT) ? schema_type_from_text(type.text)
                                                : SCHEMA_TYPE_UNKNOWN;

        // page numbers are positive and fit in 32 bits; views and triggers use 0
        Value rootpage;
        if (record_get_value(&record, SCHEMA_COL_ROOTPAGE, &rootpage) ||
            rootpage.type != VALUE_INTEGER ||
            rootpage.integer < 0 || rootpage.integer > UINT32_MAX) {
            return 1;
        }
        entry->rootpage = (uint32_t) rootpage.integer;

        // allocate schema entry text fields as null-terminated c-strings
        // schema entries may outlive the heap-allocated pages they are read from
        if (alloc_column_text(&record, SCHEMA_COL_NAME, &entry->name) ||
            alloc_column_text(&record, SCHEMA_COL_TBL_NAME, &entry->tbl_name) ||
            alloc_column_text(&record, SCHEMA_COL_SQL, &entry->sql)) {
            return 1;
        }
    }

    return 0;
}

int read_schema(Database* db) {
    Page page;
    if (read_page(db, SCHEMA_ROOT_PAGE, &page)) return 1;
    const int result = parse_schema_page(db, &page);
    free_page(&page);
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
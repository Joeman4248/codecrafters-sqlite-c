#include <stdlib.h>
#include <string.h>

#include "record/schema.h"
#include "btree/btree_cursor.h"
#include "btree/btree_page.h"
#include "record/record.h"

// type strings, indexed by their SchemaType value; SCHEMA_TYPE_COUNT keeps
// this list in sync with the enum
static const StringView SCHEMA_TYPE_NAMES[SCHEMA_TYPE_COUNT] = {
    [SCHEMA_TYPE_TABLE]   = SV_LIT("table"),
    [SCHEMA_TYPE_INDEX]   = SV_LIT("index"),
    [SCHEMA_TYPE_VIEW]    = SV_LIT("view"),
    [SCHEMA_TYPE_TRIGGER] = SV_LIT("trigger"),
};

// maps a sqlite_schema type string to its enum value
static enum SchemaType schema_type_from_text(const StringView text) {
    return (enum SchemaType) sv_find(SCHEMA_TYPE_NAMES, SCHEMA_TYPE_COUNT, text);
}

// copies a text column into a new heap string in *out, since schema entries
// outlive the page they're read from. Leaves *out NULL for a NULL column (sql
// is NULL for automatic indexes).
static int alloc_column_text(const Record* record, const uint32_t index, char** out) {
    const Value value = record_get_value(record, index);
    if (value.type == VALUE_NULL) return 0;
    if (value.type != VALUE_TEXT) return 1;

    *out = sv_to_cstr(value.text);
    return *out == NULL;
}

// makes room for one more entry in db->schema, growing it if it is full. New
// slots are zeroed, so an entry a failed parse leaves partly filled has NULL
// strings for close_database to free.
static int grow_schema(Database* db, uint32_t* capacity) {
    if (db->schema_count < *capacity) return 0;

    // a full array doubles, so the copying realloc does adds up to a constant
    // amount per entry rather than growing with the schema
    const uint32_t grown_capacity = (*capacity == 0) ? 8 : *capacity * 2;
    if (grown_capacity < *capacity) return 1;   // doubling would wrap around

    SchemaEntry* grown = realloc(db->schema, grown_capacity * sizeof(SchemaEntry));
    if (grown == NULL) return 1;    // the old array is still valid, caller frees it

    memset(&grown[*capacity], 0, (grown_capacity - *capacity) * sizeof(SchemaEntry));
    db->schema = grown;
    *capacity = grown_capacity;
    return 0;
}

// parses one sqlite_schema row into a new entry at the end of db->schema
static int add_schema_entry(Database* db, uint32_t* capacity, const ByteView payload) {
    RecordColumn columns[SCHEMA_COL_COUNT];
    Record record = { .header = { .columns = columns, .capacity = SCHEMA_COL_COUNT } };
    if (parse_record(payload, &record) || record.header.count != SCHEMA_COL_COUNT) {
        return 1;
    }

    if (grow_schema(db, capacity)) return 1;

    // counted before it is filled, so close_database frees it if a later step fails
    SchemaEntry* entry = &db->schema[db->schema_count++];

    // sqlite_schema columns: type, name, tbl_name, rootpage, sql.
    // The type is compared where it sits in the page. A type that isn't text stays
    // empty, and so unknown, rather than failing the whole schema.
    const Value type = record_get_value(&record, SCHEMA_COL_TYPE);
    entry->type = (type.type == VALUE_TEXT) ? schema_type_from_text(type.text)
                                            : SCHEMA_TYPE_UNKNOWN;

    // page numbers are positive and fit in 32 bits; views and triggers use 0
    const Value rootpage = record_get_value(&record, SCHEMA_COL_ROOTPAGE);
    if (rootpage.type != VALUE_INTEGER ||
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

    return 0;
}

int read_schema(Database* db) {
    // the schema is a table b-tree like any other, rooted at page 1, so a
    // cursor walks it whether it fits on one leaf page or spans several
    TableCursor cursor;
    if (cursor_open(&cursor, &db->pager, SCHEMA_ROOT_PAGE)) return 1;

    uint32_t capacity = 0;
    int result = 0;

    TableLeafCell cell;
    enum CursorStatus status;
    while ((status = cursor_next(&cursor, &cell)) == CURSOR_ROW) {
        if (add_schema_entry(db, &capacity, cell.payload)) {
            result = 1;
            break;
        }
    }
    if (status == CURSOR_ERROR) result = 1;

    cursor_close(&cursor);
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
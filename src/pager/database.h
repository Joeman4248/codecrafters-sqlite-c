#ifndef CODECRAFTERS_SQLITE_DATABASE_H
#define CODECRAFTERS_SQLITE_DATABASE_H

#include <stdint.h>
#include <stdio.h>

#include "pager/file_header.h"
#include "pager/pager.h"
#include "util/view.h"

// the value stored in a sqlite_schema row's type column
enum SchemaType {
    SCHEMA_TYPE_TABLE,
    SCHEMA_TYPE_INDEX,
    SCHEMA_TYPE_VIEW,
    SCHEMA_TYPE_TRIGGER,
    SCHEMA_TYPE_COUNT,                          // number of real type names
    SCHEMA_TYPE_UNKNOWN = SCHEMA_TYPE_COUNT,    // not one of the four standard types
};

// one row of sqlite_schema, storing the fields .tables uses
typedef struct SchemaEntry {
    enum SchemaType type;
    char*           name;
    char*           tbl_name;
    char*           sql;
    uint32_t        rootpage;
} SchemaEntry;

// column indices into a sqlite_schema record
enum SchemaColumn {
    SCHEMA_COL_TYPE,        // 'table' | 'index' | 'view' | 'trigger'
    SCHEMA_COL_NAME,        // object name
    SCHEMA_COL_TBL_NAME,    // table the object belongs to
    SCHEMA_COL_ROOTPAGE,    // root b-tree page (0 for views and triggers)
    SCHEMA_COL_SQL,         // original CREATE statement
    SCHEMA_COL_COUNT,       // number of columns in a sqlite_schema record
};

typedef struct Database {
    FILE*       file;             // the open database file
    FileHeader  file_header;      // header fields read from the start of the file
    Pager       pager;            // reads and caches the file's pages

    SchemaEntry* schema;          // populated by read_schema(), one entry per row
    uint32_t    schema_count;     // number of objects in the schema
} Database;

// closes the pager and the database file and frees the schema. Safe to call on a
// zero-initialised or already-closed Database.
void close_database(Database* db);

#endif // CODECRAFTERS_SQLITE_DATABASE_H
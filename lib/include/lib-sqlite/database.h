#ifndef CODECRAFTERS_SQLITE_DATABASE_H
#define CODECRAFTERS_SQLITE_DATABASE_H

#include <stdint.h>
#include <stdio.h>

#include "file_header.h"

#define SCHEMA_TYPE_MAX 16            // "trigger" is the longest type string
#define SCHEMA_NAME_MAX 256           // longest table/index name we support

// column indices into a sqlite_schema record
enum SchemaColumn {
    SCHEMA_COL_TYPE,        // 'table' | 'index' | 'view' | 'trigger'
    SCHEMA_COL_NAME,        // object name
    SCHEMA_COL_TBL_NAME,    // table the object belongs to
    SCHEMA_COL_ROOTPAGE,    // root b-tree page (0 for views and triggers)
    SCHEMA_COL_SQL,         // original CREATE statement
};

// one row of sqlite_schema, storing the fields .tables uses
typedef struct {
    char type[SCHEMA_TYPE_MAX];
    char name[SCHEMA_NAME_MAX];
} SchemaEntry;

typedef struct {
    FILE*       file;             // the open database file
    FileHeader  file_header;      // header fields read from the start of the file

    SchemaEntry* schema;          // populated by read_schema(), one entry per row
    uint32_t    schema_count;     // number of objects in the schema
} Database;

#endif // CODECRAFTERS_SQLITE_DATABASE_H
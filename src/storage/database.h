#ifndef CODECRAFTERS_SQLITE_DATABASE_H
#define CODECRAFTERS_SQLITE_DATABASE_H

#include <stdint.h>
#include <stdio.h>

#include "storage/file_header.h"
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
typedef struct {
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

typedef struct {
    FILE*       file;             // the open database file
    FileHeader  file_header;      // header fields read from the start of the file

    SchemaEntry* schema;          // populated by read_schema(), one entry per row
    uint32_t    schema_count;     // number of objects in the schema
} Database;

// one page of the database, read into memory: its bytes, and which page it is.
// The bytes stop at the usable size, leaving out the reserved space at the end
// of the page, which no b-tree structure uses.
typedef struct {
    ByteView bytes;
    uint32_t number;    // pages are numbered from 1
} Page;

// reads page number of the database file into buf, which must hold at least
// the database's page size in bytes, and points *page at it. Returns nonzero
// if the page can't be read.
int read_page(const Database* db, uint32_t number, uint8_t* buf, Page* page);

// closes the database file and frees the schema. Safe to call on a
// zero-initialised or already-closed Database.
void close_database(Database* db);

#endif // CODECRAFTERS_SQLITE_DATABASE_H
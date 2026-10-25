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

// one page of the database, read into memory it owns, and which page it is
typedef struct {
    uint8_t* data;      // the page's bytes, freed with free_page
    uint32_t size;      // bytes to read from data: the usable size, which leaves
                        // out the reserved space at the end that no b-tree uses
    uint32_t number;    // pages are numbered from 1
} Page;

// the page's bytes as a read-only view, for reading through a Reader
static inline ByteView page_bytes(const Page* page) {
    return (ByteView) { page->data, page->size };
}

// reads page number of the database file into newly allocated memory in
// *page. On success the caller releases it with free_page. On failure
// returns nonzero and leaves nothing to free.
int read_page(const Database* db, const uint32_t number, Page* page);

// frees the page's memory. Safe to call on a zero-initialised or already
// freed Page.
void free_page(Page* page);

// closes the database file and frees the schema. Safe to call on a
// zero-initialised or already-closed Database.
void close_database(Database* db);

#endif // CODECRAFTERS_SQLITE_DATABASE_H
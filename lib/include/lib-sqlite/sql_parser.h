#ifndef CODECRAFTERS_SQLITE_SQL_PARSER_H
#define CODECRAFTERS_SQLITE_SQL_PARSER_H

#include <stdint.h>

#include "tokenizer.h"

// the column names from a CREATE TABLE statement. The names are tokens: they
// are not copied or null-terminated, but point into the SQL string, which
// must outlive the TableColumns.
typedef struct {
    Token    table_name;
    Token*   columns;       // heap-allocated, in declaration order
    uint32_t count;
    int32_t  rowid_column;  // index of the INTEGER PRIMARY KEY column, or -1
} TableColumns;

// the parts of a SELECT query. Like TableColumns, the tokens point into the
// query string, which must outlive the SelectQuery.
typedef struct {
    Token column;
    Token table;
} SelectQuery;

// parses a query of the form:
//
//     SELECT column FROM table
//
// with nothing after the table name. On success fills in *query; nothing is
// allocated, so there is nothing to free. On failure prints an error to
// stderr and returns nonzero.
int parse_select(const char* sql, SelectQuery* query);

// parses a CREATE TABLE statement such as the sql column of a sqlite_schema
// row:
//
//     CREATE TABLE name (column type constraints..., column ..., ...)
//
// Each column's type and constraints are skipped, including anything inside
// nested parentheses such as varchar(255). Table-level constraints such as
// PRIMARY KEY (a, b) are not recognised and would be read as columns.
//
// On success fills in *columns, which the caller releases with
// free_table_columns. On failure prints an error to stderr, leaves nothing
// to free and returns nonzero.
int parse_table_columns(const char* sql, TableColumns* columns);

// frees the column list. Safe to call on a zero-initialised TableColumns.
void free_table_columns(TableColumns* columns);

// returns the position of the column whose name matches the text of name
// (compared ignoring case), or -1 if there is none
int32_t find_column(const TableColumns* columns, Token name);

#endif // CODECRAFTERS_SQLITE_SQL_PARSER_H

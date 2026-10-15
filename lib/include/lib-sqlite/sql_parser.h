#ifndef CODECRAFTERS_SQLITE_SQL_PARSER_H
#define CODECRAFTERS_SQLITE_SQL_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "tokenizer.h"

// the column names from a CREATE TABLE statement. The names are tokens: they
// are not copied or null-terminated, but point into the SQL string, which
// must outlive the TableColumns.
typedef struct {
    Token    table;
    Token*   columns;       // heap-allocated, in declaration order
    uint32_t count;
    int32_t  rowid_column;  // index of the INTEGER PRIMARY KEY column, or -1
} TableColumns;

// the parts of a SELECT query. Like TableColumns, the tokens point into the
// query string, which must outlive the SelectQuery.
typedef struct {
    Token table;
    Token name;         // a column name, or TOKEN_STAR. Inside COUNT, * means
                        // every row; outside it, every column.
    bool is_count;      // COUNT(name) rather than name itself
} SelectQuery;

// returns the position of the column whose name matches the text of name
// (compared ignoring case), or -1 if there is none
int32_t find_column(const TableColumns* columns, Token name);

// which kind of statement a Statement holds
enum StatementType {
    STATEMENT_TYPE_SELECT,
    STATEMENT_TYPE_CREATE_TABLE,
    STATEMENT_TYPE_COUNT,                               // number of real types
    STATEMENT_TYPE_UNKNOWN = STATEMENT_TYPE_COUNT,      // not a supported statement
};

// a parsed statement. Only the union member matching type is valid.
typedef struct {
    enum StatementType type;
    union {
        SelectQuery  select;
        TableColumns create_table;
    };
} Statement;

// parses one SQL statement, choosing the parser by its first keyword. The
// supported forms are:
//
//     SELECT column FROM table        (column may be *)
//     SELECT COUNT(column) FROM table (column may be *)
//     CREATE TABLE name (column type constraints..., ...)
//
// A SELECT must have nothing after the table name. A CREATE TABLE, such as
// the sql column of a sqlite_schema row, is parsed for its column names only:
// each column's type and constraints are skipped, including anything inside
// nested parentheses such as varchar(255). Table-level constraints such as
// PRIMARY KEY (a, b) are not recognised and would be read as columns.
//
// On success fills in *statement, which the caller releases with
// free_statement. On failure prints an error to stderr, leaves nothing to
// free and returns nonzero.
int parse_statement(const char* sql, Statement* statement);

// frees whatever the statement allocated
void free_statement(Statement* statement);

#endif // CODECRAFTERS_SQLITE_SQL_PARSER_H

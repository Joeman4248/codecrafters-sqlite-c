#ifndef CODECRAFTERS_SQLITE_SQL_PARSER_H
#define CODECRAFTERS_SQLITE_SQL_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "tokenizer.h"

// a heap-allocated list of tokens that grows as tokens are added. Freed by
// free_statement along with the statement holding it.
typedef struct {
    Token*   items;
    uint32_t count;
} TokenList;

// the column names from a CREATE TABLE statement. The names are tokens: they
// are not copied or null-terminated, but point into the SQL string, which
// must outlive the TableColumns.
typedef struct {
    Token     table;
    TokenList names;            // in declaration order
    bool      has_rowid_alias;  // whether a column is the INTEGER PRIMARY KEY
    uint32_t  rowid_column;     // its index, when has_rowid_alias
} TableColumns;

// the parts of a CREATE INDEX statement. Like TableColumns, the tokens point
// into the SQL string, which must outlive the IndexColumns.
typedef struct {
    Token     name;
    Token     table;
    TokenList columns;      // the indexed column names, in index order
    bool      is_plain;     // every column is a bare name in ascending order and
                            // there is no WHERE, so the index holds an entry for
                            // every row, sorted by the columns' values
} IndexColumns;

// a WHERE condition of the form column = 'text'
typedef struct {
    Token column;   // TOKEN_IDENTIFIER: the column to compare
    Token value;    // TOKEN_STRING: the text it must equal, without the quotes
} Condition;

// the parts of a SELECT query. Like TableColumns, the tokens point into the
// query string, which must outlive the SelectQuery.
typedef struct {
    Token     table;
    TokenList names;        // column names and TOKEN_STARs, in query order. A *
                            // means every column, except inside COUNT, where the
                            // list holds just the one name or *, and * means every row.
    bool      is_count;     // COUNT(names.items) rather than the values themselves
    bool      has_where;    // whether the query has a WHERE clause
    Condition where;        // only valid when has_where is true
} SelectQuery;

// sets *index to the position of the column whose name matches the text of
// name (compared ignoring case). Returns false, leaving *index alone, if there
// is none.
bool find_column(const TableColumns* columns, Token name, uint32_t* index);

// which kind of statement a Statement holds
enum StatementType {
    STATEMENT_TYPE_SELECT,
    STATEMENT_TYPE_CREATE_TABLE,
    STATEMENT_TYPE_CREATE_INDEX,
    STATEMENT_TYPE_COUNT,                               // number of real types
    STATEMENT_TYPE_UNKNOWN = STATEMENT_TYPE_COUNT,      // not a supported statement
};

// a parsed statement. Only the union member matching type is valid.
typedef struct {
    enum StatementType type;
    union {
        SelectQuery  select;
        TableColumns create_table;
        IndexColumns create_index;
    };
} Statement;

// parses one SQL statement, choosing the parser by its first keywords. The
// supported forms are:
//
//     SELECT names FROM table [WHERE column = 'text']
//     SELECT COUNT(column) FROM table [WHERE column = 'text']
//     CREATE TABLE name (column type constraints..., ...)
//     CREATE [UNIQUE] INDEX name ON table (column, ...)
//
// In a SELECT, names is a comma-separated list of columns and *s, and the
// column in COUNT may be *. Nothing may follow the table name but the WHERE.
//
// A CREATE TABLE, such as the sql column of a sqlite_schema row, is parsed for
// its column names only: each column's type and constraints are skipped,
// including anything inside nested parentheses such as varchar(255).
// Table-level constraints such as PRIMARY KEY (a, b) are not recognised and
// would be read as columns.
//
// A CREATE INDEX is parsed for its column names. Anything else after a name
// (DESC, COLLATE, an expression) or after the column list (a WHERE) is
// skipped and clears is_plain.
//
// On success fills in *statement, which the caller releases with
// free_statement. On failure prints an error to stderr, leaves nothing to
// free and returns nonzero.
int parse_statement(const char* sql, Statement* statement);

// frees whatever the statement allocated
void free_statement(Statement* statement);

#endif // CODECRAFTERS_SQLITE_SQL_PARSER_H

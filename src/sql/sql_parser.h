#ifndef CODECRAFTERS_SQLITE_SQL_PARSER_H
#define CODECRAFTERS_SQLITE_SQL_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "sql/tokenizer.h"

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
    Token     table;        // the table it indexes
    TokenList columns;      // the indexed column names, in index order
    bool      has_where;    // a partial index (... WHERE ...), with entries for only some rows
    Token     name;         // the index's name
} IndexColumns;

// a WHERE condition of the form column = 'text' or column = 123
typedef struct {
    Token column;   // TOKEN_IDENTIFIER: the column to compare
    Token value;    // TOKEN_STRING (the text, without the quotes) or TOKEN_INTEGER:
                    // the value it must equal
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

// parses one SQL statement, matching it against the grammar for the kind of
// statement its first keywords name. The grammar is written out in
// sql_grammar.c; so far it supports:
//
//     SELECT column, ... FROM table [WHERE column = value]
//     SELECT COUNT(column) FROM table [WHERE column = value]
//     CREATE TABLE name (column type constraints..., ...)
//
// A SELECT column may be *, and so may COUNT's. A WHERE value is a 'string'
// or an integer.
//
// A CREATE TABLE, such as the sql column of a sqlite_schema row, is read for
// its column names and which column, if any, is the INTEGER PRIMARY KEY. The
// rest of each column's type and constraints isn't interpreted.
//
// On success fills in *statement, which the caller releases with
// free_statement. On failure prints an error to stderr, leaves nothing to
// free and returns nonzero.
int parse_statement(const char* sql, Statement* statement);

// frees whatever the statement allocated
void free_statement(Statement* statement);

#endif // CODECRAFTERS_SQLITE_SQL_PARSER_H

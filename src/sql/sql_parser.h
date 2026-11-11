#ifndef CODECRAFTERS_SQLITE_SQL_PARSER_H
#define CODECRAFTERS_SQLITE_SQL_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "sql/tokenizer.h"

// the column names from a CREATE TABLE statement. The names are tokens: they
// are not copied or null-terminated, but point into the SQL string, which
// must outlive the TableColumns.
typedef struct TableColumns {
    Token     table;
    TokenList names;            // in declaration order
    bool      has_rowid_alias;  // whether a column is the INTEGER PRIMARY KEY
    uint32_t  rowid_column;     // its index, when has_rowid_alias
} TableColumns;

// the parts of a CREATE INDEX statement. Like TableColumns, the tokens point
// into the SQL string, which must outlive the IndexColumns.
typedef struct IndexColumns {
    Token     table;        // the table it indexes
    TokenList columns;      // the indexed column names, in index order
    bool      has_where;    // a partial index (... WHERE ...), with entries for only some rows
    Token     name;         // the index's name
} IndexColumns;

// the comparison operators a WHERE condition can use. Like CompareOrder they
// are a closed set: nothing looks an operator up by name, so there is no
// COMPARE_COUNT and no COMPARE_UNKNOWN.
enum CompareOp : uint8_t {
    COMPARE_EQUAL,          // = or ==
    COMPARE_LESS,           // <
    COMPARE_LESS_EQUAL,     // <=
    COMPARE_GREATER,        // >
    COMPARE_GREATER_EQUAL,  // >=
    COMPARE_NOT_EQUAL,      // != or <>
};

// a WHERE condition: a column compared with a literal, in either order
typedef struct Condition {
    Token           column;         // TOKEN_IDENTIFIER: the column to compare
    Token           value;          // TOKEN_STRING (the text, without the quotes),
                                    // TOKEN_INTEGER or TOKEN_NULL: the value to compare it with
    enum CompareOp  op;             // how they compare, as the query wrote it
    bool            literal_first;  // whether the query wrote the value before the column
} Condition;

// the parts of a SELECT query. Like TableColumns, the tokens point into the
// query string, which must outlive the SelectQuery.
typedef struct SelectQuery {
    Token     table;
    TokenList names;        // column names and TOKEN_STARs, in query order. A *
                            // means every column, except inside COUNT, where the
                            // list holds just the one name or *, and * means every row.
    bool      is_count;     // COUNT(names.items) rather than the values themselves
    bool      has_where;    // whether the query has a WHERE clause
    Condition where;        // only valid when has_where is true
} SelectQuery;

// the position of the column whose name matches the text of name (compared
// ignoring case), or columns->names.count if there is none: like sv_find, a
// list indexed by position uses one past the end for not found
uint32_t find_column(const TableColumns* columns, Token name);

// which kind of statement a Statement holds
enum StatementType : uint8_t {
    STATEMENT_TYPE_SELECT,
    STATEMENT_TYPE_CREATE_TABLE,
    STATEMENT_TYPE_CREATE_INDEX,
    STATEMENT_TYPE_COUNT,                               // number of real types
    STATEMENT_TYPE_UNKNOWN = STATEMENT_TYPE_COUNT,      // not a supported statement
};

// a parsed statement. Only the union member matching type is valid.
typedef struct Statement {
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
//     SELECT column, ... FROM table [WHERE column op value]
//     SELECT COUNT(column) FROM table [WHERE column op value]
//     CREATE TABLE name (column type constraints..., ...)
//     CREATE INDEX name ON table (column, ...) [WHERE column op value]
//
// A SELECT column may be *, and so may COUNT's. A WHERE compares a column with
// a 'string', an integer or NULL, in either order, using = == < <= > >= != or <>.
//
// A CREATE TABLE, such as the sql column of a sqlite_schema row, is read for
// its column names and which column, if any, is the INTEGER PRIMARY KEY. The
// rest of each column's type and constraints isn't interpreted.
//
// A CREATE INDEX is read for its name, its table and its column names. Words
// after a column name, such as DESC, aren't interpreted, and of a partial
// index's WHERE only its presence is kept.
//
// On success fills in *statement, which the caller releases with
// free_statement. On failure prints an error to stderr, leaves nothing to
// free and returns nonzero.
int parse_statement(const char* sql, Statement* statement);

// frees whatever the statement allocated
void free_statement(Statement* statement);

#endif // CODECRAFTERS_SQLITE_SQL_PARSER_H

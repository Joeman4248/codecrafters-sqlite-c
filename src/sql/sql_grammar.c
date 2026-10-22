#include "sql/sql_grammar.h"

// ---- the grammar -----------------------------------------------------------
//
// The full grammar this parser is heading for, a simplified slice of
// https://sqlite.org/lang.html:
//
//     select_stmt       ::= SELECT result_columns FROM IDENTIFIER [ WHERE IDENTIFIER "=" STRING ] EOF
//     result_columns    ::= COUNT "(" ( IDENTIFIER | "*" ) ")"
//                         | result_column { "," result_column }
//     result_column     ::= IDENTIFIER | "*"
//
//     create_table_stmt ::= CREATE TABLE IDENTIFIER "(" column_def { "," column_def } ")" EOF
//     column_def        ::= IDENTIFIER { type_word | "(" { type_word | "," } ")" }
//     type_word         ::= IDENTIFIER | INTEGER | STRING
//
// [ ] (optional) needs RULE_OPTIONAL, which the engine doesn't have yet, so
// for now SELECT takes one result column and no WHERE.
//
// A column_def is a column's name followed by its type and constraints, such
// as "integer primary key autoincrement" or "varchar(255) not null". Those
// are read as a run of words, with one level of parentheses allowed, and not
// interpreted. Table constraints such as PRIMARY KEY (a, b) have the same
// shape, so they'd be read as a column named PRIMARY.

// result_column ::= IDENTIFIER | "*"
// both are captured as columns; the planner tells * apart by its token type
#define RESULT_COLUMN   CHOICE(TAGGED_TOK(TOKEN_IDENTIFIER, TAG_COLUMN), \
                               TAGGED_TOK(TOKEN_STAR, TAG_COLUMN))

// select_stmt ::= SELECT ( COUNT "(" result_column ")" | result_column ) FROM IDENTIFIER EOF
static const Rule* const SELECT_STMT = SEQ(
    TOK(TOKEN_SELECT),
    CHOICE(
        SEQ(TAGGED_TOK(TOKEN_COUNT, TAG_COUNT), TOK(TOKEN_LPAREN), RESULT_COLUMN, TOK(TOKEN_RPAREN)),
        RESULT_COLUMN),
    TOK(TOKEN_FROM),
    TAGGED_TOK(TOKEN_IDENTIFIER, TAG_TABLE),
    TOK(TOKEN_EOF)
);

// type_word ::= IDENTIFIER | INTEGER | STRING
// words are captured so the builder can spot INTEGER PRIMARY KEY; numbers and
// strings never take part in that, so they aren't
#define TYPE_WORD   CHOICE(TAGGED_TOK(TOKEN_IDENTIFIER, TAG_TYPE_WORD), TOK(TOKEN_INTEGER), TOK(TOKEN_STRING))

// the same, inside a column's parentheses, as in varchar(255). Nothing in
// there is captured: a PRIMARY KEY inside parentheses belongs to something
// else, not the column.
#define INNER_TYPE_WORD   CHOICE(TOK(TOKEN_IDENTIFIER), TOK(TOKEN_INTEGER), TOK(TOKEN_STRING))

// column_def ::= IDENTIFIER { type_word | "(" { type_word | "," } ")" }
#define COLUMN_DEF  SEQ(                                                        \
    TAGGED_TOK(TOKEN_IDENTIFIER, TAG_COLUMN),                                   \
    REPEAT(CHOICE(                                                              \
        TYPE_WORD,                                                              \
        SEQ(TOK(TOKEN_LPAREN), REPEAT(CHOICE(INNER_TYPE_WORD, TOK(TOKEN_COMMA))), TOK(TOKEN_RPAREN)))))

// create_table_stmt ::= CREATE TABLE IDENTIFIER "(" column_def { "," column_def } ")" EOF
static const Rule* const CREATE_TABLE_STMT = SEQ(
    TOK(TOKEN_CREATE),
    TOK(TOKEN_TABLE),
    TAGGED_TOK(TOKEN_IDENTIFIER, TAG_TABLE),
    TOK(TOKEN_LPAREN),
    COLUMN_DEF, REPEAT(SEQ(TOK(TOKEN_COMMA), COLUMN_DEF)),
    TOK(TOKEN_RPAREN),
    TOK(TOKEN_EOF)
);

const Rule* const STATEMENT_RULES[STATEMENT_TYPE_COUNT] = {
    [STATEMENT_TYPE_SELECT]       = SELECT_STMT,
    [STATEMENT_TYPE_CREATE_TABLE] = CREATE_TABLE_STMT,
    [STATEMENT_TYPE_CREATE_INDEX] = NULL,
};

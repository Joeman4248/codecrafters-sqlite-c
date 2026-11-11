#include "sql/sql_grammar.h"

// ---- the grammar -----------------------------------------------------------
//
// The full grammar this parser is heading for, a simplified slice of
// https://sqlite.org/lang.html:
//
//     select_stmt       ::= SELECT result_columns FROM IDENTIFIER [ where_clause ] EOF
//     result_columns    ::= COUNT "(" ( IDENTIFIER | "*" ) ")"
//                         | result_column { "," result_column }
//     result_column     ::= IDENTIFIER | "*"
//     where_clause      ::= WHERE expr_token { expr_token }
//     expr_token        ::= IDENTIFIER | STRING | INTEGER | NULL
//                         | "=" | "==" | "<" | "<=" | ">" | ">=" | "!=" | "<>"
//
//     create_table_stmt ::= CREATE TABLE IDENTIFIER "(" column_def { "," column_def } ")" EOF
//     column_def        ::= IDENTIFIER { type_word | "(" { type_word | "," } ")" }
//     type_word         ::= IDENTIFIER | INTEGER | STRING | NULL
//
//     create_index_stmt ::= CREATE INDEX IDENTIFIER ON IDENTIFIER
//                           "(" indexed_column { "," indexed_column } ")" [ where_clause ] EOF
//     indexed_column    ::= IDENTIFIER { IDENTIFIER }
//
// A column_def is a column's name followed by its type and constraints, such
// as "integer primary key autoincrement" or "varchar(255) not null". Those
// are read as a run of words, with one level of parentheses allowed, and not
// interpreted. Table constraints such as PRIMARY KEY (a, b) have the same
// shape, so they'd be read as a column named PRIMARY.
//
// An indexed_column is a column name, then words such as COLLATE NOCASE or
// DESC, which aren't interpreted. CREATE UNIQUE INDEX and IF NOT EXISTS aren't
// supported yet.
//
// A where_clause only says which tokens an expression may use. Its shape,
// such as which comparison goes with which, is read by parse_expr in expr.c,
// since these rules can't nest the way parentheses will need to.

// result_column ::= IDENTIFIER | "*"
// both are captured as columns; the planner tells * apart by its token type
#define RESULT_COLUMN   CHOICE(TAGGED_TOK(TOKEN_IDENTIFIER, TAG_COLUMN), \
                               TAGGED_TOK(TOKEN_STAR, TAG_COLUMN))

// expr_token ::= IDENTIFIER | STRING | INTEGER | NULL
//              | "=" | "==" | "<" | "<=" | ">" | ">=" | "!=" | "<>"
// == scans as = and <> as !=, so those are all the operator token types there are
#define EXPR_TOKEN  CHOICE(                         \
    TAGGED_TOK(TOKEN_IDENTIFIER, TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_STRING,     TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_INTEGER,    TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_NULL,       TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_EQUALS,     TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_LT,         TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_LE,         TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_GT,         TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_GE,         TAG_WHERE_TOKEN),  \
    TAGGED_TOK(TOKEN_NE,         TAG_WHERE_TOKEN))

// where_clause ::= WHERE expr_token { expr_token }
#define WHERE_CLAUSE    SEQ(TOK(TOKEN_WHERE), EXPR_TOKEN, REPEAT(EXPR_TOKEN))

// select_stmt ::= SELECT ( COUNT "(" result_column ")" | result_column { "," result_column } )
//                 FROM IDENTIFIER [ where_clause ] EOF
static const Rule* const SELECT_STMT = SEQ(
    TOK(TOKEN_SELECT),
    CHOICE(
        SEQ(TAGGED_TOK(TOKEN_COUNT, TAG_COUNT), TOK(TOKEN_LPAREN), RESULT_COLUMN, TOK(TOKEN_RPAREN)),
        SEQ(RESULT_COLUMN, REPEAT(SEQ(TOK(TOKEN_COMMA), RESULT_COLUMN)))),
    TOK(TOKEN_FROM),
    TAGGED_TOK(TOKEN_IDENTIFIER, TAG_TABLE),
    OPTIONAL(WHERE_CLAUSE),
    TOK(TOKEN_EOF)
);

// type_word ::= IDENTIFIER | INTEGER | STRING | NULL
// words are captured so the builder can spot INTEGER PRIMARY KEY; numbers,
// strings and NULL (as in NOT NULL or DEFAULT NULL) never take part in that,
// so they aren't
#define TYPE_WORD   CHOICE(TAGGED_TOK(TOKEN_IDENTIFIER, TAG_TYPE_WORD), TOK(TOKEN_INTEGER), \
                           TOK(TOKEN_STRING), TOK(TOKEN_NULL))

// the same, inside a column's parentheses, as in varchar(255). Nothing in
// there is captured: a PRIMARY KEY inside parentheses belongs to something
// else, not the column.
#define INNER_TYPE_WORD   CHOICE(TOK(TOKEN_IDENTIFIER), TOK(TOKEN_INTEGER), TOK(TOKEN_STRING), \
                                 TOK(TOKEN_NULL))

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

// indexed_column ::= IDENTIFIER { IDENTIFIER }
// only the name is captured; the words after it, such as DESC, aren't needed
#define INDEXED_COLUMN  SEQ(TAGGED_TOK(TOKEN_IDENTIFIER, TAG_COLUMN), REPEAT(TOK(TOKEN_IDENTIFIER)))

// create_index_stmt ::= CREATE INDEX IDENTIFIER ON IDENTIFIER
//                       "(" indexed_column { "," indexed_column } ")" [ where_clause ] EOF
static const Rule* const CREATE_INDEX_STMT = SEQ(
    TOK(TOKEN_CREATE),
    TOK(TOKEN_INDEX),
    TAGGED_TOK(TOKEN_IDENTIFIER, TAG_INDEX),
    TOK(TOKEN_ON),
    TAGGED_TOK(TOKEN_IDENTIFIER, TAG_TABLE),
    TOK(TOKEN_LPAREN),
    INDEXED_COLUMN, REPEAT(SEQ(TOK(TOKEN_COMMA), INDEXED_COLUMN)),
    TOK(TOKEN_RPAREN),
    OPTIONAL(WHERE_CLAUSE),
    TOK(TOKEN_EOF)
);

const Rule* const STATEMENT_RULES[STATEMENT_TYPE_COUNT] = {
    [STATEMENT_TYPE_SELECT]       = SELECT_STMT,
    [STATEMENT_TYPE_CREATE_TABLE] = CREATE_TABLE_STMT,
    [STATEMENT_TYPE_CREATE_INDEX] = CREATE_INDEX_STMT,
};

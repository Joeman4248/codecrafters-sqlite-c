#include <stdio.h>
#include <stdlib.h>

#include "sql/sql_parser.h"
#include "sql/grammar.h"
#include "sql/tokenizer.h"

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
    TOK(TOKEN_EOF),
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
    TOK(TOKEN_EOF),
);

// each statement type's grammar, indexed by its StatementType; NULL if it has
// none yet. CREATE TABLE has a grammar but no builder, so it stays NULL until
// it can be turned into a TableColumns.
static const Rule* const STATEMENT_RULES[STATEMENT_TYPE_COUNT] = {
    [STATEMENT_TYPE_SELECT]       = SELECT_STMT,
    [STATEMENT_TYPE_CREATE_TABLE] = CREATE_TABLE_STMT,
    [STATEMENT_TYPE_CREATE_INDEX] = NULL,
};

bool find_column(const TableColumns* columns, const Token name, uint32_t* index) {
    // an empty name, such as the zeroed token of a missing WHERE, matches no
    // column: every column name has at least one character
    for (uint32_t i = 0; i < columns->names.count; i++) {
        if (sv_equal_nocase(columns->names.items[i].text, name.text)) {
            *index = i;
            return true;
        }
    }
    return false;
}

static int build_create_table(Parser* parser, TableColumns* create_table) {
    
    return 0;
}

static int build_select_statement(Parser* parser, SelectQuery* query) {
    for (uint32_t i = 0; i < parser->capture_count; i++) {
        const Capture capture = parser->captures[i];
        switch (capture.tag) {
            case TAG_COLUMN: if (token_list_add(&query->names, capture.token)) return 1; break;
            case TAG_COUNT:  query->is_count = true; break;
            case TAG_TABLE:  query->table = capture.token; break;
            default: return 1;
        }
    }
    return 0;
}

static int build_statement(Parser* parser, Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:
            return build_select_statement(parser, &statement->select);
        case STATEMENT_TYPE_CREATE_TABLE:
            return build_create_table(parser, &statement->create_table);
        case STATEMENT_TYPE_CREATE_INDEX:
        case STATEMENT_TYPE_UNKNOWN:
            break;
    }
    return 1;
}

// which statement the tokens start, from their first one or two keywords
static enum StatementType statement_type_of(const TokenList* tokens) {
    const enum TokenType first  = tokens->count > 0 ? tokens->items[0].type : TOKEN_EOF;
    const enum TokenType second = tokens->count > 1 ? tokens->items[1].type : TOKEN_EOF;
    switch (first) {
        case TOKEN_SELECT: return STATEMENT_TYPE_SELECT;
        case TOKEN_CREATE:
            switch (second) {
                case TOKEN_TABLE: return STATEMENT_TYPE_CREATE_TABLE;
                case TOKEN_INDEX: return STATEMENT_TYPE_CREATE_INDEX;
                default: break;
            }
            break;
        default: break;
    }
    return STATEMENT_TYPE_UNKNOWN;
}

// matches the tokens against the grammar for statement->type and fills in the
// statement from the captures. On failure, leaves nothing in the statement to free.
static int match_statement(Parser* parser, const char* sql, Statement* statement) {
    const Rule* rule = (statement->type == STATEMENT_TYPE_UNKNOWN) ? NULL
                                                                   : STATEMENT_RULES[statement->type];
    if (rule == NULL) {
        fprintf(stderr, "Unsupported statement: %s\n", sql);
        return 1;
    }
    if (!match_rule(parser, rule)) {
        fprintf(stderr, "Syntax error: %s\n", sql);
        return 1;
    }

    // a build that fails partway may have filled in some of the statement
    const int result = build_statement(parser, statement);
    if (result) free_statement(statement);
    return result;
}

int parse_statement(const char* sql, Statement* statement) {
    // zeroed so free_statement finds nothing to free until the statement is filled in
    *statement = (Statement) { .type = STATEMENT_TYPE_UNKNOWN };

    TokenList tokens = { 0 };
    int result = tokenize_all(sql, &tokens);

    // a token is captured at most once, so one capture per token is always enough.
    // There's always at least the EOF token, so this never asks for 0 bytes.
    Capture* captures = NULL;
    if (result == 0) {
        captures = malloc(tokens.count * sizeof(Capture));
        if (captures == NULL) result = 1;
    }

    if (result == 0) {
        Parser parser = { .pos = 0, .tokens = &tokens, .captures = captures };
        statement->type = statement_type_of(&tokens);
        result = match_statement(&parser, sql, statement);
    }

    free(captures);
    free_token_list(&tokens);
    return result;
}

void free_statement(Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:         free_token_list(&statement->select.names);         break;
        case STATEMENT_TYPE_CREATE_TABLE:   free_token_list(&statement->create_table.names);   break;
        case STATEMENT_TYPE_CREATE_INDEX:   free_token_list(&statement->create_index.columns); break;
        case STATEMENT_TYPE_UNKNOWN:        break;
    }
}

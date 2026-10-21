#include <stdio.h>

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
//     column_def        ::= IDENTIFIER { anything but "," or ")" outside parentheses }
//
// [ ] (optional) and { } (repeated) need RULE_OPTIONAL and RULE_REPEAT, which
// the engine doesn't have yet, so for now SELECT takes one result column and
// no WHERE, and CREATE TABLE isn't written as rules at all.

// result_column ::= IDENTIFIER | "*"
#define RESULT_COLUMN   CHOICE(TOK(TOKEN_IDENTIFIER), TOK(TOKEN_STAR))

// select_stmt ::= SELECT ( COUNT "(" result_column ")" | result_column ) FROM IDENTIFIER EOF
static const Rule* const SELECT_STMT = SEQ(
    TOK(TOKEN_SELECT),
    CHOICE(
        SEQ(TOK(TOKEN_COUNT), TOK(TOKEN_LPAREN), RESULT_COLUMN, TOK(TOKEN_RPAREN)),
        RESULT_COLUMN),
    TOK(TOKEN_FROM),
    TOK(TOKEN_IDENTIFIER),
    TOK(TOKEN_EOF),
);

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

int parse_statement(const char* sql, Statement* statement) {
    // zeroed so free_statement finds nothing to free until the statement is filled in
    *statement = (Statement) { .type = STATEMENT_TYPE_UNKNOWN };

    TokenList tokens = { 0 };
    int result = tokenize_all(sql, &tokens);
    Parser parser = { .pos = 0, .tokens = &tokens };

    // the first token says which statement this is, and so which rule to match
    const Rule* rule = NULL;
    if (result == 0) {
        switch (tokens.items[0].type) {
            case TOKEN_SELECT:
                statement->type = STATEMENT_TYPE_SELECT;
                rule = SELECT_STMT;
                break;
            case TOKEN_CREATE:
                // CREATE TABLE has no rules yet, so rule stays NULL
                statement->type = STATEMENT_TYPE_CREATE_TABLE;
                break;
            default:
                statement->type = STATEMENT_TYPE_UNKNOWN;
                break;
        }

        if (rule == NULL) {
            fprintf(stderr, "Unsupported statement: %s\n", sql);
            result = 1;
        }
        else if (!match_rule(&parser, rule)) {
            fprintf(stderr, "Syntax error: %s\n", sql);
            result = 1;
        }
    }

    free_token_list(&tokens);
    return result;
}

void free_statement(Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:         free_token_list(&statement->select.names); break;
        case STATEMENT_TYPE_CREATE_TABLE:   free_token_list(&statement->create_table.names); break;
        case STATEMENT_TYPE_UNKNOWN:        break;
    }
}

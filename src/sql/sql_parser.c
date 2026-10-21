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
//     column_def        ::= IDENTIFIER { anything but "," or ")" outside parentheses }
//
// [ ] (optional) and { } (repeated) need RULE_OPTIONAL and RULE_REPEAT, which
// the engine doesn't have yet, so for now SELECT takes one result column and
// no WHERE, and CREATE TABLE isn't written as rules at all.

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

// each statement type's grammar, indexed by its StatementType; NULL if it has none yet
static const Rule* const STATEMENT_RULES[STATEMENT_TYPE_COUNT] = {
    [STATEMENT_TYPE_SELECT]       = SELECT_STMT,
    [STATEMENT_TYPE_CREATE_TABLE] = NULL,
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

// static int build_create_statement(Parser* parser, Statement* statement) {
//     // TODO
//     return 0;
// }

static int build_select_statement(Parser* parser, SelectQuery* query) {
    for (uint32_t i = 0; i < parser->capture_count; i++) {
        const Capture capture = parser->captures[i];
        switch (capture.tag) {
            case TAG_NONE:   return 1;
            case TAG_COLUMN: if (token_list_add(&query->names, capture.token)) return 1; break;
            case TAG_COUNT:  query->is_count = true; break;
            case TAG_TABLE:  query->table = capture.token; break;
        }
    }
    return 0;
}

static int build_statement(Parser* parser, Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:
            return build_select_statement(parser, &statement->select);
        case STATEMENT_TYPE_CREATE_TABLE:
        case STATEMENT_TYPE_CREATE_INDEX:
        case STATEMENT_TYPE_UNKNOWN:
            break;
    }
    return 1;
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
    Parser parser = { .pos = 0, .tokens = &tokens, .captures = captures };

    // the first token says which statement this is, and so which rule to match
    const enum TokenType first  = tokens.count >= 1 ? tokens.items[0].type : TOKEN_EOF;
    const enum TokenType second = tokens.count >= 2 ? tokens.items[1].type : TOKEN_EOF;
    
    switch (first) {
        case TOKEN_SELECT: statement->type = STATEMENT_TYPE_SELECT; break;
        case TOKEN_CREATE: 
            switch (second) {
                case TOKEN_TABLE: statement->type = STATEMENT_TYPE_CREATE_TABLE; break;
                case TOKEN_INDEX: statement->type = STATEMENT_TYPE_CREATE_INDEX; break;
                default: break;
            }
            break;
        default: break;
    }

    // only once tokenizing and the captures array have both succeeded
    if (result == 0) {
        const Rule* rule = (statement->type == STATEMENT_TYPE_UNKNOWN) ? NULL
                                                                       : STATEMENT_RULES[statement->type];
        if (rule == NULL) {
            fprintf(stderr, "Unsupported statement: %s\n", sql);
            result = 1;
        }
        else if (!match_rule(&parser, rule)) {
            fprintf(stderr, "Syntax error: %s\n", sql);
            result = 1;
        }
        else {
            result = build_statement(&parser, statement);
            // a build that fails partway may have filled in some of the
            // statement, and on failure the caller is left nothing to free
            if (result) free_statement(statement);
        }
    }

    free(captures);
    free_token_list(&tokens);
    return result;
}

void free_statement(Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:         free_token_list(&statement->select.names); break;
        case STATEMENT_TYPE_CREATE_TABLE:   free_token_list(&statement->create_table.names); break;
        case STATEMENT_TYPE_CREATE_INDEX:   free_token_list(&statement->create_index.columns); break;
        case STATEMENT_TYPE_UNKNOWN:        break;
    }
}

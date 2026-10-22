#include <stdio.h>
#include <stdlib.h>

#include "sql/sql_parser.h"
#include "sql/sql_grammar.h"
#include "sql/tokenizer.h"

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

// true if the type words after the column captured at index start make it the
// rowid alias: the first is "integer", and somewhere "primary" is followed by "key"
static bool is_rowid_alias(const Parser* parser, const uint32_t start) {
    const Capture* captures = parser->captures;
    const uint32_t first = start + 1;

    // only a type of exactly INTEGER makes a primary key the rowid alias; INT or BIGINT don't
    if (first >= parser->capture_count || captures[first].tag != TAG_TYPE_WORD ||
        !token_is(captures[first].token, "integer")) {
        return false;
    }

    // stops at the next column's capture, so only this column's words are looked at
    for (uint32_t j = first + 1; j < parser->capture_count && captures[j].tag == TAG_TYPE_WORD; j++) {
        if (token_is(captures[j - 1].token, "primary") && token_is(captures[j].token, "key")) {
            return true;
        }
    }
    return false;
}

static int build_create_table(Parser* parser, TableColumns* create_table) {
    for (uint32_t i = 0; i < parser->capture_count; i++) {
        const Capture capture = parser->captures[i];
        switch (capture.tag) {
            case TAG_TABLE: create_table->table = capture.token; break;
            case TAG_COLUMN: {
                if (token_list_add(&create_table->names, capture.token)) return 1;
                if (is_rowid_alias(parser, i)) {
                    create_table->has_rowid_alias = true;
                    create_table->rowid_column = create_table->names.count - 1;
                }
                break;
            }
            case TAG_TYPE_WORD: break;      // already read by is_rowid_alias
            default: return 1;
        }
    }
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
    Capture* captures = (result) ? NULL : malloc(tokens.count * sizeof(Capture));
    if (captures == NULL) result = 1;

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

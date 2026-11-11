#include <stdio.h>
#include <stdlib.h>

#include "sql/sql_parser.h"
#include "sql/parser.h"
#include "sql/tokenizer.h"

uint32_t find_column(const TableColumns* columns, const Token name) {
    // an empty name, such as the zeroed token of a missing WHERE, matches no
    // column: every column name has at least one character
    for (uint32_t i = 0; i < columns->names.count; i++) {
        if (sv_equal_nocase(columns->names.items[i].text, name.text)) return i;
    }
    return columns->names.count;
}

// ---- SELECT ----------------------------------------------------------------

// result_column ::= IDENTIFIER | "*"
// added to names; the planner tells * apart by its token type
static int parse_result_column(Parser* parser, TokenList* names) {
    Token name;
    if (!match(parser, TOKEN_IDENTIFIER, &name) && !match(parser, TOKEN_STAR, &name)) {
        syntax_error(parser, "a column name or *");
        return 1;
    }
    return token_list_add(names, name);
}

// result_columns ::= COUNT "(" result_column ")" | result_column { "," result_column }
static int parse_result_columns(Parser* parser, SelectQuery* query) {
    query->is_count = match(parser, TOKEN_COUNT, NULL);
    if (query->is_count) {
        if (expect(parser, TOKEN_LPAREN, NULL)) return 1;
        if (parse_result_column(parser, &query->names)) return 1;
        return expect(parser, TOKEN_RPAREN, NULL);
    }

    do {
        if (parse_result_column(parser, &query->names)) return 1;
    } while (match(parser, TOKEN_COMMA, NULL));
    return 0;
}

// [ WHERE expr ], leaving *where NULL when there's no WHERE
static int parse_where(Parser* parser, Expr** where) {
    if (!match(parser, TOKEN_WHERE, NULL)) return 0;

    *where = parse_expr(parser);
    return *where == NULL;
}

// select_stmt ::= SELECT result_columns FROM IDENTIFIER [ WHERE expr ] EOF
static int parse_select(Parser* parser, SelectQuery* query) {
    if (expect(parser, TOKEN_SELECT, NULL)) return 1;
    if (parse_result_columns(parser, query)) return 1;
    if (expect(parser, TOKEN_FROM, NULL)) return 1;
    if (expect(parser, TOKEN_IDENTIFIER, &query->table)) return 1;
    if (parse_where(parser, &query->where)) return 1;
    return expect(parser, TOKEN_EOF, NULL);
}

// ---- CREATE TABLE ----------------------------------------------------------

// skips the type and constraints after a column's name, such as "integer
// primary key autoincrement" or "varchar(255) not null": every token up to
// the "," or ")" that ends the column, which is left as the current token.
// Parentheses inside are skipped whole, so their commas don't end it.
// Sets *is_rowid_alias if the type is exactly INTEGER (not INT or BIGINT) and
// PRIMARY KEY follows outside parentheses. Returns nonzero if the statement
// ends first.
static int skip_column_type(Parser* parser, bool* is_rowid_alias) {
    const bool is_integer = token_is(current(parser), "integer");
    *is_rowid_alias = false;

    uint32_t depth = 0;
    Token previous = { 0 };
    while (!check(parser, TOKEN_EOF)) {
        const Token token = current(parser);
        if (depth == 0 && (token.type == TOKEN_COMMA || token.type == TOKEN_RPAREN)) return 0;

        if (token.type == TOKEN_LPAREN) depth++;
        if (token.type == TOKEN_RPAREN) depth--;
        // a PRIMARY KEY inside parentheses belongs to something else, not the column
        if (is_integer && depth == 0 && token_is(previous, "primary") && token_is(token, "key")) {
            *is_rowid_alias = true;
        }
        previous = token;
        advance(parser);
    }

    syntax_error(parser, "\")\" to end the columns");
    return 1;
}

// column_def ::= IDENTIFIER { type or constraint word, with parentheses allowed }
static int parse_column_def(Parser* parser, TableColumns* columns) {
    Token name;
    if (expect(parser, TOKEN_IDENTIFIER, &name)) return 1;
    if (token_list_add(&columns->names, name)) return 1;

    bool is_rowid_alias;
    if (skip_column_type(parser, &is_rowid_alias)) return 1;
    if (is_rowid_alias) {
        columns->has_rowid_alias = true;
        columns->rowid_column = columns->names.count - 1;
    }
    return 0;
}

// create_table_stmt ::= CREATE TABLE IDENTIFIER "(" column_def { "," column_def } ")" EOF
static int parse_create_table(Parser* parser, TableColumns* columns) {
    if (expect(parser, TOKEN_CREATE, NULL)) return 1;
    if (expect(parser, TOKEN_TABLE, NULL)) return 1;
    if (expect(parser, TOKEN_IDENTIFIER, &columns->table)) return 1;
    if (expect(parser, TOKEN_LPAREN, NULL)) return 1;

    do {
        if (parse_column_def(parser, columns)) return 1;
    } while (match(parser, TOKEN_COMMA, NULL));

    if (expect(parser, TOKEN_RPAREN, NULL)) return 1;
    return expect(parser, TOKEN_EOF, NULL);
}

// ---- CREATE INDEX ----------------------------------------------------------

// indexed_column ::= IDENTIFIER { IDENTIFIER }
// only the name is kept; the words after it, such as DESC, aren't needed
static int parse_indexed_column(Parser* parser, TokenList* columns) {
    Token name;
    if (expect(parser, TOKEN_IDENTIFIER, &name)) return 1;
    while (match(parser, TOKEN_IDENTIFIER, NULL)) {
        // skipping the words
    }
    return token_list_add(columns, name);
}

// create_index_stmt ::= CREATE INDEX IDENTIFIER ON IDENTIFIER
//                       "(" indexed_column { "," indexed_column } ")" [ WHERE ... ] EOF
static int parse_create_index(Parser* parser, IndexColumns* index) {
    if (expect(parser, TOKEN_CREATE, NULL)) return 1;
    if (expect(parser, TOKEN_INDEX, NULL)) return 1;
    if (expect(parser, TOKEN_IDENTIFIER, &index->name)) return 1;
    if (expect(parser, TOKEN_ON, NULL)) return 1;
    if (expect(parser, TOKEN_IDENTIFIER, &index->table)) return 1;
    if (expect(parser, TOKEN_LPAREN, NULL)) return 1;

    do {
        if (parse_indexed_column(parser, &index->columns)) return 1;
    } while (match(parser, TOKEN_COMMA, NULL));

    if (expect(parser, TOKEN_RPAREN, NULL)) return 1;

    // which rows a partial index covers isn't needed yet, only that it's
    // partial, so its condition is skipped without being parsed
    index->has_where = match(parser, TOKEN_WHERE, NULL);
    if (index->has_where) {
        while (!check(parser, TOKEN_EOF)) advance(parser);
    }
    return expect(parser, TOKEN_EOF, NULL);
}

// ---- any statement ---------------------------------------------------------

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

// parses the tokens as the kind of statement statement->type says
static int parse_by_type(Parser* parser, Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:       return parse_select(parser, &statement->select);
        case STATEMENT_TYPE_CREATE_TABLE: return parse_create_table(parser, &statement->create_table);
        case STATEMENT_TYPE_CREATE_INDEX: return parse_create_index(parser, &statement->create_index);
        case STATEMENT_TYPE_UNKNOWN:      break;
    }
    return 1;
}

// parses the tokens of sql into statement. On failure leaves nothing in the
// statement to free.
static int parse_tokens(const TokenList* tokens, const char* sql, Statement* statement) {
    statement->type = statement_type_of(tokens);
    if (statement->type == STATEMENT_TYPE_UNKNOWN) {
        fprintf(stderr, "Unsupported statement: %s\n", sql);
        return 1;
    }

    // a parse that fails partway may have filled in some of the statement
    Parser parser = { .tokens = tokens, .pos = 0 };
    const int result = parse_by_type(&parser, statement);
    if (result) free_statement(statement);
    return result;
}

int parse_statement(const char* sql, Statement* statement) {
    // zeroed so free_statement finds nothing to free until the statement is filled in
    *statement = (Statement) { .type = STATEMENT_TYPE_UNKNOWN };

    TokenList tokens = { 0 };
    int result = tokenize_all(sql, &tokens);
    if (result == 0) result = parse_tokens(&tokens, sql, statement);

    free_token_list(&tokens);
    return result;
}

void free_statement(Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:
            free_token_list(&statement->select.names);
            free_expr(statement->select.where);
            statement->select.where = NULL;
            break;
        case STATEMENT_TYPE_CREATE_TABLE:   free_token_list(&statement->create_table.names);   break;
        case STATEMENT_TYPE_CREATE_INDEX:   free_token_list(&statement->create_index.columns); break;
        case STATEMENT_TYPE_UNKNOWN:        break;
    }
}

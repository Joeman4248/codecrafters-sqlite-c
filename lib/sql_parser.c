#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "include/lib-sqlite/sql_parser.h"
#include "include/lib-sqlite/tokenizer.h"

typedef struct {
    Tokenizer tokenizer;
    Token     current;      // the next token to be consumed
} Parser;

static void parser_init(Parser* parser, const char* sql) {
    tokenizer_init(&parser->tokenizer, sql);
    parser->current = next_token(&parser->tokenizer);
}

// moves on to the next token
static void advance(Parser* parser) {
    parser->current = next_token(&parser->tokenizer);
}

// if the current token has the given type, consumes it and returns true;
// otherwise leaves it in place and returns false
static bool match(Parser* parser, const enum TokenType type) {
    if (parser->current.type == type) {
        advance(parser);
        return true;
    }
    return false;
}

// consumes the current token if it has the given type, storing it in *out
// when out is not NULL. Otherwise prints which token was expected and which
// was found, and returns nonzero.
static int expect(Parser* parser, const enum TokenType type, Token* out) {
    Token token = parser->current;
    if (match(parser, type)) {
        if (out != NULL) *out = token;
        return 0;
    }
    fprintf(stderr, "expected %s, got %s\n", token_type_name(type), token_type_name(token.type));
    return 1;
}

// true if token's text is word, ignoring case. For words like integer and
// primary that the tokenizer reads as identifiers rather than keywords.
static bool token_is(const Token token, const char* word) {
    const size_t len = strlen(word);
    return token.len == len && strncasecmp(token.start, word, len) == 0;
}

// the end of the query is checked with match rather than expect so the error
// can show the unsupported rest of the query instead of just its first token
int parse_select(const char* sql, SelectQuery* query) {

    Parser parser;
    parser_init(&parser, sql);

    if (expect(&parser, TOKEN_SELECT, NULL) ||
        expect(&parser, TOKEN_IDENTIFIER, &query->column) ||
        expect(&parser, TOKEN_FROM, NULL) ||
        expect(&parser, TOKEN_IDENTIFIER, &query->table)) {
        return 1;
    }

    if (!match(&parser, TOKEN_EOF)) {
        fprintf(stderr, "Unsupported query: nothing after the table name is supported yet: %s\n",
                parser.current.start);
        return 1;
    }

    return 0;
}

// reads the type and constraints of the column just added to columns:
// everything up to the comma or closing parenthesis that ends the column,
// which is left as the current token. Parentheses inside the definition, as
// in varchar(255), are tracked so their commas don't end it early. Sets
// columns->rowid_column if the column is the INTEGER PRIMARY KEY. Returns
// nonzero if the text ends first.
static int parse_column_definition(Parser* parser, TableColumns* columns) {
    uint32_t paren_depth = 0;

    // the type comes straight after the name. Only a type of exactly INTEGER
    // makes a primary key the rowid alias; INT or BIGINT do not.
    const bool is_integer = token_is(parser->current, "integer");
    Token previous = { .type = TOKEN_EOF };

    while (!match(parser, TOKEN_EOF)) {
        switch (parser->current.type) {
            case TOKEN_COMMA: {
                if (paren_depth == 0)
                    return 0;
                break;
            }
            case TOKEN_LPAREN:  paren_depth++; break;
            case TOKEN_RPAREN: {
                if (paren_depth == 0)
                    return 0;
                paren_depth--; break;
            }
            case TOKEN_IDENTIFIER: {
                // a PRIMARY KEY inside parentheses belongs to something else,
                // such as a CHECK expression
                if (paren_depth == 0 && is_integer &&
                    token_is(previous, "primary") && token_is(parser->current, "key")) {
                    columns->rowid_column = (int32_t) columns->count - 1;
                }
                break;
            }
            default: break;
        }
        previous = parser->current;
        advance(parser);
    }
    
    fprintf(stderr, "Error: The sql statement ended before the list closed\n");
    return 1;
}

// appends name to columns->columns, growing the array as needed
static int add_column(TableColumns* columns, const Token name) {
    Token* grown = realloc(columns->columns, (columns->count + 1) * sizeof(Token));
    if (grown == NULL) return 1;        // the old array is still valid, caller frees it
    columns->columns = grown;
    columns->columns[columns->count++] = name;
    return 0;
}

// parses everything from CREATE onwards into *columns. On failure *columns
// may be partly filled; the caller frees it.
static int parse_table_columns_into(Parser* parser, TableColumns* columns) {
    // parses: CREATE TABLE tbl_name (
    if (expect(parser, TOKEN_CREATE, NULL) || 
        expect(parser, TOKEN_TABLE, NULL) || 
        expect(parser, TOKEN_IDENTIFIER, &columns->table_name) ||
        expect(parser, TOKEN_LPAREN, NULL)) {
        return 1;
    }
    
    Token column_name;
    while (true) {
        if (expect(parser, TOKEN_IDENTIFIER, &column_name) ||
            add_column(columns, column_name) ||
            parse_column_definition(parser, columns) ) {
            return 1;
        }

        // comma means another column follows
        if (!match(parser, TOKEN_COMMA)) break;
    }
    
    return expect(parser, TOKEN_RPAREN, NULL);  // parses: )
}

int parse_table_columns(const char* sql, TableColumns* columns) {
    // zero would mean column 0 is the rowid alias, so start from -1
    *columns = (TableColumns) { .rowid_column = -1 };

    Parser parser;
    parser_init(&parser, sql);

    if (parse_table_columns_into(&parser, columns) != 0) {
        free_table_columns(columns);
        return 1;
    }
    return 0;
}

void free_table_columns(TableColumns* columns) {
    free(columns->columns);
    columns->columns = NULL;
    columns->count = 0;
}

int32_t find_column(const TableColumns* columns, const Token name) {
    for (uint32_t i = 0; i < columns->count; i++) {
        const Token column = columns->columns[i];
        if (column.len == name.len &&
            strncasecmp(column.start, name.start, name.len) == 0) {
            return (int32_t) i;
        }
    }
    return -1;
}

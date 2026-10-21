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
    parser->tokenizer = tokenizer_init(sql);
    parser->current = next_token(&parser->tokenizer);
}

// moves on to the next token
static void advance(Parser* parser) {
    parser->current = next_token(&parser->tokenizer);
}

// true if the current token has the given type. Never consumes it.
static bool check(const Parser* parser, const enum TokenType type) {
    return parser->current.type == type;
}

// if the current token has the given type, consumes it and returns true,
// storing it in *out when out is not NULL;
// otherwise leaves it in place and returns false
static bool match(Parser* parser, const enum TokenType type, Token* out) {
    if (check(parser, type)) {
        if (out != NULL) *out = parser->current;
        advance(parser);
        return true;
    }
    return false;
}

// consumes the current token if it has the given type, storing it in *out
// when out is not NULL. Otherwise prints which token was expected and which
// was found, and returns nonzero.
static int expect(Parser* parser, const enum TokenType type, Token* out) {
    if (match(parser, type, out)) {
        return 0;
    }
    fprintf(stderr, "expected %s, got %s\n", token_type_name(type), token_type_name(parser->current.type));
    return 1;
}

// true if token's text is word, ignoring case. For words like integer and
// primary that the tokenizer reads as identifiers rather than keywords.
static bool token_is(const Token token, const char* word) {
    return sv_equal_nocase(token.text, sv_from_cstr(word));
}

// consumes a column name or * and stores it in *out. Otherwise prints what was
// found instead and returns nonzero.
static int parse_name_or_star(Parser* parser, Token* out) {
    if (match(parser, TOKEN_IDENTIFIER, out) || match(parser, TOKEN_STAR, out)) {
        return 0;
    }
    fprintf(stderr, "expected a column name or *, got %s\n",
            token_type_name(parser->current.type));
    return 1;
}

// parses a comma-separated list of column names and *s into list
static int parse_name_list(Parser* parser, TokenList* list) {
    Token name;
    do {
        if (parse_name_or_star(parser, &name) || token_list_add(list, name)) {
            return 1;
        }
    } while (match(parser, TOKEN_COMMA, NULL));
    return 0;
}

// parses everything from SELECT onwards into *query; see parse_statement for
// the form accepted. On failure query->names may be partly filled; the
// caller frees it.
static int parse_select_into(Parser* parser, SelectQuery* query) {
    // SELECT
    if (expect(parser, TOKEN_SELECT, NULL)) return 1;

    // COUNT(name) or COUNT(*)
    if (match(parser, TOKEN_COUNT, NULL)) {
        query->is_count = true;

        Token name;
        if (expect(parser, TOKEN_LPAREN, NULL) ||
            parse_name_or_star(parser, &name) ||
            token_list_add(&query->names, name) ||
            expect(parser, TOKEN_RPAREN, NULL)) {
            return 1;
        }
    }
    // name, *, name, ...
    else if (parse_name_list(parser, &query->names)) {
        return 1;
    }

    // FROM table
    if (expect(parser, TOKEN_FROM, NULL) ||
        expect(parser, TOKEN_IDENTIFIER, &query->table)) {
        return 1;
    }

    // WHERE name = 'text'
    query->has_where = match(parser, TOKEN_WHERE, NULL);
    if (query->has_where) {
        if (expect(parser, TOKEN_IDENTIFIER, &query->where.column) ||
            expect(parser, TOKEN_EQUALS, NULL) ||
            expect(parser, TOKEN_STRING, &query->where.value)) {
            return 1;
        }
    }

    if (!match(parser, TOKEN_EOF, NULL)) {
        fprintf(stderr, "Unsupported query: only WHERE column = 'text' can follow the table name, got: %s\n",
                parser->current.text.data);
        return 1;
    }

    return 0;
}

// initialises *query, then parses into it, freeing it again on failure so
// the caller is left with nothing to free
static int parse_select(Parser* parser, SelectQuery* query) {
    *query = (SelectQuery) { 0 };

    if (parse_select_into(parser, query)) {
        free_token_list(&query->names);
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
    Token previous = { 0 };

    while (!match(parser, TOKEN_EOF, NULL)) {
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
                // a PRIMARY KEY inside parentheses belongs to something else
                if (is_integer && paren_depth == 0 && 
                    token_is(previous, "primary") && token_is(parser->current, "key")) {
                    columns->has_rowid_alias = true;
                    columns->rowid_column = columns->names.count - 1;
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

// parses everything from CREATE onwards into *columns. On failure *columns
// may be partly filled; the caller frees it.
static int parse_table_columns_into(Parser* parser, TableColumns* columns) {
    // parses: CREATE TABLE tbl_name (
    if (expect(parser, TOKEN_CREATE, NULL) || 
        expect(parser, TOKEN_TABLE, NULL) || 
        expect(parser, TOKEN_IDENTIFIER, &columns->table) ||
        expect(parser, TOKEN_LPAREN, NULL)) {
        return 1;
    }
    
    Token column_name;
    do {
        if (expect(parser, TOKEN_IDENTIFIER, &column_name) ||
            token_list_add(&columns->names, column_name) ||
            parse_column_definition(parser, columns) ) {
            return 1;
        }
    } while (match(parser, TOKEN_COMMA, NULL));     // a comma means another column follows
    
    return expect(parser, TOKEN_RPAREN, NULL);  // parses: )
}

// initialises *columns, then parses into it, freeing it again on failure so
// the caller is left with nothing to free
static int parse_create_table(Parser* parser, TableColumns* columns) {
    *columns = (TableColumns) { 0 };

    if (parse_table_columns_into(parser, columns)) {
        free_token_list(&columns->names);
        return 1;
    }
    return 0;
}

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

// the first token is only looked at, not consumed: each statement's parser
// expects its own leading keyword
int parse_statement(const char* sql, Statement* statement) {
    Parser parser;
    parser_init(&parser, sql);

    switch (parser.current.type) {
        case TOKEN_SELECT:
            statement->type = STATEMENT_TYPE_SELECT;
            return parse_select(&parser, &statement->select);
        case TOKEN_CREATE:
            statement->type = STATEMENT_TYPE_CREATE_TABLE;
            return parse_create_table(&parser, &statement->create_table);
        default:
            statement->type = STATEMENT_TYPE_UNKNOWN;
            fprintf(stderr, "Unsupported statement: %s\n", sql);
            return 1;
    }
}

void free_statement(Statement* statement) {
    switch (statement->type) {
        case STATEMENT_TYPE_SELECT:         free_token_list(&statement->select.names); break;
        case STATEMENT_TYPE_CREATE_TABLE:   free_token_list(&statement->create_table.names); break;
        case STATEMENT_TYPE_CREATE_INDEX:   free_token_list(&statement->create_index.columns); break;
        case STATEMENT_TYPE_UNKNOWN:        break;
    }
}

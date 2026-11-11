#ifndef CODECRAFTERS_SQLITE_PARSER_H
#define CODECRAFTERS_SQLITE_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "sql/tokenizer.h"

// the tokens of one statement and how far parsing has got. Every parse_*
// function reads its tokens with the helpers below.
typedef struct Parser {
    const TokenList* tokens;    // as tokenize_all made them, ending with TOKEN_EOF
    uint32_t         pos;       // index of the current token, the next to be read.
                                // Never moves past the TOKEN_EOF.
} Parser;

// the current token, without moving past it
static inline Token current(const Parser* parser) {
    return parser->tokens->items[parser->pos];
}

// moves past the current token, unless it is the TOKEN_EOF
void advance(Parser* parser);

// true if the current token has the given type. Never moves past it.
bool check(const Parser* parser, enum TokenType type);

// if the current token has the given type, moves past it and returns true,
// storing it in *out when out isn't NULL. Otherwise leaves it and returns false.
bool match(Parser* parser, enum TokenType type, Token* out);

// like match, but if the current token doesn't have the given type, prints a
// syntax error naming the type and returns nonzero
int expect(Parser* parser, enum TokenType type, Token* out);

// prints a syntax error at the current token, saying what was expected
// instead, such as "a column name or *"
void syntax_error(const Parser* parser, const char* expected);

#endif // CODECRAFTERS_SQLITE_PARSER_H

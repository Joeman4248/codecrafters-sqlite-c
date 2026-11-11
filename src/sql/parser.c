#include <stdio.h>

#include "sql/parser.h"

void advance(Parser* parser) {
    if (!check(parser, TOKEN_EOF)) parser->pos++;
}

bool check(const Parser* parser, const enum TokenType type) {
    return current(parser).type == type;
}

bool match(Parser* parser, const enum TokenType type, Token* out) {
    if (!check(parser, type)) return false;

    if (out != NULL) *out = current(parser);
    advance(parser);
    return true;
}

int expect(Parser* parser, const enum TokenType type, Token* out) {
    if (match(parser, type, out)) return 0;

    syntax_error(parser, (type == TOKEN_EOF) ? "the end of the statement" : token_type_name(type));
    return 1;
}

void syntax_error(const Parser* parser, const char* expected) {
    const Token found = current(parser);
    if (found.type == TOKEN_EOF) {
        fprintf(stderr, "Syntax error: expected %s, but the statement ended\n", expected);
    }
    else {
        fprintf(stderr, "Syntax error: expected %s, got \"" SV_FMT "\"\n", expected, SV_ARG(found.text));
    }
}

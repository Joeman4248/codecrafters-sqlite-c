#include <ctype.h>
#include <stdbool.h>

#include "include/lib-sqlite/tokenizer.h"

// token type names, indexed by their TokenType value. The first
// TOKEN_KEYWORD_COUNT entries double as the keyword spellings.
static const char* const TOKEN_NAMES[TOKEN_TYPE_COUNT] = {
    [TOKEN_SELECT]     = "SELECT",
    [TOKEN_FROM]       = "FROM",
    [TOKEN_CREATE]     = "CREATE",
    [TOKEN_TABLE]      = "TABLE",

    [TOKEN_IDENTIFIER] = "IDENTIFIER",
    [TOKEN_LPAREN]     = "(",
    [TOKEN_RPAREN]     = ")",
    [TOKEN_COMMA]      = ",",
    [TOKEN_STAR]       = "*",
    [TOKEN_EOF]        = "EOF",
    [TOKEN_UNKNOWN]    = "UNKNOWN",
};

// true for characters that can start an identifier or keyword
static bool is_word_start(const char c) {
    return isalpha((unsigned char) c) || c == '_';
}

// true for characters that can continue an identifier or keyword
static bool is_word_char(const char c) {
    return isalnum((unsigned char) c) || c == '_';
}

// moves past spaces, tabs and newlines
static void skip_whitespace(Tokenizer* tokenizer) {
    while (isspace((unsigned char) *tokenizer->pos)) {
        tokenizer->pos++;
    }
}

// returns the keyword type for the len characters at text, compared ignoring
// case against the first TOKEN_KEYWORD_COUNT entries of TOKEN_NAMES, or
// TOKEN_IDENTIFIER if it is not a keyword
static enum TokenType lookup_keyword(const char* text, const uint32_t len) {
    // TODO
    (void) text;
    (void) len;
    return TOKEN_IDENTIFIER;
}

// scans a keyword or identifier: letters, digits and underscores. Called with
// tokenizer->pos on the first character and token->start set to it; fills in
// token->type and token->len and moves tokenizer->pos past the word.
static void scan_word(Tokenizer* tokenizer, Token* token) {
    // TODO
    (void) token;
    tokenizer->pos++;
}

void tokenizer_init(Tokenizer* tokenizer, const char* sql) {
    tokenizer->pos = sql;
}

Token next_token(Tokenizer* tokenizer) {
    skip_whitespace(tokenizer);

    Token token = { .type = TOKEN_UNKNOWN, .start = tokenizer->pos, .len = 1 };
    const char c = *tokenizer->pos;

    // pos stays on the terminator, so every later call returns EOF too
    if (c == '\0') {
        token.type = TOKEN_EOF;
        token.len = 0;
        return token;
    }

    if (is_word_start(c)) {
        scan_word(tokenizer, &token);
        return token;
    }

    switch (c) {
        case '(': token.type = TOKEN_LPAREN; break;
        case ')': token.type = TOKEN_RPAREN; break;
        case ',': token.type = TOKEN_COMMA;  break;
        case '*': token.type = TOKEN_STAR;   break;

        default:  break;    // stays TOKEN_UNKNOWN
    }

    // every single-character token, known or not, is consumed here
    tokenizer->pos++;
    return token;
}

const char* token_type_name(const enum TokenType type) {
    return TOKEN_NAMES[type];
}

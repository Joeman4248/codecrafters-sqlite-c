#include <ctype.h>
#include <stdbool.h>

#include "include/lib-sqlite/tokenizer.h"
#include <strings.h>

typedef struct {
    const char* text;
    uint32_t    len;    // strlen(text), worked out at compile time
} TokenName;

// sizeof sees the string literal itself (a char array, not a pointer), so this
// is its length without the '\0'
#define NAME(s) { s, sizeof(s) - 1 }

// token type names, indexed by their TokenType value. The first
// TOKEN_KEYWORD_COUNT entries double as the keyword spellings.
static const TokenName TOKEN_NAMES[TOKEN_TYPE_COUNT] = {
    [TOKEN_SELECT]      = NAME("SELECT"),
    [TOKEN_FROM]        = NAME("FROM"),
    [TOKEN_CREATE]      = NAME("CREATE"),
    [TOKEN_TABLE]       = NAME("TABLE"),

    [TOKEN_IDENTIFIER]  = NAME("IDENTIFIER"),
    [TOKEN_STRING]      = NAME("STRING"),
    [TOKEN_LPAREN]      = NAME("("),
    [TOKEN_RPAREN]      = NAME(")"),
    [TOKEN_COMMA]       = NAME(","),
    [TOKEN_STAR]        = NAME("*"),
    [TOKEN_EOF]         = NAME("EOF"),
    [TOKEN_UNKNOWN]     = NAME("UNKNOWN"),
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
    for (int i = TOKEN_KEYWORD_START; i < TOKEN_KEYWORD_COUNT; i++) {
        const TokenName keyword = TOKEN_NAMES[i];
        if (len != keyword.len) continue;
        if (strncasecmp(text, keyword.text, len) == 0) return (enum TokenType) i;
    }
    return TOKEN_IDENTIFIER;
}

static void scan_quoted_word(Tokenizer* tokenizer, Token* token) {
    tokenizer->pos++;   // skip opening quote

    while (*tokenizer->pos != '"') {
        // string runs out before quote is closed
        if (*tokenizer->pos == '\0') {
            token->type = TOKEN_UNKNOWN;
            token->len = (uint32_t) (tokenizer->pos - token->start);
            return;
        }
        tokenizer->pos++;
    }
    
    token->type = TOKEN_IDENTIFIER;
    token->len = (uint32_t) (tokenizer->pos - token->start);

    tokenizer->pos++;   // skip the closing quote
}

// scans a keyword or identifier: letters, digits and underscores. Called with
// tokenizer->pos on the first character and token->start set to it; fills in
// token->type and token->len and moves tokenizer->pos past the word.
static void scan_word(Tokenizer* tokenizer, Token* token) {
    while (is_word_char(*tokenizer->pos)) {
        tokenizer->pos++;        
    }
    token->len = (uint32_t) (tokenizer->pos - token->start);
    token->type = lookup_keyword(token->start, token->len);
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
    return TOKEN_NAMES[type].text;
}

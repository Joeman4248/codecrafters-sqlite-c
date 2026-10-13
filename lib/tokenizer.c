#include <ctype.h>
#include <stdbool.h>

#include "include/lib-sqlite/tokenizer.h"

// token type names, indexed by their TokenType value. The first
// TOKEN_KEYWORD_COUNT entries double as the keyword spellings.
static const char* const TOKEN_NAMES[TOKEN_TYPE_COUNT] = {
    [TOKEN_SELECT]     = "SELECT",
    [TOKEN_FROM]       = "FROM",
    [TOKEN_WHERE]      = "WHERE",
    [TOKEN_AND]        = "AND",
    [TOKEN_OR]         = "OR",
    [TOKEN_NOT]        = "NOT",
    [TOKEN_NULL]       = "NULL",
    [TOKEN_CREATE]     = "CREATE",
    [TOKEN_TABLE]      = "TABLE",
    [TOKEN_INDEX]      = "INDEX",
    [TOKEN_ON]         = "ON",
    [TOKEN_PRIMARY]    = "PRIMARY",
    [TOKEN_KEY]        = "KEY",
    [TOKEN_UNIQUE]     = "UNIQUE",
    [TOKEN_CHECK]      = "CHECK",
    [TOKEN_FOREIGN]    = "FOREIGN",
    [TOKEN_CONSTRAINT] = "CONSTRAINT",

    [TOKEN_IDENTIFIER] = "IDENTIFIER",
    [TOKEN_STRING]     = "STRING",
    [TOKEN_NUMBER]     = "NUMBER",
    [TOKEN_LPAREN]     = "(",
    [TOKEN_RPAREN]     = ")",
    [TOKEN_COMMA]      = ",",
    [TOKEN_STAR]       = "*",
    [TOKEN_SEMICOLON]  = ";",
    [TOKEN_EQ]         = "=",
    [TOKEN_MINUS]      = "-",
    [TOKEN_EOF]        = "EOF",
    [TOKEN_UNKNOWN]    = "UNKNOWN",
};

// true for characters that can start an unquoted identifier or keyword
static bool is_word_start(const char c) {
    return isalpha((unsigned char) c) || c == '_';
}

// true for characters that can continue an unquoted identifier or keyword
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

// The scan_* functions below are called with tokenizer->pos on the token's
// first character, and token->start set to it. Each fills in token->type and
// token->len and moves tokenizer->pos past the end of the token.

// scans a keyword or unquoted identifier: letters, digits and underscores
static void scan_word(Tokenizer* tokenizer, Token* token) {
    // TODO
    (void) token;
    tokenizer->pos++;
}

// scans a run of digits into a TOKEN_NUMBER
static void scan_number(Tokenizer* tokenizer, Token* token) {
    // TODO
    (void) token;
    tokenizer->pos++;
}

// scans a 'string' into a TOKEN_STRING whose text excludes the quotes. A
// doubled '' inside is part of the string, not its end. Sets TOKEN_UNKNOWN if
// the text ends before the closing quote.
static void scan_string(Tokenizer* tokenizer, Token* token) {
    // TODO
    (void) token;
    tokenizer->pos++;
}

// scans a "quoted", `quoted` or [quoted] identifier into a TOKEN_IDENTIFIER
// whose text excludes the quotes. Sets TOKEN_UNKNOWN if the text ends before
// the closing quote.
static void scan_quoted_identifier(Tokenizer* tokenizer, Token* token) {
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

    if (isdigit((unsigned char) c)) {
        scan_number(tokenizer, &token);
        return token;
    }

    switch (c) {
        case '\'':
            scan_string(tokenizer, &token);
            return token;

        case '"':
        case '`':
        case '[':
            scan_quoted_identifier(tokenizer, &token);
            return token;

        case '(': token.type = TOKEN_LPAREN;    break;
        case ')': token.type = TOKEN_RPAREN;    break;
        case ',': token.type = TOKEN_COMMA;     break;
        case '*': token.type = TOKEN_STAR;      break;
        case ';': token.type = TOKEN_SEMICOLON; break;
        case '=': token.type = TOKEN_EQ;        break;
        case '-': token.type = TOKEN_MINUS;     break;

        default:  break;    // stays TOKEN_UNKNOWN
    }

    // every single-character token, known or not, is consumed here
    tokenizer->pos++;
    return token;
}

const char* token_type_name(const enum TokenType type) {
    return TOKEN_NAMES[type];
}

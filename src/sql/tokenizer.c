#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "sql/tokenizer.h"

// token type names, indexed by their TokenType value. The entries from
// TOKEN_KW_START up to TOKEN_KW_END double as the keyword spellings.
static const StringView TOKEN_NAMES[TOKEN_TYPE_COUNT] = {
    [TOKEN_EOF]         = SV_LIT("EOF"),
    [TOKEN_UNKNOWN]     = SV_LIT("UNKNOWN"),

    [TOKEN_SELECT]      = SV_LIT("SELECT"),
    [TOKEN_FROM]        = SV_LIT("FROM"),
    [TOKEN_CREATE]      = SV_LIT("CREATE"),
    [TOKEN_TABLE]       = SV_LIT("TABLE"),
    [TOKEN_INDEX]       = SV_LIT("INDEX"),
    [TOKEN_COUNT]       = SV_LIT("COUNT"),
    [TOKEN_WHERE]       = SV_LIT("WHERE"),
    [TOKEN_ON]          = SV_LIT("ON"),
    [TOKEN_AND]         = SV_LIT("AND"),
    [TOKEN_OR]          = SV_LIT("OR"),

    [TOKEN_IDENTIFIER]  = SV_LIT("IDENTIFIER"),
    [TOKEN_STRING]      = SV_LIT("STRING"),
    [TOKEN_INTEGER]     = SV_LIT("INTEGER"),
    [TOKEN_LPAREN]      = SV_LIT("("),
    [TOKEN_RPAREN]      = SV_LIT(")"),
    [TOKEN_COMMA]       = SV_LIT(","),
    [TOKEN_STAR]        = SV_LIT("*"),
    [TOKEN_EQUALS]      = SV_LIT("="),
    [TOKEN_LT]          = SV_LIT("<"),
    [TOKEN_LE]          = SV_LIT("<="),
    [TOKEN_GT]          = SV_LIT(">"),
    [TOKEN_GE]          = SV_LIT(">="),
    [TOKEN_NE]          = SV_LIT("!="),
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
    while (isspace((unsigned char) *tokenizer->pos)) tokenizer->pos++;
}

// returns the keyword type for word, compared ignoring case against the
// keyword entries of TOKEN_NAMES, or TOKEN_IDENTIFIER if it is not a keyword
static enum TokenType lookup_keyword(const StringView word) {
    // not found gives TOKEN_KW_COUNT, and the keywords end at TOKEN_IDENTIFIER
    return TOKEN_KW_START + sv_find_nocase(&TOKEN_NAMES[TOKEN_KW_START], TOKEN_KW_COUNT, word);
}

// ends the token's text just before the tokenizer's position
static void end_token_text(Token* token, const Tokenizer* tokenizer) {
    token->text.len = (uint32_t) (tokenizer->pos - token->text.data);
}

static Token scan_quoted_word(Tokenizer* tokenizer, Token* token, const char quote) {
    token->text.data = ++tokenizer->pos;    // skip opening quote

    while (*tokenizer->pos != quote) {
        // string runs out before quote is closed
        if (*tokenizer->pos == '\0') {
            token->type = TOKEN_UNKNOWN;
            end_token_text(token, tokenizer);
            return *token;
        }
        tokenizer->pos++;
    }

    token->type = (quote == '"') ? TOKEN_IDENTIFIER : TOKEN_STRING;
    end_token_text(token, tokenizer);

    tokenizer->pos++;                       // skip the closing quote
    return *token;
}

// scans a keyword or identifier: letters, digits and underscores. 
static Token scan_word(Tokenizer* tokenizer, Token* token) {
    while (is_word_char(*tokenizer->pos)) {
        tokenizer->pos++;
    }
    end_token_text(token, tokenizer);
    token->type = lookup_keyword(token->text);
    return *token;
}

// scans an integer literal: a run of digits. 
static Token scan_integer(Tokenizer* tokenizer, Token* token) {
    while (isdigit((unsigned char) *tokenizer->pos)) {
        tokenizer->pos++;
    }
    end_token_text(token, tokenizer);
    token->type = TOKEN_INTEGER;
    return *token;
}

// scans a comparison operator: = == < <= > >= != <>. A lone ! starts no
// token, so it stays a one-character TOKEN_UNKNOWN.
static Token scan_operator(Tokenizer* tokenizer, Token* token) {
    const char first = tokenizer->pos[0];
    const char second = tokenizer->pos[1];
    uint32_t len = 1;

    switch (first) {
        case '=':
            token->type = TOKEN_EQUALS;
            if (second == '=') len = 2;
            break;

        case '!':
            if (second == '=') {
                token->type = TOKEN_NE;
                len = 2;
            }
            break;

        case '<':
            if (second == '=') {
                token->type = TOKEN_LE;
                len = 2;
            }
            else if (second == '>') {
                token->type = TOKEN_NE;
                len = 2;
            }
            else token->type = TOKEN_LT;
            break;

        case '>':
            if (second == '=') {
                token->type = TOKEN_GE;
                len = 2;
            }
            else token->type = TOKEN_GT;
            break;
    }

    token->text.len = len;
    tokenizer->pos += len;
    return *token;
}

Tokenizer tokenizer_init(const char* sql) {
    return (Tokenizer) {.pos = sql};
}

Token next_token(Tokenizer* tokenizer) {
    skip_whitespace(tokenizer);

    Token token = { .type = TOKEN_UNKNOWN, .text = { tokenizer->pos, 1 } };
    const char c = *tokenizer->pos;

    switch (c) {
        case '\0':
            // pos stays on the terminator, so every later call returns EOF too
            token.type = TOKEN_EOF;
            token.text.len = 0;
            return token;

        case '"': case '\'': return scan_quoted_word(tokenizer, &token, c);

        case '<': case '>': case '!': case '=': return scan_operator(tokenizer, &token);

        case '(': token.type = TOKEN_LPAREN; break;
        case ')': token.type = TOKEN_RPAREN; break;
        case ',': token.type = TOKEN_COMMA;  break;
        case '*': token.type = TOKEN_STAR;   break;
        default:  break;    // stays TOKEN_UNKNOWN
    }

    if (is_word_start(c))           return scan_word(tokenizer, &token);
    if (isdigit((unsigned char) c)) return scan_integer(tokenizer, &token);

    // every single-character token, known or not, is consumed here
    tokenizer->pos++;
    return token;
}

int tokenize_all(const char* sql, TokenList* tokens) {
    Tokenizer tokenizer = tokenizer_init(sql);
    Token token;
    do {
        token = next_token(&tokenizer);
        if (token_list_add(tokens, token)) return 1;
    } while (token.type != TOKEN_EOF);
    return 0;
}

// a full array doubles in size, so the copying realloc does adds up to a
// constant amount per token rather than growing with the list
int token_list_add(TokenList* list, const Token token) {
    if (list->count == list->capacity) {
        if (list->capacity > UINT32_MAX / 2) return 1;      // doubling would wrap around
        const uint32_t capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        Token* grown = realloc(list->items, capacity * sizeof(Token));
        if (grown == NULL) return 1;    // the old array is still valid, caller frees it
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count++] = token;
    return 0;
}

void free_token_list(TokenList* list) {
    free(list->items);
    *list = (TokenList) { 0 };
}

bool token_is(const Token token, const char* word) {
    return sv_equal_nocase(token.text, sv_from_cstr(word));
}

// safe to hand out as a C string: every TOKEN_NAMES entry is an SV_LIT of a
// string literal, which is null-terminated
const char* token_type_name(const enum TokenType type) {
    return TOKEN_NAMES[type].data;
}

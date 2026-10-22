#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "sql/tokenizer.h"

// token type names, indexed by their TokenType value. The entries from
// TOKEN_KEYWORD_START up to TOKEN_KEYWORD_END double as the keyword spellings.
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

    [TOKEN_IDENTIFIER]  = SV_LIT("IDENTIFIER"),
    [TOKEN_STRING]      = SV_LIT("STRING"),
    [TOKEN_INTEGER]     = SV_LIT("INTEGER"),
    [TOKEN_LPAREN]      = SV_LIT("("),
    [TOKEN_RPAREN]      = SV_LIT(")"),
    [TOKEN_COMMA]       = SV_LIT(","),
    [TOKEN_STAR]        = SV_LIT("*"),
    [TOKEN_EQUALS]      = SV_LIT("="),
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

// returns the keyword type for word, compared ignoring case against the
// keyword entries of TOKEN_NAMES, or TOKEN_IDENTIFIER if it is not a keyword
static enum TokenType lookup_keyword(const StringView word) {
    uint32_t i;
    if (sv_find_nocase(&TOKEN_NAMES[TOKEN_KEYWORD_START], TOKEN_KEYWORD_COUNT, word, &i)) {
        return (enum TokenType) (TOKEN_KEYWORD_START + i);
    }
    return TOKEN_IDENTIFIER;
}

static void scan_quoted_word(Tokenizer* tokenizer, Token* token, const char quote) {
    token->text.data = ++tokenizer->pos;    // skip opening quote

    while (*tokenizer->pos != quote) {
        // string runs out before quote is closed
        if (*tokenizer->pos == '\0') {
            token->type = TOKEN_UNKNOWN;
            token->text.len = (uint32_t) (tokenizer->pos - token->text.data);
            return;
        }
        tokenizer->pos++;
    }

    token->type = (quote == '"') ? TOKEN_IDENTIFIER : TOKEN_STRING;
    token->text.len = (uint32_t) (tokenizer->pos - token->text.data);

    tokenizer->pos++;                       // skip the closing quote
}

// scans a keyword or identifier: letters, digits and underscores. Called with
// tokenizer->pos on the first character and token->text.data set to it; fills
// in token->type and token->text.len and moves tokenizer->pos past the word.
static void scan_word(Tokenizer* tokenizer, Token* token) {
    while (is_word_char(*tokenizer->pos)) {
        tokenizer->pos++;
    }
    token->text.len = (uint32_t) (tokenizer->pos - token->text.data);
    token->type = lookup_keyword(token->text);
}

// scans an integer literal: a run of digits. Called with tokenizer->pos on the
// first digit and token->text.data set to it. A leading - isn't part of it:
// in SQL that's a minus operator applied to the number.
static void scan_integer(Tokenizer* tokenizer, Token* token) {
    while (isdigit((unsigned char) *tokenizer->pos)) {
        tokenizer->pos++;
    }
    token->text.len = (uint32_t) (tokenizer->pos - token->text.data);
    token->type = TOKEN_INTEGER;
}

Tokenizer tokenizer_init(const char* sql) {
    return (Tokenizer) {.pos = sql};
}

Token next_token(Tokenizer* tokenizer) {
    skip_whitespace(tokenizer);

    Token token = { .type = TOKEN_UNKNOWN, .text = { tokenizer->pos, 1 } };
    const char c = *tokenizer->pos;

    // pos stays on the terminator, so every later call returns EOF too
    if (c == '\0') {
        token.type = TOKEN_EOF;
        token.text.len = 0;
        return token;
    }

    if (c == '"' || c == '\'') {
        scan_quoted_word(tokenizer, &token, c);
        return token;
    }

    if (is_word_start(c)) {
        scan_word(tokenizer, &token);
        return token;
    }

    if (isdigit((unsigned char) c)) {
        scan_integer(tokenizer, &token);
        return token;
    }

    switch (c) {
        case '(': token.type = TOKEN_LPAREN; break;
        case ')': token.type = TOKEN_RPAREN; break;
        case ',': token.type = TOKEN_COMMA;  break;
        case '*': token.type = TOKEN_STAR;   break;
        case '=': token.type = TOKEN_EQUALS; break;
        default:  break;    // stays TOKEN_UNKNOWN
    }

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

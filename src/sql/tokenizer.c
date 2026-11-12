#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "sql/tokenizer.h"
#include "util/view.h"

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
    [TOKEN_NOT]         = SV_LIT("NOT"),
    [TOKEN_NULL]        = SV_LIT("NULL"),

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

// the operators longer than one character, as tokens. "==" and "<>" are
// second spellings of "=" and "!=". Only their types are handed out: a scanned
// token's text still points into the SQL string. Keep longer ones first, so
// one isn't taken for a shorter one it starts with.
static const Token LONG_OPERATORS[] = {
    { TOKEN_LE,     SV_LIT("<=") },
    { TOKEN_GE,     SV_LIT(">=") },
    { TOKEN_NE,     SV_LIT("!=") },
    { TOKEN_EQUALS, SV_LIT("==") },
    { TOKEN_NE,     SV_LIT("<>") },
};

// how many entries LONG_OPERATORS has
enum { LONG_OPS_COUNT = sizeof LONG_OPERATORS / sizeof LONG_OPERATORS[0] };

// the token type of a one-character token, or TOKEN_UNKNOWN if c starts no
// token on its own
static enum TokenType single_char_token(const char c) {
    switch (c) {
        case '(': return TOKEN_LPAREN;
        case ')': return TOKEN_RPAREN;
        case ',': return TOKEN_COMMA;
        case '*': return TOKEN_STAR;
        case '=': return TOKEN_EQUALS;
        case '<': return TOKEN_LT;
        case '>': return TOKEN_GT;
        default:  break;
    }
    return TOKEN_UNKNOWN;
}

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
    for (uint32_t i = 0; i < LONG_OPS_COUNT; i++) {
        const Token op = LONG_OPERATORS[i];
        // a query that ends before op's length gives a shorter view, which
        // sv_equal rejects unread
        const StringView text = sv_from_cstr_max(tokenizer->pos, op.text.len);
        if (sv_equal(text, op.text)) {
            *token = (Token) { .type = op.type, .text = text };
            tokenizer->pos += text.len;
            return *token;
        }
    }

    token->type = single_char_token(*tokenizer->pos);
    tokenizer->pos++;
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

        case '<': case '>': case '!': case '=': 
            return scan_operator(tokenizer, &token);

        default: break;     // everything else is scanned below
    }

    if (is_word_start(c))           return scan_word(tokenizer, &token);
    if (isdigit((unsigned char) c)) return scan_integer(tokenizer, &token);

    // every single-character token, known or not, is consumed here
    token.type = single_char_token(c);
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

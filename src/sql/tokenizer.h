#ifndef CODECRAFTERS_SQLITE_TOKENIZER_H
#define CODECRAFTERS_SQLITE_TOKENIZER_H

#include <stdint.h>

#include "util/view.h"

enum TokenType {
    TOKEN_UNKNOWN,          // a character no token starts with; 0 on purpose
    TOKEN_EOF,              // end of the SQL text

    // keywords, matched ignoring case. Anything word-shaped that isn't one of
    // these (including type names like integer and function names like max)
    // is a TOKEN_IDENTIFIER.
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_CREATE,
    TOKEN_TABLE,
    TOKEN_INDEX,
    TOKEN_COUNT,            // the token 'COUNT', not the number of token types
    TOKEN_WHERE,
    TOKEN_ON,

    TOKEN_IDENTIFIER,       // table or column name
    TOKEN_STRING,           // 'text' without the quotes
    TOKEN_INTEGER,          // digits, such as 42

    TOKEN_LPAREN,           // (
    TOKEN_RPAREN,           // )
    TOKEN_COMMA,            // ,
    TOKEN_STAR,             // *
    TOKEN_EQUALS,           // =

    TOKEN_TYPE_COUNT,       // number of token types

    // aliases for the range of keywords. They come after every real token type
    // so a debugger, which shows the first name declared for a value, shows
    // the real names rather than these.
    TOKEN_KEYWORD_START = TOKEN_SELECT,                             // first keyword
    TOKEN_KEYWORD_END   = TOKEN_IDENTIFIER,                         // one past the last keyword
    TOKEN_KEYWORD_COUNT = TOKEN_KEYWORD_END - TOKEN_KEYWORD_START,  // number of keywords
};

// one token. The text is not copied: it points into the SQL string, which
// must outlive the token.
typedef struct {
    enum TokenType type;
    StringView     text;
} Token;

// a heap-allocated list of tokens that grows as tokens are added
typedef struct {
    Token*   items;
    uint32_t count;         // tokens in the list
    uint32_t capacity;      // tokens items has room for
} TokenList;

// appends token to list, growing it as needed. Returns nonzero if the
// allocation fails, leaving the list as it was.
int token_list_add(TokenList* list, Token token);

// frees the list's array. Safe to call on a zero-initialised TokenList.
void free_token_list(TokenList* list);

typedef struct {
    const char* pos;        // next character to read
} Tokenizer;

// returns a tokenizer positioned at the start of the null-terminated string sql
Tokenizer tokenizer_init(const char* sql);

// returns the next token and moves past it. Returns TOKEN_EOF once the end of
// the text is reached, and keeps returning it on later calls.
Token next_token(Tokenizer* tokenizer);

// reads every token of the null-terminated string sql into *tokens, in order,
// ending with the TOKEN_EOF. *tokens starts empty (zero-initialised); the
// caller frees it with free_token_list, even on failure. Returns nonzero if
// an allocation fails.
int tokenize_all(const char* sql, TokenList* tokens);

// true if token's text is word, ignoring case. For words like integer and
// primary that the tokenizer reads as identifiers rather than keywords.
bool token_is(Token token, const char* word);

// returns a printable name for a token type, such as "SELECT" or "IDENTIFIER"
const char* token_type_name(enum TokenType type);

#endif // CODECRAFTERS_SQLITE_TOKENIZER_H

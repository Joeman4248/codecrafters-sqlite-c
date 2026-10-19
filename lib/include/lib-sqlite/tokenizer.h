#ifndef CODECRAFTERS_SQLITE_TOKENIZER_H
#define CODECRAFTERS_SQLITE_TOKENIZER_H

#include <stdint.h>

#include "view.h"

enum TokenType {
    TOKEN_UNKNOWN,                          // a character no token starts with; 0 on purpose
    TOKEN_EOF,                              // end of the SQL text

    // keywords, matched ignoring case. Anything word-shaped that isn't one of
    // these (including type names like integer and function names like max)
    // is a TOKEN_IDENTIFIER.
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_CREATE,
    TOKEN_TABLE,
    TOKEN_COUNT,                            // the token 'COUNT', not the number of token types
    TOKEN_WHERE,
    TOKEN_INDEX,
    TOKEN_ON,

    TOKEN_IDENTIFIER,                       // table or column name
    TOKEN_STRING,                           // 'text' without the quotes

    TOKEN_LPAREN,                           // (
    TOKEN_RPAREN,                           // )
    TOKEN_COMMA,                            // ,
    TOKEN_STAR,                             // *
    TOKEN_EQUALS,                           // =

    TOKEN_TYPE_COUNT,                       // number of token types

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

typedef struct {
    const char* pos;    // next character to read
} Tokenizer;

// starts tokenizing the null-terminated string sql
void tokenizer_init(Tokenizer* tokenizer, const char* sql);

// returns the next token and moves past it. Returns TOKEN_EOF once the end of
// the text is reached, and keeps returning it on later calls.
Token next_token(Tokenizer* tokenizer);

// returns a printable name for a token type, such as "SELECT" or "IDENTIFIER"
const char* token_type_name(enum TokenType type);

#endif // CODECRAFTERS_SQLITE_TOKENIZER_H

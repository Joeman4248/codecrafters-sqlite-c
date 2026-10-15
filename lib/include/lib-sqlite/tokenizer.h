#ifndef CODECRAFTERS_SQLITE_TOKENIZER_H
#define CODECRAFTERS_SQLITE_TOKENIZER_H

#include <stdint.h>

enum TokenType {
    // keywords, matched ignoring case. Anything word-shaped that isn't one of
    // these (including type names like integer and function names like max)
    // is a TOKEN_IDENTIFIER.
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_CREATE,
    TOKEN_TABLE,
    TOKEN_COUNT,                            // the token 'COUNT' not the number of token types
    
    TOKEN_IDENTIFIER,                       // table or column name
    TOKEN_STRING,                           // 'text' without the quotes

    TOKEN_LPAREN,                           // (
    TOKEN_RPAREN,                           // )
    TOKEN_COMMA,                            // ,
    TOKEN_STAR,                             // *

    TOKEN_EOF,                              // end of the SQL text
    TOKEN_UNKNOWN,                          // a character no token starts with
    TOKEN_TYPE_COUNT,                       // number of token types

    // aliases for looping over the keywords. They come after every real token
    // type so a debugger, which shows the first name declared for a value,
    // shows TOKEN_SELECT and TOKEN_IDENTIFIER rather than these.
    TOKEN_KEYWORD_START = TOKEN_SELECT,     // first keyword
    TOKEN_KEYWORD_COUNT = TOKEN_IDENTIFIER, // number of keywords: the first non-keyword
};

// one token. The text is not copied: start points into the SQL string, which
// must outlive the token.
typedef struct {
    enum TokenType type;
    const char*    start;
    uint32_t       len;
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

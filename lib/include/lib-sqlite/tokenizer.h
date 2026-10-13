#ifndef CODECRAFTERS_SQLITE_TOKENIZER_H
#define CODECRAFTERS_SQLITE_TOKENIZER_H

#include <stdint.h>

enum TokenType {
    // keywords, matched ignoring case. Anything word-shaped that isn't one of
    // these (including type names like integer and function names like count)
    // is a TOKEN_IDENTIFIER.
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_NULL,
    TOKEN_CREATE,
    TOKEN_TABLE,
    TOKEN_INDEX,
    TOKEN_ON,
    TOKEN_PRIMARY,
    TOKEN_KEY,
    TOKEN_UNIQUE,
    TOKEN_CHECK,
    TOKEN_FOREIGN,
    TOKEN_CONSTRAINT,
    TOKEN_KEYWORD_COUNT,                     // number of keywords

    TOKEN_IDENTIFIER = TOKEN_KEYWORD_COUNT,  // name, or "quoted name"
    TOKEN_STRING,                            // 'text'
    TOKEN_NUMBER,                            // 42 (digits only; a leading - is TOKEN_MINUS)

    TOKEN_LPAREN,                            // (
    TOKEN_RPAREN,                            // )
    TOKEN_COMMA,                             // ,
    TOKEN_STAR,                              // *
    TOKEN_SEMICOLON,                         // ;
    TOKEN_EQ,                                // =
    TOKEN_MINUS,                             // -

    TOKEN_EOF,                               // end of the SQL text
    TOKEN_UNKNOWN,                           // a character no token starts with, or an unterminated quote
    TOKEN_TYPE_COUNT,                        // number of token types
};

// one token. The text is not copied: start points into the SQL string, which
// must outlive the token. For quoted identifiers and strings, start and len
// cover the text between the quotes; a doubled quote inside ('it''s') is left
// as it is in the source.
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

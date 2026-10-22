#ifndef CODECRAFTERS_SQLITE_GRAMMAR_H
#define CODECRAFTERS_SQLITE_GRAMMAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sql/tokenizer.h"

// A grammar is a tree of Rules, built at compile time with the macros below.
// match_rule checks tokens against any grammar it's given: the knowledge of
// SQL lives in the grammar, not in the code that matches it.

enum RuleType {
    RULE_TOKEN,         // one token of a given type:          FROM
    RULE_SEQ,           // every child, one after another:     a b c
    RULE_CHOICE,        // the first child that matches:       a | b
    RULE_REPEAT,        // its one child, zero or more times:  { a }
};

// what a matched token means, so it can be found after matching. A grammar
// tags the tokens that carry information (names, values) and leaves keywords
// and punctuation untagged.
enum CaptureTag {
    TAG_NONE,           // 0, so every rule the macros make without a tag is untagged
    TAG_COUNT,
    TAG_COLUMN,
    TAG_TABLE,
    TAG_TYPE_WORD,      // a word of a column's type or constraints, such as integer or key
};

typedef struct Rule {
    enum RuleType             type;
    enum TokenType            token_type; // RULE_TOKEN: the token type it matches
    enum CaptureTag           tag;        // RULE_TOKEN: if not TAG_NONE, a match is captured under it
    const struct Rule* const* children;   // RULE_SEQ, RULE_CHOICE, RULE_REPEAT: pointers to the child
                                          // rules, ending with NULL (one child for REPEAT)
} Rule;

// Each macro makes an unnamed Rule with a compound literal and gives its
// address, so rules nest the way the grammar does:
//
//     // select ::= SELECT ( IDENTIFIER | "*" ) FROM IDENTIFIER EOF
//     static const Rule* const SELECT_STMT = SEQ(
//         TOK(TOKEN_SELECT),
//         CHOICE(TOK(TOKEN_IDENTIFIER), TOK(TOKEN_STAR)),
//         TOK(TOKEN_FROM),
//         TOK(TOKEN_IDENTIFIER),
//         TOK(TOKEN_EOF));
//
// At file scope, those unnamed Rules last for the whole program, so a grammar
// needs no allocation and nothing to free.
//
// The last rule in a SEQ or CHOICE can't be followed by a comma: the NULL that
// RULE_LIST adds after it would leave an empty entry, which doesn't compile.

// a list of rules, as an unnamed array ending with NULL. The arguments are
// written out only once: a macro that repeated them (to count them, say)
// would double the expanded text at every level of nesting.
#define RULE_LIST(...)  ((const Rule* const[]) { __VA_ARGS__, NULL })

#define TOK(t)      (&(const Rule) { .type = RULE_TOKEN, .token_type = (t) })
#define TAGGED_TOK(t, g) \
                    (&(const Rule) { .type = RULE_TOKEN, .token_type = (t), .tag = (g) })
#define SEQ(...)    (&(const Rule) { .type = RULE_SEQ,    .children = RULE_LIST(__VA_ARGS__) })
#define CHOICE(...) (&(const Rule) { .type = RULE_CHOICE, .children = RULE_LIST(__VA_ARGS__) })
#define REPEAT(r)   (&(const Rule) { .type = RULE_REPEAT, .children = RULE_LIST(r) })

// a token that matched a tagged rule, and the tag it matched under
typedef struct {
    enum CaptureTag tag;
    Token           token;
} Capture;

// the tokens being matched, how far matching has got, and what it has captured
typedef struct {
    const TokenList* tokens;        // as tokenize_all made them, ending with TOKEN_EOF
    uint32_t         pos;           // index of the next token to match
    Capture*         captures;      // room for tokens->count entries, provided by the caller.
                                    // A token is captured at most once, so it never fills up.
    uint32_t         capture_count; // how many captures are filled in
} Parser;

// true if the tokens from parser->pos onwards start with a match for rule, in
// which case parser->pos is moved past them and a capture is appended for each
// tagged token in the match, in order. On no match, parser->pos and
// parser->capture_count are left where they were, so the caller can try
// something else from the same token.
bool match_rule(Parser* parser, const Rule* rule);

#endif // CODECRAFTERS_SQLITE_GRAMMAR_H

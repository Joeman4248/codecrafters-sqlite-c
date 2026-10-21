#ifndef CODECRAFTERS_SQLITE_GRAMMAR_H
#define CODECRAFTERS_SQLITE_GRAMMAR_H

#include <stdbool.h>
#include <stdint.h>

#include "sql/tokenizer.h"

// A grammar is a tree of Rules, built at compile time with the macros below.
// match_rule checks tokens against any grammar it's given: the knowledge of
// SQL lives in the grammar, not in the code that matches it.

enum RuleType {
    RULE_TOKEN,         // one token of a given type:          FROM
    RULE_SEQ,           // every child, one after another:     a b c
    RULE_CHOICE,        // the first child that matches:       a | b
};

typedef struct Rule {
    enum RuleType             type;
    enum TokenType            token_type; // RULE_TOKEN: the token type it matches
    const struct Rule* const* children;   // RULE_SEQ, RULE_CHOICE: pointers to the child rules
    uint32_t                  count;      // RULE_SEQ, RULE_CHOICE: how many children
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

// a list of rules, as an unnamed array, and how many it holds
#define RULE_LIST(...)          ((const Rule* const[]) { __VA_ARGS__ })
#define RULE_LIST_COUNT(...)    (sizeof(RULE_LIST(__VA_ARGS__)) / sizeof(const Rule*))

#define TOK(t)      (&(const Rule) { .type = RULE_TOKEN, .token_type = (t) })
#define SEQ(...)    (&(const Rule) { .type = RULE_SEQ, \
                        .children = RULE_LIST(__VA_ARGS__), .count = RULE_LIST_COUNT(__VA_ARGS__) })
#define CHOICE(...) (&(const Rule) { .type = RULE_CHOICE, \
                        .children = RULE_LIST(__VA_ARGS__), .count = RULE_LIST_COUNT(__VA_ARGS__) })

// the tokens being matched, and how far matching has got
typedef struct {
    const TokenList* tokens;    // as tokenize_all made them, ending with TOKEN_EOF
    uint32_t         pos;       // index of the next token to match
} Parser;

// true if the tokens from parser->pos onwards start with a match for rule, in
// which case parser->pos is moved past them. On no match, parser->pos is left
// where it was, so the caller can try something else from the same token.
bool match_rule(Parser* parser, const Rule* rule);

#endif // CODECRAFTERS_SQLITE_GRAMMAR_H

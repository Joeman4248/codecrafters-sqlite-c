#ifndef CODECRAFTERS_SQLITE_EXPR_H
#define CODECRAFTERS_SQLITE_EXPR_H

#include <stdint.h>

#include "sql/parser.h"
#include "sql/tokenizer.h"
#include "util/value.h"

// the kinds of node an expression tree is built from
enum ExprType : uint8_t {
    EXPR_COLUMN,        // a column of the row
    EXPR_LITERAL,       // a value written in the query
    EXPR_COMPARE,       // left op right
    EXPR_AND,           // left AND right
    EXPR_OR,            // left OR right
    EXPR_NOT,           // NOT operand

    EXPR_TYPE_COUNT,
    EXPR_UNKNOWN = EXPR_TYPE_COUNT,
};

// one node of an expression tree, such as a WHERE condition. Only the union
// member matching type is valid. Every node is on the heap, and a tree is
// freed with free_expr from its root.
typedef struct Expr {
    enum ExprType type;
    union {
        struct {
            Token    name;      // TOKEN_IDENTIFIER, pointing into the query string
            uint32_t index;     // its position in the table, filled in by the planner
        } column;
        Value literal;          // TEXT (viewing the query string, without the quotes),
                                // INTEGER or NULL
        struct {
            struct Expr*   left;
            struct Expr*   right;
            enum CompareOp op;  // EXPR_COMPARE only
        } binary;               // EXPR_COMPARE, EXPR_AND and EXPR_OR
        struct Expr* operand;   // EXPR_NOT
    };
} Expr;

// parses one expression starting at the current token, and returns its tree,
// leaving the parser at the first token after it. Returns NULL after printing
// an error if the tokens there don't start an expression. So far one is
//
//     expr       ::= or_expr
//     or_expr    ::= and_expr { OR and_expr }
//     and_expr   ::= not_expr { AND not_expr }
//     not_expr   ::= [ NOT ] comparison
//     comparison ::= primary [ op primary ]
//     op         ::= "=" | "==" | "<" | "<=" | ">" | ">=" | "!=" | "<>"
//     primary    ::= IDENTIFIER | STRING | INTEGER | NULL | "(" expr ")"
//
// AND binds tighter than OR, so a AND b OR c is (a AND b) OR c, and a chain of
// either groups from the left: a AND b AND c is (a AND b) AND c. NOT binds
// tighter than both but looser than a comparison, so NOT a = b is NOT (a = b).
// A parenthesized primary holds any expression, so it may already be a
// comparison: (a = b) AND c. The operator is therefore optional, and a bare
// primary such as a column is a condition on its own, as in sqlite.
// The tree's text points into the query string, which must outlive it.
Expr* parse_expr(Parser* parser);

// frees expr and every node below it. Safe to call with NULL, and on a tree
// whose building stopped partway, as long as its missing children are NULL.
void free_expr(Expr* expr);

#endif // CODECRAFTERS_SQLITE_EXPR_H

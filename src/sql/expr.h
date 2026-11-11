#ifndef CODECRAFTERS_SQLITE_EXPR_H
#define CODECRAFTERS_SQLITE_EXPR_H

#include <stdint.h>

#include "sql/tokenizer.h"
#include "util/value.h"

// the kinds of node an expression tree is built from
enum ExprType : uint8_t {
    EXPR_COLUMN,        // a column of the row
    EXPR_LITERAL,       // a value written in the query
    EXPR_COMPARE,       // left op right

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
            enum CompareOp op;
            struct Expr*   left;
            struct Expr*   right;
        } compare;
    };
} Expr;

// parses all of tokens as one expression, and returns its tree, or NULL after
// printing an error if they aren't one. So far an expression is
//
//     expr       ::= comparison
//     comparison ::= primary op primary
//     primary    ::= IDENTIFIER | STRING | INTEGER | NULL
//
// The tree's text points into the query string, which must outlive it.
Expr* parse_expr(const TokenList* tokens);

// frees expr and every node below it. Safe to call with NULL, and on a tree
// whose building stopped partway, as long as its missing children are NULL.
void free_expr(Expr* expr);

#endif // CODECRAFTERS_SQLITE_EXPR_H

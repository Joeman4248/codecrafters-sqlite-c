#include <stdio.h>
#include <stdlib.h>

#include "sql/expr.h"

// ---- what a token stands for -----------------------------------------------

// the comparison operator an operator token stands for, or COMPARE_UNKNOWN
// for any other token
static enum CompareOp comparison_op(const Token token) {
    switch (token.type) {
        case TOKEN_EQUALS: return COMPARE_EQUAL;
        case TOKEN_LT:     return COMPARE_LESS;
        case TOKEN_LE:     return COMPARE_LESS_EQUAL;
        case TOKEN_GT:     return COMPARE_GREATER;
        case TOKEN_GE:     return COMPARE_GREATER_EQUAL;
        case TOKEN_NE:     return COMPARE_NOT_EQUAL;
        default: break;
    }
    return COMPARE_UNKNOWN;
}

// the INTEGER a run of digits stands for. Returns false, after printing an
// error, if the number doesn't fit in 64 bits.
static bool integer_literal(const StringView digits, int64_t* integer) {
    *integer = 0;
    for (uint32_t i = 0; i < digits.len; i++) {
        const int digit = digits.data[i] - '0';
        if (*integer > (INT64_MAX - digit) / 10) {
            fprintf(stderr, "integer too large: " SV_FMT "\n", SV_ARG(digits));
            return false;
        }
        *integer = *integer * 10 + digit;
    }
    return true;
}

// ---- building the tree -----------------------------------------------------
// Each parse_* reads one level of the grammar in expr.h and returns its tree,
// or NULL after printing an error (or when out of memory).

// a heap copy of node
static Expr* new_expr(const Expr node) {
    Expr* expr = malloc(sizeof(Expr));
    if (expr != NULL) *expr = node;
    return expr;
}

static Expr* new_literal(const Value value) {
    return new_expr((Expr) { .type = EXPR_LITERAL, .literal = value });
}

// primary ::= IDENTIFIER | STRING | INTEGER | NULL
static Expr* parse_primary(Parser* parser) {
    Token token;
    if (match(parser, TOKEN_IDENTIFIER, &token)) {
        return new_expr((Expr) { .type = EXPR_COLUMN, .column = { .name = token } });
    }
    if (match(parser, TOKEN_STRING, &token)) {
        return new_literal((Value) { .type = VALUE_TEXT, .text = token.text });
    }
    if (match(parser, TOKEN_INTEGER, &token)) {
        int64_t integer;
        if (!integer_literal(token.text, &integer)) return NULL;
        return new_literal((Value) { .type = VALUE_INTEGER, .integer = integer });
    }
    if (match(parser, TOKEN_NULL, NULL)) {
        return new_literal((Value) { .type = VALUE_NULL });
    }

    syntax_error(parser, "a column name or a value");
    return NULL;
}

// a node of type joining left and right, taking ownership of both. Either
// may be NULL, from a parse that failed, in which case the other is freed and
// so is NULL returned.
static Expr* new_binary(const enum ExprType type, Expr* left, const enum CompareOp op, Expr* right) {
    Expr* node = (left == NULL || right == NULL) ? NULL : new_expr((Expr) {
        .type = type,
        .binary = { .left = left, .right = right, .op = op },
    });
    if (node == NULL) {
        free_expr(left);
        free_expr(right);
    }
    return node;
}

// comparison ::= primary op primary
static Expr* parse_comparison(Parser* parser) {
    Expr* left = parse_primary(parser);
    if (left == NULL) return NULL;

    const enum CompareOp op = comparison_op(current(parser));
    if (op == COMPARE_UNKNOWN) {
        syntax_error(parser, "a comparison operator");
        free_expr(left);
        return NULL;
    }
    advance(parser);
    return new_binary(EXPR_COMPARE, left, op, parse_primary(parser));
}

// and_expr ::= comparison { AND comparison }
static Expr* parse_and(Parser* parser) {
    Expr* expr = parse_comparison(parser);
    while (expr != NULL && match(parser, TOKEN_AND, NULL)) {
        expr = new_binary(EXPR_AND, expr, COMPARE_UNKNOWN, parse_comparison(parser));
    }
    return expr;
}

Expr* parse_expr(Parser* parser) {
    return parse_and(parser);
}

void free_expr(Expr* expr) {
    if (expr == NULL) return;
    switch (expr->type) {
        case EXPR_COLUMN:
        case EXPR_LITERAL:
            break;
        case EXPR_COMPARE:
        case EXPR_AND:
            free_expr(expr->binary.left);
            free_expr(expr->binary.right);
            break;
        case EXPR_UNKNOWN:
            break;
    }
    free(expr);
}

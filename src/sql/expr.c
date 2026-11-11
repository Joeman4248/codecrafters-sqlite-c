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
// NULL is returned.
static Expr* new_binary(const enum ExprType type, Expr* left, const enum CompareOp op, Expr* right) {
    const Expr binary = {
        .type   = type,
        .binary = { .left = left, .right = right, .op = op },
    };

    Expr* node = NULL;
    if (left != NULL && right != NULL) node = new_expr(binary);
    if (node == NULL) {
        free_expr(left);
        free_expr(right);
    }
    return node;
}

// a node of type holding operand, taking ownership of it. A NULL operand, from
// a parse that failed, gives NULL.
static Expr* new_unary(const enum ExprType type, Expr* operand) {
    if (operand == NULL) return NULL;

    Expr* node = new_expr((Expr) { .type = type, .operand = operand });
    if (node == NULL) free_expr(operand);
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
    Expr* right = parse_primary(parser);
    return new_binary(EXPR_COMPARE, left, op, right);
}

// not_expr ::= [ NOT ] comparison
static Expr* parse_not(Parser* parser) {
    if (!match(parser, TOKEN_NOT, NULL)) return parse_comparison(parser);
    return new_unary(EXPR_NOT, parse_comparison(parser));
}

// and_expr ::= not_expr { AND not_expr }
static Expr* parse_and(Parser* parser) {
    Expr* expr = parse_not(parser);
    while (expr != NULL && match(parser, TOKEN_AND, NULL)) {
        Expr* right = parse_not(parser);
        expr = new_binary(EXPR_AND, expr, COMPARE_UNKNOWN, right);
    }
    return expr;
}

// or_expr ::= and_expr { OR and_expr }
static Expr* parse_or(Parser* parser) {
    Expr* expr = parse_and(parser);
    while (expr != NULL && match(parser, TOKEN_OR, NULL)) {
        Expr* right = parse_and(parser);
        expr = new_binary(EXPR_OR, expr, COMPARE_UNKNOWN, right);
    }
    return expr;
}

Expr* parse_expr(Parser* parser) {
    return parse_or(parser);
}

void free_expr(Expr* expr) {
    if (expr == NULL) return;
    switch (expr->type) {
        case EXPR_COLUMN:
        case EXPR_LITERAL:
            break;
        case EXPR_COMPARE:
        case EXPR_AND:
        case EXPR_OR:
            free_expr(expr->binary.left);
            free_expr(expr->binary.right);
            break;
        case EXPR_NOT:
            free_expr(expr->operand);
            break;
        case EXPR_UNKNOWN:
            break;
    }
    free(expr);
}

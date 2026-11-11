#include <stdio.h>
#include <stdlib.h>

#include "sql/expr.h"

// the tokens being parsed and how far parsing has got
typedef struct ExprParser {
    const TokenList* tokens;
    uint32_t         pos;       // index of the next token
} ExprParser;

// ---- tokens ----------------------------------------------------------------

// the next token without moving past it, or TOKEN_EOF once there are none left
static Token peek(const ExprParser* parser) {
    if (parser->pos == parser->tokens->count) return (Token) { .type = TOKEN_EOF };
    return parser->tokens->items[parser->pos];
}

// the next token, moving past it
static Token next(ExprParser* parser) {
    const Token token = peek(parser);
    if (token.type != TOKEN_EOF) parser->pos++;
    return token;
}

static void syntax_error(const Token token) {
    if (token.type == TOKEN_EOF) fprintf(stderr, "Syntax error: incomplete WHERE\n");
    else fprintf(stderr, "Syntax error in WHERE near \"" SV_FMT "\"\n", SV_ARG(token.text));
}

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
static Expr* parse_primary(ExprParser* parser) {
    const Token token = next(parser);
    switch (token.type) {
        case TOKEN_IDENTIFIER:
            return new_expr((Expr) { .type = EXPR_COLUMN, .column = { .name = token } });
        case TOKEN_STRING:
            return new_literal((Value) { .type = VALUE_TEXT, .text = token.text });
        case TOKEN_INTEGER: {
            int64_t integer;
            if (!integer_literal(token.text, &integer)) return NULL;
            return new_literal((Value) { .type = VALUE_INTEGER, .integer = integer });
        }
        case TOKEN_NULL:
            return new_literal((Value) { .type = VALUE_NULL });
        default:
            syntax_error(token);
            return NULL;
    }
}

// the compare node for left op right, taking ownership of left
static Expr* finish_comparison(ExprParser* parser, Expr* left, const enum CompareOp op) {
    Expr* right = parse_primary(parser);
    Expr* compare = (right == NULL) ? NULL : new_expr((Expr) {
        .type = EXPR_COMPARE,
        .compare = { .op = op, .left = left, .right = right },
    });
    if (compare == NULL) {
        free_expr(left);
        free_expr(right);
    }
    return compare;
}

// comparison ::= primary op primary
static Expr* parse_comparison(ExprParser* parser) {
    Expr* left = parse_primary(parser);
    if (left == NULL) return NULL;

    const Token token = next(parser);
    const enum CompareOp op = comparison_op(token);
    if (op == COMPARE_UNKNOWN) {
        syntax_error(token);
        free_expr(left);
        return NULL;
    }
    return finish_comparison(parser, left, op);
}

Expr* parse_expr(const TokenList* tokens) {
    ExprParser parser = { .tokens = tokens, .pos = 0 };
    Expr* expr = parse_comparison(&parser);
    if (expr == NULL) return NULL;

    // the whole expression has been read, so anything left over doesn't belong
    if (peek(&parser).type != TOKEN_EOF) {
        syntax_error(peek(&parser));
        free_expr(expr);
        return NULL;
    }
    return expr;
}

void free_expr(Expr* expr) {
    if (expr == NULL) return;
    switch (expr->type) {
        case EXPR_COLUMN:
        case EXPR_LITERAL:
            break;
        case EXPR_COMPARE:
            free_expr(expr->compare.left);
            free_expr(expr->compare.right);
            break;
        case EXPR_UNKNOWN:
            break;
    }
    free(expr);
}

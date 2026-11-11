#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "query/execute.h"
#include "query/rows.h"
#include "util/value.h"

// ---- WHERE -----------------------------------------------------------------

static const Value NULL_VALUE = { .type = VALUE_NULL };

// 1 for true and 0 for false, as SQL writes truth
static Value truth_value(const bool truth) {
    return (Value) { .type = VALUE_INTEGER, .integer = truth };
}

// whether value counts as true: only a nonzero number does. So far every
// condition is a comparison, AND, OR or NOT, so value is always an INTEGER or
// NULL.
static bool is_true(const Value value) {
    switch (value.type) {
        case VALUE_NULL:    return false;
        case VALUE_INTEGER: return value.integer != 0;
        case VALUE_REAL:    return value.real != 0.0;
        case VALUE_TEXT:
        case VALUE_BLOB:    break;      // sqlite3 reads a number from the start; not needed yet
    }
    return false;
}

// whether value counts as false. NULL is neither true nor false.
static bool is_false(const Value value) {
    return value.type != VALUE_NULL && !is_true(value);
}

static Value eval_expr(const PlanTable* table, const Row* row, const Expr* expr);

// left op right: NULL when either side is NULL, since NULL stands for a value
// that isn't known
static Value eval_compare(const PlanTable* table, const Row* row, const Expr* expr) {
    const Value left  = eval_expr(table, row, expr->binary.left);
    const Value right = eval_expr(table, row, expr->binary.right);
    if (left.type == VALUE_NULL || right.type == VALUE_NULL) return NULL_VALUE;
    return truth_value(value_satisfies(left, expr->binary.op, right));
}

// left AND right: false if either side is, even when the other is NULL, since
// it would be false whatever the NULL stood for. Otherwise NULL if either is.
static Value eval_and(const PlanTable* table, const Row* row, const Expr* expr) {
    const Value left  = eval_expr(table, row, expr->binary.left);
    const Value right = eval_expr(table, row, expr->binary.right);
    if (is_false(left) || is_false(right)) return truth_value(false);
    if (left.type == VALUE_NULL || right.type == VALUE_NULL) return NULL_VALUE;
    return truth_value(true);
}

// left OR right: true if either side is, even when the other is NULL, since
// it would be true whatever the NULL stood for. Otherwise NULL if either is.
static Value eval_or(const PlanTable* table, const Row* row, const Expr* expr) {
    const Value left  = eval_expr(table, row, expr->binary.left);
    const Value right = eval_expr(table, row, expr->binary.right);
    if (is_true(left) || is_true(right)) return truth_value(true);
    if (left.type == VALUE_NULL || right.type == VALUE_NULL) return NULL_VALUE;
    return truth_value(false);
}

// NOT operand: the opposite truth value, still NULL for NULL, since the
// opposite of an unknown is unknown too
static Value eval_not(const PlanTable* table, const Row* row, const Expr* expr) {
    const Value operand = eval_expr(table, row, expr->operand);
    if (operand.type == VALUE_NULL) return NULL_VALUE;
    return truth_value(!is_true(operand));
}

// the value expr has for a row of table, as SQL works it out: a condition is
// 1 when it holds, 0 when it doesn't, and NULL when it depends on a NULL
static Value eval_expr(const PlanTable* table, const Row* row, const Expr* expr) {
    switch (expr->type) {
        case EXPR_COLUMN:  return row_get_value(table, row, expr->column.index);
        case EXPR_LITERAL: return expr->literal;
        case EXPR_COMPARE: return eval_compare(table, row, expr);
        case EXPR_AND:     return eval_and(table, row, expr);
        case EXPR_OR:      return eval_or(table, row, expr);
        case EXPR_NOT:     return eval_not(table, row, expr);
        case EXPR_UNKNOWN: break;
    }
    return NULL_VALUE;
}

// whether the row passes the plan's WHERE condition: only a true one lets it
// through, not a NULL. Every row passes when there is no WHERE.
static bool row_matches(const SelectPlan* plan, const Row* row) {
    if (plan->where == NULL) return true;
    return is_true(eval_expr(&plan->table, row, plan->where));
}

// ---- running a SELECT ------------------------------------------------------

// prints the selected columns of every matching row, one row per line with |
// between values. A failed scan just ends its loop early; the caller checks
// scan_failed.
static void print_rows(const SelectPlan* plan, TableScan* scan) {
    Row row;
    while (scan_next(scan, &row)) {
        if (!row_matches(plan, &row)) continue;

        for (uint32_t j = 0; j < plan->selected.count; j++) {
            if (j > 0) printf("|");
            const uint32_t index = plan->selected.items[j];
            const Value value = row_get_value(&plan->table, &row, index);
            value_print(value, stdout);
        }
        printf("\n");
    }
}

// prints how many matching rows there are for COUNT(*), or how many of them
// have a non-NULL value in the counted column for COUNT(column). Prints
// nothing if the scan failed; the caller checks scan_failed.
static void count_values(const SelectPlan* plan, TableScan* scan) {
    uint32_t count = 0;
    Row row;
    while (scan_next(scan, &row)) {
        if (!row_matches(plan, &row)) continue;

        if (plan->count.rows) {
            count++;
            continue;
        }

        const Value value = row_get_value(&plan->table, &row, plan->count.column);
        if (value.type != VALUE_NULL) count++;
    }
    if (scan_failed(scan)) return;

    printf("%u\n", count);
}

int execute_select(Database* db, const SelectPlan* plan) {
    // a scan that fails to open has no rows, so the loops below just end
    TableScan scan;
    scan_open(&scan, &db->pager, &plan->table);
    if (plan->is_count) {
        count_values(plan, &scan);
    }
    else {
        print_rows(plan, &scan);
    }

    const int result = scan_failed(&scan);
    scan_close(&scan);
    return result;
}

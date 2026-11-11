#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "query/execute.h"
#include "query/rows.h"

// ---- WHERE -----------------------------------------------------------------

static const Value NULL_VALUE = { .type = VALUE_NULL };

// the value expr has for a row of table, as SQL works it out: a comparison
// is 1 when it holds, 0 when it doesn't, and NULL when either side is NULL
static Value eval_expr(const PlanTable* table, const Row* row, const Expr* expr) {
    switch (expr->type) {
        case EXPR_COLUMN:  return row_get_value(table, row, expr->column.index);
        case EXPR_LITERAL: return expr->literal;
        case EXPR_COMPARE: {
            const Value left  = eval_expr(table, row, expr->compare.left);
            const Value right = eval_expr(table, row, expr->compare.right);
            if (left.type == VALUE_NULL || right.type == VALUE_NULL) return NULL_VALUE;
            const bool holds = value_satisfies(left, expr->compare.op, right);
            return (Value) { .type = VALUE_INTEGER, .integer = holds };
        }
        case EXPR_UNKNOWN: break;
    }
    return NULL_VALUE;
}

// whether a WHERE that came out as value lets the row through: only a nonzero
// number does, so NULL, which a comparison with NULL gives, doesn't. So far
// every WHERE is a comparison, so value is always an INTEGER or NULL.
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

// whether the row passes the plan's WHERE condition. Every row passes when
// there is no WHERE.
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
            value_print(row_get_value(&plan->table, &row, plan->selected.items[j]), stdout);
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

        if (row_get_value(&plan->table, &row, plan->count.column).type != VALUE_NULL) count++;
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

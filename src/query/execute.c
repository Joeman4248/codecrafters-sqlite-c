#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "query/execute.h"
#include "query/rows.h"

// whether the row passes the plan's WHERE condition. Every row passes when
// there is no WHERE.
static bool row_matches(const SelectPlan* plan, const Row* row) {
    if (!plan->has_where) return true;

    const Value column = row_get_value(plan, row, plan->where_column);
    return value_satisfies(column, plan->where_op, plan->where_value);
}

// prints the selected columns of every matching row, one row per line with |
// between values. A failed scan just ends its loop early; the caller checks
// scan_failed.
static void print_rows(TableScan* scan) {
    const SelectPlan* plan = scan->plan;
    Row row;
    while (scan_next(scan, &row)) {
        if (!row_matches(plan, &row)) continue;

        for (uint32_t j = 0; j < plan->selected_count; j++) {
            if (j > 0) printf("|");
            value_print(row_get_value(plan, &row, plan->selected[j]), stdout);
        }
        printf("\n");
    }
}

// prints how many matching rows there are for COUNT(*), or how many of them
// have a non-NULL count_column for COUNT(column). Prints nothing if the scan
// failed; the caller checks scan_failed.
static void count_values(TableScan* scan) {
    const SelectPlan* plan = scan->plan;
    uint32_t count = 0;
    Row row;
    while (scan_next(scan, &row)) {
        if (!row_matches(plan, &row)) continue;

        if (plan->count_rows) {
            count++;
            continue;
        }

        if (row_get_value(plan, &row, plan->count_column).type != VALUE_NULL) count++;
    }
    if (scan_failed(scan)) return;

    printf("%u\n", count);
}

int execute_select(Database* db, const SelectPlan* plan) {
    // a scan that fails to open has no rows, so the loops below just end
    TableScan scan;
    scan_open(&scan, &db->pager, plan);
    if (plan->is_count) {
        count_values(&scan);
    }
    else {
        print_rows(&scan);
    }

    const int result = scan_failed(&scan);
    scan_close(&scan);
    return result;
}

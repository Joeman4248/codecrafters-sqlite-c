#include <stdio.h>
#include <stdlib.h>

#include "query/plan.h"
#include "record/schema.h"

// sets *index to the position of the column called name, or prints an error
static int resolve_column(const TableColumns* columns, const Token name, uint32_t* index) {
    *index = find_column(columns, name);
    if (*index == columns->names.count) {
        fprintf(stderr, "no such column: " SV_FMT "\n", SV_ARG(name.text));
        return 1;
    }
    return 0;
}

// ---- the parts of a plan ---------------------------------------------------
// Each fills in one part of the plan from the table's columns. Those that
// resolve names return nonzero, after printing an error, if a column doesn't exist.

static PlanTable plan_table(const SchemaEntry* table, const TableColumns* columns) {
    return (PlanTable) {
        .root_page       = table->rootpage,
        .column_count    = columns->names.count,
        .has_rowid_alias = columns->has_rowid_alias,
        .rowid_column    = columns->rowid_column,
    };
}

// the columns of a plain SELECT. Allocates selected->items.
static int plan_columns(const TokenList* names, const TableColumns* columns, PlanColumns* selected) {
    // sized first: each * stands for every column, each name for one
    uint32_t count = 0;
    for (uint32_t i = 0; i < names->count; i++) {
        const bool is_star = names->items[i].type == TOKEN_STAR;
        count += is_star ? columns->names.count : 1;
    }
    selected->items = malloc(count * sizeof(uint32_t));
    if (selected->items == NULL) return 1;

    for (uint32_t i = 0; i < names->count; i++) {
        const Token name = names->items[i];
        if (name.type == TOKEN_STAR) {
            for (uint32_t j = 0; j < columns->names.count; j++) {
                selected->items[selected->count++] = j;
            }
            continue;
        }
        if (resolve_column(columns, name, &selected->items[selected->count++])) return 1;
    }
    return 0;
}

// what COUNT counts, from the one name inside its parentheses: * or a column
static int plan_count(const Token name, const TableColumns* columns, PlanCount* count) {
    count->rows = name.type == TOKEN_STAR;
    if (count->rows) return 0;
    return resolve_column(columns, name, &count->column);
}

static Expr* plan_expr(const Expr* expr, const TableColumns* columns);

// fills in the children of planned, a copy of a node whose child pointers
// still belong to the query's tree. Clears them first, so planned can be freed
// on failure without touching the query's nodes.
static int plan_expr_children(Expr* planned, const TableColumns* columns) {
    switch (planned->type) {
        case EXPR_COLUMN:
            return resolve_column(columns, planned->column.name, &planned->column.index);
        case EXPR_LITERAL:
            return 0;
        case EXPR_COMPARE:
        case EXPR_AND: {
            const Expr* left  = planned->binary.left;
            const Expr* right = planned->binary.right;
            planned->binary.left  = NULL;
            planned->binary.right = NULL;

            planned->binary.left = plan_expr(left, columns);
            if (planned->binary.left == NULL) return 1;
            planned->binary.right = plan_expr(right, columns);
            return planned->binary.right == NULL;
        }
        case EXPR_UNKNOWN:
            break;
    }
    return 1;
}

// a copy of expr, the WHERE or a part of it, with its columns' indexes filled
// in. Returns NULL if a column doesn't exist (after printing an error) or
// memory runs out.
static Expr* plan_expr(const Expr* expr, const TableColumns* columns) {
    Expr* planned = malloc(sizeof(Expr));
    if (planned == NULL) return NULL;

    *planned = *expr;
    if (plan_expr_children(planned, columns)) {
        free_expr(planned);
        return NULL;
    }
    return planned;
}

// ---- planning a SELECT -----------------------------------------------------

// plans the query against its table, given the table's parsed sql. On failure
// parts of the plan may be allocated; the caller frees them.
static int plan_against(const SelectQuery* query, const SchemaEntry* table,
                        const Statement* create, SelectPlan* plan) {
    if (create->type != STATEMENT_TYPE_CREATE_TABLE) {
        fprintf(stderr, "schema sql for %s is not a CREATE TABLE\n", table->name);
        return 1;
    }
    const TableColumns* columns = &create->create_table;
    plan->table = plan_table(table, columns);

    plan->is_count = query->is_count;
    if (query->is_count) {
        if (plan_count(query->names.items[0], columns, &plan->count)) return 1;
    }
    else {
        if (plan_columns(&query->names, columns, &plan->selected)) return 1;
    }

    if (query->where == NULL) return 0;
    plan->where = plan_expr(query->where, columns);
    return plan->where == NULL;
}

int plan_select(const Database* db, const SelectQuery* query, SelectPlan* plan) {
    *plan = (SelectPlan) { 0 };

    const SchemaEntry* table = find_table(db, query->table.text);
    if (table == NULL) {
        fprintf(stderr, "no such table: " SV_FMT "\n", SV_ARG(query->table.text));
        return 1;
    }

    Statement create;
    if (parse_statement(table->sql, &create)) return 1;
    const int result = plan_against(query, table, &create, plan);

    // the plan holds positions, not names, so the table's columns aren't needed after this
    free_statement(&create);
    if (result) free_plan(plan);
    return result;
}

void free_plan(SelectPlan* plan) {
    free(plan->selected.items);
    plan->selected = (PlanColumns) { 0 };
    free_expr(plan->where);
    plan->where = NULL;
}

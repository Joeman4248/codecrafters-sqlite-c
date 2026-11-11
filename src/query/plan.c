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
        .root            = table->rootpage,
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

// the WHERE condition. The parser already gives it as column op value, so
// only the column needs looking up.
static int plan_where(const Condition* condition, const TableColumns* columns, PlanWhere* where) {
    where->op    = condition->op;
    where->value = condition->value;
    return resolve_column(columns, condition->column, &where->column);
}

// ---- planning a SELECT -----------------------------------------------------

// plans the query against its table, given the table's parsed sql. On failure
// plan->selected.items may be allocated; the caller frees it.
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

    plan->has_where = query->has_where;
    if (!query->has_where) return 0;
    return plan_where(&query->where, columns, &plan->where);
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
}

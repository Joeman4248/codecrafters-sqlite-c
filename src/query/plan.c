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
// Each fills in one part of the plan from the query and the table's columns,
// and returns nonzero, after printing an error, if a column doesn't exist.

// COUNT(*), which counts rows, or COUNT(column), which counts its non-NULL values
static int plan_count(const SelectQuery* query, const TableColumns* columns, SelectPlan* plan) {
    const Token name = query->names.items[0];
    plan->count_rows = name.type == TOKEN_STAR;
    if (plan->count_rows) return 0;
    return resolve_column(columns, name, &plan->count_column);
}

// the positions of the columns the query names, in query order, with each *
// expanded to every column. Allocates plan->selected.
static int plan_selected(const SelectQuery* query, const TableColumns* columns, SelectPlan* plan) {
    // sized first: each * stands for every column, each name for one
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->names.count; i++) {
        const bool is_star = query->names.items[i].type == TOKEN_STAR;
        count += is_star ? columns->names.count : 1;
    }
    plan->selected = malloc(count * sizeof(uint32_t));
    if (plan->selected == NULL) return 1;

    for (uint32_t i = 0; i < query->names.count; i++) {
        const Token name = query->names.items[i];
        if (name.type == TOKEN_STAR) {
            for (uint32_t j = 0; j < columns->names.count; j++) {
                plan->selected[plan->selected_count++] = j;
            }
            continue;
        }
        if (resolve_column(columns, name, &plan->selected[plan->selected_count++])) return 1;
    }
    return 0;
}

// the WHERE condition, if there is one. The parser already gives it as
// column op value, so only the column needs looking up.
static int plan_where(const SelectQuery* query, const TableColumns* columns, SelectPlan* plan) {
    plan->has_where = query->has_where;
    if (!query->has_where) return 0;

    plan->where_op    = query->where.op;
    plan->where_value = query->where.value;
    return resolve_column(columns, query->where.column, &plan->where_column);
}

// ---- planning a SELECT -----------------------------------------------------

// plans the query against its table, given the table's parsed sql. On failure
// plan->selected may be allocated; the caller frees it.
static int plan_table(const SelectQuery* query, const SchemaEntry* table,
                      const Statement* create, SelectPlan* plan) {
    if (create->type != STATEMENT_TYPE_CREATE_TABLE) {
        fprintf(stderr, "schema sql for %s is not a CREATE TABLE\n", table->name);
        return 1;
    }
    const TableColumns* columns = &create->create_table;

    plan->table_root      = table->rootpage;
    plan->column_count    = columns->names.count;
    plan->has_rowid_alias = columns->has_rowid_alias;
    plan->rowid_column    = columns->rowid_column;

    plan->is_count = query->is_count;
    if (query->is_count) {
        if (plan_count(query, columns, plan)) return 1;
    }
    else {
        if (plan_selected(query, columns, plan)) return 1;
    }
    return plan_where(query, columns, plan);
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
    const int result = plan_table(query, table, &create, plan);

    // the plan holds positions, not names, so the table's columns aren't needed after this
    free_statement(&create);
    if (result) free_plan(plan);
    return result;
}

void free_plan(SelectPlan* plan) {
    free(plan->selected);
    plan->selected = NULL;
}

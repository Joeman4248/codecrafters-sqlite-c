#include <stdio.h>
#include <stdlib.h>

#include "query/plan.h"
#include "storage/schema.h"

// sets *index to the position of the column called name, or prints an error
static int resolve_column(const TableColumns* columns, const Token name, uint32_t* index) {
    if (!find_column(columns, name, index)) {
        fprintf(stderr, "no such column: " SV_FMT "\n", SV_ARG(name.text));
        return 1;
    }
    return 0;
}

// sets *out to the value a literal in the query stands for: a string is TEXT
// and a run of digits an INTEGER. Prints an error if it's neither, or the
// integer doesn't fit in 64 bits.
static int literal_value(const Token literal, Value* out) {
    switch (literal.type) {
        case TOKEN_STRING:
            *out = (Value) { .type = VALUE_TEXT, .text = literal.text };
            return 0;

        case TOKEN_INTEGER: {
            int64_t integer = 0;
            for (uint32_t i = 0; i < literal.text.len; i++) {
                const int digit = literal.text.data[i] - '0';
                if (integer > (INT64_MAX - digit) / 10) {
                    fprintf(stderr, "integer too large: " SV_FMT "\n", SV_ARG(literal.text));
                    return 1;
                }
                integer = integer * 10 + digit;
            }
            *out = (Value) { .type = VALUE_INTEGER, .integer = integer };
            return 0;
        }

        default:
            fprintf(stderr, "expected a string or number, got: " SV_FMT "\n", SV_ARG(literal.text));
            return 1;
    }
}

// returns how many column positions the query's names expand to: every column
// for each *, and one for each name
static uint32_t count_selected(const SelectQuery* query, const TableColumns* columns) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->names.count; i++) {
        count += (query->names.items[i].type == TOKEN_STAR) ? columns->names.count : 1;
    }
    return count;
}

// fills selected with the positions of the columns the query names, in query
// order, with each * expanded to every column. selected has room for
// count_selected entries.
static int select_columns(const SelectQuery* query, const TableColumns* columns,
        uint32_t* selected, uint32_t* selected_count) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->names.count; i++) {
        const Token name = query->names.items[i];

        if (name.type == TOKEN_STAR) {
            for (uint32_t j = 0; j < columns->names.count; j++) selected[count++] = j;
            continue;
        }

        if (resolve_column(columns, name, &selected[count++])) return 1;
    }
    *selected_count = count;
    return 0;
}

// fills in everything the plan takes from the query and the table's columns.
// On failure plan->selected may be allocated; the caller frees it.
static int plan_columns(const SelectQuery* query, const TableColumns* columns, SelectPlan* plan) {
    plan->column_count = columns->names.count;
    plan->has_rowid_alias = columns->has_rowid_alias;
    plan->rowid_column = columns->rowid_column;

    plan->is_count = query->is_count;
    if (query->is_count) {
        // * in COUNT means rows, not columns
        const Token name = query->names.items[0];
        plan->count_rows = name.type == TOKEN_STAR;
        if (!plan->count_rows && resolve_column(columns, name, &plan->count_column)) return 1;
    }
    else {
        plan->selected = malloc(count_selected(query, columns) * sizeof(uint32_t));
        if (plan->selected == NULL) return 1;
        if (select_columns(query, columns, plan->selected, &plan->selected_count)) return 1;
    }

    plan->has_where = query->has_where;
    if (query->has_where) {
        if (literal_value(query->where.value, &plan->where_value) ||
            resolve_column(columns, query->where.column, &plan->where_column)) {
            return 1;
        }
    }
    return 0;
}

int plan_select(const Database* db, const SelectQuery* query, SelectPlan* plan) {
    *plan = (SelectPlan) { 0 };

    const SchemaEntry* table = find_table(db, query->table.text);
    if (table == NULL) {
        fprintf(stderr, "no such table: " SV_FMT "\n", SV_ARG(query->table.text));
        return 1;
    }
    plan->table_root = table->rootpage;

    Statement create_statement;
    if (parse_statement(table->sql, &create_statement)) return 1;

    int result = 0;
    if (create_statement.type != STATEMENT_TYPE_CREATE_TABLE) {
        fprintf(stderr, "schema sql for %s is not a CREATE TABLE\n", table->name);
        result = 1;
    }
    if (result == 0) result = plan_columns(query, &create_statement.create_table, plan);

    // the plan holds positions, not names, so the table's columns aren't needed after this
    free_statement(&create_statement);
    if (result) free_plan(plan);
    return result;
}

void free_plan(SelectPlan* plan) {
    free(plan->selected);
    plan->selected = NULL;
}

#include <stdio.h>
#include <stdlib.h>

#include "include/lib-sqlite/plan.h"
#include "include/lib-sqlite/schema.h"

// sets *index to the position of the column called name, or prints an error
static int resolve_column(const TableColumns* columns, const Token name, uint32_t* index) {
    if (!find_column(columns, name, index)) {
        fprintf(stderr, "no such column: " SV_FMT "\n", SV_ARG(name.text));
        return 1;
    }
    return 0;
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

// true if the index's sorted order can find rows by column: its first column
// is that column, and it's plain, so every row has an entry in that order
static bool index_covers(const IndexColumns* index, const TableColumns* columns,
        const uint32_t column) {
    uint32_t first;
    return index->is_plain && find_column(columns, index->columns.items[0], &first) &&
           first == column;
}

// sets plan->use_index if the table has an index that can find the rows whose
// where_column equals where_value
static void plan_index(const Database* db, const SelectQuery* query, const TableColumns* columns,
        SelectPlan* plan) {
    for (uint32_t i = 0; i < db->schema_count && !plan->use_index; i++) {
        const SchemaEntry* entry = &db->schema[i];

        // automatic indexes, made for UNIQUE and PRIMARY KEY constraints, have no sql
        if (entry->type != SCHEMA_TYPE_INDEX || entry->sql == NULL ||
            !sv_equal_nocase(sv_from_cstr(entry->tbl_name), query->table.text)) {
            continue;
        }

        // an index whose sql can't be parsed is just not used
        Statement statement;
        if (parse_statement(entry->sql, &statement)) continue;

        if (statement.type == STATEMENT_TYPE_CREATE_INDEX &&
            index_covers(&statement.create_index, columns, plan->where_column)) {
            plan->use_index = true;
            plan->index_root = entry->rootpage;
            plan->index_record_columns = statement.create_index.columns.count + 1;
        }
        free_statement(&statement);
    }
}

// fills in everything the plan takes from the query and the table's columns.
// On failure plan->selected may be allocated; the caller frees it.
static int plan_columns(const Database* db, const SelectQuery* query, const TableColumns* columns,
        SelectPlan* plan) {
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
        plan->where_value = query->where.value.text;
        if (resolve_column(columns, query->where.column, &plan->where_column)) return 1;
        plan_index(db, query, columns, plan);
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
    if (result == 0) result = plan_columns(db, query, &create_statement.create_table, plan);

    // the plan holds positions, not names, so the table's columns aren't needed after this
    free_statement(&create_statement);
    if (result) free_plan(plan);
    return result;
}

void free_plan(SelectPlan* plan) {
    free(plan->selected);
    plan->selected = NULL;
}

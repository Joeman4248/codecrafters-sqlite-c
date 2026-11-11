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

// a Value, or the failure of making one. value is valid only when ok.
typedef struct ValueResult {
    bool  ok;
    Value value;
} ValueResult;

// the INTEGER Value a run of digits stands for. Returns one whose ok is
// false, after printing an error, if the number doesn't fit in 64 bits.
static ValueResult integer_value(const StringView digits) {
    int64_t integer = 0;
    for (uint32_t i = 0; i < digits.len; i++) {
        const int digit = digits.data[i] - '0';
        if (integer > (INT64_MAX - digit) / 10) {
            fprintf(stderr, "integer too large: " SV_FMT "\n", SV_ARG(digits));
            return (ValueResult) { 0 };
        }
        integer = integer * 10 + digit;
    }
    return (ValueResult) { .ok = true,
                           .value = { .type = VALUE_INTEGER, .integer = integer } };
}

// the Value a literal in the query stands for: a string is TEXT, a run of
// digits an INTEGER, and NULL is NULL. Returns one whose ok is false, after
// printing an error, if it's none of those or the integer doesn't fit in 64 bits.
static ValueResult literal_value(const Token literal) {
    switch (literal.type) {
        case TOKEN_STRING:
            return (ValueResult) { .ok = true,
                                   .value = { .type = VALUE_TEXT, .text = literal.text } };
        case TOKEN_INTEGER:
            return integer_value(literal.text);
        case TOKEN_NULL:
            return (ValueResult) { .ok = true, .value = { .type = VALUE_NULL } };
        default: break;
    }

    fprintf(stderr, "expected a string, number or NULL, got: " SV_FMT "\n", SV_ARG(literal.text));
    return (ValueResult) { 0 };
}

// returns how many column positions the query's names expand to: every column
// for each *, and one for each name
static uint32_t count_selected(const SelectQuery* query, const TableColumns* columns) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->names.count; i++) {
        const bool token_is_star = (query->names.items[i].type == TOKEN_STAR);
        count += token_is_star ? columns->names.count : 1;
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

// the same comparison with its operands swapped: 1990 < year compares year > 1990
static enum CompareOp flip_op(const enum CompareOp op) {
    switch (op) {
        case COMPARE_EQUAL:         return COMPARE_EQUAL;
        case COMPARE_LESS:          return COMPARE_GREATER;
        case COMPARE_LESS_EQUAL:    return COMPARE_GREATER_EQUAL;
        case COMPARE_GREATER:       return COMPARE_LESS;
        case COMPARE_GREATER_EQUAL: return COMPARE_LESS_EQUAL;
        case COMPARE_NOT_EQUAL:     return COMPARE_NOT_EQUAL;
    }
    return COMPARE_EQUAL;   // unreachable: every CompareOp is listed
}

// fills in everything the plan takes from the query and the table's columns.
// On failure plan->selected may be allocated; the caller frees it.
static int plan_columns(const SelectQuery* query, const TableColumns* columns, SelectPlan* plan) {
    plan->column_count    = columns->names.count;
    plan->has_rowid_alias = columns->has_rowid_alias;
    plan->rowid_column    = columns->rowid_column;

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
        const ValueResult where_value = literal_value(query->where.value);
        if (!where_value.ok) return 1;
        plan->where_value = where_value.value;
        plan->where_op = query->where.literal_first ? flip_op(query->where.op) : query->where.op;

        if (resolve_column(columns, query->where.column, &plan->where_column)) return 1;
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

#ifndef CODECRAFTERS_SQLITE_PLAN_H
#define CODECRAFTERS_SQLITE_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "pager/database.h"
#include "sql/sql_parser.h"
#include "util/value.h"
#include "util/view.h"

// the table a plan reads: everything needed to walk its rows and get a
// column's value out of one
typedef struct PlanTable {
    uint32_t root;              // root page of the table's b-tree
    uint32_t column_count;      // columns the table declares
    bool     has_rowid_alias;
    uint32_t rowid_column;      // the INTEGER PRIMARY KEY column, stored as NULL in
                                // records, when has_rowid_alias
} PlanTable;

// the columns a plain SELECT prints, as positions, in query order, with each *
// expanded to every column
typedef struct PlanColumns {
    uint32_t* items;
    uint32_t  count;
} PlanColumns;

// what a COUNT counts: rows for COUNT(*), or else the non-NULL values of column
typedef struct PlanCount {
    bool     rows;
    uint32_t column;
} PlanCount;

// how to run a SELECT, with every name in the query resolved against the
// table's schema: columns are positions in a record, and the table is its root
// page. Execution needs only this, not the query or the schema.
typedef struct SelectPlan {
    PlanTable   table;

    bool        is_count;
    PlanColumns selected;       // when !is_count
    PlanCount   count;          // when is_count

    Expr*       where;          // the plan's own copy of the WHERE, with every column's
                                // index filled in, or NULL if there is none
} SelectPlan;

// plans query against the schema of db: finds the table, reads its columns
// from its CREATE TABLE, and resolves every column the query names. A text
// literal in where points into the query's SQL text, so the plan must not outlive
// it. On success the caller releases the plan with free_plan. On failure
// prints an error to stderr, leaves nothing to free and returns nonzero.
int plan_select(const Database* db, const SelectQuery* query, SelectPlan* plan);

// frees whatever the plan allocated
void free_plan(SelectPlan* plan);

#endif // CODECRAFTERS_SQLITE_PLAN_H

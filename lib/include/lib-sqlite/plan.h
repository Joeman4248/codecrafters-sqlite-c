#ifndef CODECRAFTERS_SQLITE_PLAN_H
#define CODECRAFTERS_SQLITE_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "database.h"
#include "sql_parser.h"
#include "view.h"

// how to run a SELECT, with every name in the query resolved against the
// table's schema: columns are positions in a record, and the table is its root
// page. Execution needs only this, not the query or the schema.
typedef struct {
    uint32_t   table_root;      // root page of the table's b-tree
    uint32_t   column_count;    // columns the table declares
    bool       has_rowid_alias;
    uint32_t   rowid_column;    // the INTEGER PRIMARY KEY column, stored as NULL in
                                // records, when has_rowid_alias

    bool       is_count;
    uint32_t*  selected;        // !is_count: column positions to print, in order,
    uint32_t   selected_count;  // with each * expanded to every column
    bool       count_rows;      // is_count: COUNT(*), which counts rows,
    uint32_t   count_column;    // or else COUNT(column), which counts its non-NULL values

    bool       has_where;
    uint32_t   where_column;    // has_where: the column to compare
    StringView where_value;     // has_where: the text it must equal
} SelectPlan;

// plans query against the schema of db: finds the table, reads its columns
// from its CREATE TABLE, and resolves every column the query names.
// where_value points into the query's SQL text, so the plan must not outlive
// it. On success the caller releases the plan with free_plan. On failure
// prints an error to stderr, leaves nothing to free and returns nonzero.
int plan_select(const Database* db, const SelectQuery* query, SelectPlan* plan);

// frees whatever the plan allocated
void free_plan(SelectPlan* plan);

#endif // CODECRAFTERS_SQLITE_PLAN_H

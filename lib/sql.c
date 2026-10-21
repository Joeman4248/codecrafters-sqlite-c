#include <stdio.h>

#include "include/lib-sqlite/sql.h"
#include "include/lib-sqlite/sql_parser.h"
#include "include/lib-sqlite/plan.h"
#include "include/lib-sqlite/execute.h"

static int run_select_query(const Database* db, const SelectQuery* query) {
    SelectPlan plan;
    if (plan_select(db, query, &plan)) return 1;

    const int result = execute_select(db, &plan);

    free_plan(&plan);
    return result;
}

int run_sql_command(const Database* db, const char* sql) {
    Statement statement;
    int result = parse_statement(sql, &statement);

    if (result == 0) {
        switch (statement.type) {
            case STATEMENT_TYPE_SELECT:
                result = run_select_query(db, &statement.select);
                break;
            case STATEMENT_TYPE_CREATE_TABLE:
                fprintf(stderr, "CREATE TABLE statements aren't supported yet\n");
                break;
            case STATEMENT_TYPE_CREATE_INDEX:
                fprintf(stderr, "CREATE INDEX statements aren't supported yet\n");
                break;
            case STATEMENT_TYPE_UNKNOWN:
                break;
        }
    }

    free_statement(&statement);
    return result;
}

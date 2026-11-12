#include <stdio.h>

#include "query/commands.h"
#include "query/execute.h"
#include "query/plan.h"
#include "sql/sql_parser.h"

// command strings, indexed by their DotCommand value; DOT_COMMAND_COUNT keeps
// this list in sync with the enum
static const StringView DOT_COMMAND_NAMES[DOT_COMMAND_COUNT] = {
    [DOT_COMMAND_DBINFO] = SV_LIT(".dbinfo"),
    [DOT_COMMAND_TABLES] = SV_LIT(".tables"),
};

enum DotCommand parse_dot_command(const char* text) {
    return sv_find(DOT_COMMAND_NAMES, DOT_COMMAND_COUNT, sv_from_cstr(text));
}

int run_dbinfo_command(const Database* db) {
    printf("database page size: %u\n", db->file_header.page_size);
    printf("number of tables: %u\n", db->schema_count);
    return 0;
}

// true for a table the user made, rather than one SQLite keeps for itself,
// such as sqlite_sequence. SQLite reserves every name starting with sqlite_.
static bool is_user_table(const SchemaEntry* entry) {
    static const StringView RESERVED_PREFIX = SV_LIT("sqlite_");
    return entry->type == SCHEMA_TYPE_TABLE && !cstr_starts_with(entry->name, RESERVED_PREFIX);
}

int run_tables_command(const Database* db) {
    const char* separator = "";     // nothing before the first name, a space before the rest
    for (uint32_t i = 0; i < db->schema_count; i++) {
        const SchemaEntry* entry = &db->schema[i];
        if (is_user_table(entry)) {
            printf("%s%s", separator, entry->name);
            separator = " ";
        }
    }
    printf("\n");
    return 0;
}

static int run_select_query(Database* db, const SelectQuery* query) {
    SelectPlan plan;
    if (plan_select(db, query, &plan)) return 1;

    const int result = execute_select(db, &plan);

    free_plan(&plan);
    return result;
}

int run_sql_command(Database* db, const char* sql) {
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

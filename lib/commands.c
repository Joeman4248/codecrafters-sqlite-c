#include <stdio.h>
#include <string.h>

#include "include/lib-sqlite/commands.h"

// command strings, indexed by their DotCommand value; DOT_COMMAND_COUNT keeps
// this list in sync with the enum
static const char* const DOT_COMMAND_NAMES[DOT_COMMAND_COUNT] = {
    [DOT_COMMAND_DBINFO] = ".dbinfo",
    [DOT_COMMAND_TABLES] = ".tables",
};

enum DotCommand parse_dot_command(const char* text) {
    for (int i = 0; i < DOT_COMMAND_COUNT; i++) {
        if (strcmp(text, DOT_COMMAND_NAMES[i]) == 0) {
            return (enum DotCommand) i;
        }
    }
    return DOT_COMMAND_UNKNOWN;
}

int run_dbinfo_command(const Database* db) {
    printf("database page size: %u\n", page_size_bytes(db->file_header.page_size));
    printf("number of tables: %u\n", db->schema_count);
    return 0;
}

int run_tables_command(const Database* db) {
    uint32_t table_count = 0;

    for (uint32_t i = 0; i < db->schema_count; i++) {
        const SchemaEntry* entry = &db->schema[i];

        if (entry->type == SCHEMA_TYPE_TABLE && strncmp(entry->name, "sqlite_", 7) != 0) {
            if (table_count > 0) printf(" ");
            printf("%s", entry->name);
            table_count++;
        }
    }

    printf("\n");
    return 0;
}

int run_sql_command(const Database* db, const char* sql) {
    (void) db;
    fprintf(stderr, "SQL queries are not supported yet: %s\n", sql);
    return 1;
}

#include <stdio.h>
#include <string.h>

#include "query/commands.h"

// command strings, indexed by their DotCommand value; DOT_COMMAND_COUNT keeps
// this list in sync with the enum
static const StringView DOT_COMMAND_NAMES[DOT_COMMAND_COUNT] = {
    [DOT_COMMAND_DBINFO] = SV_LIT(".dbinfo"),
    [DOT_COMMAND_TABLES] = SV_LIT(".tables"),
};

enum DotCommand parse_dot_command(const char* text) {
    uint32_t i;
    if (sv_find(DOT_COMMAND_NAMES, DOT_COMMAND_COUNT, sv_from_cstr(text), &i)) return (enum DotCommand) i;
    return DOT_COMMAND_UNKNOWN;
}

int run_dbinfo_command(const Database* db) {
    printf("database page size: %u\n", db->file_header.page_size);
    printf("number of tables: %u\n", db->schema_count);
    return 0;
}

// true for a table the user made, rather than one SQLite keeps for itself,
// such as sqlite_sequence. SQLite reserves every name starting with sqlite_.
static bool is_user_table(const SchemaEntry* entry) {
    return entry->type == SCHEMA_TYPE_TABLE && strncmp(entry->name, "sqlite_", 7) != 0;
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

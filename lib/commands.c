#include <stdio.h>
#include <string.h>

#include "include/lib-sqlite/commands.h"

int run_tables_command(const Database* db) {
    uint32_t table_count = 0;

    for (uint32_t i = 0; i < db->schema_count; i++) {
        const SchemaEntry* entry = &db->schema[i];

        if (strcmp(entry->type, "table") == 0 && strncmp(entry->name, "sqlite_", 7) != 0) {
            if (table_count > 0) printf(" ");
            printf("%s", entry->name);
            table_count++;
        }
    }

    printf("\n");
    return 0;
}
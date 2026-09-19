#include <stdio.h>
#include <string.h>

#include "lib-sqlite/database.h"
#include "lib-sqlite/btree_page.h"
#include "lib-sqlite/commands.h"

int main(const int argc, const char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./your_program.sh <database path> <command>\n");
        return 1;
    }

    const char *database_file_path = argv[1];
    const char *command = argv[2];

    Database db = { 0 };
    db.file = fopen(database_file_path, "rb");
    if (!db.file) {
        fprintf(stderr, "Failed to open the database file\n");
        return 1;
    }

    if (read_file_header(db.file, &db.file_header) != 0) {
        fprintf(stderr, "Error reading database file header\n");
        fclose(db.file);
        return 1;
    }

    if (strcmp(command, ".dbinfo") == 0) {
        PageHeader schema_page;
        if (read_btree_header(&db, &schema_page, SCHEMA_ROOT_PAGE) != 0) {
            fprintf(stderr, "Error reading schema page\n");
            fclose(db.file);
            return 1;
        }

        printf("database page size: %u\n", db.file_header.page_size);
        printf("number of tables: %u\n", schema_page.cell_count);
    }
    else if (strcmp(command, ".tables") == 0) {
        if (run_tables_command(&db) != 0) {
            fprintf(stderr, "Error reading table names\n");
            fclose(db.file);
            return 1;
        }
    }
    else {
        fprintf(stderr, "Unknown command %s\n", command);
        fclose(db.file);
        return 1;
    }

    fclose(db.file);
    return 0;
}
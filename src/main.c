#include <stdio.h>
#include <string.h>

#include "lib-sqlite/file_header.h"
#include "lib-sqlite/btree_page.h"

int main(const int argc, const char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./your_program.sh <database path> <command>\n");
        return 1;
    }

    const char *database_file_path = argv[1];
    const char *command = argv[2];

    FILE *database_file = fopen(database_file_path, "rb");
    if (!database_file) {
        fprintf(stderr, "Failed to open the database file\n");
        return 1;
    }

    FileHeader file_header;
    PageHeader schema_page;

    if (read_file_header(database_file, &file_header) != 0) {
        fprintf(stderr, "Error reading database file header\n");
        fclose(database_file);
        return 1;
    }

    if (read_btree_header(database_file, &schema_page, file_header.page_size, SCHEMA_ROOT_PAGE) != 0) {
        fprintf(stderr, "Error reading schema page\n");
        fclose(database_file);
        return 1;
    }

    if (strcmp(command, ".dbinfo") == 0) {
        printf("database page size: %u\n", file_header.page_size);
        printf("number of tables: %u\n", schema_page.cell_count);
    }
    else if (strcmp(command, ".tables") == 0) {

    }
    else {
        fprintf(stderr, "Unknown command %s\n", command);
        fclose(database_file);
        return 1;
    }

    fclose(database_file);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>

#include "include/lib-sqlite/database.h"

int read_page(const Database* db, const uint32_t page_number, uint8_t* page) {
    const uint32_t page_size = db->file_header.page_size;

    if (fseek(db->file, (long) (page_number - 1) * page_size, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking to start of page\n");
        return 1;
    }

    if (fread(page, 1, page_size, db->file) != page_size) {
        return 1;
    }

    return 0;
}

void close_database(Database* db) {
    // fclose(NULL) is undefined, so only close a file we actually opened
    if (db->file) {
        fclose(db->file);
        db->file = NULL;
    }

    // free(NULL) is a no-op, so entries left partly filled by a failed
    // read_schema (calloc'd, so unset fields are NULL) need no checks
    for (uint32_t i = 0; i < db->schema_count; i++) {
        free(db->schema[i].name);
        free(db->schema[i].tbl_name);
        free(db->schema[i].sql);
    }
    free(db->schema);
    db->schema = NULL;
    db->schema_count = 0;
}

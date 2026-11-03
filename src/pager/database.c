#include <stdio.h>
#include <stdlib.h>

#include "pager/database.h"

// frees the schema entries and the list itself, leaving an empty schema
static void free_schema(Database* db) {
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

int close_database(Database* db) {
    // before the file, which the pager borrows
    const int leaked = pager_close(&db->pager);

    // fclose(NULL) is undefined, so only close a file we actually opened
    if (db->file) {
        fclose(db->file);
        db->file = NULL;
    }

    free_schema(db);
    return leaked;
}

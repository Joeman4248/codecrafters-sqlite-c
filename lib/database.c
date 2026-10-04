#include <stdio.h>
#include <stdlib.h>

#include "include/lib-sqlite/database.h"

void close_database(Database* db) {
    // fclose(NULL) is undefined, so only close a file we actually opened
    if (db->file) {
        fclose(db->file);
        db->file = NULL;
    }

    // free(NULL) is a no-op, so no check needed
    free(db->schema);
    db->schema = NULL;
    db->schema_count = 0;
}

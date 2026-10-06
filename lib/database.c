#include <stdio.h>
#include <stdlib.h>

#include "include/lib-sqlite/database.h"

int read_page(const Database* db, const uint32_t page_number, uint8_t* buf) {
    const uint32_t page_size = page_size_bytes(db->file_header.page_size);

    if (fseek(db->file, (long) (page_number - 1) * page_size, SEEK_SET) != 0) {
        // fprintf(stderr, "Error seeking to start of page");
        return 1;
    }

    if (fread(buf, 1, page_size, db->file) != page_size) {
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

    // free(NULL) is a no-op, so no check needed
    free(db->schema);
    db->schema = NULL;
    db->schema_count = 0;
}

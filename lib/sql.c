#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/lib-sqlite/sql.h"
#include "include/lib-sqlite/schema.h"
#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/sql_parser.h"

// counts the rows in the table rooted at rootpage into *count, Only single-page tables are supported for now:
// returns nonzero if the root page is not a leaf table page, or on error.
static int count_rows(const Database* db, const uint32_t rootpage, uint8_t* page, uint32_t* count) {
    const uint32_t page_size = db->file_header.page_size;
    if (read_page(db, rootpage, page) != 0) return 1;

    PageHeader header;
    if (read_btree_header(page, page_size, rootpage, &header) != 0) return 1;
    
    if (header.page_type != PAGE_TYPE_LEAF_TABLE) {
        fprintf(stderr, "Tables spanning multiple pages are not supported yet\n");
        return 1;
    }

    *count = header.cell_count;
    return 0;
}

// runs an SQL statement. only SELECT COUNT(*) FROM <table> is supported so far.
// prints its own error message to stderr and returns nonzero on failure.
int run_sql_command(const Database* db, const char* sql) {
    // const SchemaEntry* table = find_table(db, table_name, );
    // if (table == NULL) {
    //     fprintf(stderr, "No such table: %s\n", table_name);
    //     return 1;
    // }

    TableColumns table_columns;
    parse_table_columns(db->schema->sql, &table_columns);

    // pages can be up to 64 KiB, so keep the buffer off the stack
    uint8_t* page = malloc(db->file_header.page_size);
    if (page == NULL) return 1;

    const int result = 0;
    free(page);

    return result;
}

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

    PageHeader header;
    if (read_btree_header(page, page_size, rootpage, &header) != 0) return 1;
    
    if (header.page_type != PAGE_TYPE_LEAF_TABLE) {
        fprintf(stderr, "Tables spanning multiple pages are not supported yet\n");
        return 1;
    }

    *count = header.cell_count;
    return 0;
}

static int run_select_query(const Database* db, SelectQuery* query) {
    const uint32_t page_size = db->file_header.page_size;
    uint8_t* page = malloc(db->file_header.page_size);
    if (page == NULL) return 1;

    const SchemaEntry* table = find_table(db, &query->table.start, &query->table.len);

    PageHeader header;
    if (read_btree_header(page, page_size, table->rootpage, &header) != 0) return 1;

    if (header.page_type != PAGE_TYPE_LEAF_TABLE) {
        fprintf(stderr, "Tables spanning multiple pages are not supported yet\n");
        return 1;
    }

    Statement table_create_statement = { 0 };
    if (parse_statement(table->sql, &table_create_statement.create_table)) return 1;
    
    const TableColumns column_names = table_create_statement.create_table;

    free(page);
    return 0;
}

int run_sql_command(const Database* db, const char* sql) {
    Statement sql_statement = { 0 };
    if (parse_statement(sql, &sql_statement)) return 1;

    switch(sql_statement.type) {
        case STATEMENT_TYPE_SELECT:
            return run_select_query(db, &sql_statement.select);
            
        case STATEMENT_TYPE_CREATE_TABLE:
            printf("CREATE type sql commands aren't supported yet\n");
            break;

        case STATEMENT_TYPE_UNKNOWN: return 1;
    }

    free_statement(&sql_statement);

    return 0;
}

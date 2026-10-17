#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/lib-sqlite/sql.h"
#include "include/lib-sqlite/schema.h"
#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/record.h"
#include "include/lib-sqlite/sql_parser.h"

// reads the page numbered rootpage into page and its header into *header.
// Only single-page tables are supported for now: returns nonzero if the root
// page is not a leaf table page, or on error.
static int read_root_page(const Database* db, const uint32_t rootpage,
        uint8_t* page, PageHeader* header) {
    if (read_page(db, rootpage, page) != 0) return 1;
    if (read_btree_header(page, db->file_header.page_size, rootpage, header) != 0) return 1;

    if (header->page_type != PAGE_TYPE_LEAF_TABLE) {
        fprintf(stderr, "Tables spanning multiple pages are not supported yet\n");
        return 1;
    }
    return 0;
}

// the rows of a leaf table page, and what's needed to read them
typedef struct {
    const Database*     db;
    const TableColumns* columns;
    const uint8_t*      page;
    const uint16_t*     cell_offsets;   // one per row
    uint16_t            row_count;
    RecordColumn*       record;         // room for columns->names.count entries, reused for every row
} TableScan;

// one row as read by read_row. Its values are in scan->record until the next
// read_row.
typedef struct {
    int64_t        rowid;
    const uint8_t* payload;
    uint32_t       payload_size;
    uint32_t       column_count;   // columns in the record, which can be fewer than the table has
} Row;

// reads row number i of the scan into *row
static int read_row(const TableScan* scan, const uint16_t i, Row* row) {
    if (read_leaf_table_cell(scan->page, scan->db->file_header.usable_size, scan->cell_offsets[i],
            &row->payload, &row->payload_size, &row->rowid)) {
        return 1;
    }
    return parse_record(row->payload, row->payload_size, scan->record,
            scan->columns->names.count, &row->column_count);
}

static bool is_rowid_alias(const TableScan* scan, const uint32_t index) {
    return (int32_t) index == scan->columns->rowid_column;
}

// true if column index of the row is NULL, including when a short record
// doesn't have that column at all. The rowid alias is never NULL.
static bool is_null(const TableScan* scan, const Row* row, const uint32_t index) {
    if (is_rowid_alias(scan, index)) return false;
    return index >= row->column_count || scan->record[index].serial_type == SERIAL_NULL;
}

// prints column index of the row without a newline. NULL prints nothing.
static int print_value(const TableScan* scan, const Row* row, const uint32_t index) {
    // the record holds NULL in place of the rowid alias
    if (is_rowid_alias(scan, index)) {
        printf("%lld", (long long) row->rowid);
        return 0;
    }
    if (is_null(scan, row, index)) return 0;

    const RecordColumn* column = &scan->record[index];

    // printed straight from the page, without copying
    if (serial_type_is_text(column->serial_type)) {
        if (column->value_offset + column->value_size > row->payload_size) return 1;
        const char* column_text = (const char*) row->payload + column->value_offset;
        const int column_text_len = (int) column->value_size;
        printf("%.*s", (int) column_text_len, column_text);
        return 0;
    }

    int64_t value;
    if (record_get_int(row->payload, row->payload_size, scan->record, row->column_count,
            index, &value) == 0) {
        printf("%lld", (long long) value);
        return 0;
    }

    fprintf(stderr, "only text and integer values can be printed so far\n");
    return 1;
}

// prints the selected columns of every row, one row per line with | between values
static int print_rows(const TableScan* scan, const uint32_t* selected, const uint32_t selected_count) {
    for (uint16_t i = 0; i < scan->row_count; i++) {
        Row row;
        if (read_row(scan, i, &row) != 0) return 1;

        for (uint32_t j = 0; j < selected_count; j++) {
            if (j > 0) printf("|");
            if (print_value(scan, &row, selected[j]) != 0) return 1;
        }
        printf("\n");
    }
    return 0;
}

// prints how many rows have a non-NULL value in column index
static int count_values(const TableScan* scan, const uint32_t index) {
    uint32_t count = 0;
    for (uint16_t i = 0; i < scan->row_count; i++) {
        Row row;
        if (read_row(scan, i, &row) != 0) return 1;
        if (!is_null(scan, &row, index)) count++;
    }
    printf("%u\n", count);
    return 0;
}

// returns how many column indexes the query's names expand to: every column
// for each *, and one for each name
static uint32_t count_selected(const SelectQuery* query, const TableColumns* columns) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->names.count; i++) {
        count += (query->names.items[i].type == TOKEN_STAR) ? columns->names.count : 1;
    }
    return count;
}

// fills selected with the indexes of the columns the query names, in query
// order, with each * expanded to every column. selected has room for
// count_selected entries.
static int select_columns(const SelectQuery* query, const TableColumns* columns,
        uint32_t* selected, uint32_t* selected_count) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->names.count; i++) {
        const Token name = query->names.items[i];

        if (name.type == TOKEN_STAR) {
            for (uint32_t j = 0; j < columns->names.count; j++) selected[count++] = j;
            continue;
        }

        const int32_t index = find_column(columns, name);
        if (index < 0) {
            fprintf(stderr, "no such column: %.*s\n", (int) name.len, name.start);
            return 1;
        }
        selected[count++] = (uint32_t) index;
    }
    *selected_count = count;
    return 0;
}

// runs the query against the table's root page, read into page
static int select_rows(const Database* db, const SelectQuery* query, const SchemaEntry* table,
        const TableColumns* columns, uint8_t* page) {
    PageHeader header;
    if (read_root_page(db, table->rootpage, page, &header) != 0) return 1;

    // COUNT(*) is just the number of rows
    if (query->is_count && query->names.items[0].type == TOKEN_STAR) {
        printf("%u\n", header.cell_count);
        return 0;
    }

    uint32_t* selected = malloc(count_selected(query, columns) * sizeof(uint32_t));
    uint16_t* cell_offsets = malloc(header.cell_count * sizeof(uint16_t));
    RecordColumn* record = malloc(columns->names.count * sizeof(RecordColumn));

    // an empty table asks for malloc(0), which may return NULL without failing
    int result = 0;
    if (selected == NULL || record == NULL || (cell_offsets == NULL && header.cell_count > 0)) {
        result = 1;
    }

    uint32_t selected_count = 0;
    if (result == 0) {
        result = select_columns(query, columns, selected, &selected_count);
    }
    if (result == 0) {
        result = read_cell_pointers(page, db->file_header.page_size, table->rootpage,
                &header, cell_offsets);
    }
    if (result == 0) {
        const TableScan scan = {
            .db = db, .columns = columns, .page = page,
            .cell_offsets = cell_offsets, .row_count = header.cell_count, .record = record,
        };
        result = query->is_count ? count_values(&scan, selected[0])
                                 : print_rows(&scan, selected, selected_count);
    }

    free(selected);
    free(cell_offsets);
    free(record);
    return result;
}

static int run_select_query(const Database* db, const SelectQuery* query) {
    const SchemaEntry* table = find_table(db, query->table.start, query->table.len);
    if (table == NULL) {
        fprintf(stderr, "no such table: %.*s\n", (int) query->table.len, query->table.start);
        return 1;
    }

    Statement create_statement;
    if (parse_statement(table->sql, &create_statement) != 0) return 1;
    if (create_statement.type != STATEMENT_TYPE_CREATE_TABLE) {
        fprintf(stderr, "schema sql for %s is not a CREATE TABLE\n", table->name);
        free_statement(&create_statement);
        return 1;
    }
    
    // pages can be up to 64 KiB, so keep the buffer off the stack
    uint8_t* page = malloc(db->file_header.page_size);
    int result = (page == NULL) ? 1 : 0;
    if (result == 0) {
        result = select_rows(db, query, table, &create_statement.create_table, page);
    }

    free(page);
    free_statement(&create_statement);
    return result;
}

int run_sql_command(const Database* db, const char* sql) {
    Statement statement;
    if (parse_statement(sql, &statement) != 0) return 1;

    int result = 1;
    switch (statement.type) {
        case STATEMENT_TYPE_SELECT:
            result = run_select_query(db, &statement.select);
            break;
        case STATEMENT_TYPE_CREATE_TABLE:
            fprintf(stderr, "CREATE TABLE statements aren't supported yet\n");
            break;
        case STATEMENT_TYPE_UNKNOWN:
            break;
    }

    free_statement(&statement);
    return result;
}

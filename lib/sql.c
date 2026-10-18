#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/lib-sqlite/sql.h"
#include "include/lib-sqlite/schema.h"
#include "include/lib-sqlite/btree_cursor.h"
#include "include/lib-sqlite/record.h"
#include "include/lib-sqlite/sql_parser.h"

// what's needed to read and filter the rows of a table
typedef struct {
    const TableColumns* columns;
    int32_t             where_column;   // index of the WHERE column, or -1 when there is no WHERE
    Token               where_value;    // the text it must equal
    RecordColumn*       storage;        // room for columns->names.count entries, reused for every row
} TableScan;

// one row as read by next_row. Its record points into the cursor's page and
// scan->storage, so it is valid until the next next_row.
typedef struct {
    int64_t rowid;
    Record  record;
} Row;

// reads the cursor's next row into *row, with its record parsed into
// scan->storage. Returns CURSOR_ROW, CURSOR_DONE once there are no more rows,
// or CURSOR_ERROR.
static enum CursorStatus next_row(const TableScan* scan, TableCursor* cursor, Row* row) {
    ByteView payload;
    const enum CursorStatus status = cursor_next(cursor, &payload, &row->rowid);
    if (status != CURSOR_ROW) return status;

    if (parse_record(payload, scan->storage, scan->columns->names.count, &row->record)) {
        return CURSOR_ERROR;
    }
    return CURSOR_ROW;
}

static bool is_rowid_alias(const TableScan* scan, const uint32_t index) {
    return (int32_t) index == scan->columns->rowid_column;
}

// true if column index of the row is NULL, including when a short record
// doesn't have that column at all. The rowid alias is never NULL.
static bool is_null(const TableScan* scan, const Row* row, const uint32_t index) {
    if (is_rowid_alias(scan, index)) return false;
    return record_is_null(&row->record, index);
}

// sets *matches to whether the row passes the scan's WHERE condition. Every
// row passes when there is no WHERE.
static int row_matches(const TableScan* scan, const Row* row, bool* matches) {
    *matches = false;

    if (scan->where_column < 0) {
        *matches = true;
        return 0;
    }

    const uint32_t index = (uint32_t) scan->where_column;

    // NULL never equals anything, and only text is compared so far
    if (!record_is_text(&row->record, index)) return 0;

    StringView text;
    if (record_text_view(&row->record, index, &text)) return 1;
    *matches = sv_equal(text, scan->where_value.text);
    return 0;
}

// prints column index of the row without a newline. NULL prints nothing.
static int print_value(const TableScan* scan, const Row* row, const uint32_t index) {
    // the record holds NULL in place of the rowid alias
    if (is_rowid_alias(scan, index)) {
        printf("%lld", (long long) row->rowid);
        return 0;
    }
    if (is_null(scan, row, index)) return 0;

    // printed straight from the page, without copying
    if (record_is_text(&row->record, index)) {
        StringView text;
        if (record_text_view(&row->record, index, &text)) return 1;
        printf(SV_FMT, SV_ARG(text));
        return 0;
    }

    int64_t value;
    if (record_get_int(&row->record, index, &value)) {
        fprintf(stderr, "only text and integer values can be printed so far\n");
        return 1;
    }

    printf("%lld", (long long) value);
    return 0;
}

// prints the selected columns of every matching row, one row per line with |
// between values
static int print_rows(const TableScan* scan, TableCursor* cursor,
        const uint32_t* selected, const uint32_t selected_count) {
    Row row;
    enum CursorStatus status;
    while ((status = next_row(scan, cursor, &row)) == CURSOR_ROW) {
        bool matches;
        if (row_matches(scan, &row, &matches)) return 1;
        if (!matches) continue;

        for (uint32_t j = 0; j < selected_count; j++) {
            if (j > 0) printf("|");
            if (print_value(scan, &row, selected[j])) return 1;
        }
        printf("\n");
    }
    return status == CURSOR_ERROR;
}

// prints how many matching rows have a non-NULL value in column index, or how
// many matching rows there are when index is -1, for COUNT(*)
static int count_values(const TableScan* scan, TableCursor* cursor, const int32_t index) {
    uint32_t count = 0;
    Row row;
    enum CursorStatus status;
    while ((status = next_row(scan, cursor, &row)) == CURSOR_ROW) {
        bool matches;
        if (row_matches(scan, &row, &matches)) return 1;
        if (!matches) continue;

        if (index < 0 || !is_null(scan, &row, (uint32_t) index)) count++;
    }
    if (status == CURSOR_ERROR) return 1;

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
            fprintf(stderr, "no such column: " SV_FMT "\n", SV_ARG(name.text));
            return 1;
        }
        selected[count++] = (uint32_t) index;
    }
    *selected_count = count;
    return 0;
}

// sets *index to the index of the query's WHERE column, or -1 when there is no WHERE
static int where_column(const SelectQuery* query, const TableColumns* columns, int32_t* index) {
    *index = -1;
    if (!query->has_where) return 0;

    *index = find_column(columns, query->where.column);
    if (*index < 0) {
        fprintf(stderr, "no such column: " SV_FMT "\n", SV_ARG(query->where.column.text));
        return 1;
    }
    return 0;
}

// runs the query over every row of the table
static int select_rows(const Database* db, const SelectQuery* query, const SchemaEntry* table,
        const TableColumns* columns) {
    uint32_t* selected = malloc(count_selected(query, columns) * sizeof(uint32_t));
    RecordColumn* storage = malloc(columns->names.count * sizeof(RecordColumn));
    // zeroed so cursor_close is safe even if cursor_open is never reached
    TableCursor cursor = { 0 };

    int result = (selected == NULL || storage == NULL) ? 1 : 0;

    uint32_t selected_count = 0;
    int32_t where_index = -1;
    if (result == 0) result = select_columns(query, columns, selected, &selected_count);
    if (result == 0) result = where_column(query, columns, &where_index);
    if (result == 0) result = cursor_open(&cursor, db, table->rootpage);

    if (result == 0) {
        const TableScan scan = {
            .columns = columns,
            .where_column = where_index,
            .where_value = query->where.value,
            .storage = storage,
        };

        if (query->is_count) {
            // * in COUNT means rows, not columns: -1 counts every row
            const bool count_rows = query->names.items[0].type == TOKEN_STAR;
            result = count_values(&scan, &cursor, count_rows ? -1 : (int32_t) selected[0]);
        } 
        else {
            result = print_rows(&scan, &cursor, selected, selected_count);
        }
    }

    cursor_close(&cursor);
    free(selected);
    free(storage);
    return result;
}

static int run_select_query(const Database* db, const SelectQuery* query) {
    const SchemaEntry* table = find_table(db, query->table.text);
    if (table == NULL) {
        fprintf(stderr, "no such table: " SV_FMT "\n", SV_ARG(query->table.text));
        return 1;
    }

    Statement create_statement;
    if (parse_statement(table->sql, &create_statement)) return 1;
    if (create_statement.type != STATEMENT_TYPE_CREATE_TABLE) {
        fprintf(stderr, "schema sql for %s is not a CREATE TABLE\n", table->name);
        free_statement(&create_statement);
        return 1;
    }

    const int result = select_rows(db, query, table, &create_statement.create_table);

    free_statement(&create_statement);
    return result;
}

int run_sql_command(const Database* db, const char* sql) {
    Statement statement;
    if (parse_statement(sql, &statement)) return 1;

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

#include <stdio.h>
#include <string.h>

#include "include/lib-sqlite/commands.h"
#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/record.h"

int run_tables_command(const Database* db) {
    PageHeader schema_page;
    if (read_btree_header(db, &schema_page, SCHEMA_ROOT_PAGE) != 0) {
        return 1;
    }

    uint16_t cell_offsets[schema_page.cell_count];
    if (read_cell_pointers(db, &schema_page, SCHEMA_ROOT_PAGE, cell_offsets) != 0) {
        return 1;
    }

    uint32_t table_count = 0;

    // the payload of a leaf table cell always fits fully on its page, so the
    // largest possible inline payload is bounded by the maximum page size
    uint8_t payload[PAGE_SIZE_MAX];
    uint64_t payload_size = 0;
    uint64_t rowid = 0;

    for (uint32_t i = 0; i < schema_page.cell_count; i++) {

        if (read_leaf_table_cell(db, SCHEMA_ROOT_PAGE, cell_offsets[i],
                payload, sizeof(payload), &payload_size, &rowid) != 0) {
            return 1;
        }

        RecordColumn columns[16];
        uint32_t column_count = 0;
        if (parse_record(payload, (uint32_t) payload_size,
                columns, sizeof(columns) / sizeof(columns[0]), &column_count) != 0) {
            return 1;
        }

        char type[64];
        char name[256];
        if (record_get_text(payload, (uint32_t) payload_size,
                columns, column_count, 0, type, sizeof(type)) != 0) {
            return 1;
        }
        if (record_get_text(payload, (uint32_t) payload_size,
                columns, column_count, 1, name, sizeof(name)) != 0) {
            return 1;
        }

        if (strcmp(type, "table") == 0 && strncmp(name, "sqlite_", 7) != 0) {
            if (table_count > 0) printf(" ");
            printf("%s", name);
            table_count++;
        }
    }

    printf("\n");
    return 0;
}
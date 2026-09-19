#include <stdlib.h>

#include "include/lib-sqlite/schema.h"
#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/record.h"

int read_schema(Database* db) {
    PageHeader schema_page;
    if (read_btree_header(db, &schema_page, SCHEMA_ROOT_PAGE) != 0) {
        return 1;
    }

    uint16_t cell_offsets[schema_page.cell_count];
    if (read_cell_pointers(db, &schema_page, SCHEMA_ROOT_PAGE, cell_offsets) != 0) {
        return 1;
    }

    if (schema_page.cell_count > 0) {
        db->schema = calloc(schema_page.cell_count, sizeof(SchemaEntry));
        if (db->schema == NULL) return 1;
    }
    db->schema_count = schema_page.cell_count;

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

        if (column_count < 2) {
            return 1;
        }

        SchemaEntry* entry = &db->schema[i];

        // sqlite_schema columns: type, name, tbl_name, rootpage, sql
        if (record_get_text(payload, (uint32_t) payload_size,
                columns, column_count, SCHEMA_COL_TYPE, entry->type, sizeof(entry->type)) != 0) {
            return 1;
        }
        if (record_get_text(payload, (uint32_t) payload_size,
                columns, column_count, SCHEMA_COL_NAME, entry->name, sizeof(entry->name)) != 0) {
            return 1;
        }
    }

    return 0;
}
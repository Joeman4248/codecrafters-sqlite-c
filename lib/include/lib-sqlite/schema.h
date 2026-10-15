#ifndef CODECRAFTERS_SQLITE_SCHEMA_H
#define CODECRAFTERS_SQLITE_SCHEMA_H

#include <stdint.h>

#include "database.h"

// walks the sqlite_schema b-tree on page 1, parsing each leaf cell into a
// SchemaEntry. Allocates db->schema on the heap and sets db->schema_count.
// Returns nonzero on error.
int read_schema(Database* db);

// returns the schema entry for the table whose name matches the len
// characters at name, compared ignoring case (type == "table"), or NULL if
// there is none. name need not be null-terminated, so a token can be passed
// straight in. The pointer is into db->schema and stays valid until
// close_database.
const SchemaEntry* find_table(const Database* db, const char* name, uint32_t len);

#endif // CODECRAFTERS_SQLITE_SCHEMA_H
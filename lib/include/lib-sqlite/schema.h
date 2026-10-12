#ifndef CODECRAFTERS_SQLITE_SCHEMA_H
#define CODECRAFTERS_SQLITE_SCHEMA_H

#include <stdint.h>

#include "database.h"

// walks the sqlite_schema b-tree on page 1, parsing each leaf cell into a
// SchemaEntry. Allocates db->schema on the heap and sets db->schema_count.
// Returns nonzero on error.
int read_schema(Database* db);

// returns the schema entry for the table named name (type == "table"), or
// NULL if there is none. The pointer is into db->schema and stays valid
// until close_database.
const SchemaEntry* find_table(const Database* db, const char* name);

#endif // CODECRAFTERS_SQLITE_SCHEMA_H
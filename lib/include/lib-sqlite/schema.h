#ifndef CODECRAFTERS_SQLITE_SCHEMA_H
#define CODECRAFTERS_SQLITE_SCHEMA_H

#include <stdint.h>

#include "database.h"

// walks the sqlite_schema b-tree on page 1, parsing each leaf cell into a
// SchemaEntry. Allocates db->schema on the heap and sets db->schema_count.
// Returns nonzero on error.
int read_schema(Database* db);

#endif // CODECRAFTERS_SQLITE_SCHEMA_H
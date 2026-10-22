#ifndef CODECRAFTERS_SQLITE_SQL_H
#define CODECRAFTERS_SQLITE_SQL_H

#include "storage/database.h"

// runs an SQL statement
// prints its own error message to stderr and returns nonzero on failure.
int run_sql_command(const Database* db, const char* sql);

#endif // CODECRAFTERS_SQLITE_SQL_H

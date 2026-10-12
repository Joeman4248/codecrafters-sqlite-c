#ifndef CODECRAFTERS_SQLITE_SQL_H
#define CODECRAFTERS_SQLITE_SQL_H

#include "database.h"

// runs an SQL statement
int run_sql_command(const Database* db, const char* sql);

#endif // CODECRAFTERS_SQLITE_SQL_H

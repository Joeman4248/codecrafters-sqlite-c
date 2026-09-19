#ifndef CODECRAFTERS_SQLITE_COMMANDS_H
#define CODECRAFTERS_SQLITE_COMMANDS_H

#include "database.h"

// prints the names of all user tables in the sqlite_schema b-tree
// (rows where type == "table" and the name does not start with "sqlite_")
int run_tables_command(const Database* db);

#endif // CODECRAFTERS_SQLITE_COMMANDS_H
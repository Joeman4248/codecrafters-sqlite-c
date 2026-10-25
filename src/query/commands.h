#ifndef CODECRAFTERS_SQLITE_COMMANDS_H
#define CODECRAFTERS_SQLITE_COMMANDS_H

#include "storage/database.h"

// dot-commands accepted on the command line (SQL text is handled separately)
enum DotCommand {
    DOT_COMMAND_DBINFO,
    DOT_COMMAND_TABLES,
    DOT_COMMAND_COUNT,                          // number of real commands
    DOT_COMMAND_UNKNOWN = DOT_COMMAND_COUNT,    // not a recognised dot-command
};

// maps a command string such as ".tables" to its enum value, or to
// DOT_COMMAND_UNKNOWN if it isn't one. Matching is exact, case included.
enum DotCommand parse_dot_command(const char* text);

// each run_*_command prints its own error message to stderr and returns
// nonzero on failure, so callers only need to pass the status through

// prints the page size and the number of sqlite_schema rows
int run_dbinfo_command(const Database* db);

// prints the names of all user tables in the sqlite_schema b-tree
// (rows where type == "table" and the name does not start with "sqlite_")
int run_tables_command(const Database* db);

#endif // CODECRAFTERS_SQLITE_COMMANDS_H

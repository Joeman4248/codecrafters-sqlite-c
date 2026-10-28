#include <stdio.h>

#include "storage/database.h"
#include "storage/schema.h"
#include "query/commands.h"
#include "query/sql.h"

// positions of the command-line arguments in argv
enum Arg { ARG_PROGRAM, ARG_DATABASE_PATH, ARG_COMMAND, ARG_COUNT, };

// reads the header and schema, then runs the command.
// Returns nonzero on error. Does not release db; the caller does.
static int run(Database* db, const char* command) {
    if (read_file_header(db->file, &db->file_header)) {
        fprintf(stderr, "Error reading database file header\n");
        return 1;
    }

    if (read_schema(db)) {
        fprintf(stderr, "Error reading database schema\n");
        return 1;
    }

    // anything that isn't a dot-command is treated as SQL
    if (command[0] != '.') return run_sql_command(db, command);

    switch (parse_dot_command(command)) {
        case DOT_COMMAND_DBINFO:  return run_dbinfo_command(db);
        case DOT_COMMAND_TABLES:  return run_tables_command(db);
        case DOT_COMMAND_UNKNOWN: break;
    }

    fprintf(stderr, "Unknown command %s\n", command);
    return 1;
}

int main(const int argc, const char* argv[]) {
    if (argc != ARG_COUNT) {
        fprintf(stderr, "Usage: ./your_program.sh <database path> <command>\n");
        return 1;
    }

    const char* database_file_path = argv[ARG_DATABASE_PATH];
    const char* command = argv[ARG_COMMAND];

    Database db = { 0 };              // zero-init so close_database is always safe
    db.file = fopen(database_file_path, "rb");
    if (db.file == NULL) {
        fprintf(stderr, "Failed to open the database file\n");
        return 1;
    }

    const int status = run(&db, command);
    close_database(&db);
    return status;
}

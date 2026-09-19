#ifndef CODECRAFTERS_SQLITE_DATABASE_H
#define CODECRAFTERS_SQLITE_DATABASE_H

#include <stdio.h>

#include "file_header.h"

typedef struct {
    FILE* file;             // the open database file
    FileHeader file_header; // header fields read from the start of the file

    // future: parsed schema entries (table name -> root page), one open
    // database per struct, so multiple databases are multiple structs
} Database;

#endif // CODECRAFTERS_SQLITE_DATABASE_H
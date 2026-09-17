#ifndef CODECRAFTERS_SQLITE_FILE_HELPERS_H
#define CODECRAFTERS_SQLITE_FILE_HELPERS_H

#include <stdint.h>
#include <stdio.h>

int read_uint8(FILE* file, uint8_t* value);
int read_uint16(FILE* file, uint16_t* value);
int read_uint32(FILE* file, uint32_t* value);

#endif // CODECRAFTERS_SQLITE_FILE_HELPERS_H
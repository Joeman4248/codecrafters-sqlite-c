#ifndef CODECRAFTERS_SQLITE_FILE_H
#define CODECRAFTERS_SQLITE_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// reads len bytes starting offset bytes into file, into buf. Offsets past
// 2 GB work too, though fseek's long is only 32 bits on Windows. Returns
// nonzero if it can't seek there or the file ends first; buf may then hold
// part of the bytes.
int file_read_at(FILE* file, uint64_t offset, void* buf, size_t len);

#endif //CODECRAFTERS_SQLITE_FILE_H

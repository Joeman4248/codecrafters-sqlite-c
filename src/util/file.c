// a 64-bit off_t for fseeko on 32-bit systems; must come before any include
#define _FILE_OFFSET_BITS 64

#include "util/file.h"

#include <stdint.h>

// seeks to offset from the start of file, nonzero on failure
static int seek_to(FILE* file, const uint64_t offset) {
    if (offset > INT64_MAX) return 1;
#ifdef _WIN32
    return _fseeki64(file, (int64_t) offset, SEEK_SET);
#else
    return fseeko(file, (off_t) offset, SEEK_SET);
#endif
}

int file_read_at(FILE* file, const uint64_t offset, void* buf, const size_t len) {
    if (seek_to(file, offset)) return 1;
    return fread(buf, 1, len, file) != len;
}

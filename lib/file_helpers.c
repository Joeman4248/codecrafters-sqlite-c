#include "include/lib-sqlite/file_helpers.h"

// reads 1 byte into a uint8 value
int read_uint8(FILE* file, uint8_t* value) {
    if (fread(value, 1, 1, file) != 1)
        return 1;
    return 0;
}

// reads 2 bytes into a uint16 value in big-endian format
int read_uint16(FILE* file, uint16_t* value) {
    uint8_t buffer[2];
    if (fread(buffer, 1, 2, file) != 2) return 1;
    *value = (buffer[0] << 8) | buffer[1];
    return 0;
}

// reads 4 bytes into a uint32 value in big-endian format
int read_uint32(FILE* file, uint32_t* value) {
    uint8_t buffer[4];
    if (fread(buffer, 1, 4, file) != 4) return 1;
    *value = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    return 0;
}
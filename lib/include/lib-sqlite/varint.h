#ifndef CODECRAFTERS_SQLITE_VARINT_H
#define CODECRAFTERS_SQLITE_VARINT_H

#include <stdint.h>
#include <stdio.h>

#define VARINT_MAX_SZ 9               // a varint can be up to 9 bytes long

// reads a variable-length integer (1-9 bytes) from an in-memory buffer,
// advancing offset past the bytes it consumed
int decode_varint(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint64_t* value);

#endif // CODECRAFTERS_SQLITE_VARINT_H
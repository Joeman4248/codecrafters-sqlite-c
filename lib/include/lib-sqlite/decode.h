#ifndef CODECRAFTERS_SQLITE_DECODE_H
#define CODECRAFTERS_SQLITE_DECODE_H

#include <stdint.h>

#define VARINT_MAX_SZ 9               // a varint can be up to 9 bytes long

// decoders over an in-memory buffer of size bytes. Each reads at *offset,
// advances offset past the bytes it consumed, and returns nonzero if the
// value would run past the end of the buffer.

// big-endian fixed-width unsigned integers
int decode_uint8(const uint8_t* buf, uint32_t size, uint32_t* offset, uint8_t* value);
int decode_uint16(const uint8_t* buf, uint32_t size, uint32_t* offset, uint16_t* value);
int decode_uint32(const uint8_t* buf, uint32_t size, uint32_t* offset, uint32_t* value);

// a varint is between 1 and 9 bytes long: the lower 7 bits of each of the
// first 1-8 bytes (which all have the high bit set) plus all 8 bits of the
// ninth byte (which never has the high bit set), big-endian.
int decode_varint(const uint8_t* buf, uint32_t size, uint32_t* offset, uint64_t* value);

#endif // CODECRAFTERS_SQLITE_DECODE_H

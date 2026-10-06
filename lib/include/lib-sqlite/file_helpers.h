#ifndef CODECRAFTERS_SQLITE_FILE_HELPERS_H
#define CODECRAFTERS_SQLITE_FILE_HELPERS_H

#include <stdint.h>

// big-endian decoders over an in-memory buffer of size bytes. Each reads at
// *offset, advances offset past the bytes it consumed, and returns nonzero if
// the value would run past the end of the buffer.
int decode_uint8(const uint8_t* buf, uint32_t size, uint32_t* offset, uint8_t* value);
int decode_uint16(const uint8_t* buf, uint32_t size, uint32_t* offset, uint16_t* value);
int decode_uint32(const uint8_t* buf, uint32_t size, uint32_t* offset, uint32_t* value);

#endif // CODECRAFTERS_SQLITE_FILE_HELPERS_H

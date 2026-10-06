#include "include/lib-sqlite/file_helpers.h"

// decodes 1 byte from buf into a uint8 value
int decode_uint8(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint8_t* value) {
    if (*offset >= size) return 1;
    *value = buf[(*offset)++];
    return 0;
}

// decodes 2 bytes from buf into a uint16 value in big-endian format,
// as an upper and lower uint8
int decode_uint16(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint16_t* value) {
    uint8_t upper, lower;
    if (decode_uint8(buf, size, offset, &upper)) return 1;
    if (decode_uint8(buf, size, offset, &lower)) return 1;
    *value = ((uint16_t)upper << 8) | lower;
    return 0;
}

// decodes 4 bytes from buf into a uint32 value in big-endian format,
// as an upper and lower uint16
int decode_uint32(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint32_t* value) {
    uint16_t upper, lower;
    if (decode_uint16(buf, size, offset, &upper)) return 1;
    if (decode_uint16(buf, size, offset, &lower)) return 1;
    *value = ((uint32_t)upper << 16) | lower;
    return 0;
}

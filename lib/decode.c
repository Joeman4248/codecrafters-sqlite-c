#include "include/lib-sqlite/decode.h"

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

// decodes 3 bytes from buf into a uint32 value in big-endian format
// as an upper uint16 and lower uint8
int decode_uint24(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint32_t* value) {
    uint16_t upper;
    uint8_t  lower;
    if (decode_uint16(buf, size, offset, &upper)) return 1;
    if (decode_uint8(buf, size, offset, &lower))  return 1;
    *value = ((uint32_t)upper << 8) | lower;
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

// decodes 6 bytes from buf into a uint64 value in big-endian format
// as an upper uint32 and lower uint16
int decode_uint48(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint64_t* value) {
    uint32_t upper;
    uint16_t lower;
    if (decode_uint32(buf, size, offset, &upper)) return 1;
    if (decode_uint16(buf, size, offset, &lower)) return 1;
    *value = ((uint64_t)upper << 16) | lower;
    return 0;
}

// decodes 8 bytes from buf into a uint64 value in big-endian format,
// as an upper and lower uint32
int decode_uint64(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint64_t* value) {
    uint32_t upper, lower;
    if (decode_uint32(buf, size, offset, &upper)) return 1;
    if (decode_uint32(buf, size, offset, &lower)) return 1;
    *value = ((uint64_t)upper << 32) | lower;
    return 0;
}

// bytes 1-8 each contribute their lower 7 bits and use the high bit as a
// "more bytes follow" flag, so the loop stops at the first byte with it clear.
// If all 8 have it set, the 9th byte contributes all 8 of its bits, for
// 8 * 7 + 8 = 64 bits in total.
//
// a varint holds the bits of a 64-bit two's-complement integer, but the value
// is built in a uint64_t because shifting bits into or past the sign bit of a
// signed type is undefined behavior. Callers that need a signed value (e.g. a
// rowid) convert the result themselves.
//
// on failure, offset may already have advanced past some of the varint's bytes.
int decode_varint(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint64_t* value) {
    if (*offset >= size) return 1;

    uint64_t result = 0;

    // copy the first 1-8 bytes
    for (uint8_t i = 1; i <= 8; i++) {
        // copy the 7 lower bits
        const uint8_t byte = buf[(*offset)++];
        result = (result << 7) | (byte & 0x7F);

        // final byte never has 8th-most bit set
        if ((byte & 0x80) == 0) {
            *value = result;
            return 0;
        }

        if (*offset >= size) return 1;
    }

    if (*offset >= size) return 1;

    // copy the entire 9th byte
    result = (result << 8) | buf[(*offset)++];
    *value = result;
    return 0;
}

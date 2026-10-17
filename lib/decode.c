#include "include/lib-sqlite/decode.h"

int read_u8(Reader* reader, uint8_t* value) {
    if (reader->pos >= reader->bytes.len) return 1;
    *value = reader->bytes.data[reader->pos++];
    return 0;
}

// 2 bytes, big-endian, as an upper and lower uint8
int read_u16(Reader* reader, uint16_t* value) {
    uint8_t upper, lower;
    if (read_u8(reader, &upper)) return 1;
    if (read_u8(reader, &lower)) return 1;
    *value = ((uint16_t)upper << 8) | lower;
    return 0;
}

// 3 bytes, big-endian, as an upper uint16 and lower uint8
int read_u24(Reader* reader, uint32_t* value) {
    uint16_t upper;
    uint8_t  lower;
    if (read_u16(reader, &upper)) return 1;
    if (read_u8(reader, &lower))  return 1;
    *value = ((uint32_t)upper << 8) | lower;
    return 0;
}

// 4 bytes, big-endian, as an upper and lower uint16
int read_u32(Reader* reader, uint32_t* value) {
    uint16_t upper, lower;
    if (read_u16(reader, &upper)) return 1;
    if (read_u16(reader, &lower)) return 1;
    *value = ((uint32_t)upper << 16) | lower;
    return 0;
}

// 6 bytes, big-endian, as an upper uint32 and lower uint16
int read_u48(Reader* reader, uint64_t* value) {
    uint32_t upper;
    uint16_t lower;
    if (read_u32(reader, &upper)) return 1;
    if (read_u16(reader, &lower)) return 1;
    *value = ((uint64_t)upper << 16) | lower;
    return 0;
}

// 8 bytes, big-endian, as an upper and lower uint32
int read_u64(Reader* reader, uint64_t* value) {
    uint32_t upper, lower;
    if (read_u32(reader, &upper)) return 1;
    if (read_u32(reader, &lower)) return 1;
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
int read_varint(Reader* reader, uint64_t* value) {
    uint64_t result = 0;

    // the first 1-8 bytes
    for (uint8_t i = 1; i <= 8; i++) {
        uint8_t byte;
        if (read_u8(reader, &byte)) return 1;

        // copy the 7 lower bits
        result = (result << 7) | (byte & 0x7F);

        // final byte never has 8th-most bit set
        if ((byte & 0x80) == 0) {
            *value = result;
            return 0;
        }
    }

    // the entire 9th byte
    uint8_t last;
    if (read_u8(reader, &last)) return 1;
    *value = (result << 8) | last;
    return 0;
}

int reader_take(Reader* reader, const uint32_t len, ByteView* out) {
    if (bv_slice(reader->bytes, reader->pos, len, out)) return 1;
    reader->pos += len;
    return 0;
}

int reader_skip(Reader* reader, const uint32_t len) {
    ByteView skipped;
    return reader_take(reader, len, &skipped);
}

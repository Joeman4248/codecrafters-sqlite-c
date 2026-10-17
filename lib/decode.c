#include "include/lib-sqlite/decode.h"

// every other read goes through this one (or reader_take), so this is where
// running out of bytes is detected and made sticky
int read_u8(Reader* reader, uint8_t* value) {
    if (reader->failed || reader->pos >= reader->bytes.len) {
        reader->failed = true;
        *value = 0;
        return 1;
    }
    *value = reader->bytes.data[reader->pos++];
    return 0;
}

// The wider reads below read every part even after a failure, since a failed
// read just stores 0, then store 0 overall if anything failed rather than a
// half-built value.

// 2 bytes, big-endian, as an upper and lower uint8
int read_u16(Reader* reader, uint16_t* value) {
    uint8_t upper, lower;
    read_u8(reader, &upper);
    read_u8(reader, &lower);
    *value = reader->failed ? 0 : ((uint16_t)upper << 8) | lower;
    return reader->failed;
}

// 3 bytes, big-endian, as an upper uint16 and lower uint8
int read_u24(Reader* reader, uint32_t* value) {
    uint16_t upper;
    uint8_t  lower;
    read_u16(reader, &upper);
    read_u8(reader, &lower);
    *value = reader->failed ? 0 : ((uint32_t)upper << 8) | lower;
    return reader->failed;
}

// 4 bytes, big-endian, as an upper and lower uint16
int read_u32(Reader* reader, uint32_t* value) {
    uint16_t upper, lower;
    read_u16(reader, &upper);
    read_u16(reader, &lower);
    *value = reader->failed ? 0 : ((uint32_t)upper << 16) | lower;
    return reader->failed;
}

// 6 bytes, big-endian, as an upper uint32 and lower uint16
int read_u48(Reader* reader, uint64_t* value) {
    uint32_t upper;
    uint16_t lower;
    read_u32(reader, &upper);
    read_u16(reader, &lower);
    *value = reader->failed ? 0 : ((uint64_t)upper << 16) | lower;
    return reader->failed;
}

// 8 bytes, big-endian, as an upper and lower uint32
int read_u64(Reader* reader, uint64_t* value) {
    uint32_t upper, lower;
    read_u32(reader, &upper);
    read_u32(reader, &lower);
    *value = reader->failed ? 0 : ((uint64_t)upper << 32) | lower;
    return reader->failed;
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
        if (read_u8(reader, &byte)) {
            *value = 0;
            return 1;
        }

        // copy the 7 lower bits
        result = (result << 7) | (byte & 0x7F);

        // final byte never has 8th-most bit set
        if ((byte & 0x80) == 0) {
            *value = result;
            return 0;
        }
    }

    // the entire 9th byte
    uint8_t last_byte;
    if (read_u8(reader, &last_byte)) {
        *value = 0;
        return 1;
    }
    *value = (result << 8) | last_byte;
    return 0;
}

// the same encoding as read_varint; only the range of the result is narrower
int read_varint32(Reader* reader, uint32_t* value) {
    uint64_t wide;
    if (read_varint(reader, &wide) || wide > UINT32_MAX) {
        reader->failed = true;
        *value = 0;
        return 1;
    }
    *value = (uint32_t) wide;
    return 0;
}

int reader_take(Reader* reader, const uint32_t len, ByteView* out) {
    if (reader->failed || bv_slice(reader->bytes, reader->pos, len, out)) {
        reader->failed = true;
        *out = (ByteView) { 0 };
        return 1;
    }
    reader->pos += len;
    return 0;
}

int reader_skip(Reader* reader, const uint32_t len) {
    ByteView skipped;
    return reader_take(reader, len, &skipped);
}

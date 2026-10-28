#include <string.h>

#include "storage/reader.h"

// every other read goes through this one (or reader_take), so this is where
// running out of bytes is detected and made sticky
uint8_t read_u8(Reader* reader) {
    if (reader->failed || reader->pos >= reader->bytes.len) {
        reader->failed = true;
        return 0;
    }
    return reader->bytes.data[reader->pos++];
}

// The wider reads below read every part even after a failure, since a failed
// read just returns 0, then return 0 overall if anything failed rather than a
// half-built value.

// 2 bytes, big-endian, as an upper and lower uint8
uint16_t read_u16(Reader* reader) {
    const uint16_t upper = read_u8(reader);
    const uint16_t lower = read_u8(reader);
    return reader->failed ? 0 : ((upper << 8) | lower);
}

// 3 bytes, big-endian, as an upper uint16 and lower uint8
uint32_t read_u24(Reader* reader) {
    const uint32_t upper = read_u16(reader);
    const uint32_t lower = read_u8(reader);
    return reader->failed ? 0 : (upper << 8) | lower;
}

// 4 bytes, big-endian, as an upper and lower uint16
uint32_t read_u32(Reader* reader) {
    const uint32_t upper = read_u16(reader);
    const uint32_t lower = read_u16(reader);
    return reader->failed ? 0 : (upper << 16) | lower;
}

// 6 bytes, big-endian, as an upper uint32 and lower uint16
uint64_t read_u48(Reader* reader) {
    const uint64_t upper = read_u32(reader);
    const uint64_t lower = read_u16(reader);
    return reader->failed ? 0 : (upper << 16) | lower;
}

// 8 bytes, big-endian, as an upper and lower uint32
uint64_t read_u64(Reader* reader) {
    const uint64_t upper = read_u32(reader);
    const uint64_t lower = read_u32(reader);
    return reader->failed ? 0 : (upper << 32) | lower;
}

// raw already holds the two's-complement bits of the signed value; memcpy
// reinterprets them without the implementation-defined conversion that a cast
// of an unsigned value above INT64_MAX would be
static int64_t as_signed(const uint64_t raw) {
    int64_t value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

int64_t read_i64(Reader* reader) {
    return as_signed(read_u64(reader));
}

// memcpy reinterprets the 64 bits as a double without breaking C's aliasing rules
double read_f64(Reader* reader) {
    const uint64_t raw = read_u64(reader);
    double value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

// bytes 1-8 each contribute their lower 7 bits and use the high bit as a
// "more bytes follow" flag, so the loop stops at the first byte with it clear.
// If all 8 have it set, the 9th byte contributes all 8 of its bits, for
// 8 * 7 + 8 = 64 bits in total.
//
// a varint holds the bits of a 64-bit two's-complement integer, but the value
// is built in a uint64_t because shifting bits into or past the sign bit of a
// signed type is undefined behavior. read_varint_i64 makes it signed after.
uint64_t read_varint_u64(Reader* reader) {
    uint64_t result = 0;

    // the first 1-8 bytes. A failed read returns 0, whose high bit is clear,
    // so the loop stops there too.
    for (uint8_t i = 1; i <= 8; i++) {
        const uint8_t byte = read_u8(reader);

        // copy the 7 lower bits
        result = (result << 7) | (byte & 0x7F);

        // final byte never has 8th-most bit set
        if ((byte & 0x80) == 0) return reader->failed ? 0 : result;
    }

    // the entire 9th byte
    const uint8_t byte = read_u8(reader);
    return reader->failed ? 0 : (result << 8) | byte;
}

int64_t read_varint_i64(Reader* reader) {
    return as_signed(read_varint_u64(reader));
}

// the same encoding as read_varint_u64; only the range of the result is narrower
uint32_t read_varint_u32(Reader* reader) {
    const uint64_t wide = read_varint_u64(reader);
    if (wide > UINT32_MAX) {
        reader_fail(reader);
        return 0;
    }
    return (uint32_t) wide;
}

ByteView reader_take(Reader* reader, const uint32_t len) {
    ByteView out = { 0 };
    if (reader->failed || bv_slice(reader->bytes, reader->pos, len, &out)) {
        reader->failed = true;
    }
    else {
        reader->pos += len;
    }
    return out;
}

void reader_skip(Reader* reader, const uint32_t len) {
    reader_take(reader, len);
}

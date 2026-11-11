#include <string.h>

#include "util/reader.h"

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

// 4 bytes, big-endian, as an upper and lower uint16
uint32_t read_u32(Reader* reader) {
    const uint32_t upper = read_u16(reader);
    const uint32_t lower = read_u16(reader);
    return reader->failed ? 0 : (upper << 16) | lower;
}

// 8 bytes, big-endian, as an upper and lower uint32
uint64_t read_u64(Reader* reader) {
    const uint64_t upper = read_u32(reader);
    const uint64_t lower = read_u32(reader);
    return reader->failed ? 0 : (upper << 32) | lower;
}

// The odd widths: records also store 3- and 6-byte integers, always signed,
// so these are only the first step of read_i24 and read_i48. Each is returned
// in the next wider type.

// 3 bytes, big-endian, as an upper uint16 and lower uint8
static uint32_t read_u24(Reader* reader) {
    const uint32_t upper = read_u16(reader);
    const uint32_t lower = read_u8(reader);
    return reader->failed ? 0 : (upper << 8) | lower;
}

// 6 bytes, big-endian, as an upper uint32 and lower uint16
static uint64_t read_u48(Reader* reader) {
    const uint64_t upper = read_u32(reader);
    const uint64_t lower = read_u16(reader);
    return reader->failed ? 0 : (upper << 16) | lower;
}

// The signed reads take the unsigned bits and reinterpret them.

// raw already holds the two's-complement bits of the signed value; memcpy
// reinterprets them without the implementation-defined conversion that a cast
// of an unsigned value above INT64_MAX would be
static int64_t as_signed(const uint64_t raw) {
    int64_t value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

// interprets the low 'bits' bits of raw as a two's-complement value. XOR-ing
// the sign bit and subtracting it back maps e.g. 0xFF (8 bits) to -1 using
// only well-defined arithmetic, since raw < 2^48 always fits in an int64_t.
static int64_t sign_extend(const uint64_t raw, const unsigned bits) {
    const int64_t sign_bit = INT64_C(1) << (bits - 1);
    return ((int64_t) raw ^ sign_bit) - sign_bit;
}

int64_t read_i8(Reader* reader)  { return sign_extend(read_u8(reader), 8); }
int64_t read_i16(Reader* reader) { return sign_extend(read_u16(reader), 16); }
int64_t read_i24(Reader* reader) { return sign_extend(read_u24(reader), 24); }
int64_t read_i32(Reader* reader) { return sign_extend(read_u32(reader), 32); }
int64_t read_i48(Reader* reader) { return sign_extend(read_u48(reader), 48); }

// sign_extend's trick needs room above the sign bit, which 64 bits don't
// leave, so the full width is reinterpreted instead
int64_t read_i64(Reader* reader) { return as_signed(read_u64(reader)); }

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
    // written so the subtraction can't wrap: pos <= bytes.len is checked first
    const bool fits = reader->pos <= reader->bytes.len && len <= reader->bytes.len - reader->pos;
    if (reader->failed || !fits) {
        reader->failed = true;
        return (ByteView) { 0 };
    }

    const ByteView out = { reader->bytes.data + reader->pos, len };
    reader->pos += len;
    return out;
}

void reader_skip(Reader* reader, const uint32_t len) {
    reader_take(reader, len);
}

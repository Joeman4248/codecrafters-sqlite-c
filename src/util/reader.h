#ifndef CODECRAFTERS_SQLITE_READER_H
#define CODECRAFTERS_SQLITE_READER_H

#include <stdbool.h>
#include <stdint.h>

#include "util/view.h"

// reads values one after another from a run of bytes
typedef struct Reader {
    ByteView bytes;
    uint32_t pos;       // offset of the next byte to read
    bool     failed;    // set by the first read that runs out of bytes, or by
                        // reader_fail when the bytes make no sense
} Reader;

// a reader over bytes, starting at offset pos
static inline Reader reader_at(const ByteView bytes, const uint32_t pos) {
    return (Reader) { bytes, pos, false };
}

// moves the reader to offset pos. A pos past the end isn't an error until
// something is read there.
static inline void reader_seek(Reader* reader, const uint32_t pos) {
    reader->pos = pos;
}

// marks the reader failed, for bytes that are all there but malformed, so the
// caller's single check of reader.failed catches that too
static inline void reader_fail(Reader* reader) {
    reader->failed = true;
}

// Each read_* function reads at reader->pos, advances it past the bytes it
// consumed, and returns the value. If the value would run past the end of the
// bytes, it sets reader->failed and returns 0 (or an empty view).
//
// The failure is sticky: once failed is set, every later read also fails and
// returns 0. So a run of reads can be checked once at the end with
// reader.failed instead of one by one, as long as no value read is used to
// make a decision (a switch on a type, a loop count, an index, a size to
// allocate) before that check.

// big-endian fixed-width unsigned integers
uint8_t  read_u8(Reader* reader);
uint16_t read_u16(Reader* reader);
uint32_t read_u32(Reader* reader);
uint64_t read_u64(Reader* reader);

// big-endian fixed-width two's-complement signed integers, all returned as
// int64_t since that's what every caller stores them in. The 24- and 48-bit
// widths are for record integer values.
int64_t read_i8(Reader* reader);
int64_t read_i16(Reader* reader);
int64_t read_i24(Reader* reader);
int64_t read_i32(Reader* reader);
int64_t read_i48(Reader* reader);
int64_t read_i64(Reader* reader);

// 8 bytes, big-endian, as an IEEE 754 double
double read_f64(Reader* reader);

// a varint is between 1 and 9 bytes long: the lower 7 bits of each of the
// first 1-8 bytes (which all have the high bit set) plus all 8 bits of the
// ninth byte (which never has the high bit set), big-endian. This reads its
// 64 bits as they are, unsigned.
uint64_t read_varint_u64(Reader* reader);

// reads a varint holding a two's-complement signed integer, such as a rowid
int64_t read_varint_i64(Reader* reader);

// reads a varint that must fit in 32 bits, such as a size. A larger value
// fails the same way as running out of bytes.
uint32_t read_varint_u32(Reader* reader);

// returns a view of the next len bytes and moves past them. Fails the reader,
// returning an empty view, if fewer than len bytes are left.
ByteView reader_take(Reader* reader, uint32_t len);

// moves past the next len bytes
void reader_skip(Reader* reader, uint32_t len);

#endif // CODECRAFTERS_SQLITE_READER_H

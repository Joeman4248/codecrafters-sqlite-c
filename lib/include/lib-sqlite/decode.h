#ifndef CODECRAFTERS_SQLITE_DECODE_H
#define CODECRAFTERS_SQLITE_DECODE_H

#include <stdbool.h>
#include <stdint.h>

#include "view.h"

// reads values one after another from a run of bytes
typedef struct {
    ByteView bytes;
    uint32_t pos;       // offset of the next byte to read
    bool     failed;    // set by the first read that runs out of bytes
} Reader;

// a reader over bytes, starting at offset pos
static inline Reader reader_at(const ByteView bytes, const uint32_t pos) {
    return (Reader) { bytes, pos, false };
}

// Each read_* function reads at reader->pos and advances it past the bytes it
// consumed. If the value would run past the end of the bytes, it sets
// reader->failed, stores 0 (or an empty view), and returns nonzero.
//
// The failure is sticky: once failed is set, every later read also fails and
// stores 0. So a run of reads can be checked once at the end with
// reader.failed instead of one by one, as long as no value read is used to
// make a decision before that check.

// big-endian fixed-width unsigned integers. The 24- and 48-bit widths (used by
// record integer values) are returned in the next wider type.
int read_u8(Reader* reader, uint8_t* value);
int read_u16(Reader* reader, uint16_t* value);
int read_u24(Reader* reader, uint32_t* value);
int read_u32(Reader* reader, uint32_t* value);
int read_u48(Reader* reader, uint64_t* value);
int read_u64(Reader* reader, uint64_t* value);

// a varint is between 1 and 9 bytes long: the lower 7 bits of each of the
// first 1-8 bytes (which all have the high bit set) plus all 8 bits of the
// ninth byte (which never has the high bit set), big-endian.
int read_varint(Reader* reader, uint64_t* value);

// reads a varint that must fit in 32 bits, such as a size. A larger value
// fails the same way as running out of bytes.
int read_varint32(Reader* reader, uint32_t* value);

// points *out at the next len bytes and moves past them
int reader_take(Reader* reader, uint32_t len, ByteView* out);

// moves past the next len bytes
int reader_skip(Reader* reader, uint32_t len);

#endif // CODECRAFTERS_SQLITE_DECODE_H

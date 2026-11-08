#include "record/record.h"
#include "util/reader.h"

// serial type codes for the record format
enum SerialType {
    SERIAL_NULL,
    SERIAL_INT8,  SERIAL_INT16, SERIAL_INT24,
    SERIAL_INT32, SERIAL_INT48, SERIAL_INT64,
    SERIAL_FLOAT, SERIAL_ZERO,  SERIAL_ONE,

    // serial type codes 10, 11 never appear in a well-formed database file

    // the smallest BLOB and TEXT codes. Every larger even code is also a BLOB
    // and every larger odd code is also TEXT, with the length folded in.
    SERIAL_BLOB = 12, SERIAL_TEXT = 13,
};

// the enum case for a serial type code: codes 12 and up truncate to
// SERIAL_BLOB or SERIAL_TEXT by their low bit; smaller codes are their own case
// (10 and 11 included, which match no case)
static enum SerialType serial_type_kind(const uint64_t serial_type) {
    if (serial_type >= SERIAL_BLOB) return SERIAL_BLOB + serial_type % 2;
    return serial_type;
}

// the byte size of the value described by a serial type code (0 for NULL and
// for serial types 8 and 9, whose value is the code itself). Fails the reader
// and returns 0 for the reserved codes 10 and 11.
static uint32_t serial_type_size(Reader* reader, const uint64_t serial_type) {
    const enum SerialType kind = serial_type_kind(serial_type);
    switch (kind) {
        case SERIAL_NULL: case SERIAL_ZERO: case SERIAL_ONE:
            return 0;

        case SERIAL_INT8:   return 1;
        case SERIAL_INT16:  return 2;
        case SERIAL_INT24:  return 3;
        case SERIAL_INT32:  return 4;
        case SERIAL_INT48:  return 6;

        case SERIAL_INT64: case SERIAL_FLOAT:
            return 8;

        // the length is folded in above the base code
        case SERIAL_BLOB: case SERIAL_TEXT:
            return (uint32_t) ((serial_type - kind) / 2);

        default: break;
    }
    reader_fail(reader);
    return 0;
}

// reads the header's serial types into header->columns, one per column, and
// sets header->count. reader covers only the header, positioned just after its
// size varint, so a malformed serial type can't run on into the values. Each
// value's offset counts from the start of the body. Fails the reader if a value
// would run past body_len, a serial type is reserved (10 or 11), or there are
// more columns than header->capacity.
static void parse_record_header(Reader* reader, const uint32_t body_len, RecordHeader* header) {
    uint32_t count = 0;
    uint32_t offset = 0;    // where the next value starts in the body

    while (!reader->failed && reader->pos < reader->bytes.len) {
        if (count >= header->capacity) {
            reader_fail(reader);
            break;
        }

        // checked before its size is used to place the value
        const uint64_t serial_type = read_varint_u64(reader);
        if (reader->failed) break;

        const uint32_t value_size = serial_type_size(reader, serial_type);
        if (reader->failed) break;
        if (value_size > body_len - offset) {
            reader_fail(reader);
            break;
        }

        header->columns[count++] = (RecordColumn) { serial_type, offset, value_size };
        offset += value_size;
    }

    header->count = count;
}

Record read_record(Reader* reader, const RecordHeader header) {
    Record record = { .header = header };
    const ByteView all = reader->bytes;
    const uint32_t start = reader->pos;

    // the header size counts the varint holding it, so it can't be smaller than that
    const uint32_t header_size = read_varint_u32(reader);
    if (reader->failed) return record;
    if (header_size < reader->pos - start || header_size > all.len - start) {
        reader_fail(reader);
        return record;
    }

    // the header is the next header_size bytes, and the body everything after.
    // The reader is cut to the header while it is parsed, then given back its bytes.
    const uint32_t body_start = start + header_size;
    const ByteView body = { all.data + body_start, all.len - body_start };
    reader->bytes = bv_prefix(all, body_start);
    parse_record_header(reader, body.len, &record.header);
    reader->bytes = all;
    if (reader->failed) return record;

    record.body = body;
    reader_seek(reader, all.len);
    return record;
}

// interprets the low 'bits' bits of raw as a two's-complement value. XOR-ing
// the sign bit and subtracting it back maps e.g. 0xFF (8 bits) to -1 using
// only well-defined arithmetic, since raw < 2^48 always fits in an int64_t.
static int64_t sign_extend(const uint64_t raw, const unsigned bits) {
    const int64_t sign_bit = INT64_C(1) << (bits - 1);
    return ((int64_t) raw ^ sign_bit) - sign_bit;
}

// reads an integer of serial type 1-6 at the reader's position. Fails the
// reader and returns 0 for any other serial type.
static int64_t read_int(Reader* reader, const uint64_t serial_type) {
    switch (serial_type) {
        case SERIAL_INT8:  return sign_extend(read_u8(reader), 8);
        case SERIAL_INT16: return sign_extend(read_u16(reader), 16);
        case SERIAL_INT24: return sign_extend(read_u24(reader), 24);
        case SERIAL_INT32: return sign_extend(read_u32(reader), 32);
        case SERIAL_INT48: return sign_extend(read_u48(reader), 48);
        case SERIAL_INT64: return read_i64(reader);
    }
    reader_fail(reader);
    return 0;
}

Value record_get_value(const Record* record, const uint32_t index) {
    if (index >= record->header.count) return (Value) { .type = VALUE_NULL };

    const RecordColumn* col = &record->header.columns[index];
    Reader reader = reader_at(record->body, col->value_offset);

    switch (serial_type_kind(col->serial_type)) {
        case SERIAL_NULL: return (Value) { .type = VALUE_NULL };
        case SERIAL_ZERO: return (Value) { .type = VALUE_INTEGER, .integer = 0 };
        case SERIAL_ONE:  return (Value) { .type = VALUE_INTEGER, .integer = 1 };

        case SERIAL_INT8:  case SERIAL_INT16: case SERIAL_INT24:
        case SERIAL_INT32: case SERIAL_INT48: case SERIAL_INT64:
            return (Value) { .type = VALUE_INTEGER,
                             .integer = read_int(&reader, col->serial_type) };

        case SERIAL_FLOAT:
            return (Value) { .type = VALUE_REAL, .real = read_f64(&reader) };

        // read_record has already checked the length fits in the body
        case SERIAL_BLOB:
            return (Value) { .type = VALUE_BLOB, 
                             .blob = reader_take(&reader, col->value_size) };
        case SERIAL_TEXT:
            return (Value) { .type = VALUE_TEXT,
                             .text = bv_as_text(reader_take(&reader, col->value_size)) };

        // 10 and 11: read_record rejects them, so a column never holds one
        default:
            return (Value) { .type = VALUE_NULL };
    }
}

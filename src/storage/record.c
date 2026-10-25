#include "storage/record.h"
#include "storage/reader.h"

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

// true if a serial type code holds a TEXT value (odd, 13 and up)
static bool serial_type_is_text(const uint64_t serial_type) {
    return serial_type >= SERIAL_TEXT && serial_type % 2 == 1;
}

// the byte size of the value described by a serial type code (0 for NULL and
// for serial types 8 and 9, whose value is the code itself)
static uint32_t serial_type_size(const uint64_t serial_type) {
    switch (serial_type) {
        case SERIAL_NULL: case SERIAL_ZERO: case SERIAL_ONE: 
            return 0;

        case SERIAL_INT8:   return 1;
        case SERIAL_INT16:  return 2;
        case SERIAL_INT24:  return 3;
        case SERIAL_INT32:  return 4;
        case SERIAL_INT48:  return 6;

        case SERIAL_INT64: case SERIAL_FLOAT:
            return 8;

        default:
            // even serial types >= 12: BLOB of (serial_type - 12) / 2 bytes in length
            // odd serial types >= 13: string of (serial_type - 13) / 2 bytes in length
            if (serial_type_is_text(serial_type))
                return (uint32_t) ((serial_type - SERIAL_TEXT) / 2);
            else
                return (uint32_t) ((serial_type - SERIAL_BLOB) / 2);
    }
}

// reads the header's serial types into header->columns, one per column, and
// sets header->count. reader covers only the header, positioned just after its
// size varint, so a malformed serial type can't run on into the values. Each
// value's offset counts from the start of the body. Fails the reader if a value
// would run past body_len or there are more columns than header->capacity.
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

        const uint32_t value_size = serial_type_size(serial_type);
        if (value_size > body_len - offset) {
            reader_fail(reader);
            break;
        }

        header->columns[count++] = (RecordColumn) { serial_type, offset, value_size };
        offset += value_size;
    }

    header->count = count;
}

int parse_record(const ByteView payload, Record* record) {
    Reader reader = reader_at(payload, 0);

    // the header size counts the varint holding it, so it can't be smaller than that
    const uint32_t header_size = read_varint_u32(&reader);
    if (reader.failed || header_size < reader.pos || header_size > payload.len) return 1;

    // the header is the first header_size bytes, and the body everything after
    reader.bytes = bv_prefix(payload, header_size);
    const ByteView body = { payload.data + header_size, payload.len - header_size };
    parse_record_header(&reader, body.len, &record->header);
    if (reader.failed) return 1;

    record->body = body;
    return 0;
}

// interprets the low 'bits' bits of raw as a two's-complement value. XOR-ing
// the sign bit and subtracting it back maps e.g. 0xFF (8 bits) to -1 using
// only well-defined arithmetic, since raw < 2^48 always fits in an int64_t.
static int64_t sign_extend(const uint64_t raw, const unsigned bits) {
    const int64_t sign_bit = INT64_C(1) << (bits - 1);
    return ((int64_t) raw ^ sign_bit) - sign_bit;
}

// reads an integer of serial type 1-6 at the reader's position
static int64_t read_int(Reader* reader, const uint64_t serial_type) {
    switch (serial_type) {
        case SERIAL_INT8:  return sign_extend(read_u8(reader), 8);
        case SERIAL_INT16: return sign_extend(read_u16(reader), 16);
        case SERIAL_INT24: return sign_extend(read_u24(reader), 24);
        case SERIAL_INT32: return sign_extend(read_u32(reader), 32);
        case SERIAL_INT48: return sign_extend(read_u48(reader), 48);
        case SERIAL_INT64: return read_i64(reader);
        default: return 0;
    }
}

int record_get_value(const Record* record, const uint32_t index, Value* out) {
    if (index >= record->header.count) {
        *out = (Value) { .type = VALUE_NULL };
        return 0;
    }

    const RecordColumn* col = &record->header.columns[index];
    Reader reader = reader_at(record->body, col->value_offset);

    switch (col->serial_type) {
        case SERIAL_NULL: *out = (Value) { .type = VALUE_NULL }; return 0;
        case SERIAL_ZERO: *out = (Value) { .type = VALUE_INTEGER, .integer = 0 }; return 0;
        case SERIAL_ONE:  *out = (Value) { .type = VALUE_INTEGER, .integer = 1 }; return 0;

        case SERIAL_INT8:  case SERIAL_INT16: case SERIAL_INT24:
        case SERIAL_INT32: case SERIAL_INT48: case SERIAL_INT64:
            *out = (Value) { .type = VALUE_INTEGER, .integer = read_int(&reader, col->serial_type) };
            break;

        case SERIAL_FLOAT:
            *out = (Value) { .type = VALUE_REAL, .real = read_f64(&reader) };
            break;

        default: {
            // text or blob: the length is folded into the serial type, and
            // parse_record has already checked it fits in the body
            const ByteView bytes = reader_take(&reader, col->value_size);
            *out = serial_type_is_text(col->serial_type)
                ? (Value) { .type = VALUE_TEXT, .text = bv_as_text(bytes) }
                : (Value) { .type = VALUE_BLOB, .blob = bytes };
            break;
        }
    }
    return reader.failed;
}
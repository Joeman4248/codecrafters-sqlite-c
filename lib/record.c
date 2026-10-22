#include <string.h>

#include "include/lib-sqlite/record.h"
#include "include/lib-sqlite/decode.h"

// serial type codes for the record format
enum SerialType {
    SERIAL_NULL,
    SERIAL_INT8, SERIAL_INT16, SERIAL_INT24, 
    SERIAL_INT32, SERIAL_INT48, SERIAL_INT64,
    SERIAL_FLOAT, SERIAL_ZERO, SERIAL_ONE,

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
// value's offset counts from the start of the body, and one that would run past
// body_len is an error.
static int parse_record_header(Reader* reader, const uint32_t body_len, RecordHeader* header) {
    uint32_t count = 0;
    uint32_t offset = 0;    // where the next value starts in the body

    while (reader->pos < reader->bytes.len) {
        if (count >= header->capacity) return 1;

        uint64_t serial_type = 0;
        if (read_varint_u64(reader, &serial_type)) return 1;

        const uint32_t value_size = serial_type_size(serial_type);
        if (value_size > body_len - offset) return 1;

        header->columns[count++] = (RecordColumn) { serial_type, offset, value_size };
        offset += value_size;
    }

    header->count = count;
    return 0;
}

int parse_record(const ByteView payload, Record* record) {
    Reader reader = reader_at(payload, 0);

    // the header size counts the varint holding it, so it can't be smaller than that
    uint32_t header_size = 0;
    if (read_varint_u32(&reader, &header_size) ||
        header_size < reader.pos || header_size > payload.len) {
        return 1;
    }

    // the header is the first header_size bytes, and the body everything after
    reader.bytes = bv_prefix(payload, header_size);
    const ByteView body = { payload.data + header_size, payload.len - header_size };
    if (parse_record_header(&reader, body.len, &record->header)) return 1;

    record->body = body;
    return 0;
}

bool record_is_null(const Record* record, const uint32_t index) {
    return index >= record->header.count ||
           record->header.columns[index].serial_type == SERIAL_NULL;
}

bool record_is_text(const Record* record, const uint32_t index) {
    return index < record->header.count &&
           serial_type_is_text(record->header.columns[index].serial_type);
}

// interprets the low 'bits' bits of raw as a two's-complement value. XOR-ing
// the sign bit and subtracting it back maps e.g. 0xFF (8 bits) to -1 using
// only well-defined arithmetic, since raw < 2^48 always fits in an int64_t.
static int64_t sign_extend(const uint64_t raw, const unsigned bits) {
    const int64_t sign_bit = INT64_C(1) << (bits - 1);
    return ((int64_t) raw ^ sign_bit) - sign_bit;
}

// reads an integer of serial type 1-6 at reader into *value
static int read_int(Reader* reader, const uint64_t serial_type, int64_t* value) {
    switch (serial_type) {
        case SERIAL_INT8: {
            uint8_t raw;
            if (read_u8(reader, &raw)) return 1;
            *value = sign_extend(raw, 8);
            return 0;
        }
        case SERIAL_INT16: {
            uint16_t raw;
            if (read_u16(reader, &raw)) return 1;
            *value = sign_extend(raw, 16);
            return 0;
        }
        case SERIAL_INT24: {
            uint32_t raw;
            if (read_u24(reader, &raw)) return 1;
            *value = sign_extend(raw, 24);
            return 0;
        }
        case SERIAL_INT32: {
            uint32_t raw;
            if (read_u32(reader, &raw)) return 1;
            *value = sign_extend(raw, 32);
            return 0;
        }
        case SERIAL_INT48: {
            uint64_t raw;
            if (read_u48(reader, &raw)) return 1;
            *value = sign_extend(raw, 48);
            return 0;
        }
        default:
            return read_i64(reader, value);
    }
}

// reads a big-endian IEEE 754 double at reader into *value. memcpy reinterprets
// the 64 bits as a double without breaking C's aliasing rules.
static int read_real(Reader* reader, double* value) {
    uint64_t raw;
    if (read_u64(reader, &raw)) return 1;
    memcpy(value, &raw, sizeof(*value));
    return 0;
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
            out->type = VALUE_INTEGER;
            return read_int(&reader, col->serial_type, &out->integer);

        case SERIAL_FLOAT:
            out->type = VALUE_REAL;
            return read_real(&reader, &out->real);

        default: {
            // text or blob: the length is folded into the serial type, and
            // parse_record has already checked it fits in the body
            ByteView bytes;
            if (reader_take(&reader, col->value_size, &bytes)) return 1;
            *out = serial_type_is_text(col->serial_type)
                ? (Value) { .type = VALUE_TEXT, .text = bv_as_text(bytes) }
                : (Value) { .type = VALUE_BLOB, .blob = bytes };
            return 0;
        }
    }
}

int record_get_int(const Record* record, const uint32_t index, int64_t* value) {
    Value v;
    if (record_get_value(record, index, &v) || v.type != VALUE_INTEGER) return 1;
    *value = v.integer;
    return 0;
}

int record_text_view(const Record* record, const uint32_t index, StringView* out) {
    Value v;
    if (record_get_value(record, index, &v) || v.type != VALUE_TEXT) return 1;
    *out = v.text;
    return 0;
}
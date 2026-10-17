#include <string.h>

#include "include/lib-sqlite/record.h"
#include "include/lib-sqlite/decode.h"

uint32_t serial_type_size(uint64_t serial_type) {
    switch (serial_type) {
        case SERIAL_NULL: case SERIAL_ZERO: case SERIAL_ONE: 
            return 0;
        case SERIAL_INT64: case SERIAL_FLOAT:
            return 8;
        case SERIAL_INT8:  return 1;
        case SERIAL_INT16: return 2;
        case SERIAL_INT24: return 3;
        case SERIAL_INT32: return 4;
        case SERIAL_INT48: return 6;
        // serial type codes 10, 11 never appear in a well-formed database file
        default:
            // even serial types >= 12: BLOB of (serial_type - 12) / 2 bytes in length
            // odd serial types >= 13: string of (serial_type - 13) / 2 bytes in length
            if (serial_type_is_text(serial_type))
                return (uint32_t) ((serial_type - SERIAL_TEXT) / 2);
            else
                return (uint32_t) ((serial_type - SERIAL_BLOB) / 2);
    }
}

bool serial_type_is_text(const uint64_t serial_type) {
    return serial_type >= SERIAL_TEXT && serial_type % 2 == 1;
}

int parse_record(const ByteView payload,
        RecordColumn* columns, uint32_t max_columns, uint32_t* column_count) {
    Reader reader = reader_at(payload, 0);
    uint64_t header_size = 0;

    if (read_varint(&reader, &header_size)) return 1;
    if (header_size < reader.pos || header_size > payload.len) return 1;

    // the serial types are read from the header only, so a malformed one
    // can't run on into the values
    reader.bytes = bv_prefix(payload, (uint32_t) header_size);

    uint32_t count = 0;
    uint32_t body_size = 0;

    while (reader.pos < reader.bytes.len) {
        if (count >= max_columns) return 1;

        uint64_t serial_type = 0;
        if (read_varint(&reader, &serial_type)) return 1;

        const uint32_t value_size = serial_type_size(serial_type);
        columns[count].serial_type = serial_type;
        columns[count].value_size = value_size;
        columns[count].value_offset = (uint32_t) header_size + body_size;

        body_size += value_size;
        count++;
    }

    if (body_size > payload.len || header_size > payload.len - body_size) return 1;

    *column_count = count;
    return 0;
}

// interprets the low 'bits' bits of raw as a two's-complement value. XOR-ing
// the sign bit and subtracting it back maps e.g. 0xFF (8 bits) to -1 using
// only well-defined arithmetic, since raw < 2^48 always fits in an int64_t.
static int64_t sign_extend(const uint64_t raw, const unsigned bits) {
    const int64_t sign_bit = INT64_C(1) << (bits - 1);
    return ((int64_t) raw ^ sign_bit) - sign_bit;
}

int record_get_int(const ByteView payload,
        const RecordColumn* columns, const uint32_t column_count,
        const uint32_t index, int64_t* value) {
    if (index >= column_count) return 1;

    const RecordColumn* col = &columns[index];
    Reader reader = reader_at(payload, col->value_offset);

    switch (col->serial_type) {
        case SERIAL_ZERO: *value = 0; return 0;
        case SERIAL_ONE:  *value = 1; return 0;

        case SERIAL_INT8: {
            uint8_t raw;
            if (read_u8(&reader, &raw)) return 1;
            *value = sign_extend(raw, 8);
            return 0;
        }
        case SERIAL_INT16: {
            uint16_t raw;
            if (read_u16(&reader, &raw)) return 1;
            *value = sign_extend(raw, 16);
            return 0;
        }
        case SERIAL_INT24: {
            uint32_t raw;
            if (read_u24(&reader, &raw)) return 1;
            *value = sign_extend(raw, 24);
            return 0;
        }
        case SERIAL_INT32: {
            uint32_t raw;
            if (read_u32(&reader, &raw)) return 1;
            *value = sign_extend(raw, 32);
            return 0;
        }
        case SERIAL_INT48: {
            uint64_t raw;
            if (read_u48(&reader, &raw)) return 1;
            *value = sign_extend(raw, 48);
            return 0;
        }
        case SERIAL_INT64: {
            // all 64 bits are already the two's-complement pattern; memcpy
            // copies them without an implementation-defined conversion
            uint64_t raw;
            if (read_u64(&reader, &raw)) return 1;
            memcpy(value, &raw, sizeof(*value));
            return 0;
        }

        default:
            // NULL, float, text, or blob
            return 1;
    }
}

int record_text_view(const ByteView payload,
        const RecordColumn* columns, const uint32_t column_count,
        const uint32_t index, StringView* out) {
    if (index >= column_count) return 1;

    const RecordColumn* col = &columns[index];
    if (!serial_type_is_text(col->serial_type)) return 1;

    ByteView bytes;
    if (bv_slice(payload, col->value_offset, col->value_size, &bytes)) return 1;

    *out = bv_as_text(bytes);
    return 0;
}
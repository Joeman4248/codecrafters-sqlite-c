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

int parse_record(const ByteView payload, RecordColumn* storage, const uint32_t max_columns,
        Record* out) {
    Reader reader = reader_at(payload, 0);

    uint32_t header_size = 0;
    if (read_varint_u32(&reader, &header_size) ||
        header_size < reader.pos || header_size > payload.len) {
        return 1;
    }

    // the serial types are read from the header only, so a malformed one
    // can't run on into the values
    reader.bytes = bv_prefix(payload, header_size);

    uint32_t count = 0;
    uint32_t body_size = 0;

    while (reader.pos < reader.bytes.len) {
        if (count >= max_columns) return 1;

        uint64_t serial_type = 0;
        if (read_varint_u64(&reader, &serial_type)) return 1;

        const uint32_t value_size = serial_type_size(serial_type);
        storage[count].serial_type = serial_type;
        storage[count].value_size = value_size;
        storage[count].value_offset = header_size + body_size;

        body_size += value_size;
        count++;
    }

    if (body_size > payload.len || header_size > payload.len - body_size) return 1;

    *out = (Record) { payload, storage, count };
    return 0;
}

bool record_is_null(const Record* record, const uint32_t index) {
    return index >= record->count || record->columns[index].serial_type == SERIAL_NULL;
}

bool record_is_text(const Record* record, const uint32_t index) {
    return index < record->count && serial_type_is_text(record->columns[index].serial_type);
}

// interprets the low 'bits' bits of raw as a two's-complement value. XOR-ing
// the sign bit and subtracting it back maps e.g. 0xFF (8 bits) to -1 using
// only well-defined arithmetic, since raw < 2^48 always fits in an int64_t.
static int64_t sign_extend(const uint64_t raw, const unsigned bits) {
    const int64_t sign_bit = INT64_C(1) << (bits - 1);
    return ((int64_t) raw ^ sign_bit) - sign_bit;
}

int record_get_int(const Record* record, const uint32_t index, int64_t* value) {
    if (index >= record->count) return 1;

    const RecordColumn* col = &record->columns[index];
    Reader reader = reader_at(record->payload, col->value_offset);

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
        case SERIAL_INT64:
            return read_i64(&reader, value);

        // NULL, float, text, or blob
        default: return 1;
    }
}

int record_text_view(const Record* record, const uint32_t index, StringView* out) {
    if (!record_is_text(record, index)) return 1;

    const RecordColumn* col = &record->columns[index];
    ByteView bytes;
    if (bv_slice(record->payload, col->value_offset, col->value_size, &bytes)) return 1;

    *out = bv_as_text(bytes);
    return 0;
}
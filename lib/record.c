#include <string.h>

#include "include/lib-sqlite/record.h"
#include "include/lib-sqlite/varint.h"

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
            if (serial_type % 2 == 0)
                return (uint32_t) ((serial_type - 12) / 2);
            else
                return (uint32_t) ((serial_type - 13) / 2);
    }
}

// the record body (each column's value) begins immediately after the header,
// with the columns laid out back-to-back in the order their serial types appear
int parse_record(const uint8_t* data, uint32_t size,
        RecordColumn* columns, uint32_t max_columns, uint32_t* column_count) {
    uint32_t offset = 0;
    uint64_t header_size = 0;

    if (decode_varint(data, size, &offset, &header_size)) return 1;
    if (header_size < offset || header_size > size) return 1;

    uint32_t count = 0;
    uint32_t body_size = 0;

    while (offset < (uint32_t) header_size) {
        if (count >= max_columns) return 1;

        uint64_t serial_type = 0;
        if (decode_varint(data, size, &offset, &serial_type)) return 1;

        const uint32_t value_size = serial_type_size(serial_type);
        columns[count].serial_type = serial_type;
        columns[count].value_size = value_size;
        columns[count].value_offset = (uint32_t) header_size + body_size;

        body_size += value_size;
        count++;
    }

    if (body_size > size || header_size > size - body_size) return 1;

    *column_count = count;
    return 0;
}

int record_get_text(const uint8_t* data, const uint32_t size,
        const RecordColumn* columns, const uint32_t column_count,
        const uint32_t column, char* out, const uint32_t out_capacity) {
    if (column >= column_count) return 1;

    const RecordColumn* col = &columns[column];
    // text values use odd serial types 13 and up
    if (col->serial_type < 13 || col->serial_type % 2 == 0) return 1;
    if (col->value_offset + col->value_size > size)         return 1;
    if (col->value_size >= out_capacity)                    return 1;

    memcpy(out, data + col->value_offset, col->value_size);
    out[col->value_size] = '\0';
    return 0;
}
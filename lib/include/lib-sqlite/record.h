#ifndef CODECRAFTERS_SQLITE_RECORD_H
#define CODECRAFTERS_SQLITE_RECORD_H

#include <stdint.h>

#define VARINT_MAX_SZ 9               // a varint can be up to 9 bytes long

// a varint is between 1 and 9 bytes long: the lower 7 bits of each of the
// first 1-8 bytes (which all have the high bit set) plus all 8 bits of the
// ninth byte (which never has the high bit set), big-endian.
// Reads a variable-length integer (1-9 bytes) from an in-memory buffer,
// advancing offset past the bytes it consumed.
int decode_varint(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint64_t* value);

// serial type codes for the record format
enum SerialType {
    SERIAL_NULL,
    SERIAL_INT8,
    SERIAL_INT16,
    SERIAL_INT24,
    SERIAL_INT32,
    SERIAL_INT48,
    SERIAL_INT64,
    SERIAL_FLOAT,
    SERIAL_ZERO,
    SERIAL_ONE,
};

typedef struct {
    uint64_t serial_type;     // serial type code for this column
    uint32_t value_offset;    // byte offset of the column value within the record
    uint32_t value_size;      // size of the column value in bytes
} RecordColumn;

// returns the byte size of the value described by a serial type code
// (0 for NULL and for serial types 8 and 9)
uint32_t serial_type_size(uint64_t serial_type);

// parses a record's header: reads the header-size varint and the per-column
// serial type varints from data, filling the caller-provided columns array
// (which must hold at least max_columns entries). Stores the number of columns
// in column_count. Returns nonzero if the record is malformed.
int parse_record(const uint8_t* data, uint32_t size,
        RecordColumn* columns, uint32_t max_columns, uint32_t* column_count);

// copies a TEXT column (odd serial type >= 13) from a parsed record into a
// null-terminated buffer of out_capacity bytes. Returns nonzero if the column
// is not text or does not fit in the buffer.
int record_get_text(const uint8_t* data, uint32_t size,
        const RecordColumn* columns, uint32_t column_count,
        uint32_t column, char* out, uint32_t out_capacity);

#endif // CODECRAFTERS_SQLITE_RECORD_H
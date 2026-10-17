#ifndef CODECRAFTERS_SQLITE_RECORD_H
#define CODECRAFTERS_SQLITE_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "view.h"

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

    // the smallest BLOB and TEXT codes. Every larger even code is also a BLOB
    // and every larger odd code is also TEXT, with the length folded in.
    SERIAL_BLOB = 12,
    SERIAL_TEXT = 13,
};

typedef struct {
    uint64_t serial_type;     // serial type code for this column
    uint32_t value_offset;    // byte offset of the column value within the record
    uint32_t value_size;      // size of the column value in bytes
} RecordColumn;

// returns the byte size of the value described by a serial type code
// (0 for NULL and for serial types 8 and 9)
uint32_t serial_type_size(uint64_t serial_type);

// true if a serial type code holds a TEXT value (odd, 13 and up)
bool serial_type_is_text(uint64_t serial_type);

// parses a record's header: reads the header-size varint and the per-column
// serial type varints from data, filling the caller-provided columns array
// (which must hold at least max_columns entries). Stores the number of columns
// in column_count. Returns nonzero if the record is malformed.
int parse_record(const uint8_t* data, uint32_t size,
        RecordColumn* columns, uint32_t max_columns, uint32_t* column_count);

// reads an INTEGER column (serial types 1-6, 8 and 9) from a parsed record into
// value as a signed 64-bit integer. Returns nonzero if the column is not an
// integer or its bytes run past the end of the record.
int record_get_int(const uint8_t* data, uint32_t size,
        const RecordColumn* columns, uint32_t column_count,
        uint32_t index, int64_t* value);

// points *out at a TEXT column (odd serial type >= 13) of a parsed record,
// without copying it: the view is into data, so it is valid while data is.
// Returns nonzero if the column is not text or runs past the end of the
// record.
int record_text_view(const uint8_t* data, uint32_t size,
        const RecordColumn* columns, uint32_t column_count,
        uint32_t index, StringView* out);

// copies a TEXT column (odd serial type >= 13) from a parsed record into a
// null-terminated buffer of out_capacity bytes. Returns nonzero if the column
// is not text or does not fit in the buffer.
int record_get_text(const uint8_t* data, uint32_t size,
        const RecordColumn* columns, uint32_t column_count,
        uint32_t column, char* out, uint32_t out_capacity);

#endif // CODECRAFTERS_SQLITE_RECORD_H
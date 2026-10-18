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

// a record's payload together with its parsed header, which says where each
// column's value sits in the payload. Neither is copied: payload points into a
// page and columns into storage the caller owns, both of which must outlive it.
typedef struct {
    ByteView      payload;
    RecordColumn* columns;
    uint32_t      count;    // columns in this record, which can be fewer than its table has
} Record;

// returns the byte size of the value described by a serial type code
// (0 for NULL and for serial types 8 and 9)
uint32_t serial_type_size(uint64_t serial_type);

// true if a serial type code holds a TEXT value (odd, 13 and up)
bool serial_type_is_text(uint64_t serial_type);

// parses a record's header: reads the header-size varint and the per-column
// serial type varints from payload into storage, which must hold at least
// max_columns entries, and points *out at both. Returns nonzero if the record
// is malformed or has more than max_columns columns.
int parse_record(ByteView payload, RecordColumn* storage, uint32_t max_columns, Record* out);

// true if column index is NULL, including when the record is too short to
// have it at all (a row written before its table gained that column)
bool record_is_null(const Record* record, uint32_t index);

// true if column index holds TEXT
bool record_is_text(const Record* record, uint32_t index);

// reads an INTEGER column (serial types 1-6, 8 and 9) into value as a signed
// 64-bit integer. Returns nonzero if the column is not an integer or its bytes
// run past the end of the record.
int record_get_int(const Record* record, uint32_t index, int64_t* value);

// points *out at a TEXT column (odd serial type >= 13) without copying it: the
// view is into the payload, so it is valid while the payload is. Use
// sv_to_cstr for a copy that outlives it. Returns nonzero if the column is not
// text or runs past the end of the record.
int record_text_view(const Record* record, uint32_t index, StringView* out);

#endif // CODECRAFTERS_SQLITE_RECORD_H
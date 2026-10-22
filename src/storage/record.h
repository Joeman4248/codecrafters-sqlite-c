#ifndef CODECRAFTERS_SQLITE_RECORD_H
#define CODECRAFTERS_SQLITE_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "util/value.h"
#include "util/view.h"

// A record is a row's values, stored as a header followed by a body:
//
//     [header size] [serial type 0] [serial type 1] ... | [value 0] [value 1] ...
//      └──────────────────── header ──────────────────┘   └─────── body ───────┘
//
// The header says what kind of value each column holds and so how many bytes
// it takes; the body holds the values back to back, in column order. How
// values are encoded is private to record.c: other code asks about them
// through the record_ functions below.

// one column's entry in the record header: what kind of value it holds, and
// where that value sits in the body. Callers only make room for these (see
// RecordHeader); parse_record fills them in and the record_ functions read them.
typedef struct {
    uint64_t serial_type;     // serial type code for this column
    uint32_t value_offset;    // byte offset of the value from the start of the body
    uint32_t value_size;      // size of the value in bytes
} RecordColumn;

// a record header, parsed: one entry per column. The caller provides the
// storage by setting columns and capacity; parse_record fills it and sets count.
typedef struct {
    RecordColumn* columns;  // storage the caller owns
    uint32_t      capacity; // entries columns has room for
    uint32_t      count;    // columns in this record, which can be fewer than its table has
} RecordHeader;

// a record split into its header and body. Nothing is copied: body points into
// a page and the header's columns into storage the caller owns, both of which
// must outlive the Record.
typedef struct {
    RecordHeader header;    // what each column holds and where
    ByteView     body;      // the values, back to back
} Record;

// splits payload, a whole record, into record's header and body. The header's
// serial types are parsed into record->header.columns, which the caller has
// already pointed at storage for record->header.capacity entries; count is set
// to how many there are, and body to the values. Returns nonzero if the record
// is malformed, a value runs past the end of the body, or it has more columns
// than capacity.
int parse_record(ByteView payload, Record* record);

// decodes column index into *out, whatever its storage class. A column past the
// end of a short record (a row written before its table gained that column) is
// NULL. Text and blobs view the body without copying, so they are valid while
// the body is. Returns nonzero if the value runs past the end of the record.
int record_get_value(const Record* record, uint32_t index, Value* out);

#endif // CODECRAFTERS_SQLITE_RECORD_H
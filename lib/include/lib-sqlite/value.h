#ifndef CODECRAFTERS_SQLITE_VALUE_H
#define CODECRAFTERS_SQLITE_VALUE_H

#include <stdbool.h>
#include <stdint.h>

#include "view.h"

// SQLite's five storage classes: every value in a database is one of these
enum ValueType {
    VALUE_NULL,
    VALUE_INTEGER,
    VALUE_REAL,         // a 64-bit IEEE 754 double
    VALUE_TEXT,
    VALUE_BLOB,
};

// one value of any storage class. Only the union member matching type is
// valid, and NULL has none. Text and blobs are views, not copies, so they are
// valid only while the bytes they view are, such as a record's page.
typedef struct {
    enum ValueType type;
    union {
        int64_t    integer;     // VALUE_INTEGER
        double     real;        // VALUE_REAL
        StringView text;        // VALUE_TEXT
        ByteView   blob;        // VALUE_BLOB
    };
} Value;

// true if a and b are equal values, the way SQL's = compares them: integers
// and reals compare as numbers, text and blobs byte for byte, and values of
// any other two different types are never equal. NULL equals nothing, not even
// NULL.
bool value_equal(Value a, Value b);

#endif // CODECRAFTERS_SQLITE_VALUE_H

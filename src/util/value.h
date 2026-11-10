#ifndef CODECRAFTERS_SQLITE_VALUE_H
#define CODECRAFTERS_SQLITE_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "util/view.h"

// SQLite's five storage classes: every value in a database is one of these
enum ValueType : uint8_t {
    VALUE_NULL,
    VALUE_INTEGER,
    VALUE_REAL,         // a 64-bit IEEE 754 double
    VALUE_TEXT,
    VALUE_BLOB,
};

// one value of any storage class. Only the union member matching type is
// valid, and NULL has none. Text and blobs are views, not copies, so they are
// valid only while the bytes they view are, such as a record's page.
typedef struct Value {
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

// the result of comparing two values. The three are exhaustive, so there is no
// ORDER_COUNT: nothing looks an order up by name, and a switch over it has no
// "not found" to report.
enum CompareOrder : uint8_t {
    ORDER_LESS,
    ORDER_EQUAL,
    ORDER_GREATER,
};

// compares a and b as SQL orders values, so the result is a total order: NULL
// sorts below every number, numbers (integer or real, compared as one class)
// below text, text below blob, and values of the same class compare by their
// contents, text and blobs byte for byte. NULLs compare as equal to each other.
//
// A comparison in a WHERE still drops rows whose side is NULL, which is a rule
// of the comparison, not of the order: callers check for NULL before comparing.
enum CompareOrder value_compare(Value a, Value b);

// writes value to out without a newline, the way sqlite3 prints it: nothing
// for NULL, floats always with a decimal point, and text and blobs as their
// raw bytes
void value_print(Value value, FILE* out);

#endif // CODECRAFTERS_SQLITE_VALUE_H

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

// the result of comparing two values. The three are exhaustive, so there is no
// ORDER_COUNT: nothing looks an order up by name, and a switch over it has no
// "not found" to report.
enum CompareOrder : uint8_t {
    ORDER_LESS,
    ORDER_EQUAL,
    ORDER_GREATER,
};

// the comparison operators SQL can write between two values. Like CompareOrder
// they are a closed set: nothing looks an operator up by name, so there is no
// COMPARE_COUNT and no COMPARE_UNKNOWN.
enum CompareOp : uint8_t {
    COMPARE_EQUAL,          // = or ==
    COMPARE_LESS,           // <
    COMPARE_LESS_EQUAL,     // <=
    COMPARE_GREATER,        // >
    COMPARE_GREATER_EQUAL,  // >=
    COMPARE_NOT_EQUAL,      // != or <>
};

// compares a and b as SQL orders values, so the result is a total order: NULL
// sorts below every number, numbers (integer or real, compared as one class)
// below text, text below blob, and values of the same class compare by their
// contents, text and blobs byte for byte. NULLs compare as equal to each other.
// This is the order sorting and index b-trees use; a WHERE uses value_satisfies.
enum CompareOrder value_compare(Value a, Value b);

// whether a op b holds the way a WHERE compares: like value_compare, except
// that a comparison with NULL on either side never holds, whatever the
// operator, so NULL = NULL and NULL != 1 are both false
bool value_satisfies(Value a, enum CompareOp op, Value b);

// the operator that makes the same comparison with its operands swapped:
// 1990 < year is year > 1990
enum CompareOp compare_op_swapped(enum CompareOp op);

// writes value to out without a newline, the way sqlite3 prints it: nothing
// for NULL, floats always with a decimal point, and text and blobs as their
// raw bytes
void value_print(Value value, FILE* out);

#endif // CODECRAFTERS_SQLITE_VALUE_H

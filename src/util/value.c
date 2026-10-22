#include <string.h>

#include "util/value.h"

bool value_equal(const Value a, const Value b) {
    // an integer and a real are both numbers
    if (a.type == VALUE_INTEGER && b.type == VALUE_REAL) return (double) a.integer == b.real;
    if (a.type == VALUE_REAL && b.type == VALUE_INTEGER) return a.real == (double) b.integer;
    if (a.type != b.type) return false;

    switch (a.type) {
        case VALUE_NULL:    return false;
        case VALUE_INTEGER: return a.integer == b.integer;
        case VALUE_REAL:    return a.real == b.real;
        case VALUE_TEXT:    return sv_equal(a.text, b.text);
        case VALUE_BLOB:
            return a.blob.len == b.blob.len && memcmp(a.blob.data, b.blob.data, a.blob.len) == 0;
    }
    return false;
}

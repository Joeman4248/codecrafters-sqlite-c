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

// prints a REAL the way sqlite3 does: up to 15 significant digits, and always
// with a decimal point, which %g leaves out of whole numbers ("3", "1e+100"
// rather than "3.0", "1.0e+100")
static void print_real(const double real, FILE* out) {
    char text[32];
    snprintf(text, sizeof(text), "%.15g", real);

    // already has a point, or is inf or nan
    if (strchr(text, '.') != NULL || strchr(text, 'n') != NULL) {
        fputs(text, out);
        return;
    }

    const char* exponent = strchr(text, 'e');
    if (exponent == NULL) fprintf(out, "%s.0", text);
    else fprintf(out, "%.*s.0%s", (int) (exponent - text), text, exponent);
}

void value_print(const Value value, FILE* out) {
    // text and blobs are written straight from wherever they're viewed, without copying
    switch (value.type) {
        case VALUE_NULL:    break;
        case VALUE_INTEGER: fprintf(out, "%lld", (long long) value.integer); break;
        case VALUE_REAL:    print_real(value.real, out); break;
        case VALUE_TEXT:    fprintf(out, SV_FMT, SV_ARG(value.text)); break;
        case VALUE_BLOB:    fwrite(value.blob.data, 1, value.blob.len, out); break;
    }
}

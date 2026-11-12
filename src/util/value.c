#include <string.h>

#include "util/macros.h"
#include "util/value.h"

// ORDER_LESS when less, ORDER_GREATER when greater, and ORDER_EQUAL when
// neither, so each comparison below is one line
static enum CompareOrder order_of(const bool less, const bool greater) {
    if (less)    return ORDER_LESS;
    if (greater) return ORDER_GREATER;
    return ORDER_EQUAL;
}

// the order of two storage classes: real and integer are one number class, and
// ValueType is declared in SQL's order, so a class's own value is its rank.
// ORDER_EQUAL means only that both sides are in the same class, so their
// contents still have to decide.
static enum CompareOrder compare_classes(const enum ValueType a, const enum ValueType b) {
    const uint8_t a_rank = (a == VALUE_REAL) ? VALUE_INTEGER : a;
    const uint8_t b_rank = (b == VALUE_REAL) ? VALUE_INTEGER : b;
    if (a_rank == b_rank) return ORDER_EQUAL;
    return (a_rank < b_rank) ? ORDER_LESS : ORDER_GREATER;
}

// the three-way order of two runs of bytes, as SQL's BINARY collation orders
// text and blobs: the first differing byte decides, and when one run is a
// prefix of the other, the shorter one is less
static enum CompareOrder compare_bytes(const ByteView a, const ByteView b) {
    const uint32_t common = MIN(a.len, b.len);

    // memcmp must not be given NULL, even for a length of 0
    if (common > 0) {
        const int order = memcmp(a.data, b.data, common);
        if (order < 0) return ORDER_LESS;
        if (order > 0) return ORDER_GREATER;
    }

    if (a.len == b.len) return ORDER_EQUAL;
    return (a.len < b.len) ? ORDER_LESS : ORDER_GREATER;
}

// int64's ends as doubles. INT64_MIN is a power of two and exact; INT64_MAX is
// not representable and rounds up to 2^63, which is the bound we want anyway,
// since no double lies between the two.
static const double INT64_MIN_AS_DOUBLE = -0x1p63;
static const double INT64_MAX_AS_DOUBLE =  0x1p63;

// the same order seen from the other side: a < b read as b > a
static enum CompareOrder flipped(const enum CompareOrder order) {
    switch (order) {
        case ORDER_LESS:    return ORDER_GREATER;
        case ORDER_EQUAL:   return ORDER_EQUAL;
        case ORDER_GREATER: return ORDER_LESS;
        case ORDER_UNKNOWN: break;
    }
    return ORDER_UNKNOWN;
}

// orders an integer against a real exactly. Casting the integer to a double
// would round past 2^53, so the whole parts are compared as integers first.
static enum CompareOrder compare_int_real(const int64_t integer, const double real) {
    if (real < INT64_MIN_AS_DOUBLE)  return ORDER_GREATER;  // real is below every integer
    if (real >= INT64_MAX_AS_DOUBLE) return ORDER_LESS;     // real is above every integer

    const int64_t whole = (int64_t) real;   // truncates toward zero
    if (integer != whole) return order_of(integer < whole, integer > whole);

    // same whole part, so only the fraction is left, and both sides are doubles now
    return order_of((double) integer < real, (double) integer > real);
}

// orders two values that are both numbers, however they are stored: two
// integers, two reals, or one of each
static enum CompareOrder compare_numbers(const Value a, const Value b) {
    if (a.type == VALUE_INTEGER && b.type == VALUE_INTEGER) {
        return order_of(a.integer < b.integer, a.integer > b.integer);
    }
    if (a.type == VALUE_REAL && b.type == VALUE_REAL) {
        return order_of(a.real < b.real, a.real > b.real);
    }

    // one of each: order the integer against the real, flipping when the real is
    // the left operand
    if (a.type == VALUE_INTEGER) return compare_int_real(a.integer, b.real);
    return flipped(compare_int_real(b.integer, a.real));
}

enum CompareOrder value_compare(const Value a, const Value b) {
    const enum CompareOrder classes = compare_classes(a.type, b.type);
    if (classes != ORDER_EQUAL) return classes;

    // same class, so b is the same type or its number twin
    switch (a.type) {
        case VALUE_NULL:    return ORDER_EQUAL;
        case VALUE_INTEGER:
        case VALUE_REAL:    return compare_numbers(a, b);
        case VALUE_TEXT:    return compare_bytes(sv_as_bytes(a.text), sv_as_bytes(b.text));
        case VALUE_BLOB:    return compare_bytes(a.blob, b.blob);
    }
    return ORDER_UNKNOWN;
}

// whether order is one that op accepts
static bool order_satisfies(const enum CompareOrder order, const enum CompareOp op) {
    // no op accepts it, though "anything but greater" for <= would
    if (order == ORDER_UNKNOWN) return false;

    switch (op) {
        case COMPARE_EQUAL:         return order == ORDER_EQUAL;
        case COMPARE_LESS:          return order == ORDER_LESS;
        case COMPARE_LESS_EQUAL:    return order != ORDER_GREATER;
        case COMPARE_GREATER:       return order == ORDER_GREATER;
        case COMPARE_GREATER_EQUAL: return order != ORDER_LESS;
        case COMPARE_NOT_EQUAL:     return order != ORDER_EQUAL;
        case COMPARE_UNKNOWN:       break;
    }
    return false;
}

bool value_satisfies(const Value a, const enum CompareOp op, const Value b) {
    if (a.type == VALUE_NULL || b.type == VALUE_NULL) return false;
    const enum CompareOrder order = value_compare(a, b);
    return order_satisfies(order, op);
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

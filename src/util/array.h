#ifndef CODECRAFTERS_SQLITE_ARRAY_H
#define CODECRAFTERS_SQLITE_ARRAY_H

#include <stdbool.h>
#include <stdint.h>

// the capacity a full array of capacity elements grows to, in *grown: 8 for
// an empty one, otherwise double. Doubling means the copying realloc does
// adds up to a constant amount per element rather than growing with the
// array. Returns false if doubling would wrap around.
static inline bool grown_capacity(const uint32_t capacity, uint32_t* grown) {
    if (capacity > UINT32_MAX / 2) return false;
    *grown = (capacity == 0) ? 8 : capacity * 2;
    return true;
}

#endif //CODECRAFTERS_SQLITE_ARRAY_H

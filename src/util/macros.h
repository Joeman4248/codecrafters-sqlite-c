#ifndef CODECRAFTERS_SQLITE_MACROS_H
#define CODECRAFTERS_SQLITE_MACROS_H

// small macros for what a function can't do: work on any type, or give a
// constant the compiler can use

// the smaller of a and b. A macro, so it works for any type, but each
// argument may be evaluated twice: pass plain values, not i++ or calls.
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// how many elements an array has, as a constant. Only for a real array, not
// a pointer to one: sizeof a pointer gives nonsense (gcc warns about it).
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#endif //CODECRAFTERS_SQLITE_MACROS_H

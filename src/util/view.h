#ifndef CODECRAFTERS_SQLITE_VIEW_H
#define CODECRAFTERS_SQLITE_VIEW_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// characters that are not copied: data points into a longer string, such as
// the SQL text or a page, which must outlive the view. The end is given by
// len, so a view is not necessarily null-terminated.
typedef struct StringView {
    const char* data;
    uint32_t    len;
} StringView;

// bytes that are neither copied nor owned, such as a page or a record payload
typedef struct ByteView {
    const uint8_t* data;
    uint32_t       len;
} ByteView;

// initialises a StringView from a string literal. sizeof sees the literal
// itself (a char array, not a pointer), so the length is worked out at compile
// time, without the '\0'. Usable in static initialisers.
#define SV_LIT(s)   { s, sizeof(s) - 1 }

// prints a StringView with printf: printf("no such table: " SV_FMT "\n", SV_ARG(name))
#define SV_FMT      "%.*s"
#define SV_ARG(s)   (int) (s).len, (s).data

// views at most max characters of a null-terminated string, or all of it if
// it's shorter
static inline StringView sv_from_cstr_max(const char* s, const uint32_t max) {
    return (StringView) { s, (uint32_t) strnlen(s, max) };
}

// views a null-terminated string, without its terminator
static inline StringView sv_from_cstr(const char* s) {
    return sv_from_cstr_max(s, UINT32_MAX);
}

// copies s into a new null-terminated heap string, for text that must outlive
// what s points into. The caller frees it. Returns NULL if the allocation fails.
static inline char* sv_to_cstr(const StringView s) {
    char* copy = malloc(s.len + 1);
    if (copy == NULL) return NULL;
    // memcpy must not be given NULL, even for a length of 0
    if (s.len > 0) memcpy(copy, s.data, s.len);
    copy[s.len] = '\0';
    return copy;
}

// true if a and b hold the same characters
static inline bool sv_equal(const StringView a, const StringView b) {
    // memcmp must not be given NULL, even for a length of 0
    return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

// true if the null-terminated string s starts with prefix. Never reads past
// s's terminator.
static inline bool cstr_starts_with(const char* s, const StringView prefix) {
    return sv_equal(sv_from_cstr_max(s, prefix.len), prefix);
}

// true if a and b hold the same characters, ignoring ASCII case. For names,
// which SQL compares case-insensitively.
static inline bool sv_equal_nocase(const StringView a, const StringView b) {
    return a.len == b.len && (a.len == 0 || strncasecmp(a.data, b.data, a.len) == 0);
}

// the first len bytes of bytes, or all of them if there are fewer
static inline ByteView bv_prefix(const ByteView bytes, const uint32_t len) {
    return (ByteView) { bytes.data, len < bytes.len ? len : bytes.len };
}

// the same characters viewed as bytes, for the comparisons text and blobs share
static inline ByteView sv_as_bytes(const StringView text) {
    return (ByteView) { (const uint8_t*) text.data, text.len };
}

// the same bytes viewed as text
static inline StringView bv_as_text(const ByteView bytes) {
    return (StringView) { (const char*) bytes.data, bytes.len };
}

// returns the index of the first of the count views in list that equals s,
// or count if none does. For mapping text to an enum through a table of names
// indexed by the enum's values: with FOO_UNKNOWN = FOO_COUNT, the result casts
// straight to the enum, not-found included.
static inline uint32_t sv_find(const StringView* list, const uint32_t count, const StringView s) {
    uint32_t i = 0;
    while (i < count && !sv_equal(list[i], s)) i++;
    return i;
}

// sv_find, ignoring ASCII case
static inline uint32_t sv_find_nocase(const StringView* list, const uint32_t count, const StringView s) {
    uint32_t i = 0;
    while (i < count && !sv_equal_nocase(list[i], s)) i++;
    return i;
}

#endif // CODECRAFTERS_SQLITE_VIEW_H

#include "include/lib-sqlite/varint.h"

// a varint is between 1 and 9 bytes long: the lower 7 bits of each of the
// first 8 bytes (which all have the high bit set) plus all 8 bits of the
// ninth byte (which never has the high bit set), big-endian
int decode_varint(const uint8_t* buf, const uint32_t size, uint32_t* offset, uint64_t* value) {
    if (*offset >= size) return 1;

    uint64_t result = 0;

    for (int i = 0; i < 8; i++) {
        const uint8_t byte = buf[(*offset)++];
        result = (result << 7) | (byte & 0x7f);

        if ((byte & 0x80) == 0) {
            *value = result;
            return 0;
        }

        if (*offset >= size) return 1;
    }

    if (*offset >= size) return 1;

    result = (result << 8) | buf[(*offset)++];
    *value = result;
    return 0;
}
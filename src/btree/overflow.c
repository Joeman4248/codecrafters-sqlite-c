#include <stdbool.h>
#include <stdlib.h>

#include "btree/overflow.h"
#include "util/array.h"
#include "util/macros.h"
#include "util/reader.h"

// makes room in buffer for at least size bytes, doubling until it fits
static int reserve(OverflowBuffer* buffer, const uint32_t size) {
    uint32_t capacity = buffer->capacity;
    while (capacity < size) {
        // size comes from the file, so a corrupt one could ask for too much
        if (!grown_capacity(capacity, &capacity)) return 1;
    }
    if (capacity == buffer->capacity) return 0;

    uint8_t* grown = realloc(buffer->data, capacity);
    if (grown == NULL) return 1;    // the old bytes are still valid, the caller frees them
    buffer->data = grown;
    buffer->capacity = capacity;
    return 0;
}

// copies len bytes of the record from overflow page page_number to dest, and
// gives the number of the page after it in *next_page, 0 if it's the last
static int copy_overflow_page(Pager* pager, const uint32_t page_number, uint8_t* dest,
                              const uint32_t len, uint32_t* next_page) {
    Page* page = pager_get(pager, page_number);
    if (page == NULL) return 1;

    Reader reader = reader_at(page_bytes(page), 0);
    *next_page = read_u32(&reader);
    const ByteView part = reader_take(&reader, len);
    if (!reader.failed) bv_copy(dest, part);

    pager_unref(pager, page);
    return reader.failed;
}

int assemble_payload(Pager* pager, const TableLeafCell* cell, OverflowBuffer* buffer,
                     ByteView* record) {
    if (cell->overflow_page == 0) {
        *record = cell->payload;
        return 0;
    }
    if (reserve(buffer, cell->payload_size)) return 1;

    bv_copy(buffer->data, cell->payload);
    uint32_t copied = cell->payload.len;

    // each overflow page starts with the next one's number, then the record
    const uint32_t per_page = pager->usable_size - 4;
    uint32_t page_number = cell->overflow_page;
    while (copied < cell->payload_size) {
        if (page_number == 0) return 1;     // the chain ends before the record does

        const uint32_t len = MIN(cell->payload_size - copied, per_page);
        if (copy_overflow_page(pager, page_number, buffer->data + copied, len, &page_number)) {
            return 1;
        }
        copied += len;
    }
    // the record is whole, so the chain must end here; a loop in it ends up here too
    if (page_number != 0) return 1;

    *record = (ByteView) { buffer->data, cell->payload_size };
    return 0;
}

void free_overflow_buffer(OverflowBuffer* buffer) {
    free(buffer->data);
    *buffer = (OverflowBuffer) { 0 };
}

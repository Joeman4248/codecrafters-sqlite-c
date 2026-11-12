#ifndef CODECRAFTERS_SQLITE_OVERFLOW_H
#define CODECRAFTERS_SQLITE_OVERFLOW_H

#include <stdint.h>

#include "btree/btree_page.h"
#include "pager/pager.h"
#include "util/view.h"

// room to put a record that spills onto overflow pages back together. One
// buffer is reused for record after record: it only grows, when a bigger
// record comes along. Start it zeroed and free it with free_overflow_buffer.
typedef struct OverflowBuffer {
    uint8_t* data;
    uint32_t capacity;      // bytes data has room for
} OverflowBuffer;

// gives cell's whole record in *record. One that fits in the cell is viewed
// where it is, uncopied. One that spills is put together in buffer: the part
// in the cell, then each overflow page's part, following the chain from
// cell->overflow_page; *record then views it inside buffer, valid until buffer
// is used again or freed. Returns nonzero if buffer can't grow, a page can't
// be read, or the chain doesn't fit the record's size (it ends early or runs
// on), which only a corrupt file has.
int assemble_payload(Pager* pager, const TableLeafCell* cell, OverflowBuffer* buffer,
                     ByteView* record);

// frees the buffer's bytes and leaves it zeroed, ready to use again
void free_overflow_buffer(OverflowBuffer* buffer);

#endif // CODECRAFTERS_SQLITE_OVERFLOW_H

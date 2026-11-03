#ifndef CODECRAFTERS_SQLITE_PCACHE_H
#define CODECRAFTERS_SQLITE_PCACHE_H

#include <stdbool.h>
#include <stdint.h>

#include "util/view.h"


// one page of the database, read into memory the page cache owns, and which page it is
typedef struct Page {
    uint8_t* data;          // the page's bytes
    uint32_t size;          // bytes to read from data: the usable size, which leaves
                            // out the reserved space at the end that no b-tree uses
    uint32_t page_number;   // pages are numbered from 1
} Page;

// the page's bytes as a read-only view, for reading through a Reader
static inline ByteView page_bytes(const Page* page) {
    return (ByteView) { page->data, page->size };
}

// buckets in the hash table. Fixed for now; growing it comes later.
#define PCACHE_BUCKET_COUNT 64

// one page's place in the cache. The cache creates slots empty (not loaded,
// no refs, data NULL); the pager reads the page into it and counts refs.
// Once loaded, page.data is owned by the cache and freed by pcache_close.
typedef struct PageSlot {
    Page             page;          // the page itself, stored by value
    uint32_t         page_number;   // the key; set when the slot is created, before
                                    // the page is loaded and page.page_number is
    uint32_t         ref_count;     // how many callers hold it right now
    bool             loaded;        // whether the page has been read from disk
    struct PageSlot* next;          // the next slot in the same bucket, or NULL
} PageSlot;

// the pages asked for so far, in a hash table: each page number goes in bucket
// page_number % bucket_count, and a bucket is a linked list of the slots that
// landed there. Only pages that were asked for have slots. Nothing is evicted yet.
typedef struct PageCache {
    PageSlot** buckets;         // bucket_count list heads, NULL when empty
    uint32_t   bucket_count;
    uint32_t   slot_count;      // slots in the table
    uint32_t   max_page;        // pages in the file, for the range check
} PageCache;

// allocates an empty table for a file of page_count pages. On failure returns
// nonzero and leaves nothing to free.
int pcache_open(PageCache* cache, uint32_t page_count);

// returns page number's slot, creating an empty one (not loaded) the first
// time it is asked for. Returns NULL for page 0, a page number past the end,
// or a failed allocation. The slot stays at the same address until
// pcache_close.
PageSlot* pcache_fetch(PageCache* cache, uint32_t page_number);

// frees every loaded page and the slots, and zeroes the cache. Returns
// nonzero, naming each page on stderr, if any page still has refs. Safe to
// call on a zero-initialised or already-closed PageCache.
int pcache_close(PageCache* cache);

#endif // CODECRAFTERS_SQLITE_PCACHE_H

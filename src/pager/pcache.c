#include "pager/pcache.h"

#include <stdio.h>
#include <stdlib.h>

int pcache_open(PageCache* cache, uint32_t page_count) {
    // zeroed first, so a failed allocation leaves nothing to free without
    // depending on the caller having passed an empty cache
    *cache = (PageCache) { .buckets = calloc(PCACHE_BUCKET_COUNT, sizeof(PageSlot*)) };
    if (cache->buckets == NULL) return 1;
    cache->bucket_count = PCACHE_BUCKET_COUNT;
    cache->max_page = page_count;
    return 0;
}

// which bucket page number belongs in
static uint32_t bucket_index(const PageCache* cache, const uint32_t page_number) {
    return page_number % cache->bucket_count;
}

// the slot for page number, or NULL if it isn't in the table
static PageSlot* table_find(PageCache* cache, const uint32_t page_number) {
    // compares page_number, not page.page_number, which is only set once the
    // page is loaded
    PageSlot* slot = cache->buckets[bucket_index(cache, page_number)];
    while (slot != NULL && slot->page_number != page_number) {
        slot = slot->next;
    }
    return slot;
}

// a new empty slot for page number, added to the table. NULL if out of memory.
static PageSlot* table_insert(PageCache* cache, const uint32_t page_number) {
    PageSlot* slot = malloc(sizeof(PageSlot));
    if (slot == NULL) return NULL;
    
    const uint32_t index = bucket_index(cache, page_number);

    *slot = (PageSlot) { .page_number = page_number, 
                         .next = cache->buckets[index] };
    cache->buckets[index] = slot;
    cache->slot_count++;
    return slot;
}

PageSlot* pcache_fetch(PageCache* cache, const uint32_t page_number) {
    if (page_number == 0 || page_number > cache->max_page) return NULL;
    PageSlot* slot = table_find(cache, page_number);
    return (slot != NULL) ? slot : table_insert(cache, page_number);
}

// frees every slot in the list starting at head, and their pages. Returns
// nonzero, naming each page on stderr, if any still has refs.
static int free_bucket(PageSlot* head) {
    int leaked = 0;

    PageSlot* slot = head;
    while (slot != NULL) {
        if (slot->ref_count > 0) {
            fprintf(stderr, "page %u still has %u refs\n", slot->page_number, slot->ref_count);
            leaked = 1;
        }

        // saved first: the slot's memory is gone after free
        PageSlot* next = slot->next;
        // free(NULL) is a no-op, so slots that were never loaded need no check
        free(slot->page.data);
        free(slot);
        slot = next;
    }
    return leaked;
}

int pcache_close(PageCache* cache) {
    int leaked = 0;

    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        leaked |= free_bucket(cache->buckets[i]);
    }
    free(cache->buckets);
    *cache = (PageCache) { 0 };
    return leaked;
}

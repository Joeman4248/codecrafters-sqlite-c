#include "pager/pcache.h"

#include <stdio.h>
#include <stdlib.h>

int pcache_open(PageCache* cache, uint32_t page_count) {
    // zeroed first, so a failed allocation leaves nothing to free without
    // depending on the caller having passed an empty cache
    *cache = (PageCache) { 0 };

    // calloc, so every slot starts not loaded, with no refs and data NULL
    cache->slots = calloc(page_count, sizeof(PageSlot));
    if (cache->slots == NULL) return 1;

    cache->slot_count = page_count;
    return 0;
}

PageSlot* pcache_fetch(PageCache* cache, const uint32_t page_number) {
    if (page_number == 0 || page_number > cache->slot_count) return NULL;
    return &cache->slots[page_number - 1];
}

int pcache_close(PageCache* cache) {
    int leaked = 0;

    for (uint32_t i = 0; i < cache->slot_count; i++) {
        const PageSlot* slot = &cache->slots[i];
        if (slot->ref_count > 0) {
            fprintf(stderr, "page %u still has %u refs\n", i + 1, slot->ref_count);
            leaked = 1;
        }
        // free(NULL) is a no-op, so slots that were never loaded need no check
        free(slot->page.data);
    }

    free(cache->slots);
    *cache = (PageCache) { 0 };
    return leaked;
}

#include "pager/pager.h"

#include <stdio.h>
#include <stdlib.h>

// fills data, which holds page_size bytes, with page number of the file
static int read_page_bytes(const Pager* pager, const uint32_t page_number, uint8_t* data) {
    const uint32_t page_size = pager->page_size;

    if (fseek(pager->file, (long) (page_number - 1) * page_size, SEEK_SET)) {
        fprintf(stderr, "Error seeking to start of page\n");
        return 1;
    }

    return fread(data, 1, page_size, pager->file) != page_size;
}

// reads page number into newly allocated memory in *page. On failure returns
// nonzero and leaves page->data NULL, so the slot still looks unloaded.
static int read_page(const Pager* pager, const uint32_t page_number, Page* page) {
    // the whole page is read, reserved bytes included, but only the usable
    // part is ever looked at
    *page = (Page) { .data        = malloc(pager->page_size),
                     .size        = pager->usable_size,
                     .page_number = page_number };
    if (page->data == NULL || read_page_bytes(pager, page_number, page->data)) {
        free(page->data);
        page->data = NULL;
        return 1;
    }
    return 0;
}

int pager_open(Pager* pager, FILE* file, const FileHeader* header) {
    *pager = (Pager) { .file = file, 
                       .page_size = header->page_size, 
                       .usable_size = header->usable_size };
    return pcache_open(&pager->cache, header->db_size_in_pages);
}

// reads page number into slot unless it's already there
static int load_page(const Pager* pager, PageSlot* slot, const uint32_t page_number) {
    if (slot->loaded) return 0;

    if (read_page(pager, page_number, &slot->page)) return 1;
    slot->loaded = true;
    return 0;
}

Page* pager_get(Pager* pager, const uint32_t page_number) {
    PageSlot* slot = pcache_fetch(&pager->cache, page_number);
    if (slot == NULL || load_page(pager, slot, page_number)) return NULL;

    // pinned last, so a failed call never leaves a ref behind
    slot->ref_count++;

    return &slot->page;
}

void pager_unref(Pager* pager, Page* page) {
    PageSlot* slot = pcache_fetch(&pager->cache, page->page_number);
    if (slot == NULL || slot->ref_count == 0) {
        // a caller bug: an extra unref, or a page this pager never handed out
        fprintf(stderr, "pager_unref: page %u is not pinned\n", page->page_number);
        return;
    }
    slot->ref_count--;
}

int pager_close(Pager* pager) {
    const int result = pcache_close(&pager->cache);
    *pager = (Pager) { 0 };
    return result;
}

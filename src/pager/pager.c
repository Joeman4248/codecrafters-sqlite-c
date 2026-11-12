#include "pager/pager.h"

#include <stdio.h>
#include <stdlib.h>

#include "util/file.h"

// fills data, which holds page_size bytes, with page number of the file
static int read_page_bytes(const Pager* pager, const uint32_t page_number, uint8_t* data) {
    // page 1 starts at the beginning of the file; 64 bits, since a large
    // file's offsets don't fit in 32
    const uint64_t offset = (uint64_t) (page_number - 1) * pager->page_size;
    if (file_read_at(pager->file, offset, data, pager->page_size)) {
        fprintf(stderr, "Error reading page %u\n", page_number);
        return 1;
    }
    return 0;
}

// reads page number into a new heap Page, or one whose data is NULL, so the
// slot it lands in still looks unloaded
static Page read_page(const Pager* pager, const uint32_t page_number) {
    // the whole page is read, reserved bytes included, but only the usable
    // part is ever looked at
    Page page = (Page) { .data = malloc(pager->page_size),
                         .size = pager->usable_size,
                         .page_number = page_number };
    if (page.data == NULL || read_page_bytes(pager, page_number, page.data)) {
        free(page.data);
        return (Page) { 0 };
    }
    return page;
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

    slot->page = read_page(pager, page_number);
    if (slot->page.data == NULL) return 1;
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

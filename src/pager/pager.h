#ifndef CODECRAFTERS_SQLITE_PAGER_H
#define CODECRAFTERS_SQLITE_PAGER_H

#include <stdint.h>
#include <stdio.h>

#include "pager/file_header.h"
#include "pager/pcache.h"

// hands out the pages of one database file, reading each from disk the first
// time it is asked for and caching it after. The file is borrowed: whoever
// opened it closes it, after pager_close.
typedef struct Pager {
    FILE*     file;
    uint32_t  page_size;    // bytes per page in the file
    uint32_t  usable_size;  // page_size minus the reserved space at the end
    PageCache cache;
} Pager;

// sets up pager for file, taking the page size and count from header. Reads
// nothing yet. On failure returns nonzero and leaves nothing to close.
int pager_open(Pager* pager, FILE* file, const FileHeader* header);

// sets *page to page number, reading it if it isn't cached, and pins it: the
// page stays valid until the matching pager_unref. Every successful call needs
// exactly one pager_unref. Returns nonzero for page 0, a page number past the
// end or a failed read, leaving *page untouched.
int pager_get(Pager* pager, uint32_t page_number, Page** page);

// releases one pin taken by pager_get. The page stays cached; page must not
// be used afterwards. Unref-ing a page with no pins is reported on stderr.
void pager_unref(Pager* pager, Page* page);

// frees every cached page. Does not close the file. Returns nonzero if any
// page is still pinned (a missing pager_unref). Safe to call twice.
int pager_close(Pager* pager);

#endif // CODECRAFTERS_SQLITE_PAGER_H

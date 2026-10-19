#ifndef CODECRAFTERS_SQLITE_BTREE_SEARCH_H
#define CODECRAFTERS_SQLITE_BTREE_SEARCH_H

#include <stdbool.h>
#include <stdint.h>

#include "database.h"
#include "view.h"

// a heap-allocated list of rowids that grows as rowids are added
typedef struct {
    int64_t* items;
    uint32_t count;
} RowidList;

// frees the list's array. Safe to call on a zero-initialised RowidList.
void free_rowid_list(RowidList* list);

// finds the row with this rowid in the table b-tree rooted at rootpage,
// following the one path from the root that can hold it and reading each page
// into buf, which holds a page. Sets *found to whether the row exists, and if
// it does, points *payload at its record inside buf, valid until buf is next
// written. Returns nonzero on error.
int table_find_row(const Database* db, uint32_t rootpage, int64_t rowid,
        uint8_t* buf, ByteView* payload, bool* found);

// adds to *rowids, in index order, the rowid of every entry whose first value
// is the text value in the index b-tree rooted at rootpage. Each entry's
// record holds column_count values: the indexed columns, then the rowid. Only
// the pages that can hold such entries are read. Returns nonzero on error.
int index_find_rowids(const Database* db, uint32_t rootpage, uint32_t column_count,
        StringView value, RowidList* rowids);

#endif // CODECRAFTERS_SQLITE_BTREE_SEARCH_H

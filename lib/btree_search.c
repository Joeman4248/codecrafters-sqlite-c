#include <stdlib.h>

#include "include/lib-sqlite/btree_search.h"
#include "include/lib-sqlite/btree_cursor.h"
#include "include/lib-sqlite/btree_page.h"
#include "include/lib-sqlite/record.h"

void free_rowid_list(RowidList* list) {
    free(list->items);
    *list = (RowidList) { 0 };
}

// appends rowid to list, growing the array by one
static int rowid_list_add(RowidList* list, const int64_t rowid) {
    int64_t* grown = realloc(list->items, (list->count + 1) * sizeof(int64_t));
    if (grown == NULL) return 1;        // the old array is still valid, caller frees it
    list->items = grown;
    list->items[list->count++] = rowid;
    return 0;
}

// reads page_number into buf and decodes its b-tree header
static int load_page(const Database* db, const uint32_t page_number, uint8_t* buf,
        PageHeader* header) {
    // pages are numbered from 1; 0 means "no page"
    if (page_number == 0 || page_number > db->file_header.db_size_in_pages) return 1;
    if (read_page(db, page_number, buf)) return 1;
    return read_btree_header((ByteView) { buf, db->file_header.page_size }, page_number, header);
}

// sets *child to the child of an interior table page that holds rowid: the
// left child of the first cell whose key is >= rowid, or the rightmost child
// if every key is smaller. The keys are in order, so it's a binary search.
static int table_child_for(const ByteView page, const uint32_t page_number,
        const PageHeader* header, const int64_t rowid, uint32_t* child) {
    // the cell sought is in [low, high], where cell_count stands for the rightmost child
    uint16_t low = 0;
    uint16_t high = header->cell_count;
    while (low < high) {
        const uint16_t mid = low + (high - low) / 2;
        uint16_t offset;
        uint32_t left_child;
        int64_t key;
        if (read_cell_pointer(page, page_number, header, mid, &offset) ||
            read_interior_table_cell(page, offset, &left_child, &key)) {
            return 1;
        }
        if (key >= rowid) {
            high = mid;
        }
        else {
            low = mid + 1;
        }
    }

    if (low == header->cell_count) {
        *child = header->rightmost_ptr;
        return 0;
    }
    uint16_t offset;
    int64_t key;
    return read_cell_pointer(page, page_number, header, low, &offset) ||
           read_interior_table_cell(page, offset, child, &key);
}

// binary-searches a table leaf page, sorted by rowid, for the row with rowid
static int leaf_find_row(const ByteView page, const ByteView usable, const uint32_t page_number,
        const PageHeader* header, const int64_t rowid, ByteView* payload, bool* found) {
    uint16_t low = 0;
    uint16_t high = header->cell_count;
    while (low < high) {
        const uint16_t mid = low + (high - low) / 2;
        uint16_t offset;
        ByteView cell_payload;
        int64_t cell_rowid;
        if (read_cell_pointer(page, page_number, header, mid, &offset) ||
            read_leaf_table_cell(usable, offset, &cell_payload, &cell_rowid)) {
            return 1;
        }
        if (cell_rowid == rowid) {
            *payload = cell_payload;
            *found = true;
            return 0;
        }
        if (cell_rowid < rowid) {
            low = mid + 1;
        }
        else {
            high = mid;
        }
    }
    return 0;
}

int table_find_row(const Database* db, const uint32_t rootpage, const int64_t rowid,
        uint8_t* buf, ByteView* payload, bool* found) {
    const ByteView page = { buf, db->file_header.page_size };
    const ByteView usable = bv_prefix(page, db->file_header.usable_size);
    *found = false;

    // one page per level, each read over the last
    uint32_t page_number = rootpage;
    for (uint32_t depth = 0; depth < BTREE_MAX_DEPTH; depth++) {
        PageHeader header;
        if (load_page(db, page_number, buf, &header)) return 1;

        switch (header.page_type) {
            case PAGE_TYPE_LEAF_TABLE:
                return leaf_find_row(page, usable, page_number, &header, rowid, payload, found);
            case PAGE_TYPE_INTERIOR_TABLE:
                if (table_child_for(page, page_number, &header, rowid, &page_number)) return 1;
                break;
            default:
                return 1;
        }
    }
    return 1;   // deeper than any real tree: the pages must point back at each other
}

// what an index search looks for, shared by every page it visits
typedef struct {
    const Database* db;
    uint32_t        column_count;   // values in each entry's record, the rowid last
    StringView      value;
    RecordColumn*   storage;        // room for column_count entries, reused for every entry
    RowidList*      rowids;
} IndexSearch;

// orders the entry whose record is payload against the search value by its
// first value, the way SQLite sorts values: NULLs and numbers before any text,
// blobs after it, and text byte by byte. Also reads the entry's rowid.
static int compare_entry(const IndexSearch* search, const ByteView payload,
        int* order, int64_t* rowid) {
    Record record;
    if (parse_record(payload, search->storage, search->column_count, &record) ||
        record.count != search->column_count ||
        record_get_int(&record, record.count - 1, rowid)) {
        return 1;
    }

    if (record_is_text(&record, 0)) {
        StringView key;
        if (record_text_view(&record, 0, &key)) return 1;
        *order = sv_compare(key, search->value);
    }
    else {
        *order = record_is_blob(&record, 0) ? 1 : -1;
    }
    return 0;
}

static int search_index_subtree(const IndexSearch* search, uint32_t page_number, uint32_t depth);

// adds the matching entries of the subtree at page_number, read into buf.
// The page's entries are in order, so it skips those before the value along
// with the children to their left, and stops at the first one after it.
static int search_index_page(const IndexSearch* search, const uint32_t page_number,
        const uint32_t depth, uint8_t* buf) {
    const Database* db = search->db;
    const ByteView page = { buf, db->file_header.page_size };
    const ByteView usable = bv_prefix(page, db->file_header.usable_size);

    PageHeader header;
    if (load_page(db, page_number, buf, &header)) return 1;
    if (header.page_type != PAGE_TYPE_INTERIOR_INDEX && header.page_type != PAGE_TYPE_LEAF_INDEX) {
        return 1;
    }
    const bool is_leaf = header.page_type == PAGE_TYPE_LEAF_INDEX;

    for (uint16_t i = 0; i < header.cell_count; i++) {
        uint16_t offset;
        uint32_t left_child = 0;
        ByteView payload;
        if (read_cell_pointer(page, page_number, &header, i, &offset)) return 1;
        if (is_leaf ? read_leaf_index_cell(usable, offset, &payload)
                    : read_interior_index_cell(usable, offset, &left_child, &payload)) {
            return 1;
        }

        int order;
        int64_t rowid;
        if (compare_entry(search, payload, &order, &rowid)) return 1;

        // before the value, and so is everything in the child to its left
        if (order < 0) continue;

        // at or after the value: equal entries may still be in the child to its left.
        // Searched before this entry is added, so the rowids stay in index order.
        if (!is_leaf && search_index_subtree(search, left_child, depth + 1)) return 1;

        // after the value, and so is everything to its right
        if (order > 0) return 0;
        if (rowid_list_add(search->rowids, rowid)) return 1;
    }

    // every entry here was at or before the value, so equal ones may continue
    // in the rightmost child
    return is_leaf ? 0 : search_index_subtree(search, header.rightmost_ptr, depth + 1);
}

// searches the subtree at page_number in a page buffer of its own, since the
// parent's page is still being read
static int search_index_subtree(const IndexSearch* search, const uint32_t page_number,
        const uint32_t depth) {
    if (depth == BTREE_MAX_DEPTH) return 1;

    uint8_t* buf = malloc(search->db->file_header.page_size);
    if (buf == NULL) return 1;
    const int result = search_index_page(search, page_number, depth, buf);
    free(buf);
    return result;
}

int index_find_rowids(const Database* db, const uint32_t rootpage, const uint32_t column_count,
        const StringView value, RowidList* rowids) {
    // at least one indexed value, then the rowid
    if (column_count < 2) return 1;

    RecordColumn* storage = malloc(column_count * sizeof(RecordColumn));
    if (storage == NULL) return 1;

    const IndexSearch search = {
        .db = db,
        .column_count = column_count,
        .value = value,
        .storage = storage,
        .rowids = rowids,
    };
    const int result = search_index_subtree(&search, rootpage, 0);

    free(storage);
    return result;
}

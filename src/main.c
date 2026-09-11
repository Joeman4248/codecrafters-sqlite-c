#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define HEADER_STR_SZ 16
#define VALID_HEADER_STR "SQLite format 3"

#define FILE_HEADER_SZ 100

uint8_t header_string[HEADER_STR_SZ];

typedef enum {
    PAGE_TYPE_INTERIOR_TABLE,
    PAGE_TYPE_INTERIOR_INDEX,
    PAGE_TYPE_LEAF_TABLE,
    PAGE_TYPE_LEAF_INDEX,
} PageType;

typedef struct {
    PageType type;
    uint16_t freeblock_offset;
    uint16_t cell_count;
    uint16_t cell_area_start;
    uint8_t num_free_bytes;
} PageHeader;

typedef struct {
    uint16_t page_size;
} FileHeader;

int read_uint8(FILE* file, uint8_t* value) {
    uint8_t buffer[1];
    if (fread(buffer, 1, 1, file) != 1) return 1;
    *value = buffer[0];
    return 0;
}

int read_uint16(FILE* file, uint16_t* value) {
    uint8_t buffer[2];
    if (fread(buffer, 1, 2, file) != 2) return 1;
    *value = (buffer[0] << 8) | buffer[1];
    return 0;
}

int read_btree_header(FILE* file, PageHeader* header) {
    if (fseek(file, FILE_HEADER_SZ, SEEK_SET) != 0) return 1;

    uint8_t page_type_flag;
    if (read_uint8(file, &page_type_flag) != 0) {
        return 1;
    }

    switch (page_type_flag) {
        case 0x02: // means the page is an interior index b-tree page.
            header->type = PAGE_TYPE_INTERIOR_INDEX;
            break;
        case 0x05: // means the page is an interior table b-tree page.
            header->type = PAGE_TYPE_INTERIOR_TABLE;
            break;
        case 0x0a: // means the page is a leaf index b-tree page.
            header->type = PAGE_TYPE_LEAF_INDEX;
            break;
        case 0x0d: // means the page is a leaf table b-tree page.
            header->type = PAGE_TYPE_LEAF_TABLE;
            break;
        default:
            fprintf(stderr, "Invalid page header type flag\n");
            return 1;
    }

    if (read_uint16(file, &header->freeblock_offset) != 0) {
        return 1;
    }
    if (read_uint16(file, &header->cell_count) != 0) {
        return 1;
    }
    if (read_uint16(file, &header->cell_area_start) != 0) {
        return 1;
    }
    if (read_uint8(file, &header->num_free_bytes) != 0) {
        return 1;
    }
    
    return 0;
}

int read_db_header(FILE* file, FileHeader* header) {
    if (fseek(file, 0, SEEK_SET) != 0) return 1;
    
    if (fread(header_string, 1, HEADER_STR_SZ, file) != HEADER_STR_SZ) {
        fprintf(stderr, "Error seeking to the beginning of the file");
        return 1;
    }
    if (memcmp(header_string, VALID_HEADER_STR, HEADER_STR_SZ) != 0) {
        fprintf(stderr, "Invalid header string");
        return 1;
    }

    if (read_uint16(file, &header->page_size) != 0) {
        fprintf(stderr, "Error reading page size");
        return 1;
    }

    return 0;
}

int main(const int argc, const char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./your_program.sh <database path> <command>\n");
        return 1;
    }

    const char *database_file_path = argv[1];
    const char *command = argv[2];


    FILE *database_file = fopen(database_file_path, "rb");
    if (!database_file) {
        fprintf(stderr, "Failed to open the database file\n");
        return 1;
    }

    FileHeader header;
    PageHeader schema_page;

    if (read_db_header(database_file, &header) != 0) {
        fprintf(stderr, "Error reading database header\n");
        return 1;
    }
    
    if (read_btree_header(database_file, &schema_page) != 0) {
        fprintf(stderr, "Error reading schema page\n");
        return 1;
    }

    if (strcmp(command, ".dbinfo") == 0) {
        printf("database page size: %u\n", header.page_size);
        printf("number of tables: %u\n", schema_page.cell_count);
    }
    else {
        fprintf(stderr, "Unknown command %s\n", command);
        fclose(database_file);
        return 1;
    }

    fclose(database_file);
    return 0;
}
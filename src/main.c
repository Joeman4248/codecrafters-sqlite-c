#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define FILE_HEADER_SZ 100

#define HEADER_STR_SZ 16
#define VALID_HEADER_STR "SQLite format 3"
uint8_t header_string[HEADER_STR_SZ];

enum PageType {
    PAGE_TYPE_INTERIOR_TABLE = 0x02,
    PAGE_TYPE_INTERIOR_INDEX = 0x05,
    PAGE_TYPE_LEAF_TABLE = 0x0a,
    PAGE_TYPE_LEAF_INDEX = 0x0d,
};

typedef struct {
    uint8_t page_type;          // offset 0
    uint8_t num_free_bytes;     // offset 7

    uint16_t freeblock_offset;  // offset 1
    uint16_t cell_count;        // offset 3
    uint16_t cell_area_start;   // offset 5
    uint32_t rightmost_ptr;     // offset 8, only exists on interior pages

    uint8_t header_size;    // 8 bytes in size for leaf pages and 12 bytes for interior pages
} PageHeader;

typedef struct {
    uint16_t page_size;
} FileHeader;

int read_uint8(FILE* file, uint8_t* value) {
    if (fread(value, 1, 1, file) != 1) return 1;
    return 0;
}

int read_uint16(FILE* file, uint16_t* value) {
    uint8_t buffer[2];
    if (fread(buffer, 1, 2, file) != 2) return 1;
    *value = (buffer[0] << 8) | buffer[1];
    return 0;
}

int read_uint32(FILE* file, uint32_t* value) {
    uint8_t buffer[4];
    if (fread(buffer, 1, 4, file) != 4) return 1;
    *value = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    return 0;
}

int read_btree_header(FILE* file, PageHeader* header, 
        const uint16_t page_size, const uint32_t page_number) {
    const long offset = FILE_HEADER_SZ + (page_number - 1) * page_size;
    if (fseek(file, (long) offset, SEEK_SET) != 0) {
        return 1;
    }

    if (read_uint8(file, &header->page_type) != 0) {
        return 1;
    }

    switch (header->page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            header->header_size = 12;
            break;

        case PAGE_TYPE_LEAF_INDEX:
        case PAGE_TYPE_LEAF_TABLE:
            header->header_size = 8;
            break;

        default:
            fprintf(stderr, "Invalid page header type flag\n");
            return 1;
    }

    read_uint16(file, &header->freeblock_offset);
    read_uint16(file, &header->cell_count);
    read_uint16(file, &header->cell_area_start);
    read_uint8(file, &header->num_free_bytes);
    header->rightmost_ptr = 0;

    switch (header->page_type) {
        case PAGE_TYPE_INTERIOR_INDEX:
        case PAGE_TYPE_INTERIOR_TABLE:
            read_uint32(file, &header->rightmost_ptr);
    }

    return 0;
}

int read_file_header(FILE* file, FileHeader* header) {
    if (fseek(file, 0, SEEK_SET) != 0) {
        return 1;
    }
    
    if (fread(header_string, 1, HEADER_STR_SZ, file) != HEADER_STR_SZ) {
        return 1;
    }

    if (memcmp(header_string, VALID_HEADER_STR, HEADER_STR_SZ) != 0) {
        fprintf(stderr, "Invalid header string");
        return 1;
    }

    if (read_uint16(file, &header->page_size) != 0) {
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

    FileHeader file_header;
    PageHeader schema_page;

    if (read_file_header(database_file, &file_header) != 0) {
        fprintf(stderr, "Error reading database file header\n");
        return 1;
    }
    
    if (read_btree_header(database_file, &schema_page, file_header.page_size, 1) != 0) {
        fprintf(stderr, "Error reading schema page\n");
        return 1;
    }

    if (strcmp(command, ".dbinfo") == 0) {
        printf("database page size: %u\n", file_header.page_size);
        printf("number of tables: %u\n", schema_page.cell_count);
    }
    else if (strcmp(command, ".tables") == 0) {
        
    }
    else {
        fprintf(stderr, "Unknown command %s\n", command);
        fclose(database_file);
        return 1;
    }

    fclose(database_file);
    return 0;
}
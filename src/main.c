#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(const int argc, const char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./your_program.sh <database path> <command>\n");
        return 1;
    }

    const char *database_file_path = argv[1];
    const char *command = argv[2];

    if (strcmp(command, ".dbinfo") == 0) {
        FILE *database_file = fopen(database_file_path, "rb");
        if (!database_file) {
            fprintf(stderr, "Failed to open the database file\n");
            return 1;
        }

        fseek(database_file, 16, SEEK_SET);  // Skip the first 16 bytes of the header
        uint8_t buffer[2];
        fread(buffer, 1, 2, database_file);
        const uint16_t page_size = (buffer[0] << 8) | buffer[1];

        printf("database page size: %u\n", page_size);

        fclose(database_file);
    }
    else {
        fprintf(stderr, "Unknown command %s\n", command);
        return 1;
    }

    return 0;
}
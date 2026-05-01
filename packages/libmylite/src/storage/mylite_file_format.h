#ifndef MYLITE_STORAGE_MYLITE_FILE_FORMAT_H
#define MYLITE_STORAGE_MYLITE_FILE_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#define MYLITE_FILE_MAGIC_TEXT "MyLite format 1"

enum {
    MYLITE_FILE_MAGIC_SIZE = sizeof(MYLITE_FILE_MAGIC_TEXT),
    MYLITE_FILE_FORMAT_VERSION_OFFSET = 16,
    MYLITE_FILE_RESERVED_OFFSET = 18,
    MYLITE_FILE_FORMAT_VERSION = 1,
    MYLITE_FILE_PREAMBLE_SIZE = 4096,
    MYLITE_FILE_RESERVED_SIZE = MYLITE_FILE_PREAMBLE_SIZE - MYLITE_FILE_RESERVED_OFFSET,
    MYLITE_FILE_SQLITE_PAYLOAD_OFFSET = 4096,
    MYLITE_FILE_BITS_PER_BYTE = 8,
    MYLITE_FILE_U16_HIGH_BYTE_SHIFT = 8,
    MYLITE_FILE_BYTE_MASK = 0xff,
};

void mylite_file_preamble_init(unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]);
int mylite_file_preamble_validate(const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]);

uint16_t mylite_file_preamble_get_u16(const unsigned char *preamble, size_t offset);

#endif

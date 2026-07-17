#ifndef MYLITE_STORAGE_MYLITE_FILE_FORMAT_H
#define MYLITE_STORAGE_MYLITE_FILE_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#define MYLITE_FILE_MAGIC_TEXT "MyLite format 1"
#define MYLITE_FILE_FORMAT_VERSION_TEXT "3"

enum mylite_file_lifecycle_state {
    MYLITE_FILE_LIFECYCLE_INVALID = 0,
    MYLITE_FILE_LIFECYCLE_INITIALIZING = 1,
    MYLITE_FILE_LIFECYCLE_COMMITTED = 2,
    MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED = 3,
};

enum {
    MYLITE_FILE_MAGIC_SIZE = sizeof(MYLITE_FILE_MAGIC_TEXT),
    MYLITE_FILE_FORMAT_VERSION_OFFSET = 16,
    MYLITE_FILE_LIFECYCLE_STATE_OFFSET = 18,
    MYLITE_FILE_RESERVED_OFFSET = 19,
    MYLITE_FILE_LEGACY_FORMAT_VERSION = 1,
    MYLITE_FILE_LIFECYCLE_FORMAT_VERSION = 2,
    MYLITE_FILE_FORMAT_VERSION = 3,
    MYLITE_FILE_PREAMBLE_SIZE = 4096,
    MYLITE_FILE_RESERVED_SIZE = MYLITE_FILE_PREAMBLE_SIZE - MYLITE_FILE_RESERVED_OFFSET,
    MYLITE_FILE_SQLITE_PAYLOAD_OFFSET = 4096,
    MYLITE_FILE_SQLITE_MINIMUM_DATABASE_SIZE = 512,
    MYLITE_FILE_PHYSICAL_LOCK_BYTE = 1073741824,
    MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET =
        MYLITE_FILE_PHYSICAL_LOCK_BYTE - MYLITE_FILE_SQLITE_PAYLOAD_OFFSET,
    MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE = MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET,
    MYLITE_FILE_BITS_PER_BYTE = 8,
    MYLITE_FILE_U16_HIGH_BYTE_SHIFT = 8,
    MYLITE_FILE_BYTE_MASK = 0xff,
};

void mylite_file_preamble_init(unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]);
void mylite_file_preamble_init_with_state(
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE],
    enum mylite_file_lifecycle_state state
);
int mylite_file_preamble_validate(const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]);
enum mylite_file_lifecycle_state mylite_file_preamble_lifecycle_state(
    const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]
);
uint16_t mylite_file_preamble_format_version(
    const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]
);

uint16_t mylite_file_preamble_get_u16(const unsigned char *preamble, size_t offset);

#endif

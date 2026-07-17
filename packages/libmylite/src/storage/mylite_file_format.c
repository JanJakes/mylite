#include "mylite_file_format.h"

#include <string.h>

static void set_u16(unsigned char *preamble, size_t offset, uint16_t value);
static int reserved_bytes_are_zero(
    const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE],
    size_t offset
);

void mylite_file_preamble_init(unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]) {
    mylite_file_preamble_init_with_state(preamble, MYLITE_FILE_LIFECYCLE_COMMITTED);
}

void mylite_file_preamble_init_with_state(
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE],
    enum mylite_file_lifecycle_state state
) {
    memset(preamble, 0, MYLITE_FILE_PREAMBLE_SIZE);
    memcpy(preamble, MYLITE_FILE_MAGIC_TEXT, MYLITE_FILE_MAGIC_SIZE);
    set_u16(preamble, MYLITE_FILE_FORMAT_VERSION_OFFSET, MYLITE_FILE_FORMAT_VERSION);
    preamble[MYLITE_FILE_LIFECYCLE_STATE_OFFSET] = (unsigned char)state;
}

int mylite_file_preamble_validate(const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]) {
    return mylite_file_preamble_lifecycle_state(preamble) == MYLITE_FILE_LIFECYCLE_COMMITTED;
}

enum mylite_file_lifecycle_state mylite_file_preamble_lifecycle_state(
    const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]
) {
    uint16_t version = 0U;

    if (memcmp(preamble, MYLITE_FILE_MAGIC_TEXT, MYLITE_FILE_MAGIC_SIZE) != 0) {
        return MYLITE_FILE_LIFECYCLE_INVALID;
    }
    version = mylite_file_preamble_get_u16(preamble, MYLITE_FILE_FORMAT_VERSION_OFFSET);
    if (version == MYLITE_FILE_LEGACY_FORMAT_VERSION) {
        return reserved_bytes_are_zero(preamble, MYLITE_FILE_LIFECYCLE_STATE_OFFSET)
                   ? MYLITE_FILE_LIFECYCLE_COMMITTED
                   : MYLITE_FILE_LIFECYCLE_INVALID;
    }
    if ((version < MYLITE_FILE_LIFECYCLE_FORMAT_VERSION || version > MYLITE_FILE_FORMAT_VERSION) ||
        !reserved_bytes_are_zero(preamble, MYLITE_FILE_RESERVED_OFFSET)) {
        return MYLITE_FILE_LIFECYCLE_INVALID;
    }

    switch (preamble[MYLITE_FILE_LIFECYCLE_STATE_OFFSET]) {
    case MYLITE_FILE_LIFECYCLE_INITIALIZING:
        return MYLITE_FILE_LIFECYCLE_INITIALIZING;
    case MYLITE_FILE_LIFECYCLE_COMMITTED:
        return MYLITE_FILE_LIFECYCLE_COMMITTED;
    case MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED:
        return MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED;
    default:
        return MYLITE_FILE_LIFECYCLE_INVALID;
    }
}

uint16_t mylite_file_preamble_format_version(
    const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]
) {
    if (mylite_file_preamble_lifecycle_state(preamble) == MYLITE_FILE_LIFECYCLE_INVALID) {
        return 0U;
    }

    return mylite_file_preamble_get_u16(preamble, MYLITE_FILE_FORMAT_VERSION_OFFSET);
}

uint16_t mylite_file_preamble_get_u16(const unsigned char *preamble, size_t offset) {
    return (uint16_t)(((uint16_t)preamble[offset] << MYLITE_FILE_U16_HIGH_BYTE_SHIFT) |
                      (uint16_t)preamble[offset + 1U]);
}

static void set_u16(unsigned char *preamble, size_t offset, uint16_t value) {
    preamble[offset] =
        (unsigned char)((value >> MYLITE_FILE_U16_HIGH_BYTE_SHIFT) & MYLITE_FILE_BYTE_MASK);
    preamble[offset + 1U] = (unsigned char)(value & MYLITE_FILE_BYTE_MASK);
}

static int reserved_bytes_are_zero(
    const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE],
    size_t offset
) {
    static const unsigned char zeroes[MYLITE_FILE_PREAMBLE_SIZE];

    return memcmp(&preamble[offset], zeroes, MYLITE_FILE_PREAMBLE_SIZE - offset) == 0;
}

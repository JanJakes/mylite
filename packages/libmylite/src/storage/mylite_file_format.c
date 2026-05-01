#include "mylite_file_format.h"

#include <string.h>

static void set_u16(unsigned char *preamble, size_t offset, uint16_t value);
static int reserved_bytes_are_zero(const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]);

void mylite_file_preamble_init(unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE])
{
    memset(preamble, 0, MYLITE_FILE_PREAMBLE_SIZE);
    memcpy(preamble, MYLITE_FILE_MAGIC_TEXT, MYLITE_FILE_MAGIC_SIZE);
    set_u16(preamble, MYLITE_FILE_FORMAT_VERSION_OFFSET, MYLITE_FILE_FORMAT_VERSION);
}

int mylite_file_preamble_validate(const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE])
{
    if (memcmp(preamble, MYLITE_FILE_MAGIC_TEXT, MYLITE_FILE_MAGIC_SIZE) != 0) {
        return 0;
    }
    if (mylite_file_preamble_get_u16(preamble, MYLITE_FILE_FORMAT_VERSION_OFFSET) !=
        MYLITE_FILE_FORMAT_VERSION) {
        return 0;
    }
    if (!reserved_bytes_are_zero(preamble)) {
        return 0;
    }

    return 1;
}

uint16_t mylite_file_preamble_get_u16(const unsigned char *preamble, size_t offset)
{
    return (uint16_t)(((uint16_t)preamble[offset] << MYLITE_FILE_U16_HIGH_BYTE_SHIFT) |
                      (uint16_t)preamble[offset + 1U]);
}

static void set_u16(unsigned char *preamble, size_t offset, uint16_t value)
{
    preamble[offset] =
        (unsigned char)((value >> MYLITE_FILE_U16_HIGH_BYTE_SHIFT) & MYLITE_FILE_BYTE_MASK);
    preamble[offset + 1U] = (unsigned char)(value & MYLITE_FILE_BYTE_MASK);
}

static int reserved_bytes_are_zero(const unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE])
{
    static const unsigned char zeroes[MYLITE_FILE_RESERVED_SIZE];

    return memcmp(&preamble[MYLITE_FILE_RESERVED_OFFSET], zeroes, MYLITE_FILE_RESERVED_SIZE) == 0;
}

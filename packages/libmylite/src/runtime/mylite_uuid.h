#ifndef MYLITE_RUNTIME_MYLITE_UUID_H
#define MYLITE_RUNTIME_MYLITE_UUID_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    MYLITE_UUID_BINARY_SIZE = 16,
    MYLITE_UUID_TEXT_SIZE = 36,
};

bool mylite_uuid_string_is_valid(const void *input, size_t input_size);
int mylite_uuid_string_to_binary(
    const void *input,
    size_t input_size,
    bool swap_time_parts,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE],
    bool *out_valid
);
int mylite_uuid_binary_to_string(
    const void *input,
    size_t input_size,
    bool swap_time_parts,
    char out_text[MYLITE_UUID_TEXT_SIZE + 1U],
    bool *out_valid
);
int mylite_uuid_set_incorrect_string_error(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    const char *function_name
);
int mylite_sqlite_register_uuid_functions(sqlite3 *sqlite);

#endif

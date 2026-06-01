#ifndef MYLITE_RUNTIME_MYLITE_WEIGHT_STRING_H
#define MYLITE_RUNTIME_MYLITE_WEIGHT_STRING_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int mylite_weight_string_value(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    bool input_is_null,
    bool has_binary_length,
    int64_t binary_length,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_is_null
);
int mylite_sqlite_register_weight_string_functions(sqlite3 *sqlite);

#endif

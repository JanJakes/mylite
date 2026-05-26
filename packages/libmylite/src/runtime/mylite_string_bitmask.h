#ifndef MYLITE_RUNTIME_MYLITE_STRING_BITMASK_H
#define MYLITE_RUNTIME_MYLITE_STRING_BITMASK_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_string_bitmask_slice {
    const char *text;
    size_t length;
    bool is_null;
};

int mylite_string_export_set_value(
    uint64_t bits,
    bool bits_is_null,
    struct mylite_string_bitmask_slice on,
    struct mylite_string_bitmask_slice off,
    struct mylite_string_bitmask_slice separator,
    int64_t number_of_bits,
    bool number_of_bits_is_null,
    bool has_number_of_bits,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
int mylite_string_make_set_value(
    uint64_t bits,
    bool bits_is_null,
    const struct mylite_string_bitmask_slice *values,
    size_t value_count,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
int mylite_sqlite_register_string_bitmask_functions(sqlite3 *sqlite);

#endif

#ifndef MYLITE_RUNTIME_MYLITE_STRING_UNHEX_H
#define MYLITE_RUNTIME_MYLITE_STRING_UNHEX_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_string_unhex_decode(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
);
int mylite_string_unhex_append_incorrect_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
);
int mylite_string_unhex_format_warning_input(
    const void *input,
    size_t input_size,
    char *destination,
    size_t destination_size
);
int mylite_sqlite_register_string_unhex_function(sqlite3 *sqlite);

#endif

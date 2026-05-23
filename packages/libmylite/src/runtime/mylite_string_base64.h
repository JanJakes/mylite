#ifndef MYLITE_RUNTIME_MYLITE_STRING_BASE64_H
#define MYLITE_RUNTIME_MYLITE_STRING_BASE64_H

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_string_base64_encode(
    const void *input,
    size_t input_size,
    char **out_text,
    size_t *out_size
);
int mylite_string_base64_decode(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
);
int mylite_sqlite_register_string_base64_functions(sqlite3 *sqlite);

#endif

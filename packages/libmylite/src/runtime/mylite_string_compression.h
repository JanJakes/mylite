#ifndef MYLITE_RUNTIME_MYLITE_STRING_COMPRESSION_H
#define MYLITE_RUNTIME_MYLITE_STRING_COMPRESSION_H

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;

int mylite_string_compress(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size
);
int mylite_string_uncompress(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
);
int mylite_string_uncompressed_length(
    const void *input,
    size_t input_size,
    uint32_t *out_length,
    bool *out_valid
);
int mylite_string_compression_append_zlib_warning(struct mylite_db *database);
int mylite_sqlite_register_string_compression_functions(sqlite3 *sqlite);

#endif

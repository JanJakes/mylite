#ifndef MYLITE_RUNTIME_MYLITE_RANDOM_BYTES_H
#define MYLITE_RUNTIME_MYLITE_RANDOM_BYTES_H

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;

int mylite_random_bytes_generate(size_t length, unsigned char **out_bytes);
int mylite_random_bytes_length_from_int64(
    struct mylite_db *database,
    int64_t value,
    size_t *out_length
);
int mylite_random_bytes_length_from_double(
    struct mylite_db *database,
    double value,
    size_t *out_length
);
int mylite_random_bytes_length_from_text(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    size_t *out_length
);
void mylite_random_bytes_set_length_out_of_range_error(struct mylite_db *database);
int mylite_sqlite_register_random_bytes_function(sqlite3 *sqlite);

#endif

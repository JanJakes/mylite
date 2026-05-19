#ifndef MYLITE_RUNTIME_MYLITE_STRING_SUBSTRING_INDEX_H
#define MYLITE_RUNTIME_MYLITE_STRING_SUBSTRING_INDEX_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>

int mylite_string_substring_index_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *delimiter,
    size_t delimiter_length,
    int64_t count,
    char **out_text,
    size_t *out_text_length
);
int mylite_sqlite_register_string_substring_index_function(sqlite3 *sqlite);

#endif

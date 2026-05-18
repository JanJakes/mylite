#ifndef MYLITE_RUNTIME_MYLITE_STRING_REPLACE_H
#define MYLITE_RUNTIME_MYLITE_STRING_REPLACE_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>

int mylite_string_replace_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *search,
    size_t search_length,
    const char *replacement,
    size_t replacement_length,
    char **out_text,
    size_t *out_text_length
);
int mylite_sqlite_register_string_replace_function(sqlite3 *sqlite);

#endif

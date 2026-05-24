#ifndef MYLITE_RUNTIME_MYLITE_STRING_INSERT_H
#define MYLITE_RUNTIME_MYLITE_STRING_INSERT_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>

struct mylite_string_insert_slice {
    const char *text;
    size_t length;
};

struct mylite_string_insert_arguments {
    struct mylite_string_insert_slice value;
    int64_t position;
    int64_t length;
    struct mylite_string_insert_slice replacement;
};

int mylite_string_insert_value(
    struct mylite_db *database,
    const struct mylite_string_insert_arguments *arguments,
    char **out_text,
    size_t *out_text_length
);
int mylite_sqlite_register_string_insert_function(sqlite3 *sqlite);

#endif

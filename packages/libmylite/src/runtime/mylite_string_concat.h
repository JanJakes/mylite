#ifndef MYLITE_RUNTIME_MYLITE_STRING_CONCAT_H
#define MYLITE_RUNTIME_MYLITE_STRING_CONCAT_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_string_concat_argument {
    const char *text;
    size_t text_length;
    bool is_null;
};

int mylite_string_concat_ws_value(
    struct mylite_db *database,
    const struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    char **out_text
);
int mylite_sqlite_register_string_concat_functions(sqlite3 *sqlite);

#endif

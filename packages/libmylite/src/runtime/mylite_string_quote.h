#ifndef MYLITE_RUNTIME_MYLITE_STRING_QUOTE_H
#define MYLITE_RUNTIME_MYLITE_STRING_QUOTE_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_string_quote_sql_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_null,
    char **out_text,
    size_t *out_text_length
);
int mylite_sqlite_register_string_quote_function(sqlite3 *sqlite);

#endif

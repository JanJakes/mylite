#ifndef MYLITE_RUNTIME_MYLITE_STRING_SOUNDEX_H
#define MYLITE_RUNTIME_MYLITE_STRING_SOUNDEX_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>

int mylite_string_soundex_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
);
int mylite_sqlite_register_string_soundex_function(sqlite3 *sqlite);

#endif

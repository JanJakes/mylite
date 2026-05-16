#ifndef MYLITE_RUNTIME_MYLITE_STRING_TRIM_H
#define MYLITE_RUNTIME_MYLITE_STRING_TRIM_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>

enum mylite_string_trim_kind {
    MYLITE_STRING_TRIM_BOTH = 0,
    MYLITE_STRING_TRIM_LEADING = 1,
    MYLITE_STRING_TRIM_TRAILING = 2,
};

int mylite_string_trim_value(
    struct mylite_db *database,
    enum mylite_string_trim_kind trim_kind,
    const char *value,
    size_t value_length,
    const char *remove_string,
    size_t remove_string_length,
    char **out_text
);
int mylite_sqlite_register_string_trim_functions(sqlite3 *sqlite);

#endif

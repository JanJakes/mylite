#ifndef MYLITE_RUNTIME_MYLITE_STRING_CASE_H
#define MYLITE_RUNTIME_MYLITE_STRING_CASE_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_string_case_kind {
    MYLITE_STRING_CASE_LOWER = 0,
    MYLITE_STRING_CASE_UPPER = 1,
};

int mylite_string_case_ascii_value(
    struct mylite_db *database,
    enum mylite_string_case_kind case_kind,
    const char *value,
    size_t value_length,
    char **out_text
);
int mylite_sqlite_register_string_case_functions(sqlite3 *sqlite);

#endif

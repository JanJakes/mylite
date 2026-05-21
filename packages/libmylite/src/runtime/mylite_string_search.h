#ifndef MYLITE_RUNTIME_MYLITE_STRING_SEARCH_H
#define MYLITE_RUNTIME_MYLITE_STRING_SEARCH_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>

int mylite_string_search_locate_ascii_ci_value(
    struct mylite_db *database,
    const char *needle,
    size_t needle_length,
    const char *haystack,
    size_t haystack_length,
    int64_t position,
    int64_t *out_position
);
int mylite_string_search_find_in_set_ascii_ci_value(
    struct mylite_db *database,
    const char *search,
    size_t search_length,
    const char *list,
    size_t list_length,
    int64_t *out_position
);
int mylite_string_search_strcmp_ascii_ci_value(
    struct mylite_db *database,
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    int64_t *out_result
);
int mylite_sqlite_register_string_search_functions(sqlite3 *sqlite);

#endif

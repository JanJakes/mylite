#ifndef MYLITE_RUNTIME_MYLITE_SYSTEM_FUNCTIONS_H
#define MYLITE_RUNTIME_MYLITE_SYSTEM_FUNCTIONS_H

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;

int mylite_system_sleep_seconds(struct mylite_db *database, double seconds, int64_t *out_value);
int mylite_system_sleep_invalid_argument(struct mylite_db *database, int64_t *out_value);
int mylite_system_append_truncated_double_warning(
    struct mylite_db *database,
    const char *text,
    size_t text_size
);
int mylite_system_extract_value(
    struct mylite_db *database,
    const char *xml,
    size_t xml_size,
    const char *xpath,
    size_t xpath_size,
    char **out_text,
    size_t *out_text_size,
    bool *out_is_null
);
int mylite_system_update_xml(
    struct mylite_db *database,
    const char *xml,
    size_t xml_size,
    const char *xpath,
    size_t xpath_size,
    const char *replacement,
    size_t replacement_size,
    char **out_text,
    size_t *out_text_size,
    bool *out_is_null
);
int mylite_sqlite_register_system_functions(sqlite3 *sqlite);

#endif

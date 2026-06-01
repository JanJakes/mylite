#ifndef MYLITE_RUNTIME_MYLITE_CONVERT_TZ_H
#define MYLITE_RUNTIME_MYLITE_CONVERT_TZ_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_convert_tz_value(
    struct mylite_db *database,
    const char *datetime_text,
    size_t datetime_length,
    bool datetime_is_null,
    const char *from_tz_text,
    size_t from_tz_length,
    bool from_tz_is_null,
    const char *to_tz_text,
    size_t to_tz_length,
    bool to_tz_is_null,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_convert_tz_function(sqlite3 *sqlite);

#endif

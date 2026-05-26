#ifndef MYLITE_RUNTIME_MYLITE_TIMESTAMP_FUNCTION_H
#define MYLITE_RUNTIME_MYLITE_TIMESTAMP_FUNCTION_H

#include <mylite/mylite.h>

#include "mylite_date_interval_second.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_timestamp_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    const char *time_value,
    size_t time_value_length,
    bool time_value_is_null,
    bool has_time_value,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_timestamp_function(sqlite3 *sqlite);

#endif

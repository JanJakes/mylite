#ifndef MYLITE_RUNTIME_MYLITE_DATE_INTERVAL_SECOND_H
#define MYLITE_RUNTIME_MYLITE_DATE_INTERVAL_SECOND_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_date_interval_second_input_kind {
    MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING = 0,
    MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE = 1,
    MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME = 2,
    MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP = 3,
};

const char *mylite_date_interval_second_input_kind_name(
    enum mylite_date_interval_second_input_kind kind
);
bool mylite_date_interval_second_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_date_interval_second_input_kind *out_kind
);

int mylite_date_interval_second_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_seconds,
    bool interval_is_null,
    bool subtract,
    char **out_text,
    bool *out_is_null
);
int mylite_date_interval_second_value_with_overflow_message(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_seconds,
    bool interval_is_null,
    bool subtract,
    const char *overflow_message,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_date_interval_second_function(sqlite3 *sqlite);

#endif

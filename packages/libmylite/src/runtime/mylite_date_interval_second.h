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

enum mylite_date_interval_unit {
    MYLITE_DATE_INTERVAL_UNIT_YEAR = 0,
    MYLITE_DATE_INTERVAL_UNIT_QUARTER = 1,
    MYLITE_DATE_INTERVAL_UNIT_MONTH = 2,
    MYLITE_DATE_INTERVAL_UNIT_WEEK = 3,
    MYLITE_DATE_INTERVAL_UNIT_DAY = 4,
    MYLITE_DATE_INTERVAL_UNIT_HOUR = 5,
    MYLITE_DATE_INTERVAL_UNIT_MINUTE = 6,
    MYLITE_DATE_INTERVAL_UNIT_SECOND = 7,
    MYLITE_DATE_INTERVAL_UNIT_MICROSECOND = 8,
};

const char *mylite_date_interval_second_input_kind_name(
    enum mylite_date_interval_second_input_kind kind
);
bool mylite_date_interval_second_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_date_interval_second_input_kind *out_kind
);

const char *mylite_date_interval_unit_name(enum mylite_date_interval_unit unit);
bool mylite_date_interval_unit_from_name(
    const char *name,
    size_t name_length,
    enum mylite_date_interval_unit *out_unit
);
bool mylite_date_interval_unit_has_time_part(enum mylite_date_interval_unit unit);

int mylite_date_interval_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    char **out_text,
    bool *out_is_null
);
int mylite_date_interval_value_with_overflow_message(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    const char *overflow_message,
    char **out_text,
    bool *out_is_null
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

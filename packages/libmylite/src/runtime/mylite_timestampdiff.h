#ifndef MYLITE_RUNTIME_MYLITE_TIMESTAMPDIFF_H
#define MYLITE_RUNTIME_MYLITE_TIMESTAMPDIFF_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_timestampdiff_unit {
    MYLITE_TIMESTAMPDIFF_UNIT_YEAR = 0,
    MYLITE_TIMESTAMPDIFF_UNIT_QUARTER = 1,
    MYLITE_TIMESTAMPDIFF_UNIT_MONTH = 2,
    MYLITE_TIMESTAMPDIFF_UNIT_WEEK = 3,
    MYLITE_TIMESTAMPDIFF_UNIT_DAY = 4,
    MYLITE_TIMESTAMPDIFF_UNIT_HOUR = 5,
    MYLITE_TIMESTAMPDIFF_UNIT_MINUTE = 6,
    MYLITE_TIMESTAMPDIFF_UNIT_SECOND = 7,
    MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND = 8,
};

enum mylite_timestampdiff_input_kind {
    MYLITE_TIMESTAMPDIFF_INPUT_STRING = 0,
    MYLITE_TIMESTAMPDIFF_INPUT_DATE = 1,
    MYLITE_TIMESTAMPDIFF_INPUT_DATETIME = 2,
    MYLITE_TIMESTAMPDIFF_INPUT_TIMESTAMP = 3,
};

const char *mylite_timestampdiff_unit_name(enum mylite_timestampdiff_unit unit);
bool mylite_timestampdiff_unit_from_name(
    const char *name,
    size_t name_length,
    enum mylite_timestampdiff_unit *out_unit
);

const char *mylite_timestampdiff_input_kind_name(enum mylite_timestampdiff_input_kind kind);
bool mylite_timestampdiff_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_timestampdiff_input_kind *out_kind
);

int mylite_timestampdiff_value(
    struct mylite_db *database,
    enum mylite_timestampdiff_unit unit,
    const char *left_value,
    size_t left_value_length,
    enum mylite_timestampdiff_input_kind left_input_kind,
    bool left_is_null,
    const char *right_value,
    size_t right_value_length,
    enum mylite_timestampdiff_input_kind right_input_kind,
    bool right_is_null,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_timestampdiff_function(sqlite3 *sqlite);

#endif

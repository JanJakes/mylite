#ifndef MYLITE_RUNTIME_MYLITE_TEMPORAL_EXTRACT_H
#define MYLITE_RUNTIME_MYLITE_TEMPORAL_EXTRACT_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_temporal_extract_kind {
    MYLITE_TEMPORAL_EXTRACT_DATE = 0,
    MYLITE_TEMPORAL_EXTRACT_YEAR = 1,
    MYLITE_TEMPORAL_EXTRACT_MONTH = 2,
    MYLITE_TEMPORAL_EXTRACT_DAY = 3,
    MYLITE_TEMPORAL_EXTRACT_HOUR = 4,
    MYLITE_TEMPORAL_EXTRACT_MINUTE = 5,
    MYLITE_TEMPORAL_EXTRACT_SECOND = 6,
};

enum mylite_temporal_extract_input_kind {
    MYLITE_TEMPORAL_EXTRACT_INPUT_STRING = 0,
    MYLITE_TEMPORAL_EXTRACT_INPUT_DATE = 1,
    MYLITE_TEMPORAL_EXTRACT_INPUT_TIME = 2,
    MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME = 3,
    MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP = 4,
};

const char *mylite_temporal_extract_kind_name(enum mylite_temporal_extract_kind kind);
bool mylite_temporal_extract_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_temporal_extract_kind *out_kind
);
bool mylite_temporal_extract_kind_is_date_part(enum mylite_temporal_extract_kind kind);
bool mylite_temporal_extract_kind_is_time_part(enum mylite_temporal_extract_kind kind);

const char *mylite_temporal_extract_input_kind_name(enum mylite_temporal_extract_input_kind kind);
bool mylite_temporal_extract_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_temporal_extract_input_kind *out_kind
);

int mylite_temporal_extract_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_temporal_extract_kind extract_kind,
    enum mylite_temporal_extract_input_kind input_kind,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_temporal_extract_function(sqlite3 *sqlite);

#endif

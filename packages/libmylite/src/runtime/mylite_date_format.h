#ifndef MYLITE_RUNTIME_MYLITE_DATE_FORMAT_H
#define MYLITE_RUNTIME_MYLITE_DATE_FORMAT_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_date_format_input_kind {
    MYLITE_DATE_FORMAT_INPUT_STRING = 0,
    MYLITE_DATE_FORMAT_INPUT_DATE = 1,
    MYLITE_DATE_FORMAT_INPUT_DATETIME = 2,
    MYLITE_DATE_FORMAT_INPUT_TIMESTAMP = 3,
};

const char *mylite_date_format_input_kind_name(enum mylite_date_format_input_kind kind);
bool mylite_date_format_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_date_format_input_kind *out_kind
);

int mylite_date_format_validate_format(
    struct mylite_db *database,
    const char *format,
    size_t format_length
);
int mylite_date_format_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_format_input_kind input_kind,
    const char *format,
    size_t format_length,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_date_format_function(sqlite3 *sqlite);

#endif

#ifndef MYLITE_RUNTIME_MYLITE_UNIX_TIMESTAMP_H
#define MYLITE_RUNTIME_MYLITE_UNIX_TIMESTAMP_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_unix_timestamp_input_kind {
    MYLITE_UNIX_TIMESTAMP_INPUT_STRING = 0,
    MYLITE_UNIX_TIMESTAMP_INPUT_DATE = 1,
    MYLITE_UNIX_TIMESTAMP_INPUT_DATETIME = 2,
    MYLITE_UNIX_TIMESTAMP_INPUT_TIMESTAMP = 3,
};

const char *mylite_unix_timestamp_input_kind_name(enum mylite_unix_timestamp_input_kind kind);
bool mylite_unix_timestamp_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_unix_timestamp_input_kind *out_kind
);

int mylite_unix_timestamp_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_unix_timestamp_input_kind input_kind,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_unix_timestamp_function(sqlite3 *sqlite);

#endif

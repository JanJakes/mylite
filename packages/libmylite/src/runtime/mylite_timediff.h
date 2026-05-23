#ifndef MYLITE_RUNTIME_MYLITE_TIMEDIFF_H
#define MYLITE_RUNTIME_MYLITE_TIMEDIFF_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_timediff_input_kind {
    MYLITE_TIMEDIFF_INPUT_STRING = 0,
    MYLITE_TIMEDIFF_INPUT_DATE = 1,
    MYLITE_TIMEDIFF_INPUT_TIME = 2,
    MYLITE_TIMEDIFF_INPUT_DATETIME = 3,
    MYLITE_TIMEDIFF_INPUT_TIMESTAMP = 4,
};

const char *mylite_timediff_input_kind_name(enum mylite_timediff_input_kind kind);
bool mylite_timediff_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_timediff_input_kind *out_kind
);

int mylite_timediff_value(
    struct mylite_db *database,
    const char *left_value,
    size_t left_value_length,
    enum mylite_timediff_input_kind left_input_kind,
    bool left_is_null,
    const char *right_value,
    size_t right_value_length,
    enum mylite_timediff_input_kind right_input_kind,
    bool right_is_null,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_timediff_function(sqlite3 *sqlite);

#endif

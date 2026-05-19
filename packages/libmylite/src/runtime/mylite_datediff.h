#ifndef MYLITE_RUNTIME_MYLITE_DATEDIFF_H
#define MYLITE_RUNTIME_MYLITE_DATEDIFF_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_datediff_input_kind {
    MYLITE_DATEDIFF_INPUT_STRING = 0,
    MYLITE_DATEDIFF_INPUT_DATE = 1,
    MYLITE_DATEDIFF_INPUT_DATETIME = 2,
    MYLITE_DATEDIFF_INPUT_TIMESTAMP = 3,
};

const char *mylite_datediff_input_kind_name(enum mylite_datediff_input_kind kind);
bool mylite_datediff_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_datediff_input_kind *out_kind
);

int mylite_datediff_value(
    struct mylite_db *database,
    const char *left_value,
    size_t left_value_length,
    enum mylite_datediff_input_kind left_input_kind,
    bool left_is_null,
    const char *right_value,
    size_t right_value_length,
    enum mylite_datediff_input_kind right_input_kind,
    bool right_is_null,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_datediff_function(sqlite3 *sqlite);

#endif

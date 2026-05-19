#ifndef MYLITE_RUNTIME_MYLITE_STRING_PADDING_H
#define MYLITE_RUNTIME_MYLITE_STRING_PADDING_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_string_padding_side {
    MYLITE_STRING_PADDING_LEFT = 0,
    MYLITE_STRING_PADDING_RIGHT = 1,
};

struct mylite_string_padding_slice {
    const char *text;
    size_t length;
};

int mylite_string_pad_value(
    struct mylite_db *database,
    enum mylite_string_padding_side side,
    struct mylite_string_padding_slice value,
    int64_t target_length,
    struct mylite_string_padding_slice pad,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
int mylite_string_repeat_value(
    struct mylite_db *database,
    struct mylite_string_padding_slice value,
    int64_t count,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
int mylite_string_space_value(
    int64_t count,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
int mylite_sqlite_register_string_padding_functions(sqlite3 *sqlite);

#endif

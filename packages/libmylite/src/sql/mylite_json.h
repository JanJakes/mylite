#ifndef MYLITE_SQL_MYLITE_JSON_H
#define MYLITE_SQL_MYLITE_JSON_H

#include <stdbool.h>
#include <stddef.h>

enum mylite_json_type {
    MYLITE_JSON_TYPE_INVALID = 0,
    MYLITE_JSON_TYPE_NULL = 1,
    MYLITE_JSON_TYPE_BOOLEAN = 2,
    MYLITE_JSON_TYPE_INTEGER = 3,
    MYLITE_JSON_TYPE_DOUBLE = 4,
    MYLITE_JSON_TYPE_STRING = 5,
    MYLITE_JSON_TYPE_ARRAY = 6,
    MYLITE_JSON_TYPE_OBJECT = 7,
};

struct mylite_json_error {
    const char *message;
    size_t position;
};

bool mylite_json_validate(const char *text, size_t length, enum mylite_json_type *out_type,
                          struct mylite_json_error *out_error);
const char *mylite_json_type_name(enum mylite_json_type type);
int mylite_json_quote_string(const char *text, size_t length, char **out_text, size_t *out_length);
int mylite_json_unquote_string(const char *text, size_t length, char **out_text, size_t *out_length,
                               struct mylite_json_error *out_error);

#endif

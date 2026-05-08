#ifndef MYLITE_SQL_MYLITE_JSON_H
#define MYLITE_SQL_MYLITE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

enum mylite_json_status {
    MYLITE_JSON_STATUS_OK = 0,
    MYLITE_JSON_STATUS_NOMEM = -1,
    MYLITE_JSON_STATUS_INVALID_DOCUMENT = 1,
    MYLITE_JSON_STATUS_INVALID_PATH = 2,
    MYLITE_JSON_STATUS_PATH_WILDCARD_NOT_ALLOWED = 3,
};

bool mylite_json_validate(
    const char *text,
    size_t length,
    enum mylite_json_type *out_type,
    struct mylite_json_error *out_error
);
int mylite_json_normalize(
    const char *text,
    size_t length,
    char **out_text,
    size_t *out_length,
    struct mylite_json_error *out_error
);
const char *mylite_json_type_name(enum mylite_json_type type);
int mylite_json_quote_string(const char *text, size_t length, char **out_text, size_t *out_length);
int mylite_json_unquote_string(
    const char *text,
    size_t length,
    char **out_text,
    size_t *out_length,
    struct mylite_json_error *out_error
);
int mylite_json_extract(
    const char *document,
    size_t document_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    char **out_json,
    size_t *out_json_length,
    bool *out_found,
    struct mylite_json_error *out_error
);
int mylite_json_contains_path(
    const char *document,
    size_t document_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    bool require_all,
    bool *out_contains,
    struct mylite_json_error *out_error
);
int mylite_json_keys(
    const char *document,
    size_t document_length,
    const char *path,
    size_t path_length,
    bool has_path,
    char **out_json,
    size_t *out_json_length,
    bool *out_found,
    struct mylite_json_error *out_error
);
int mylite_json_length(
    const char *document,
    size_t document_length,
    const char *path,
    size_t path_length,
    bool has_path,
    uint64_t *out_length,
    bool *out_found,
    struct mylite_json_error *out_error
);

#endif

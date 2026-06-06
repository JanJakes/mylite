#ifndef MYLITE_RUNTIME_MYLITE_JSON_H
#define MYLITE_RUNTIME_MYLITE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_json_normalize_status {
    MYLITE_JSON_NORMALIZE_OK = 0,
    MYLITE_JSON_NORMALIZE_INVALID = 1,
    MYLITE_JSON_NORMALIZE_UNSUPPORTED = 2,
    MYLITE_JSON_NORMALIZE_INVALID_PATH = 3,
    MYLITE_JSON_NORMALIZE_PATH_NOT_ALLOWED = 4,
};

enum mylite_json_error_detail {
    MYLITE_JSON_ERROR_INVALID_VALUE = 0,
    MYLITE_JSON_ERROR_MISSING_OBJECT_MEMBER_NAME = 1,
};

struct mylite_json_normalize_result {
    enum mylite_json_normalize_status status;
    size_t position;
    enum mylite_json_error_detail error_detail;
};

const char *mylite_json_invalid_text_error_message(const struct mylite_json_normalize_result *result
);

enum mylite_json_sql_value_kind {
    MYLITE_JSON_SQL_VALUE_NULL = 0,
    MYLITE_JSON_SQL_VALUE_INTEGER = 1,
    MYLITE_JSON_SQL_VALUE_BOOLEAN = 2,
    MYLITE_JSON_SQL_VALUE_STRING = 3,
    MYLITE_JSON_SQL_VALUE_JSON = 4,
};

struct mylite_json_sql_value {
    enum mylite_json_sql_value_kind kind;
    const char *text;
    size_t text_length;
    int64_t integer;
    bool boolean;
};

int mylite_json_normalize(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_validate(const char *text, size_t text_length, bool *out_is_valid);
int mylite_json_type(
    const char *text,
    size_t text_length,
    const char **out_type,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_length(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    bool has_path,
    int64_t *out_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_keys(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    bool has_path,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_extract(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_value(
    const char *text,
    size_t text_length,
    const char *path,
    size_t path_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_contains(
    const char *target,
    size_t target_length,
    const char *candidate,
    size_t candidate_length,
    const char *path,
    size_t path_length,
    bool has_path,
    int64_t *out_contains,
    bool *out_is_null,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_contains_path(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    bool require_all,
    int64_t *out_contains,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_set(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_replace(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_insert(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_remove(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_mutation_validate_before_null(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_remove_validate_before_null(
    const char *text,
    size_t text_length,
    const char *const *paths,
    const size_t *path_lengths,
    size_t path_count,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_path_validate(
    const char *path,
    size_t path_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_unquote(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_quote_string(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
);
int mylite_json_array_from_sql_values(
    const struct mylite_json_sql_value *values,
    size_t value_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_object_from_sql_values(
    const struct mylite_json_sql_value *keys,
    const struct mylite_json_sql_value *values,
    size_t pair_count,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);

#endif

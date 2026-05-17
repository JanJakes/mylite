#ifndef MYLITE_RUNTIME_MYLITE_JSON_H
#define MYLITE_RUNTIME_MYLITE_JSON_H

#include <stdbool.h>
#include <stddef.h>

enum mylite_json_normalize_status {
    MYLITE_JSON_NORMALIZE_OK = 0,
    MYLITE_JSON_NORMALIZE_INVALID = 1,
    MYLITE_JSON_NORMALIZE_UNSUPPORTED = 2,
};

struct mylite_json_normalize_result {
    enum mylite_json_normalize_status status;
    size_t position;
};

int mylite_json_normalize(
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length,
    struct mylite_json_normalize_result *out_result
);
int mylite_json_validate(const char *text, size_t text_length, bool *out_is_valid);

#endif

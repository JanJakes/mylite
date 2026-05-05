#ifndef MYLITE_SQL_MYLITE_REGEXP_H
#define MYLITE_SQL_MYLITE_REGEXP_H

#include <stdbool.h>
#include <stddef.h>

enum {
    MYLITE_REGEXP_OK = 0,
    MYLITE_REGEXP_NOMEM = -1,
    MYLITE_REGEXP_PATTERN_ERROR = 1,
};

struct mylite_regexp_options {
    bool case_sensitive;
    bool multiline;
    bool dot_matches_newline;
};

struct mylite_regexp_error {
    unsigned int code;
    const char *message;
};

int mylite_regexp_match(const char *value, size_t value_length, const char *pattern,
                        size_t pattern_length, struct mylite_regexp_options options,
                        bool *out_match, struct mylite_regexp_error *out_error);

#endif

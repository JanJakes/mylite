#ifndef MYLITE_RUNTIME_MYLITE_REGEXP_H
#define MYLITE_RUNTIME_MYLITE_REGEXP_H

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_regexp_compile_status {
    MYLITE_REGEXP_COMPILE_OK = 0,
    MYLITE_REGEXP_COMPILE_NOMEM = 1,
    MYLITE_REGEXP_COMPILE_UNSUPPORTED = 2,
    MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET = 3,
    MYLITE_REGEXP_COMPILE_INVALID_RANGE = 4,
    MYLITE_REGEXP_COMPILE_DANGLING_ESCAPE = 5,
    MYLITE_REGEXP_COMPILE_TOO_LARGE = 6,
};

enum mylite_regexp_match_status {
    MYLITE_REGEXP_MATCH_OK = 0,
    MYLITE_REGEXP_MATCH_NOMEM = 1,
    MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE = 2,
    MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE = 3,
};

struct mylite_regexp_program;

int mylite_sqlite_register_regexp_functions(sqlite3 *sqlite);
enum mylite_regexp_compile_status mylite_regexp_compile_ascii_ci(
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_program **out_program
);
enum mylite_regexp_compile_status mylite_regexp_compile_ascii_cs(
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_program **out_program
);
void mylite_regexp_program_free(void *program);
enum mylite_regexp_match_status mylite_regexp_program_match_ascii_ci(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    bool *out_matches
);
enum mylite_regexp_match_status mylite_regexp_program_match_ascii_cs(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    bool *out_matches
);

#endif

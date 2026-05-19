#ifndef MYLITE_RUNTIME_MYLITE_STRING_CODEPOINT_H
#define MYLITE_RUNTIME_MYLITE_STRING_CODEPOINT_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_string_codepoint_kind {
    MYLITE_STRING_CODEPOINT_ASCII = 0,
    MYLITE_STRING_CODEPOINT_ORD = 1,
};

int mylite_string_codepoint_value(
    enum mylite_string_codepoint_kind kind,
    const unsigned char *value,
    size_t value_length,
    bool is_binary,
    uint64_t *out_codepoint
);
int mylite_sqlite_register_string_codepoint_functions(sqlite3 *sqlite);

#endif

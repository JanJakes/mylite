#ifndef MYLITE_RUNTIME_MYLITE_STRCMP_COMPARE_H
#define MYLITE_RUNTIME_MYLITE_STRCMP_COMPARE_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_charset_collation_info;

struct mylite_strcmp_compare_options {
    bool ignore_trailing_spaces;
    bool case_sensitive;
};

int mylite_strcmp_compare_texts(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    struct mylite_strcmp_compare_options options
);
struct mylite_strcmp_compare_options mylite_strcmp_compare_options_for_collation(
    const struct mylite_charset_collation_info *info
);

#endif

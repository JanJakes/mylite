#ifndef MYLITE_RUNTIME_MYLITE_SELECT_COMPARE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_COMPARE_H

#include <stddef.h>

struct mylite_expression_value;

int mylite_select_compare_values(const struct mylite_expression_value *left,
                                 const struct mylite_expression_value *right);
int mylite_select_compare_binary_text_values(const char *left, size_t left_length,
                                             const char *right, size_t right_length);

#endif

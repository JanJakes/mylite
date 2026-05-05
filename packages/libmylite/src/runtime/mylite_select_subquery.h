#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_select_types.h"

#include <stddef.h>

int mylite_select_subquery_copy_row_values(mylite_stmt *stmt, size_t width,
                                           struct mylite_row_expression_values *out_values);
int mylite_select_subquery_copy_row_value(mylite_stmt *stmt, size_t index,
                                          struct mylite_expression_value *out_value);
int mylite_select_subquery_copy_column_value(mylite_stmt *stmt,
                                             struct mylite_expression_value *out_value);
int mylite_select_subquery_append_warnings(struct mylite_expression_warnings *destination,
                                           const struct mylite_expression_warnings *source);
void mylite_select_subquery_row_values_deinit(struct mylite_row_expression_values *values);

#endif

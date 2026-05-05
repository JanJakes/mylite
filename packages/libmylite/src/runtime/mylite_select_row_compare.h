#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ROW_COMPARE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ROW_COMPARE_H

#include "mylite_select_types.h"

bool mylite_select_row_expression_values_has_null(
    const struct mylite_row_expression_values *values);
int mylite_select_compare_row_values(enum mylite_sql_ast_operator operator_kind,
                                     const struct mylite_row_expression_values *left,
                                     const struct mylite_row_expression_values *right,
                                     struct mylite_expression_warnings *warnings, int *out_truth);

#endif

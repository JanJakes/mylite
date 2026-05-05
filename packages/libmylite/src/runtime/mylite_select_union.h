#ifndef MYLITE_RUNTIME_MYLITE_SELECT_UNION_H
#define MYLITE_RUNTIME_MYLITE_SELECT_UNION_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_select_eval.h"
#include "mylite_select_types.h"

#include <stddef.h>

struct mylite_select_union_callbacks {
    const struct mylite_select_eval_callbacks *select_eval_callbacks;
    int (*execute_scalar_select)(mylite_stmt *stmt);
    int (*execute_table_select)(mylite_stmt *stmt);
    int (*copy_operand_row_value)(mylite_stmt *stmt, size_t index,
                                  struct mylite_expression_value *out_value);
    int (*append_warnings)(struct mylite_expression_warnings *destination,
                           const struct mylite_expression_warnings *source);
    int (*set_unsupported_order_error)(mylite_db *database);
};

int mylite_select_union_execute_query(mylite_stmt *stmt,
                                      const struct mylite_select_union_callbacks *callbacks);

#endif

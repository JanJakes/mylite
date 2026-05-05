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

struct mylite_select_union_prepare_callbacks {
    int (*prepare_select_subquery)(mylite_db *database, const struct mylite_sql_ast_node *statement,
                                   mylite_stmt **out_stmt);
    int (*clone_order_expressions)(mylite_stmt *stmt, const char *sql, size_t sql_length);
    int (*set_ambiguous_order_column_error)(mylite_db *database, const char *column_name);
    int (*set_unsupported_order_error)(mylite_db *database);
};

int mylite_select_union_prepare_query_expression(
    mylite_db *database, const struct mylite_sql_ast_node *statement, const char *sql,
    size_t sql_length, mylite_stmt **out_stmt,
    const struct mylite_select_union_prepare_callbacks *callbacks);
int mylite_select_union_bind_global_order_by_clause(
    mylite_db *database, const struct mylite_sql_ast_node *order_by_clause,
    struct mylite_select_plan *plan, const struct mylite_select_union_prepare_callbacks *callbacks);
int mylite_select_union_execute_query(mylite_stmt *stmt,
                                      const struct mylite_select_union_callbacks *callbacks);

#endif

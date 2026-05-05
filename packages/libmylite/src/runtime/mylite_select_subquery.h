#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

bool mylite_select_subquery_row_expression_is_supported(
    const struct mylite_sql_ast_node *expression);
bool mylite_select_subquery_row_expression_is_membership(
    const struct mylite_sql_ast_node *expression);
bool mylite_select_subquery_row_expression_is_positive_membership(
    const struct mylite_sql_ast_node *expression);
bool mylite_select_subquery_binary_expression_is_row(const struct mylite_sql_ast_node *expression);
bool mylite_select_subquery_binary_expression_is_row_in(
    const struct mylite_sql_ast_node *expression);
bool mylite_select_subquery_binary_expression_is_row_scalar(
    const struct mylite_sql_ast_node *expression);
const struct mylite_sql_ast_node *
mylite_select_subquery_row_select_statement(const struct mylite_sql_ast_node *expression);
bool mylite_select_subquery_quantified_comparison_has_row_left(
    const struct mylite_sql_ast_node *expression);
bool mylite_select_subquery_quantified_comparison_is_row_alias(
    const struct mylite_sql_ast_node *expression);
size_t mylite_select_subquery_row_constructor_width(const struct mylite_sql_ast_node *row);
bool mylite_select_subquery_binary_expression_is_in(const struct mylite_sql_ast_node *expression);

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

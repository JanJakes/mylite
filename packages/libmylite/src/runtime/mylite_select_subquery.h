#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_expression_eval_context;
struct mylite_select_eval_callbacks;

struct mylite_select_subquery_eval_callbacks {
    int (*prepare_select_subquery)(mylite_db *database, const struct mylite_sql_ast_node *statement,
                                   mylite_stmt **out_stmt);
    const struct mylite_select_eval_callbacks *table_select_eval_callbacks;
};

struct mylite_select_subquery_bind_callbacks {
    int (*prepare_select_subquery)(mylite_db *database, const struct mylite_sql_ast_node *statement,
                                   mylite_stmt **out_stmt);
    int (*set_unsupported_where_error)(mylite_db *database);
};

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
bool mylite_select_subquery_quantified_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind);
size_t mylite_select_subquery_row_constructor_width(const struct mylite_sql_ast_node *row);
bool mylite_select_subquery_binary_expression_is_in(const struct mylite_sql_ast_node *expression);

int mylite_select_subquery_bind_select_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression, bool scalar_context,
    const struct mylite_select_subquery_bind_callbacks *callbacks);
int mylite_select_subquery_bind_in_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks);
int mylite_select_subquery_bind_row_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks);
int mylite_select_subquery_bind_quantified_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks);
int mylite_select_subquery_validate_scalar_select_list(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement);
int mylite_select_subquery_validate_in_select(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement);
int mylite_select_subquery_validate_in_prepared_columns(mylite_db *database,
                                                        const mylite_stmt *stmt);
int mylite_select_subquery_validate_row_select_columns(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement,
                                                       size_t expected_width);
int mylite_select_subquery_validate_row_prepared_columns(mylite_db *database,
                                                         const mylite_stmt *stmt,
                                                         size_t expected_width);
int mylite_select_subquery_set_operand_columns_error(mylite_db *database);
int mylite_select_subquery_set_in_limit_error(mylite_db *database);
int mylite_select_subquery_set_row_quantified_non_alias_error(
    mylite_db *database, const struct mylite_sql_ast_node *expression);
int mylite_select_subquery_set_scalar_cardinality_error(mylite_db *database);

int mylite_select_subquery_eval(mylite_stmt *stmt, const struct mylite_sql_ast_node *subquery,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value,
                                const struct mylite_select_subquery_eval_callbacks *callbacks);
int mylite_select_subquery_eval_in(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                   const struct mylite_expression_value *left,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value,
                                   const struct mylite_select_subquery_eval_callbacks *callbacks);
int mylite_select_subquery_eval_quantified(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks);
int mylite_select_subquery_eval_row(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                    const struct mylite_expression_eval_context *expression_context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_subquery_eval_callbacks *callbacks);

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

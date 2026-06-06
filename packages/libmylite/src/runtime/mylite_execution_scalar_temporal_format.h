#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_TEMPORAL_FORMAT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_TEMPORAL_FORMAT_H

#include "mylite_ast.h"
#include "mylite_date_interval_second.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_sql_ast_node;
struct session_scalar_cell;

int mylite_execution_scalar_date_interval_second_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_addtime_subtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_date_format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_get_format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_time_format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_str_to_date_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_date_format_numeric_comparison_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
bool mylite_execution_scalar_is_date_format_numeric_comparison_expression(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_date_format_numeric_comparison_sides(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_date_format,
    const struct mylite_sql_ast_node **out_numeric,
    enum mylite_sql_ast_operator *out_operator_kind
);
bool mylite_execution_date_format_numeric_comparison_format_is_supported(
    const char *format,
    size_t format_length
);
bool mylite_execution_is_date_interval_second_function_kind(enum mylite_sql_ast_node_kind kind);
bool mylite_execution_is_time_arithmetic_function_kind(enum mylite_sql_ast_node_kind kind);
bool mylite_execution_str_to_date_child_is_null_literal(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_str_to_date_child_is_identifier_reference(
    const struct mylite_sql_ast_node *expression
);
const char *mylite_execution_date_interval_second_function_name(enum mylite_sql_ast_node_kind kind);
bool mylite_execution_date_interval_second_function_subtracts(enum mylite_sql_ast_node_kind kind);
int mylite_execution_validate_date_interval_second_function_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
int mylite_execution_date_interval_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit_node,
    const char *function_name,
    enum mylite_date_interval_unit *out_unit
);
const struct mylite_sql_ast_node *mylite_execution_date_interval_unit_node(
    const struct mylite_sql_ast_node *expression
);
const struct mylite_sql_ast_node *mylite_execution_date_interval_second_temporal_node(
    const struct mylite_sql_ast_node *expression
);
const struct mylite_sql_ast_node *mylite_execution_date_interval_second_interval_node(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_date_interval_second_message(
    char *buffer,
    size_t buffer_size,
    const char *function_name,
    const char *suffix
);
int mylite_execution_date_interval_second_interval_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    enum mylite_date_interval_unit unit,
    int64_t *out_interval,
    bool *out_is_null
);

#endif

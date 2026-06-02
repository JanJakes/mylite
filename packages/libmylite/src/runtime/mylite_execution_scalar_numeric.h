#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_NUMERIC_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_NUMERIC_H

#include <stddef.h>

struct mylite_db;
struct mylite_sql_ast_node;
struct session_scalar_cell;

int mylite_execution_scalar_division_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_bitwise_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_abs_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_sign_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_rounding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_sqrt_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_angle_conversion_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_inverse_trig_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_direct_trig_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_atan_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_exp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_logarithm_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_power_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_truncate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_format_double_text(
    struct mylite_db *database,
    double value,
    const char *function_name,
    char *buffer,
    size_t buffer_size
);
int mylite_execution_format_c_locale_text(
    char *buffer,
    size_t buffer_size,
    const char *format,
    ...
);
int mylite_execution_parse_c_locale_double(const char *text, char **out_end, double *out_value);
int mylite_execution_copy_normalized_double_text(
    struct mylite_db *database,
    const char *candidate,
    const char *function_name,
    char *buffer,
    size_t buffer_size
);
int mylite_execution_copy_normalized_scientific_double_text(
    struct mylite_db *database,
    const char *candidate,
    const char *function_name,
    char *buffer,
    size_t buffer_size
);
void mylite_execution_set_double_format_error(
    struct mylite_db *database,
    const char *function_name
);

#endif

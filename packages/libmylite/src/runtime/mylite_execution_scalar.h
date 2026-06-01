#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_json_normalize_result;
struct mylite_sql_ast_node;
struct mylite_sql_source_span;

enum {
    mylite_execution_scalar_integer_text_capacity = 32,
    mylite_execution_scalar_literal_projection_text_capacity = 83,
    mylite_execution_scalar_datetime_text_length = 19,
    mylite_execution_scalar_base_conversion_text_capacity = 66,
    mylite_execution_scalar_double_text_capacity = 32,
};

enum planned_json_mutation_kind {
    PLANNED_JSON_MUTATION_SET = 0,
    PLANNED_JSON_MUTATION_REPLACE = 1,
    PLANNED_JSON_MUTATION_INSERT = 2,
    PLANNED_JSON_MUTATION_REMOVE = 3,
};

struct session_scalar_cell {
    const char *value;
    char *owned_text;
    size_t value_size;
    size_t staged_division_by_zero_warning_count;
    size_t staged_invalid_logarithm_warning_count;
    const char *staged_truncated_integer_text;
    size_t staged_signed_complement_warning_count;
    size_t staged_unsigned_complement_warning_count;
    bool has_value_size;
    bool has_staged_truncated_integer_warning;
    bool has_staged_truncated_decimal_warning;
    bool has_staged_unhex_incorrect_string_warning;
    char datetime_text[mylite_execution_scalar_datetime_text_length + 1U];
    char integer_text[mylite_execution_scalar_integer_text_capacity];
    char double_text[mylite_execution_scalar_double_text_capacity];
    char base_conversion_text[mylite_execution_scalar_base_conversion_text_capacity];
    char literal_text[mylite_execution_scalar_literal_projection_text_capacity];
    char staged_truncated_decimal_text[mylite_execution_scalar_literal_projection_text_capacity];
    char staged_unhex_incorrect_string_text
        [mylite_execution_scalar_literal_projection_text_capacity];
};

int mylite_execution_scalar_json_valid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_extract_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_value_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_contains_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_contains_path_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_keys_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_type_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_unquote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_array_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_object_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_remove_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);

bool mylite_execution_scalar_json_valid_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_scalar_json_collect_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **out_arguments,
    size_t *out_argument_count
);
int mylite_execution_scalar_json_require_mutation_argument_count(
    struct mylite_db *database,
    size_t argument_count,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind
);
int mylite_execution_scalar_json_finish_path_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
int mylite_execution_scalar_json_finish_length_path_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
int mylite_execution_scalar_json_finish_keys_path_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);

const struct mylite_sql_ast_node *mylite_execution_child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
const struct mylite_sql_ast_node *mylite_execution_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
);

int mylite_execution_decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
);
int mylite_execution_cast_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_convert_binary_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_convert_using_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
void mylite_execution_set_parse_error(struct mylite_db *database);
void mylite_execution_set_unsupported_error(struct mylite_db *database, const char *message);
void mylite_execution_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_set_invalid_json_function_text_error(
    struct mylite_db *database,
    size_t position
);
int mylite_execution_append_invalid_json_value_warning(
    struct mylite_db *database,
    const struct mylite_json_normalize_result *result
);
void mylite_execution_set_invalid_json_path_error(struct mylite_db *database, size_t position);
void mylite_execution_set_json_path_not_allowed_error(struct mylite_db *database);
void mylite_execution_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_set_invalid_json_one_or_all_error(struct mylite_db *database);
void mylite_execution_set_json_unquote_incorrect_type_error(struct mylite_db *database);
void mylite_execution_set_json_quote_incorrect_type_error(struct mylite_db *database);
void mylite_execution_set_json_binary_charset_error(struct mylite_db *database);
void mylite_execution_set_json_null_member_name_error(struct mylite_db *database);
bool mylite_execution_text_equals_ascii_case_insensitive(const char *left, const char *right);
int mylite_execution_set_unknown_column_reference_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
void mylite_execution_set_nomem_error(struct mylite_db *database);
void mylite_execution_set_runtime_error(struct mylite_db *database, const char *message);
void mylite_execution_session_scalar_cell_deinit(struct session_scalar_cell *cell);

#endif

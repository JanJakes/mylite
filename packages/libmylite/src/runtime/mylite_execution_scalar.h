#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_H

#include "mylite_ast.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_text_internal.h"
#include "mylite_temporal_extract.h"
#include "mylite_timestampdiff.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_catalog_column_descriptor;
struct mylite_json_normalize_result;
struct mylite_sql_ast_node;
struct mylite_sql_source_span;

enum {
    mylite_execution_scalar_integer_text_capacity = 32,
    mylite_execution_scalar_literal_projection_text_capacity = 83,
    mylite_execution_scalar_datetime_text_length = 19,
    mylite_execution_scalar_fractional_datetime_text_length = 26,
    mylite_execution_scalar_base_conversion_text_capacity = 66,
    mylite_execution_scalar_double_text_capacity = 32,
};

enum planned_json_mutation_kind {
    PLANNED_JSON_MUTATION_SET = 0,
    PLANNED_JSON_MUTATION_REPLACE = 1,
    PLANNED_JSON_MUTATION_INSERT = 2,
    PLANNED_JSON_MUTATION_ARRAY_APPEND = 3,
    PLANNED_JSON_MUTATION_ARRAY_INSERT = 4,
    PLANNED_JSON_MUTATION_REMOVE = 5,
    PLANNED_JSON_MUTATION_MERGE = 6,
    PLANNED_JSON_MUTATION_MERGE_PATCH = 7,
    PLANNED_JSON_MUTATION_MERGE_PRESERVE = 8,
};

enum planned_string_length_function_kind {
    PLANNED_STRING_LENGTH_FUNCTION_NONE = 0,
    PLANNED_STRING_LENGTH_FUNCTION_BYTE = 1,
    PLANNED_STRING_LENGTH_FUNCTION_BIT = 2,
    PLANNED_STRING_LENGTH_FUNCTION_CHARACTER = 3,
};

enum planned_string_case_function_kind {
    PLANNED_STRING_CASE_FUNCTION_NONE = 0,
    PLANNED_STRING_CASE_FUNCTION_LOWER = 1,
    PLANNED_STRING_CASE_FUNCTION_UPPER = 2,
};

enum planned_string_codepoint_function_kind {
    PLANNED_STRING_CODEPOINT_FUNCTION_NONE = 0,
    PLANNED_STRING_CODEPOINT_FUNCTION_ASCII = 1,
    PLANNED_STRING_CODEPOINT_FUNCTION_ORD = 2,
};

enum planned_string_trim_function_kind {
    PLANNED_STRING_TRIM_FUNCTION_NONE = 0,
    PLANNED_STRING_TRIM_FUNCTION_BOTH = 1,
    PLANNED_STRING_TRIM_FUNCTION_LEADING = 2,
    PLANNED_STRING_TRIM_FUNCTION_TRAILING = 3,
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
    char fractional_datetime_text[mylite_execution_scalar_fractional_datetime_text_length + 1U];
    char integer_text[mylite_execution_scalar_integer_text_capacity];
    char double_text[mylite_execution_scalar_double_text_capacity];
    char base_conversion_text[mylite_execution_scalar_base_conversion_text_capacity];
    char literal_text[mylite_execution_scalar_literal_projection_text_capacity];
    char staged_truncated_decimal_text[mylite_execution_scalar_literal_projection_text_capacity];
    char staged_unhex_incorrect_string_text
        [mylite_execution_scalar_literal_projection_text_capacity];
};

struct scalar_arithmetic_value {
    bool is_null;
    bool has_numeric_real;
    int64_t integer;
    double numeric_real;
    size_t division_by_zero_warning_count;
};

struct scalar_bitwise_value {
    bool is_null;
    uint64_t integer;
    size_t division_by_zero_warning_count;
};

enum scalar_convert_charset_warning {
    SCALAR_CONVERT_CHARSET_WARNING_NONE,
    SCALAR_CONVERT_CHARSET_WARNING_UTF8_ALIAS,
    SCALAR_CONVERT_CHARSET_WARNING_UTF8MB3_DEPRECATED,
};

struct scalar_convert_charset_info {
    const char *charset;
    const char *collation;
    bool ascii_only_value;
    enum scalar_convert_charset_warning warning;
};

struct mylite_execution_temporal_fractional_precision_context {
    const char *subject_name;
    const char *unsupported_message;
};

int mylite_execution_scalar_string_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_codepoint_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_case_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_string_trim_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_unix_timestamp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_timestamp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_datediff_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_timestampdiff_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_timediff_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_temporal_extract_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_sec_to_time_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_from_unixtime_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_temporal_constructor_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_period_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_convert_tz_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_timestampdiff_function_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit,
    enum mylite_timestampdiff_unit *out_unit
);
int mylite_execution_resolve_temporal_extract_call(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind *out_extract_kind,
    const struct mylite_sql_ast_node **out_argument,
    int *out_mode
);
int mylite_execution_from_unixtime_scalar_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    int64_t *out_seconds
);
bool mylite_execution_is_temporal_extract_function_kind(enum mylite_sql_ast_node_kind ast_kind);
enum planned_string_length_function_kind mylite_execution_string_length_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_length_function_kind(enum mylite_sql_ast_node_kind ast_kind);
bool mylite_execution_string_length_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
enum planned_string_codepoint_function_kind mylite_execution_string_codepoint_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_codepoint_function_kind(enum mylite_sql_ast_node_kind ast_kind);
bool mylite_execution_string_codepoint_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
enum planned_string_case_function_kind mylite_execution_string_case_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_case_function_kind(enum mylite_sql_ast_node_kind ast_kind);
bool mylite_execution_string_case_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
enum planned_string_trim_function_kind mylite_execution_string_trim_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_string_trim_function_kind(enum mylite_sql_ast_node_kind ast_kind);
bool mylite_execution_string_trim_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_string_length_session_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_active_stmt_parameter_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *parameter,
    struct session_scalar_cell *out_cell
);
bool mylite_execution_active_stmt_is_analyzing_unbound_parameters(const struct mylite_db *database);
int mylite_execution_scalar_base_conversion_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_conv_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_bit_count_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_crc32_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_digest_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
bool mylite_execution_scalar_aes_function_match(const struct mylite_sql_ast_node *expression);
int mylite_execution_scalar_aes_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_hex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_weight_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_to_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_from_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_compress_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_uncompress_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_uncompressed_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_random_bytes_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_ip_address_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_unhex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_uuid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_uuid_swap_flag_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_swap
);
int mylite_execution_scalar_char_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_format_hex_bytes(
    struct mylite_db *database,
    const unsigned char *bytes,
    size_t byte_count,
    char **out_text
);

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
int mylite_execution_scalar_json_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_overlaps_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_member_of_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_depth_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_storage_size_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_storage_free_function_value(
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
int mylite_execution_scalar_json_pretty_function_value(
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
int mylite_execution_scalar_json_array_append_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_array_insert_function_value(
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
int mylite_execution_scalar_json_merge_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_merge_patch_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_json_merge_preserve_function_value(
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
int mylite_execution_scalar_json_require_merge_argument_count(
    struct mylite_db *database,
    size_t argument_count,
    const char *function_name
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

const struct mylite_sql_ast_node *mylite_execution_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
);
bool mylite_execution_is_scalar_arithmetic_projection_expression(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_is_scalar_division_projection_expression(
    const struct mylite_sql_ast_node *expression
);
bool mylite_execution_is_scalar_bitwise_projection_expression(
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_evaluate_scalar_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
int mylite_execution_evaluate_scalar_bitwise_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
int mylite_execution_evaluate_bit_count_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
int mylite_execution_scalar_hex_numeric_runtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
);
int mylite_execution_accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
);
int mylite_execution_accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t staged_count,
    size_t *inout_warning_count
);
int mylite_execution_append_division_by_zero_warnings(
    struct mylite_db *database,
    size_t warning_count
);
int mylite_execution_scalar_rand_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
);
int mylite_execution_validate_utf8_text(
    const char *text,
    size_t text_length,
    size_t *out_character_count
);
int mylite_execution_utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
bool mylite_execution_is_session_scalar_expression(const struct mylite_sql_ast_node *expression);
int mylite_execution_session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
);
int mylite_execution_session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
bool mylite_execution_text_value_is_supported_string_key(const char *text, size_t text_length);
const char *mylite_execution_scalar_pi_text(void);

int mylite_execution_decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
);
int mylite_execution_current_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
int mylite_execution_validate_temporal_fractional_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_execution_temporal_fractional_precision_context context
);
int mylite_execution_sysdate_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_current_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
int mylite_execution_current_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
int mylite_execution_utc_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
int mylite_execution_utc_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
int mylite_execution_utc_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
int mylite_execution_system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
);
int mylite_execution_decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
);
int mylite_execution_normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
);
int mylite_execution_format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
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
int mylite_execution_convert_using_charset_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_collate_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_convert_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
);
int mylite_execution_scalar_convert_charset_info_by_name(
    struct mylite_db *database,
    const char *charset_name,
    struct scalar_convert_charset_info *out_info
);
int mylite_execution_scalar_char_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
);
int mylite_execution_rand_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed
);
int64_t mylite_execution_current_timestamp_epoch(const struct mylite_db *database);
int mylite_execution_date_add_set_unknown_identifier_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
size_t mylite_execution_temporal_constructor_function_argument_count(
    enum mylite_sql_ast_node_kind ast_kind
);
const char *mylite_execution_temporal_constructor_function_name(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_temporal_constructor_function_kind(enum mylite_sql_ast_node_kind ast_kind);
int mylite_execution_copy_identifier_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    char *destination,
    size_t destination_size,
    const char *identifier_kind,
    const char *nul_message
);
const char *mylite_execution_national_character_set_name(void);
const char *mylite_execution_national_collation_name(void);
void mylite_execution_set_parse_error(struct mylite_db *database);
void mylite_execution_set_unsupported_error(struct mylite_db *database, const char *message);
int mylite_execution_format_approximate_result_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    double value,
    char *buffer,
    size_t buffer_size
);
void mylite_execution_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_set_scalar_division_unsupported_error(struct mylite_db *database);
void mylite_execution_set_abs_signed_minimum_overflow_error(struct mylite_db *database);
void mylite_execution_set_abs_unsupported_error(struct mylite_db *database);
void mylite_execution_set_sign_unsupported_error(struct mylite_db *database);
void mylite_execution_set_rounding_unsupported_error(struct mylite_db *database);
int mylite_execution_set_rounding_signed_overflow_error(struct mylite_db *database);
void mylite_execution_set_sqrt_unsupported_error(struct mylite_db *database);
void mylite_execution_set_angle_conversion_unsupported_error(struct mylite_db *database);
void mylite_execution_set_inverse_trig_unsupported_error(struct mylite_db *database);
void mylite_execution_set_direct_trig_unsupported_error(struct mylite_db *database);
void mylite_execution_set_atan_unsupported_error(struct mylite_db *database);
void mylite_execution_set_exp_log_power_unsupported_error(struct mylite_db *database);
void mylite_execution_set_format_unsupported_error(struct mylite_db *database);
void mylite_execution_set_truncate_unsupported_error(struct mylite_db *database);
void mylite_execution_set_base_conversion_unsupported_error(struct mylite_db *database);
void mylite_execution_set_bit_count_unsupported_error(struct mylite_db *database);
void mylite_execution_set_crc32_unsupported_error(struct mylite_db *database);
void mylite_execution_set_hex_unsupported_error(struct mylite_db *database);
void mylite_execution_scalar_set_base64_argument_unsupported_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_scalar_set_uuid_unsupported_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_set_regexp_illegal_argument_error(struct mylite_db *database);
void mylite_execution_set_regexp_error(struct mylite_db *database, const char *message);
void mylite_execution_set_regexp_character_range_error(
    struct mylite_db *database,
    const char *message
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
void mylite_execution_set_json_path_not_array_cell_error(struct mylite_db *database);
void mylite_execution_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_set_invalid_json_one_or_all_error(struct mylite_db *database);
void mylite_execution_set_invalid_json_one_or_all_function_error(
    struct mylite_db *database,
    const char *function_name
);
void mylite_execution_set_incorrect_arguments_to_escape_error(struct mylite_db *database);
void mylite_execution_set_json_unquote_incorrect_type_error(struct mylite_db *database);
void mylite_execution_set_json_quote_incorrect_type_error(struct mylite_db *database);
void mylite_execution_set_json_binary_charset_error(struct mylite_db *database);
void mylite_execution_set_json_null_member_name_error(struct mylite_db *database);
bool mylite_execution_text_equals_ascii_case_insensitive(const char *left, const char *right);
int mylite_execution_set_unknown_column_reference_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
int mylite_execution_scalar_concat_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_greatest_least_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_subquery_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
void mylite_execution_set_illegal_mix_of_collations_error(
    struct mylite_db *database,
    const char *first_collation,
    const char *second_collation,
    const char *operation
);
void mylite_execution_set_unknown_collation_error(
    struct mylite_db *database,
    const char *collation_name
);
void mylite_execution_set_collation_not_valid_for_charset_error(
    struct mylite_db *database,
    const char *collation_name,
    const char *charset_name
);
void mylite_execution_set_nomem_error(struct mylite_db *database);
void mylite_execution_set_runtime_error(struct mylite_db *database, const char *message);
void mylite_execution_session_scalar_cell_deinit(struct session_scalar_cell *cell);

#endif

#include "mylite_execution_scalar.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_execution_catalog.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_regexp.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_case.h"
#include "mylite_string_codepoint.h"
#include "mylite_string_concat.h"
#include "mylite_string_insert.h"
#include "mylite_string_padding.h"
#include "mylite_string_quote.h"
#include "mylite_string_replace.h"
#include "mylite_string_reverse.h"
#include "mylite_string_search.h"
#include "mylite_string_soundex.h"
#include "mylite_string_substring_index.h"
#include "mylite_string_trim.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    byte_bit_count = CHAR_BIT,
};

struct string_slice_right_bounds {
    size_t text_length;
    uint64_t requested_length;
    size_t character_count;
};

struct substring_text_bounds {
    const char *text;
    size_t text_length;
    int64_t position;
    bool has_length;
    int64_t requested_length;
};

struct string_bitmask_scalar_text_argument {
    struct mylite_string_bitmask_slice slice;
    struct session_scalar_cell cell;
    char *owned_text;
};

struct regexp_string_function_call_shape {
    enum planned_regexp_string_function_kind kind;
    size_t child_count;
};

/* Static helper prototypes. */
static int string_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_length_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int string_length_session_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static bool string_length_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static enum planned_string_length_function_kind string_length_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_length_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int string_codepoint_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_codepoint_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    bool *out_is_binary,
    bool *out_is_null
);
static bool string_codepoint_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static enum planned_string_codepoint_function_kind string_codepoint_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static enum mylite_string_codepoint_kind string_codepoint_function_to_value_kind(
    enum planned_string_codepoint_function_kind function_kind
);
static bool is_string_codepoint_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int string_case_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_case_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool string_case_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static enum planned_string_case_function_kind string_case_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_case_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static enum mylite_string_case_kind string_case_function_to_value_kind(
    enum planned_string_case_function_kind function_kind
);
static int string_trim_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_trim_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool string_trim_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static enum planned_string_trim_function_kind string_trim_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_trim_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static enum mylite_string_trim_kind string_trim_function_to_value_kind(
    enum planned_string_trim_function_kind function_kind
);
static int string_slice_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_slice_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_slice_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length,
    bool *out_is_null
);
static int evaluate_string_slice_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
);
static int slice_utf8_text_value(
    struct mylite_db *database,
    enum planned_string_slice_function_kind function_kind,
    const char *text,
    size_t text_length,
    int64_t requested_length,
    struct session_scalar_cell *out_cell
);
static int substring_utf8_text_value(
    struct mylite_db *database,
    const struct substring_text_bounds *bounds,
    struct session_scalar_cell *out_cell
);
static int string_slice_empty_result(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int find_left_slice_end(
    const char *text,
    size_t text_length,
    uint64_t requested_length,
    size_t *out_end
);
static int find_right_slice_start(
    const char *text,
    const struct string_slice_right_bounds *bounds,
    size_t *out_start
);
static bool string_slice_scalar_text_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static bool string_slice_length_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int string_slice_signed_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
);
static int string_slice_signed_position_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
);
static int string_slice_signed_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
);
static enum planned_string_slice_function_kind string_slice_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_slice_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int string_padding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_pad_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_string_padding_function_kind function_kind,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_repeat_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_space_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_string_padding_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_padding_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_count,
    bool *out_is_null
);
static int string_padding_set_owned_result(
    struct mylite_db *database,
    int rc,
    char *value,
    size_t value_length,
    bool is_null,
    struct session_scalar_cell *out_cell
);
static enum planned_string_padding_function_kind string_padding_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static enum mylite_string_padding_side string_padding_function_to_side(
    enum planned_string_padding_function_kind function_kind
);
static bool is_string_padding_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_export_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_make_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_string_bitmask_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
);
static int evaluate_string_bitmask_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct string_bitmask_scalar_text_argument *out_argument
);
static int string_bitmask_set_owned_result(
    struct mylite_db *database,
    int rc,
    char *value,
    size_t value_length,
    bool is_null,
    struct session_scalar_cell *out_cell
);
static void string_bitmask_scalar_text_argument_deinit(
    struct string_bitmask_scalar_text_argument *argument
);
static enum planned_string_bitmask_function_kind string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int string_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_search_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_search_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
);
static int format_string_search_result(
    struct mylite_db *database,
    int64_t result,
    struct session_scalar_cell *out_cell
);
static enum planned_string_search_function_kind string_search_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_search_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int concat_ws_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_concat_ws_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool concat_ws_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int string_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int string_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_replace_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool string_replace_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static int evaluate_string_insert_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_insert_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
);
static int evaluate_string_insert_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length,
    bool *out_is_null
);
static int string_reverse_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_reverse_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool string_reverse_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static int soundex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_soundex_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool soundex_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_quote_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool quote_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int substring_index_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_substring_index_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_substring_index_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_count,
    bool *out_is_null
);
static int find_in_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int strcmp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int regexp_like_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int regexp_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int validate_regexp_string_function_argument_count(
    struct mylite_db *database,
    const struct regexp_string_function_call_shape *shape
);
static enum planned_regexp_string_function_kind regexp_string_function_kind(
    enum mylite_sql_ast_node_kind kind
);
static const char *regexp_string_function_name(enum planned_regexp_string_function_kind kind);
static const char *regexp_string_function_argument_count_error_name(
    enum mylite_sql_ast_node_kind kind
);
static const struct mylite_sql_ast_node *regexp_string_function_arguments(
    const struct mylite_sql_ast_node *expression
);
static size_t regexp_string_function_argument_count(const struct mylite_sql_ast_node *expression);
static const struct mylite_sql_ast_node *regexp_string_function_argument_at(
    const struct mylite_sql_ast_node *expression,
    size_t index
);
static int evaluate_find_in_set_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_strcmp_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_regexp_like_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool allow_session_scalar,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool regexp_like_literal_or_unary_expression_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static int evaluate_regexp_like_literal_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int regexp_like_cell_text_result(
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_regexp_like_match_type_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_null,
    bool *out_case_sensitive
);
static int regexp_like_case_sensitive_from_match_type(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool *out_case_sensitive
);
static int validate_regexp_like_pattern(
    struct mylite_db *database,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    const char *unsupported_message
);
static int set_regexp_like_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status,
    const char *unsupported_message
);
static int match_regexp_like_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    bool *out_matches
);
static int regexp_string_find_match(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_match *out_match
);
static int regexp_string_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status
);
static int regexp_string_match_error(
    struct mylite_db *database,
    enum mylite_regexp_match_status status
);
static int regexp_string_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    bool value_is_null,
    const char *pattern,
    size_t pattern_length,
    bool pattern_is_null,
    const char *replacement,
    size_t replacement_length,
    bool replacement_is_null,
    struct session_scalar_cell *out_cell
);
static int regexp_instr_or_substr_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct session_scalar_cell *out_cell
);
static int regexp_substr_result_value(
    struct mylite_db *database,
    const char *value,
    const struct mylite_regexp_match *match,
    struct session_scalar_cell *out_cell
);
static int regexp_replace_result_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    const char *replacement,
    size_t replacement_length,
    struct session_scalar_cell *out_cell
);
static int regexp_replace_append(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell,
    const char *text,
    size_t text_length
);
static int charset_collation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int charset_collation_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_non_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int apply_coercibility_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char **inout_result,
    bool *inout_has_non_null_argument,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation
);
static int coercibility_binary_wrapper_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_validate_binary_wrapper_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument
);
static int coercibility_literal_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static const char *coercibility_concat_argument_result(const char *argument_result);
static bool coercibility_binary_wrapper_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_column_reference
);
static int set_unknown_column_for_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int charset_collation_concat_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int validate_charset_collation_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_binary_string_argument,
    const struct mylite_execution_catalog_scalar_collation **out_explicit_collation
);
static int merge_concat_explicit_collation(
    struct mylite_db *database,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation,
    const struct mylite_execution_catalog_scalar_collation *argument_collation
);
static int charset_collation_convert_using_charset_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int charset_collation_collate_expression_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int charset_collation_rand_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
);
static int scalar_expression_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
);
static int scalar_collation_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_execution_catalog_scalar_collation **out_info
);
static int scalar_expression_base_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
);
static const struct mylite_execution_catalog_scalar_collation *scalar_collation_info_by_name(
    const char *collation_name
);
static enum planned_charset_collation_function_kind charset_collation_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_charset_collation_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int charset_collation_select_result(
    enum planned_charset_collation_function_kind function_kind,
    const char *charset,
    const char *collation,
    const char **out_result
);

int mylite_execution_scalar_string_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_length_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_codepoint_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_codepoint_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_case_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_case_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_trim_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_trim_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_slice_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_slice_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_padding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_padding_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_bitmask_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_search_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_concat_ws_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return concat_ws_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_replace_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_insert_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_reverse_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_reverse_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_soundex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return soundex_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return quote_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_substring_index_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return substring_index_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_find_in_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return find_in_set_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_strcmp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return strcmp_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_regexp_like_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return regexp_like_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_regexp_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return regexp_string_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_charset_collation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return charset_collation_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_expression_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    return scalar_expression_charset_collation_metadata(
        database,
        expression,
        out_charset,
        out_collation
    );
}

int mylite_execution_charset_collation_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    return charset_collation_scalar_result(database, function_kind, expression, out_result);
}

int mylite_execution_charset_collation_select_result(
    enum planned_charset_collation_function_kind function_kind,
    const char *charset,
    const char *collation,
    const char **out_result
) {
    return charset_collation_select_result(function_kind, charset, collation, out_result);
}

int mylite_execution_scalar_collation_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_execution_catalog_scalar_collation **out_info
) {
    return scalar_collation_info_for_expression(database, expression, out_info);
}

const struct mylite_execution_catalog_scalar_collation *mylite_execution_scalar_collation_info_by_name(
    const char *collation_name
) {
    return scalar_collation_info_by_name(collation_name);
}

bool mylite_execution_coercibility_binary_wrapper_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_column_reference
) {
    return coercibility_binary_wrapper_column_reference(expression, out_column_reference);
}

enum planned_string_length_function_kind mylite_execution_string_length_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_length_function_kind(ast_kind);
}

bool mylite_execution_is_string_length_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_length_function_kind(ast_kind);
}

bool mylite_execution_string_length_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_length_scalar_argument_is_admitted(expression);
}

enum planned_string_codepoint_function_kind mylite_execution_string_codepoint_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_codepoint_function_kind(ast_kind);
}

bool mylite_execution_is_string_codepoint_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_codepoint_function_kind(ast_kind);
}

bool mylite_execution_string_codepoint_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_codepoint_scalar_argument_is_admitted(expression);
}

enum planned_string_case_function_kind mylite_execution_string_case_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_case_function_kind(ast_kind);
}

bool mylite_execution_is_string_case_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_case_function_kind(ast_kind);
}

bool mylite_execution_string_case_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_case_scalar_argument_is_admitted(expression);
}

enum planned_string_trim_function_kind mylite_execution_string_trim_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_trim_function_kind(ast_kind);
}

bool mylite_execution_is_string_trim_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_trim_function_kind(ast_kind);
}

bool mylite_execution_string_trim_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_trim_scalar_argument_is_admitted(expression);
}

enum planned_string_slice_function_kind mylite_execution_string_slice_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_slice_function_kind(ast_kind);
}

bool mylite_execution_is_string_slice_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_slice_function_kind(ast_kind);
}

bool mylite_execution_string_slice_scalar_text_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_slice_scalar_text_argument_is_admitted(expression);
}

bool mylite_execution_string_slice_length_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_slice_length_argument_is_admitted(expression);
}

int mylite_execution_string_length_session_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_length_session_scalar_argument_value(database, expression, out_cell);
}

int mylite_execution_string_slice_signed_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_length_value(database, expression, out_value, out_is_null);
}

int mylite_execution_string_slice_signed_position_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_position_value(database, expression, out_value, out_is_null);
}

int mylite_execution_string_slice_signed_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_integer_value(
        database,
        expression,
        unsupported_message,
        range_message,
        out_value,
        out_is_null
    );
}

enum planned_string_padding_function_kind mylite_execution_string_padding_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_padding_function_kind(ast_kind);
}

bool mylite_execution_is_string_padding_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_padding_function_kind(ast_kind);
}

enum planned_string_bitmask_function_kind mylite_execution_string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_bitmask_function_kind(ast_kind);
}

bool mylite_execution_is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_bitmask_function_kind(ast_kind);
}

enum planned_string_search_function_kind mylite_execution_string_search_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_search_function_kind(ast_kind);
}

bool mylite_execution_is_string_search_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_search_function_kind(ast_kind);
}

bool mylite_execution_concat_ws_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return concat_ws_scalar_argument_is_admitted(expression);
}

bool mylite_execution_string_replace_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_replace_scalar_argument_is_admitted(expression);
}

bool mylite_execution_string_reverse_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_reverse_scalar_argument_is_admitted(expression);
}

bool mylite_execution_soundex_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return soundex_scalar_argument_is_admitted(expression);
}

bool mylite_execution_quote_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return quote_scalar_argument_is_admitted(expression);
}

int mylite_execution_evaluate_regexp_like_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool allow_session_scalar,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    return evaluate_regexp_like_text_argument(
        database,
        expression,
        allow_session_scalar,
        messages,
        inout_cell,
        out_owned_text,
        out_text,
        out_text_length,
        out_is_null
    );
}

int mylite_execution_evaluate_regexp_like_match_type_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_null,
    bool *out_case_sensitive
) {
    return evaluate_regexp_like_match_type_argument(
        database,
        expression,
        out_is_null,
        out_case_sensitive
    );
}

int mylite_execution_validate_regexp_like_pattern(
    struct mylite_db *database,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    const char *unsupported_message
) {
    return validate_regexp_like_pattern(
        database,
        pattern,
        pattern_length,
        case_sensitive,
        unsupported_message
    );
}

enum planned_regexp_string_function_kind mylite_execution_regexp_string_function_kind(
    enum mylite_sql_ast_node_kind kind
) {
    return regexp_string_function_kind(kind);
}

const char *mylite_execution_regexp_string_function_name(
    enum planned_regexp_string_function_kind kind
) {
    return regexp_string_function_name(kind);
}

const char *mylite_execution_regexp_string_function_argument_count_error_name(
    enum mylite_sql_ast_node_kind kind
) {
    return regexp_string_function_argument_count_error_name(kind);
}

size_t mylite_execution_regexp_string_function_argument_count(
    const struct mylite_sql_ast_node *expression
) {
    return regexp_string_function_argument_count(expression);
}

const struct mylite_sql_ast_node *mylite_execution_regexp_string_function_argument_at(
    const struct mylite_sql_ast_node *expression,
    size_t index
) {
    return regexp_string_function_argument_at(expression, index);
}

enum planned_charset_collation_function_kind mylite_execution_charset_collation_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return charset_collation_function_kind(ast_kind);
}

bool mylite_execution_is_charset_collation_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_charset_collation_function_kind(ast_kind);
}

static int string_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_length_function_kind function_kind = PLANNED_STRING_LENGTH_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t character_count = 0U;
    uint64_t result = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_LENGTH_FUNCTION_NONE
                                       : string_length_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_LENGTH_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "string length functions support exactly one argument"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_length_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_text);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    if (function_kind == PLANNED_STRING_LENGTH_FUNCTION_CHARACTER) {
        rc = mylite_execution_validate_utf8_text(text, text_length, &character_count);
        if (rc != MYLITE_OK) {
            mylite_execution_set_runtime_error(
                database,
                "invalid UTF-8 value in string length function"
            );
            free(owned_text);
            mylite_execution_session_scalar_cell_deinit(&argument_cell);
            return MYLITE_ERROR;
        }
        result = (uint64_t)character_count;
    } else if (function_kind == PLANNED_STRING_LENGTH_FUNCTION_BIT) {
        if (text_length > UINT64_MAX / byte_bit_count) {
            mylite_execution_set_unsupported_error(database, "BIT_LENGTH() result is out of range");
            free(owned_text);
            mylite_execution_session_scalar_cell_deinit(&argument_cell);
            return MYLITE_ERROR;
        }
        result = (uint64_t)text_length * byte_bit_count;
    } else {
        result = (uint64_t)text_length;
    }

    rc = mylite_execution_format_uint64(
        database,
        result,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_length_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_length_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string length functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string length functions support only string literals",
                "string length function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int string_length_session_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        if (database->session.has_selected_schema) {
            out_cell->value = database->session.selected_schema;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        out_cell->value = database->session.client_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        out_cell->value = database->session.current_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        out_cell->value = "NONE";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.connection_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format CONNECTION_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PI_FUNCTION:
        out_cell->value = mylite_execution_scalar_pi_text();
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
        return mylite_execution_scalar_rand_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_FUNCTION:
        return mylite_execution_scalar_uuid_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
        mylite_execution_set_native_function_parameter_count_error(database, "UUID");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        return mylite_execution_current_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        mylite_execution_set_native_function_parameter_count_error(database, "CURRENT_TIMESTAMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        return mylite_execution_sysdate_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
        mylite_execution_set_native_function_parameter_count_error(database, "SYSDATE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        return mylite_execution_current_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        return mylite_execution_current_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        return mylite_execution_utc_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        return mylite_execution_utc_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        return mylite_execution_utc_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            database->session.previous_row_count
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format ROW_COUNT() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.found_rows
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format FOUND_ROWS() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.last_insert_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format LAST_INSERT_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return mylite_execution_system_variable_value(database, expression, out_cell);
    default:
        mylite_execution_set_unsupported_error(
            database,
            "string length functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }
}

static bool string_length_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        return false;
    }
    if (mylite_execution_is_session_scalar_expression(expression)) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static enum planned_string_length_function_kind string_length_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
        return PLANNED_STRING_LENGTH_FUNCTION_BYTE;
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
        return PLANNED_STRING_LENGTH_FUNCTION_BIT;
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
        return PLANNED_STRING_LENGTH_FUNCTION_CHARACTER;
    default:
        return PLANNED_STRING_LENGTH_FUNCTION_NONE;
    }
}

static bool is_string_length_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_length_function_kind(ast_kind) != PLANNED_STRING_LENGTH_FUNCTION_NONE;
}

static int string_codepoint_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_codepoint_function_kind function_kind =
        PLANNED_STRING_CODEPOINT_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    uint64_t result = 0U;
    bool is_binary = false;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_CODEPOINT_FUNCTION_NONE
                                       : string_codepoint_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_CODEPOINT_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "string codepoint functions support exactly one argument"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_codepoint_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &bytes,
        &byte_count,
        &is_binary,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_text);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    rc = mylite_string_codepoint_value(
        string_codepoint_function_to_value_kind(function_kind),
        bytes,
        byte_count,
        is_binary,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_format_uint64(
            database,
            result,
            out_cell->integer_text,
            sizeof(out_cell->integer_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_codepoint_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    bool *out_is_binary,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_bytes == NULL ||
        out_byte_count == NULL || out_is_binary == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_is_binary = false;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_codepoint_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string codepoint functions support only string, hex, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal_with_policy(
                database,
                expression,
                "string codepoint functions support only string literals",
                "string codepoint function literals are invalid",
                true,
                out_owned_text,
                out_byte_count
            );
            if (rc == MYLITE_OK) {
                *out_bytes = (const unsigned char *)*out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
            rc = mylite_execution_decode_binary_hex_literal(
                database,
                expression,
                out_owned_text,
                out_byte_count
            );
            if (rc == MYLITE_OK) {
                *out_bytes = (const unsigned char *)*out_owned_text;
                *out_is_binary = true;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_bytes = (const unsigned char *)inout_cell->value;
    if (inout_cell->has_value_size) {
        *out_byte_count = inout_cell->value_size;
    } else {
        *out_byte_count = strlen(inout_cell->value);
    }
    return MYLITE_OK;
}

static bool string_codepoint_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        return false;
    }
    if (mylite_execution_is_session_scalar_expression(expression)) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
            literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static enum planned_string_codepoint_function_kind string_codepoint_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_ASCII_FUNCTION:
        return PLANNED_STRING_CODEPOINT_FUNCTION_ASCII;
    case MYLITE_SQL_AST_ORD_FUNCTION:
        return PLANNED_STRING_CODEPOINT_FUNCTION_ORD;
    default:
        return PLANNED_STRING_CODEPOINT_FUNCTION_NONE;
    }
}

static enum mylite_string_codepoint_kind string_codepoint_function_to_value_kind(
    enum planned_string_codepoint_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_CODEPOINT_FUNCTION_ASCII:
        return MYLITE_STRING_CODEPOINT_ASCII;
    case PLANNED_STRING_CODEPOINT_FUNCTION_ORD:
        return MYLITE_STRING_CODEPOINT_ORD;
    case PLANNED_STRING_CODEPOINT_FUNCTION_NONE:
        break;
    }
    return MYLITE_STRING_CODEPOINT_ASCII;
}

static bool is_string_codepoint_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_codepoint_function_kind(ast_kind) != PLANNED_STRING_CODEPOINT_FUNCTION_NONE;
}

static int string_case_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_case_function_kind function_kind = PLANNED_STRING_CASE_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_CASE_FUNCTION_NONE
                                       : string_case_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_CASE_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "string case functions support exactly one argument"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_case_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_text);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    rc = mylite_string_case_ascii_value(
        database,
        string_case_function_to_value_kind(function_kind),
        text,
        text_length,
        &out_cell->owned_text
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_case_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_case_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string case functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string case functions support only string literals",
                "string case function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool string_case_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    return string_length_scalar_argument_is_admitted(expression);
}

static enum planned_string_case_function_kind string_case_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
        return PLANNED_STRING_CASE_FUNCTION_LOWER;
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
        return PLANNED_STRING_CASE_FUNCTION_UPPER;
    default:
        return PLANNED_STRING_CASE_FUNCTION_NONE;
    }
}

static bool is_string_case_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_case_function_kind(ast_kind) != PLANNED_STRING_CASE_FUNCTION_NONE;
}

static enum mylite_string_case_kind string_case_function_to_value_kind(
    enum planned_string_case_function_kind function_kind
) {
    if (function_kind == PLANNED_STRING_CASE_FUNCTION_UPPER) {
        return MYLITE_STRING_CASE_UPPER;
    }
    return MYLITE_STRING_CASE_LOWER;
}

static int string_trim_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_trim_function_kind function_kind = PLANNED_STRING_TRIM_FUNCTION_NONE;
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell remove_cell = {0};
    char *owned_value = NULL;
    char *owned_remove = NULL;
    const char *value = NULL;
    const char *remove_string = " ";
    size_t value_length = 0U;
    size_t remove_string_length = 1U;
    size_t argument_count = 0U;
    bool value_is_null = false;
    bool remove_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_TRIM_FUNCTION_NONE
                                       : string_trim_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_TRIM_FUNCTION_NONE ||
        (argument_count != 1U && argument_count != 2U)) {
        mylite_execution_set_unsupported_error(
            database,
            "trim functions support one value and an optional remove string"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_trim_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK && argument_count == 2U) {
        rc = evaluate_string_trim_scalar_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &remove_cell,
            &owned_remove,
            &remove_string,
            &remove_string_length,
            &remove_is_null
        );
    }
    if (rc != MYLITE_OK || value_is_null || remove_is_null) {
        free(owned_value);
        free(owned_remove);
        mylite_execution_session_scalar_cell_deinit(&value_cell);
        mylite_execution_session_scalar_cell_deinit(&remove_cell);
        return rc;
    }

    rc = mylite_string_trim_value(
        database,
        string_trim_function_to_value_kind(function_kind),
        value,
        value_length,
        remove_string,
        remove_string_length,
        &out_cell->owned_text
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_value);
    free(owned_remove);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&remove_cell);
    return rc;
}

static int evaluate_string_trim_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_trim_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "trim functions support only string, integer, boolean, NULL, session scalar, and "
            "system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "trim functions support only string literals",
                "trim function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool string_trim_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    return string_length_scalar_argument_is_admitted(expression);
}

static enum planned_string_trim_function_kind string_trim_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
        return PLANNED_STRING_TRIM_FUNCTION_LEADING;
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
        return PLANNED_STRING_TRIM_FUNCTION_TRAILING;
    case MYLITE_SQL_AST_TRIM_FUNCTION:
        return PLANNED_STRING_TRIM_FUNCTION_BOTH;
    default:
        return PLANNED_STRING_TRIM_FUNCTION_NONE;
    }
}

static bool is_string_trim_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_trim_function_kind(ast_kind) != PLANNED_STRING_TRIM_FUNCTION_NONE;
}

static enum mylite_string_trim_kind string_trim_function_to_value_kind(
    enum planned_string_trim_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_TRIM_FUNCTION_LEADING:
        return MYLITE_STRING_TRIM_LEADING;
    case PLANNED_STRING_TRIM_FUNCTION_TRAILING:
        return MYLITE_STRING_TRIM_TRAILING;
    case PLANNED_STRING_TRIM_FUNCTION_BOTH:
    case PLANNED_STRING_TRIM_FUNCTION_NONE:
        break;
    }
    return MYLITE_STRING_TRIM_BOTH;
}

static int string_slice_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_slice_function_kind function_kind = PLANNED_STRING_SLICE_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    int64_t position = 0;
    int64_t requested_length = 0;
    size_t argument_count = 0U;
    bool text_is_null = false;
    bool position_is_null = false;
    bool length_is_null = false;
    bool has_length = true;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_SLICE_FUNCTION_NONE
                                       : string_slice_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_NONE) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support exactly two arguments"
        );
        return MYLITE_ERROR;
    }
    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        has_length = argument_count == 3U;
        if (argument_count != 2U && argument_count != 3U) {
            mylite_execution_set_unsupported_error(
                database,
                "SUBSTRING functions support two or three arguments"
            );
            return MYLITE_ERROR;
        }
    } else if (argument_count != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support exactly two arguments"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_slice_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &text_is_null
    );
    if (rc == MYLITE_OK && function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        rc = evaluate_string_slice_position_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &position,
            &position_is_null
        );
    } else if (rc == MYLITE_OK) {
        rc = evaluate_string_slice_length_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &requested_length,
            &length_is_null
        );
    }
    if (rc == MYLITE_OK && function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING && has_length) {
        rc = evaluate_string_slice_length_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &requested_length,
            &length_is_null
        );
    }
    if (rc != MYLITE_OK || text_is_null || position_is_null || length_is_null) {
        free(owned_text);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        struct substring_text_bounds substring_bounds = {
            .text = text,
            .text_length = text_length,
            .position = position,
            .has_length = has_length,
            .requested_length = requested_length,
        };

        rc = substring_utf8_text_value(database, &substring_bounds, out_cell);
    } else {
        rc = slice_utf8_text_value(
            database,
            function_kind,
            text,
            text_length,
            requested_length,
            out_cell
        );
    }
    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_slice_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable string arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string slice functions support only string literals",
                "string slice function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_string_slice_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length,
    bool *out_is_null
) {
    if (out_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0;
    *out_is_null = false;

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL length literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_length_value(database, expression, out_length, out_is_null);
}

static int evaluate_string_slice_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
) {
    if (out_position == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    *out_is_null = false;

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL position literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_position_value(database, expression, out_position, out_is_null);
}

static int slice_utf8_text_value(
    struct mylite_db *database,
    enum planned_string_slice_function_kind function_kind,
    const char *text,
    size_t text_length,
    int64_t requested_length,
    struct session_scalar_cell *out_cell
) {
    size_t character_count = 0U;
    size_t start = 0U;
    size_t end = 0U;
    size_t slice_length = 0U;
    char *value = NULL;
    int rc = MYLITE_OK;

    if (text == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (requested_length <= 0) {
        return string_slice_empty_result(database, out_cell);
    }

    rc = mylite_execution_validate_utf8_text(text, text_length, &character_count);
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }

    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_LEFT) {
        rc = find_left_slice_end(text, text_length, (uint64_t)requested_length, &end);
    } else if (function_kind == PLANNED_STRING_SLICE_FUNCTION_RIGHT) {
        struct string_slice_right_bounds bounds = {
            .text_length = text_length,
            .requested_length = (uint64_t)requested_length,
            .character_count = character_count,
        };

        end = text_length;
        rc = find_right_slice_start(text, &bounds, &start);
    } else {
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }
    slice_length = end - start;
    if (slice_length == SIZE_MAX) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    value = (char *)malloc(slice_length + 1U);
    if (value == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(value, text + start, slice_length);
    value[slice_length] = '\0';
    out_cell->owned_text = value;
    out_cell->value = value;
    return MYLITE_OK;
}

static int substring_utf8_text_value(
    struct mylite_db *database,
    const struct substring_text_bounds *bounds,
    struct session_scalar_cell *out_cell
) {
    uint64_t start_character = 0U;
    uint64_t end_character = 0U;
    uint64_t character_count64 = 0U;
    size_t character_count = 0U;
    size_t start = 0U;
    size_t end = 0U;
    size_t slice_length = 0U;
    char *value = NULL;
    int rc = MYLITE_OK;

    if (bounds == NULL || bounds->text == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (bounds->position == 0 || (bounds->has_length && bounds->requested_length <= 0)) {
        return string_slice_empty_result(database, out_cell);
    }

    rc = mylite_execution_validate_utf8_text(bounds->text, bounds->text_length, &character_count);
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }
    character_count64 = (uint64_t)character_count;

    if (bounds->position > 0) {
        start_character = (uint64_t)bounds->position - 1U;
        if (start_character >= character_count64) {
            return string_slice_empty_result(database, out_cell);
        }
    } else {
        uint64_t magnitude = bounds->position == INT64_MIN ? (uint64_t)INT64_MAX + 1U
                                                           : (uint64_t)(-bounds->position);

        if (magnitude > character_count64) {
            return string_slice_empty_result(database, out_cell);
        }
        start_character = character_count64 - magnitude;
    }

    if (bounds->has_length) {
        uint64_t remaining = character_count64 - start_character;
        uint64_t requested = (uint64_t)bounds->requested_length;

        end_character = start_character + (requested > remaining ? remaining : requested);
    } else {
        end_character = character_count64;
    }

    rc = find_left_slice_end(bounds->text, bounds->text_length, start_character, &start);
    if (rc == MYLITE_OK) {
        rc = find_left_slice_end(bounds->text, bounds->text_length, end_character, &end);
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }

    slice_length = end - start;
    if (slice_length == SIZE_MAX) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    value = (char *)malloc(slice_length + 1U);
    if (value == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(value, bounds->text + start, slice_length);
    value[slice_length] = '\0';
    out_cell->owned_text = value;
    out_cell->value = value;
    return MYLITE_OK;
}

static int string_slice_empty_result(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    out_cell->owned_text = (char *)malloc(1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_cell->owned_text[0] = '\0';
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static int find_left_slice_end(
    const char *text,
    size_t text_length,
    uint64_t requested_length,
    size_t *out_end
) {
    size_t index = 0U;
    uint64_t character_index = 0U;

    if (text == NULL || out_end == NULL) {
        return MYLITE_MISUSE;
    }
    while (index < text_length && character_index < requested_length) {
        size_t width = 0U;
        int rc = mylite_execution_utf8_sequence_width(text, text_length, index, &width);

        if (rc != MYLITE_OK) {
            return rc;
        }
        index += width;
        ++character_index;
    }
    *out_end = index;
    return MYLITE_OK;
}

static int find_right_slice_start(
    const char *text,
    const struct string_slice_right_bounds *bounds,
    size_t *out_start
) {
    size_t index = 0U;
    size_t skip_count = 0U;

    if (text == NULL || bounds == NULL || out_start == NULL) {
        return MYLITE_MISUSE;
    }
    if (bounds->requested_length >= bounds->character_count) {
        *out_start = 0U;
        return MYLITE_OK;
    }

    skip_count = bounds->character_count - (size_t)bounds->requested_length;
    for (size_t character_index = 0U; character_index < skip_count; ++character_index) {
        size_t width = 0U;
        int rc = mylite_execution_utf8_sequence_width(text, bounds->text_length, index, &width);

        if (rc != MYLITE_OK) {
            return rc;
        }
        index += width;
    }
    *out_start = index;
    return MYLITE_OK;
}

static bool string_slice_scalar_text_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_length_scalar_argument_is_admitted(expression);
}

static bool string_slice_length_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static int string_slice_signed_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_integer_value(
        database,
        expression,
        "string slice functions support only integer, boolean, and NULL length literals",
        "string slice function length literals must fit the signed 64-bit range",
        out_value,
        out_is_null
    );
}

static int string_slice_signed_position_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_integer_value(
        database,
        expression,
        "string slice functions support only integer, boolean, and NULL position literals",
        "string slice function position literals must fit the signed 64-bit range",
        out_value,
        out_is_null
    );
}

static int string_slice_signed_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal =
        mylite_execution_unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = false;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(literal, 0U)
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        mylite_execution_set_unsupported_error(database, range_message);
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        *out_value = INT64_MIN;
    } else if (is_negative) {
        *out_value = -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static enum planned_string_slice_function_kind string_slice_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LEFT_FUNCTION:
        return PLANNED_STRING_SLICE_FUNCTION_LEFT;
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
        return PLANNED_STRING_SLICE_FUNCTION_RIGHT;
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
        return PLANNED_STRING_SLICE_FUNCTION_SUBSTRING;
    default:
        return PLANNED_STRING_SLICE_FUNCTION_NONE;
    }
}

static bool is_string_slice_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_slice_function_kind(ast_kind) != PLANNED_STRING_SLICE_FUNCTION_NONE;
}

static int string_padding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_padding_function_kind function_kind = PLANNED_STRING_PADDING_FUNCTION_NONE;
    char *result = NULL;
    size_t result_length = 0U;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_PADDING_FUNCTION_NONE
                                       : string_padding_function_kind(expression->kind);
    switch (function_kind) {
    case PLANNED_STRING_PADDING_FUNCTION_LPAD:
    case PLANNED_STRING_PADDING_FUNCTION_RPAD:
        rc = evaluate_pad_string_padding_function(
            database,
            expression,
            function_kind,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_PADDING_FUNCTION_REPEAT:
        rc = evaluate_repeat_string_padding_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_PADDING_FUNCTION_SPACE:
        rc = evaluate_space_string_padding_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    default:
        mylite_execution_set_unsupported_error(
            database,
            "string padding functions support LPAD, RPAD, REPEAT, and SPACE"
        );
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = string_padding_set_owned_result(
            database,
            rc,
            result,
            result_length,
            result_is_null,
            out_cell
        );
        result = NULL;
    } else if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string padding function"
        );
    }

    free(result);
    return rc;
}

static int evaluate_pad_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_string_padding_function_kind function_kind,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    struct session_scalar_cell first_cell = {0};
    struct session_scalar_cell third_cell = {0};
    char *owned_first_text = NULL;
    char *owned_third_text = NULL;
    const char *first_text = NULL;
    const char *third_text = NULL;
    size_t first_length = 0U;
    size_t third_length = 0U;
    int64_t count = 0;
    size_t argument_count = 0U;
    bool first_is_null = false;
    bool count_is_null = false;
    bool third_is_null = false;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (argument_count != 3U) {
        mylite_execution_set_native_function_parameter_count_error(
            database,
            function_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ? "LPAD" : "RPAD"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_padding_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &first_cell,
        &owned_first_text,
        &first_text,
        &first_length,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_padding_count_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &count,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_padding_text_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &third_cell,
            &owned_third_text,
            &third_text,
            &third_length,
            &third_is_null
        );
    }
    if (rc == MYLITE_OK && !first_is_null && !count_is_null && !third_is_null) {
        rc = mylite_string_pad_value(
            database,
            string_padding_function_to_side(function_kind),
            (struct mylite_string_padding_slice){
                .text = first_text,
                .length = first_length,
            },
            count,
            (struct mylite_string_padding_slice){
                .text = third_text,
                .length = third_length,
            },
            out_result,
            out_result_length,
            out_is_null
        );
    } else {
        *out_is_null = rc == MYLITE_OK;
    }

    free(owned_first_text);
    free(owned_third_text);
    mylite_execution_session_scalar_cell_deinit(&first_cell);
    mylite_execution_session_scalar_cell_deinit(&third_cell);
    return rc;
}

static int evaluate_repeat_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    struct session_scalar_cell first_cell = {0};
    char *owned_first_text = NULL;
    const char *first_text = NULL;
    size_t first_length = 0U;
    size_t argument_count = 0U;
    int64_t count = 0;
    bool first_is_null = false;
    bool count_is_null = false;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (argument_count != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REPEAT");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_padding_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &first_cell,
        &owned_first_text,
        &first_text,
        &first_length,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_padding_count_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &count,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK && !first_is_null && !count_is_null) {
        rc = mylite_string_repeat_value(
            database,
            (struct mylite_string_padding_slice){
                .text = first_text,
                .length = first_length,
            },
            count,
            out_result,
            out_result_length,
            out_is_null
        );
    } else {
        *out_is_null = rc == MYLITE_OK;
    }

    free(owned_first_text);
    mylite_execution_session_scalar_cell_deinit(&first_cell);
    return rc;
}

static int evaluate_space_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    int64_t count = 0;
    size_t argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    bool count_is_null = false;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;
    if (argument_count != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "SPACE");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_padding_count_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &count,
        &count_is_null
    );
    if (rc == MYLITE_OK && !count_is_null) {
        rc = mylite_string_space_value(count, out_result, out_result_length, out_is_null);
    } else {
        *out_is_null = rc == MYLITE_OK;
    }
    return rc;
}

static int evaluate_string_padding_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string padding functions support only string, integer, boolean, NULL, session "
            "scalar, and system variable string arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string padding functions support only string literals",
                "string padding function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_string_padding_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_count,
    bool *out_is_null
) {
    if (out_count == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0;
    *out_is_null = false;

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string padding functions support only integer, boolean, and NULL length/count "
            "literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_integer_value(
        database,
        expression,
        "string padding functions support only integer, boolean, and NULL length/count literals",
        "string padding function length/count literals must fit the signed 64-bit range",
        out_count,
        out_is_null
    );
}

static int string_padding_set_owned_result(
    struct mylite_db *database,
    int rc,
    char *value,
    size_t value_length,
    bool is_null,
    struct session_scalar_cell *out_cell
) {
    if (out_cell == NULL) {
        free(value);
        return MYLITE_MISUSE;
    }
    if (rc == MYLITE_NOMEM) {
        free(value);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (rc != MYLITE_OK) {
        free(value);
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string padding function"
        );
        return rc;
    }
    if (is_null) {
        free(value);
        return MYLITE_OK;
    }
    if (value == NULL) {
        return MYLITE_MISUSE;
    }
    if (strlen(value) != value_length) {
        free(value);
        mylite_execution_set_runtime_error(
            database,
            "invalid NUL byte in string padding function result"
        );
        return MYLITE_ERROR;
    }
    out_cell->owned_text = value;
    out_cell->value = value;
    return MYLITE_OK;
}

static enum planned_string_padding_function_kind string_padding_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LPAD_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_LPAD;
    case MYLITE_SQL_AST_RPAD_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_RPAD;
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_REPEAT;
    case MYLITE_SQL_AST_SPACE_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_SPACE;
    default:
        return PLANNED_STRING_PADDING_FUNCTION_NONE;
    }
}

static enum mylite_string_padding_side string_padding_function_to_side(
    enum planned_string_padding_function_kind function_kind
) {
    return function_kind == PLANNED_STRING_PADDING_FUNCTION_RPAD ? MYLITE_STRING_PADDING_RIGHT
                                                                 : MYLITE_STRING_PADDING_LEFT;
}

static bool is_string_padding_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_padding_function_kind(ast_kind) != PLANNED_STRING_PADDING_FUNCTION_NONE;
}

static int string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_bitmask_function_kind function_kind = PLANNED_STRING_BITMASK_FUNCTION_NONE;
    char *result = NULL;
    size_t result_length = 0U;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_BITMASK_FUNCTION_NONE
                                       : string_bitmask_function_kind(expression->kind);
    switch (function_kind) {
    case PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET:
        rc = evaluate_export_set_string_bitmask_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET:
        rc = evaluate_make_set_string_bitmask_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_BITMASK_FUNCTION_NONE:
        mylite_execution_set_unsupported_error(
            database,
            "string bitmask functions support EXPORT_SET and MAKE_SET"
        );
        return MYLITE_ERROR;
    }

    rc = string_bitmask_set_owned_result(
        database,
        rc,
        result,
        result_length,
        result_is_null,
        out_cell
    );
    return rc;
}

static int evaluate_export_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    static const struct mylite_string_bitmask_slice default_separator = {
        .text = ",",
        .length = 1U,
        .is_null = false,
    };
    const struct mylite_sql_ast_node *arguments = NULL;
    struct string_bitmask_scalar_text_argument on = {0};
    struct string_bitmask_scalar_text_argument off = {0};
    struct string_bitmask_scalar_text_argument separator = {0};
    struct mylite_string_bitmask_slice separator_slice = default_separator;
    int64_t bits_value = 0;
    int64_t count_value = 0;
    bool bits_is_null = false;
    bool count_is_null = false;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;

    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "EXPORT_SET");
        return MYLITE_ERROR;
    }
    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_native_function_parameter_count_error(database, "EXPORT_SET");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < string_bitmask_export_set_min_argument_count ||
        argument_count > string_bitmask_export_set_max_argument_count) {
        mylite_execution_set_native_function_parameter_count_error(database, "EXPORT_SET");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_bitmask_integer_argument(
        database,
        mylite_execution_child_at(arguments, 0U),
        "EXPORT_SET() bitmask supports only signed integer, boolean, and NULL literals",
        "EXPORT_SET() bitmask literals must fit the signed 64-bit range",
        &bits_value,
        &bits_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, 1U),
            &on
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, 2U),
            &off
        );
    }
    if (rc == MYLITE_OK && argument_count >= 4U) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, 3U),
            &separator
        );
        separator_slice = separator.slice;
    }
    if (rc == MYLITE_OK && argument_count == string_bitmask_export_set_max_argument_count) {
        rc = evaluate_string_bitmask_integer_argument(
            database,
            mylite_execution_child_at(arguments, 4U),
            "EXPORT_SET() number_of_bits supports only signed integer, boolean, and NULL literals",
            "EXPORT_SET() number_of_bits literals must fit the signed 64-bit range",
            &count_value,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_export_set_value(
            (uint64_t)bits_value,
            bits_is_null,
            on.slice,
            off.slice,
            separator_slice,
            count_value,
            count_is_null,
            argument_count == string_bitmask_export_set_max_argument_count,
            out_result,
            out_result_length,
            out_is_null
        );
    }

    string_bitmask_scalar_text_argument_deinit(&separator);
    string_bitmask_scalar_text_argument_deinit(&off);
    string_bitmask_scalar_text_argument_deinit(&on);
    return rc;
}

static int evaluate_make_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    struct string_bitmask_scalar_text_argument *values = NULL;
    struct mylite_string_bitmask_slice *slices = NULL;
    int64_t bits_value = 0;
    bool bits_is_null = false;
    size_t argument_count = 0U;
    size_t value_count = 0U;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;

    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "MAKE_SET");
        return MYLITE_ERROR;
    }
    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_native_function_parameter_count_error(database, "MAKE_SET");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "MAKE_SET");
        return MYLITE_ERROR;
    }
    value_count = argument_count - 1U;
    if (value_count > SIZE_MAX / sizeof(*values) || value_count > SIZE_MAX / sizeof(*slices)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (struct string_bitmask_scalar_text_argument *)calloc(value_count, sizeof(*values));
    slices = (struct mylite_string_bitmask_slice *)calloc(value_count, sizeof(*slices));
    if (values == NULL || slices == NULL) {
        free(values);
        free(slices);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = evaluate_string_bitmask_integer_argument(
        database,
        mylite_execution_child_at(arguments, 0U),
        "MAKE_SET() bitmask supports only signed integer, boolean, and NULL literals",
        "MAKE_SET() bitmask literals must fit the signed 64-bit range",
        &bits_value,
        &bits_is_null
    );
    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, value_index + 1U),
            &values[value_index]
        );
        if (rc == MYLITE_OK) {
            slices[value_index] = values[value_index].slice;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_make_set_value(
            (uint64_t)bits_value,
            bits_is_null,
            slices,
            value_count,
            out_result,
            out_result_length,
            out_is_null
        );
    }

    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        string_bitmask_scalar_text_argument_deinit(&values[value_index]);
    }
    free(values);
    free(slices);
    return rc;
}

static int evaluate_string_bitmask_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
) {
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = false;
    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = string_slice_signed_integer_value(
        database,
        expression,
        unsupported_message,
        range_message,
        &value,
        out_is_null
    );
    if (rc == MYLITE_OK) {
        *out_value = value;
    }
    return rc;
}

static int evaluate_string_bitmask_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct string_bitmask_scalar_text_argument *out_argument
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct string_bitmask_scalar_text_argument){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string bitmask functions support only string, integer, boolean, NULL, session "
            "scalar, and system variable string arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string bitmask functions support only string literals",
                "string bitmask function literals do not support NUL bytes",
                &out_argument->owned_text,
                &out_argument->slice.length
            );
            if (rc == MYLITE_OK) {
                out_argument->slice.text = out_argument->owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, &out_argument->cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, &out_argument->cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_argument->cell.value == NULL) {
        out_argument->slice.is_null = true;
        return MYLITE_OK;
    }
    out_argument->slice.text = out_argument->cell.value;
    out_argument->slice.length = strlen(out_argument->cell.value);
    return MYLITE_OK;
}

static int string_bitmask_set_owned_result(
    struct mylite_db *database,
    int rc,
    char *value,
    size_t value_length,
    bool is_null,
    struct session_scalar_cell *out_cell
) {
    if (out_cell == NULL) {
        free(value);
        return MYLITE_MISUSE;
    }
    if (rc == MYLITE_NOMEM) {
        free(value);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (rc != MYLITE_OK) {
        free(value);
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            mylite_execution_set_runtime_error(
                database,
                "failed to evaluate string bitmask function"
            );
        }
        return rc;
    }
    if (is_null) {
        free(value);
        return MYLITE_OK;
    }
    if (value == NULL) {
        return MYLITE_MISUSE;
    }
    if (strlen(value) != value_length) {
        free(value);
        mylite_execution_set_runtime_error(
            database,
            "invalid NUL byte in string bitmask function result"
        );
        return MYLITE_ERROR;
    }
    out_cell->owned_text = value;
    out_cell->value = value;
    out_cell->value_size = value_length;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static void string_bitmask_scalar_text_argument_deinit(
    struct string_bitmask_scalar_text_argument *argument
) {
    if (argument == NULL) {
        return;
    }
    free(argument->owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument->cell);
    *argument = (struct string_bitmask_scalar_text_argument){0};
}

static enum planned_string_bitmask_function_kind string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_EXPORT_SET_FUNCTION:
        return PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET;
    case MYLITE_SQL_AST_MAKE_SET_FUNCTION:
        return PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET;
    default:
        return PLANNED_STRING_BITMASK_FUNCTION_NONE;
    }
}

static bool is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_bitmask_function_kind(ast_kind) != PLANNED_STRING_BITMASK_FUNCTION_NONE;
}

static int string_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_search_function_kind function_kind = PLANNED_STRING_SEARCH_FUNCTION_NONE;
    struct session_scalar_cell first_cell = {0};
    struct session_scalar_cell second_cell = {0};
    char *owned_first_text = NULL;
    char *owned_second_text = NULL;
    const char *needle = NULL;
    const char *haystack = NULL;
    size_t needle_length = 0U;
    size_t haystack_length = 0U;
    int64_t position = 1;
    int64_t result = 0;
    size_t argument_count = 0U;
    bool first_is_null = false;
    bool second_is_null = false;
    bool position_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_SEARCH_FUNCTION_NONE
                                       : string_search_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SEARCH_FUNCTION_NONE ||
        (argument_count != 2U && argument_count != 3U) ||
        (function_kind != PLANNED_STRING_SEARCH_FUNCTION_LOCATE && argument_count != 2U)) {
        mylite_execution_set_unsupported_error(
            database,
            "string search functions support LOCATE(substr,str[,pos]), INSTR(str,substr), "
            "and POSITION(substr IN str)"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_search_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &first_cell,
        &owned_first_text,
        &needle,
        &needle_length,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_search_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &second_cell,
            &owned_second_text,
            &haystack,
            &haystack_length,
            &second_is_null
        );
    }
    if (rc == MYLITE_OK && argument_count == 3U) {
        rc = evaluate_string_search_position_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &position,
            &position_is_null
        );
    }
    if (rc != MYLITE_OK || first_is_null || second_is_null || position_is_null) {
        free(owned_first_text);
        free(owned_second_text);
        mylite_execution_session_scalar_cell_deinit(&first_cell);
        mylite_execution_session_scalar_cell_deinit(&second_cell);
        return rc;
    }

    if (function_kind == PLANNED_STRING_SEARCH_FUNCTION_INSTR) {
        const char *tmp_text = needle;
        size_t tmp_length = needle_length;

        needle = haystack;
        needle_length = haystack_length;
        haystack = tmp_text;
        haystack_length = tmp_length;
    }
    rc = mylite_string_search_locate_ascii_ci_value(
        database,
        needle,
        needle_length,
        haystack,
        haystack_length,
        position,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = format_string_search_result(database, result, out_cell);
    }

    free(owned_first_text);
    free(owned_second_text);
    mylite_execution_session_scalar_cell_deinit(&first_cell);
    mylite_execution_session_scalar_cell_deinit(&second_cell);
    return rc;
}

static int evaluate_string_search_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string search functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string search functions support only string literals",
                "string search function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_string_search_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
) {
    if (out_position == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    *out_is_null = false;

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "LOCATE() position supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_integer_value(
        database,
        expression,
        "LOCATE() position supports only integer, boolean, and NULL literals",
        "LOCATE() position literals must fit the signed 64-bit range",
        out_position,
        out_is_null
    );
}

static int format_string_search_result(
    struct mylite_db *database,
    int64_t result,
    struct session_scalar_cell *out_cell
) {
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, result);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        mylite_execution_set_runtime_error(database, "failed to format string search result");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static enum planned_string_search_function_kind string_search_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LOCATE_FUNCTION:
        return PLANNED_STRING_SEARCH_FUNCTION_LOCATE;
    case MYLITE_SQL_AST_INSTR_FUNCTION:
        return PLANNED_STRING_SEARCH_FUNCTION_INSTR;
    case MYLITE_SQL_AST_POSITION_FUNCTION:
        return PLANNED_STRING_SEARCH_FUNCTION_POSITION;
    default:
        return PLANNED_STRING_SEARCH_FUNCTION_NONE;
    }
}

static bool is_string_search_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_search_function_kind(ast_kind) != PLANNED_STRING_SEARCH_FUNCTION_NONE;
}

static int concat_ws_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    struct mylite_string_concat_argument *concat_arguments = NULL;
    struct session_scalar_cell *cells = NULL;
    char **owned_texts = NULL;
    char *result = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_WS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }

    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*concat_arguments) ||
        argument_count > SIZE_MAX / sizeof(*cells) ||
        argument_count > SIZE_MAX / sizeof(*owned_texts)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    concat_arguments =
        (struct mylite_string_concat_argument *)calloc(argument_count, sizeof(*concat_arguments));
    cells = (struct session_scalar_cell *)calloc(argument_count, sizeof(*cells));
    owned_texts = (char **)calloc(argument_count, sizeof(*owned_texts));
    if (concat_arguments == NULL || cells == NULL || owned_texts == NULL) {
        free(concat_arguments);
        free(cells);
        free((void *)owned_texts);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = evaluate_concat_ws_scalar_argument(
            database,
            argument,
            &cells[argument_index],
            &owned_texts[argument_index],
            &concat_arguments[argument_index].text,
            &concat_arguments[argument_index].text_length,
            &concat_arguments[argument_index].is_null
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        mylite_execution_set_parse_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_concat_ws_value(database, concat_arguments, argument_count, &result);
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    }
    if (rc == MYLITE_OK && result != NULL) {
        out_cell->owned_text = result;
        out_cell->value = out_cell->owned_text;
        result = NULL;
    }

    free(result);
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        free(owned_texts[argument_index]);
        mylite_execution_session_scalar_cell_deinit(&cells[argument_index]);
    }
    free((void *)owned_texts);
    free(cells);
    free(concat_arguments);
    return rc;
}

static int evaluate_concat_ws_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!concat_ws_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "CONCAT_WS() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "CONCAT_WS() supports only string literals",
                "CONCAT_WS() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool concat_ws_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_CONCAT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION) {
        return false;
    }
    return string_length_scalar_argument_is_admitted(expression);
}

static int string_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell cells[3] = {{0}, {0}, {0}};
    char *owned_texts[3] = {NULL, NULL, NULL};
    const char *texts[3] = {NULL, NULL, NULL};
    size_t text_lengths[3] = {0U, 0U, 0U};
    bool is_nulls[3] = {false, false, false};
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REPLACE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REPLACE");
        return MYLITE_ERROR;
    }

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = evaluate_string_replace_scalar_argument(
            database,
            mylite_execution_child_at(expression, argument_index),
            &cells[argument_index],
            &owned_texts[argument_index],
            &texts[argument_index],
            &text_lengths[argument_index],
            &is_nulls[argument_index]
        );
    }
    if (rc == MYLITE_OK && !is_nulls[0] && !is_nulls[1] && !is_nulls[2]) {
        rc = mylite_string_replace_value(
            database,
            texts[0],
            text_lengths[0],
            texts[1],
            text_lengths[1],
            texts[2],
            text_lengths[2],
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK && out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    for (size_t argument_index = 0U; argument_index < 3U; ++argument_index) {
        free(owned_texts[argument_index]);
        mylite_execution_session_scalar_cell_deinit(&cells[argument_index]);
    }
    return rc;
}

static int string_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell replacement_cell = {0};
    char *owned_value = NULL;
    char *owned_replacement = NULL;
    const char *value = NULL;
    const char *replacement = NULL;
    size_t value_length = 0U;
    size_t replacement_length = 0U;
    size_t result_length = 0U;
    int64_t position = 0;
    int64_t length = 0;
    bool value_is_null = false;
    bool position_is_null = false;
    bool length_is_null = false;
    bool replacement_is_null = false;
    bool helper_called = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_INSERT_STRING_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 4U) {
        mylite_execution_set_native_function_parameter_count_error(database, "INSERT");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_insert_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_insert_position_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &position,
            &position_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_insert_length_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &length,
            &length_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_insert_scalar_argument(
            database,
            mylite_execution_child_at(expression, 3U),
            &replacement_cell,
            &owned_replacement,
            &replacement,
            &replacement_length,
            &replacement_is_null
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !position_is_null && !length_is_null &&
        !replacement_is_null) {
        helper_called = true;
        rc = mylite_string_insert_value(
            database,
            &(struct mylite_string_insert_arguments){
                .value = {.text = value, .length = value_length},
                .position = position,
                .length = length,
                .replacement = {.text = replacement, .length = replacement_length},
            },
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (helper_called && rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "invalid UTF-8 value in INSERT()");
    } else if (out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_value);
    free(owned_replacement);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&replacement_cell);
    return rc;
}

static int evaluate_string_replace_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "REPLACE() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "REPLACE() supports only string literals",
                "REPLACE() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool string_replace_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_length_scalar_argument_is_admitted(expression);
}

static int evaluate_string_insert_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "INSERT() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "INSERT() supports only string literals",
                "INSERT() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_string_insert_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
) {
    if (out_position == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    *out_is_null = false;

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "INSERT() position supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_integer_value(
        database,
        expression,
        "INSERT() position supports only integer, boolean, and NULL literals",
        "INSERT() position literals must fit the signed 64-bit range",
        out_position,
        out_is_null
    );
}

static int evaluate_string_insert_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length,
    bool *out_is_null
) {
    if (out_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0;
    *out_is_null = false;

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "INSERT() length supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_integer_value(
        database,
        expression,
        "INSERT() length supports only integer, boolean, and NULL literals",
        "INSERT() length literals must fit the signed 64-bit range",
        out_length,
        out_is_null
    );
}

static int string_reverse_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t result_length = 0U;
    bool is_null = false;
    bool helper_called = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REVERSE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REVERSE");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_reverse_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK && !is_null) {
        helper_called = true;
        rc = mylite_string_reverse_utf8_value(
            database,
            text,
            text_length,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (helper_called && rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "invalid UTF-8 value in REVERSE()");
    } else if (out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_reverse_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "REVERSE() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }
    if (!string_reverse_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "REVERSE() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "REVERSE() supports only string literals",
                "REVERSE() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool string_reverse_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_length_scalar_argument_is_admitted(expression);
}

static int soundex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t result_length = 0U;
    bool is_null = false;
    bool helper_called = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SOUNDEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "SOUNDEX");
        return MYLITE_ERROR;
    }

    rc = evaluate_soundex_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK && !is_null) {
        helper_called = true;
        rc = mylite_string_soundex_value(
            database,
            text,
            text_length,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (helper_called && rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "invalid UTF-8 value in SOUNDEX()");
    } else if (out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_soundex_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "SOUNDEX() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }
    if (!soundex_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "SOUNDEX() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "SOUNDEX() supports only string literals",
                "SOUNDEX() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool soundex_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    return string_length_scalar_argument_is_admitted(expression);
}

static int quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t result_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "QUOTE");
        return MYLITE_ERROR;
    }

    rc = evaluate_quote_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK) {
        rc = mylite_string_quote_sql_value(
            database,
            text,
            text_length,
            is_null,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK && out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_quote_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "QUOTE() supports only string, integer, DECIMAL, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }
    if (!quote_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "QUOTE() supports only string, integer, DECIMAL, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal_with_policy(
                database,
                expression,
                "QUOTE() supports only string literals",
                "QUOTE() string literal decoding failed",
                true,
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL) {
            *out_text = expression->span.text;
            *out_text_length = expression->span.length;
            return MYLITE_OK;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool quote_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        return false;
    }
    if (mylite_execution_is_session_scalar_expression(expression)) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static int substring_index_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell delimiter_cell = {0};
    char *owned_value = NULL;
    char *owned_delimiter = NULL;
    const char *value = NULL;
    const char *delimiter = NULL;
    size_t value_length = 0U;
    size_t delimiter_length = 0U;
    size_t result_length = 0U;
    int64_t count = 0;
    bool value_is_null = false;
    bool delimiter_is_null = false;
    bool count_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "SUBSTRING_INDEX");
        return MYLITE_ERROR;
    }

    rc = evaluate_substring_index_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_substring_index_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &delimiter_cell,
            &owned_delimiter,
            &delimiter,
            &delimiter_length,
            &delimiter_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_substring_index_count_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &count,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !delimiter_is_null && !count_is_null) {
        rc = mylite_string_substring_index_value(
            database,
            value,
            value_length,
            delimiter,
            delimiter_length,
            count,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK && out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_value);
    free(owned_delimiter);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&delimiter_cell);
    return rc;
}

static int evaluate_substring_index_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "SUBSTRING_INDEX() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable value arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "SUBSTRING_INDEX() supports only string literals",
                "SUBSTRING_INDEX() arguments do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_substring_index_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_count,
    bool *out_is_null
) {
    if (out_count == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0;
    *out_is_null = false;

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "SUBSTRING_INDEX() count supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_integer_value(
        database,
        expression,
        "SUBSTRING_INDEX() count supports only integer, boolean, and NULL literals",
        "SUBSTRING_INDEX() count literals must fit the signed 64-bit range",
        out_count,
        out_is_null
    );
}

static int find_in_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell search_cell = {0};
    struct session_scalar_cell list_cell = {0};
    char *owned_search = NULL;
    char *owned_list = NULL;
    const char *search = NULL;
    const char *list = NULL;
    size_t search_length = 0U;
    size_t list_length = 0U;
    int64_t result = 0;
    bool search_is_null = false;
    bool list_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FIND_IN_SET_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "FIND_IN_SET");
        return MYLITE_ERROR;
    }

    rc = evaluate_find_in_set_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &search_cell,
        &owned_search,
        &search,
        &search_length,
        &search_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_find_in_set_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &list_cell,
            &owned_list,
            &list,
            &list_length,
            &list_is_null
        );
    }
    if (rc != MYLITE_OK || search_is_null || list_is_null) {
        free(owned_search);
        free(owned_list);
        mylite_execution_session_scalar_cell_deinit(&search_cell);
        mylite_execution_session_scalar_cell_deinit(&list_cell);
        return rc;
    }

    rc = mylite_string_search_find_in_set_ascii_ci_value(
        database,
        search,
        search_length,
        list,
        list_length,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = format_string_search_result(database, result, out_cell);
    }

    free(owned_search);
    free(owned_list);
    mylite_execution_session_scalar_cell_deinit(&search_cell);
    mylite_execution_session_scalar_cell_deinit(&list_cell);
    return rc;
}

static int strcmp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell left_cell = {0};
    struct session_scalar_cell right_cell = {0};
    char *owned_left = NULL;
    char *owned_right = NULL;
    const char *left = NULL;
    const char *right = NULL;
    size_t left_length = 0U;
    size_t right_length = 0U;
    int64_t result = 0;
    bool left_is_null = false;
    bool right_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_STRCMP_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "STRCMP");
        return MYLITE_ERROR;
    }

    rc = evaluate_strcmp_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &left_cell,
        &owned_left,
        &left,
        &left_length,
        &left_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_strcmp_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &right_cell,
            &owned_right,
            &right,
            &right_length,
            &right_is_null
        );
    }
    if (rc != MYLITE_OK || left_is_null || right_is_null) {
        free(owned_left);
        free(owned_right);
        mylite_execution_session_scalar_cell_deinit(&left_cell);
        mylite_execution_session_scalar_cell_deinit(&right_cell);
        return rc;
    }

    rc = mylite_string_search_strcmp_ascii_ci_value(
        database,
        left,
        left_length,
        right,
        right_length,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = format_string_search_result(database, result, out_cell);
    }

    free(owned_left);
    free(owned_right);
    mylite_execution_session_scalar_cell_deinit(&left_cell);
    mylite_execution_session_scalar_cell_deinit(&right_cell);
    return rc;
}

static int regexp_like_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct regexp_like_text_argument_messages value_messages = {
        .unsupported = "REGEXP_LIKE() supports only string, integer, boolean, NULL, session "
                       "scalar, and system "
                       "variable value arguments",
        .string_unsupported = "REGEXP_LIKE() supports only string value literals",
        .embedded_nul = "REGEXP_LIKE() value arguments do not support NUL bytes",
        .non_ascii = "REGEXP_LIKE() arguments support only ASCII text",
    };
    static const struct regexp_like_text_argument_messages pattern_messages = {
        .unsupported =
            "REGEXP_LIKE() supports only string, integer, boolean, and NULL pattern arguments",
        .string_unsupported = "REGEXP_LIKE() supports only string pattern literals",
        .embedded_nul = "REGEXP_LIKE() pattern arguments do not support NUL bytes",
        .non_ascii = "REGEXP_LIKE() arguments support only ASCII text",
    };
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell pattern_cell = {0};
    char *owned_value = NULL;
    char *owned_pattern = NULL;
    const char *value = NULL;
    const char *pattern = NULL;
    size_t value_length = 0U;
    size_t pattern_length = 0U;
    bool value_is_null = false;
    bool pattern_is_null = false;
    bool match_type_is_null = false;
    bool case_sensitive = false;
    bool matches = false;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "REGEXP_LIKE");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 2U && child_count != 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REGEXP_LIKE");
        return MYLITE_ERROR;
    }

    if (child_count == 3U) {
        rc = evaluate_regexp_like_match_type_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &match_type_is_null,
            &case_sensitive
        );
        if (rc != MYLITE_OK || match_type_is_null) {
            return rc;
        }
    }

    rc = evaluate_regexp_like_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        true,
        &value_messages,
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_regexp_like_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            false,
            &pattern_messages,
            &pattern_cell,
            &owned_pattern,
            &pattern,
            &pattern_length,
            &pattern_is_null
        );
    }
    if (rc == MYLITE_OK && !pattern_is_null) {
        rc = validate_regexp_like_pattern(
            database,
            pattern,
            pattern_length,
            case_sensitive,
            "REGEXP_LIKE() patterns support only MyLite's baseline ASCII regular expression subset"
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !pattern_is_null) {
        rc = match_regexp_like_value(
            database,
            value,
            value_length,
            pattern,
            pattern_length,
            case_sensitive,
            &matches
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !pattern_is_null) {
        uint64_t match_value = 0U;

        if (matches) {
            match_value = 1U;
        }
        rc = mylite_execution_format_session_scalar_uint64_value(database, match_value, out_cell);
    }

    free(owned_value);
    free(owned_pattern);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&pattern_cell);
    return rc;
}

static int regexp_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct regexp_like_text_argument_messages value_messages = {
        .unsupported = "REGEXP string functions support only string, integer, boolean, NULL, "
                       "session scalar, and system variable value arguments",
        .string_unsupported = "REGEXP string functions support only string value literals",
        .embedded_nul = "REGEXP string function value arguments do not support NUL bytes",
        .non_ascii = "REGEXP string function arguments support only ASCII text",
    };
    static const struct regexp_like_text_argument_messages pattern_messages = {
        .unsupported =
            "REGEXP string functions support only string, integer, boolean, and NULL patterns",
        .string_unsupported = "REGEXP string functions support only string pattern literals",
        .embedded_nul = "REGEXP string function pattern arguments do not support NUL bytes",
        .non_ascii = "REGEXP string function arguments support only ASCII text",
    };
    static const struct regexp_like_text_argument_messages replacement_messages = {
        .unsupported = "REGEXP_REPLACE() supports only string, integer, boolean, NULL, session "
                       "scalar, and system variable replacement arguments",
        .string_unsupported = "REGEXP_REPLACE() supports only string replacement literals",
        .embedded_nul = "REGEXP_REPLACE() replacement arguments do not support NUL bytes",
        .non_ascii = "REGEXP_REPLACE() arguments support only ASCII text",
    };
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell pattern_cell = {0};
    struct session_scalar_cell replacement_cell = {0};
    char *owned_value = NULL;
    char *owned_pattern = NULL;
    char *owned_replacement = NULL;
    const char *value = NULL;
    const char *pattern = NULL;
    const char *replacement = NULL;
    size_t value_length = 0U;
    size_t pattern_length = 0U;
    size_t replacement_length = 0U;
    bool value_is_null = false;
    bool pattern_is_null = false;
    bool replacement_is_null = false;
    struct regexp_string_function_call_shape shape = {
        .kind = PLANNED_REGEXP_STRING_FUNCTION_NONE,
        .child_count = 0U,
    };
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }
    shape.kind = regexp_string_function_kind(expression->kind);
    shape.child_count = regexp_string_function_argument_count(expression);
    rc = validate_regexp_string_function_argument_count(database, &shape);
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }

    rc = evaluate_regexp_like_text_argument(
        database,
        regexp_string_function_argument_at(expression, 0U),
        true,
        &value_messages,
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_regexp_like_text_argument(
            database,
            regexp_string_function_argument_at(expression, 1U),
            false,
            &pattern_messages,
            &pattern_cell,
            &owned_pattern,
            &pattern,
            &pattern_length,
            &pattern_is_null
        );
    }
    if (rc == MYLITE_OK && shape.kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        rc = evaluate_regexp_like_text_argument(
            database,
            regexp_string_function_argument_at(expression, 2U),
            true,
            &replacement_messages,
            &replacement_cell,
            &owned_replacement,
            &replacement,
            &replacement_length,
            &replacement_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = regexp_string_result_value(
            database,
            shape.kind,
            value,
            value_length,
            value_is_null,
            pattern,
            pattern_length,
            pattern_is_null,
            replacement,
            replacement_length,
            replacement_is_null,
            out_cell
        );
    }

    free(owned_value);
    free(owned_pattern);
    free(owned_replacement);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&pattern_cell);
    mylite_execution_session_scalar_cell_deinit(&replacement_cell);
    return rc;
}

static int validate_regexp_string_function_argument_count(
    struct mylite_db *database,
    const struct regexp_string_function_call_shape *shape
) {
    if (shape->kind == PLANNED_REGEXP_STRING_FUNCTION_INSTR ||
        shape->kind == PLANNED_REGEXP_STRING_FUNCTION_SUBSTR) {
        if (shape->child_count == 2U) {
            return MYLITE_OK;
        }
        if (shape->child_count > 2U) {
            mylite_execution_set_unsupported_error(
                database,
                "REGEXP_INSTR() and REGEXP_SUBSTR() optional arguments are not supported"
            );
        } else {
            mylite_execution_set_native_function_parameter_count_error(
                database,
                regexp_string_function_name(shape->kind)
            );
        }
        return MYLITE_ERROR;
    }
    if (shape->kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        if (shape->child_count == 3U) {
            return MYLITE_OK;
        }
        if (shape->child_count > 3U) {
            mylite_execution_set_unsupported_error(
                database,
                "REGEXP_REPLACE() optional arguments are not supported"
            );
        } else {
            mylite_execution_set_native_function_parameter_count_error(database, "REGEXP_REPLACE");
        }
        return MYLITE_ERROR;
    }

    mylite_execution_set_parse_error(database);
    return MYLITE_ERROR;
}

static enum planned_regexp_string_function_kind regexp_string_function_kind(
    enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
        return PLANNED_REGEXP_STRING_FUNCTION_INSTR;
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
        return PLANNED_REGEXP_STRING_FUNCTION_SUBSTR;
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
        return PLANNED_REGEXP_STRING_FUNCTION_REPLACE;
    default:
        return PLANNED_REGEXP_STRING_FUNCTION_NONE;
    }
}

static const char *regexp_string_function_name(enum planned_regexp_string_function_kind kind) {
    switch (kind) {
    case PLANNED_REGEXP_STRING_FUNCTION_INSTR:
        return "REGEXP_INSTR";
    case PLANNED_REGEXP_STRING_FUNCTION_SUBSTR:
        return "REGEXP_SUBSTR";
    case PLANNED_REGEXP_STRING_FUNCTION_REPLACE:
        return "REGEXP_REPLACE";
    case PLANNED_REGEXP_STRING_FUNCTION_NONE:
        break;
    }
    return "REGEXP";
}

static const char *regexp_string_function_argument_count_error_name(
    enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_INSTR";
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_SUBSTR";
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
        return "REGEXP_REPLACE";
    default:
        return NULL;
    }
}

static const struct mylite_sql_ast_node *regexp_string_function_arguments(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return NULL;
    }
    if (mylite_sql_ast_node_child_count(expression) == 1U) {
        arguments = mylite_execution_child_at(expression, 0U);
        if (arguments != NULL && arguments->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
            return arguments;
        }
    }
    return expression;
}

static size_t regexp_string_function_argument_count(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = regexp_string_function_arguments(expression);

    if (arguments == NULL) {
        return 0U;
    }
    return mylite_sql_ast_node_child_count(arguments);
}

static const struct mylite_sql_ast_node *regexp_string_function_argument_at(
    const struct mylite_sql_ast_node *expression,
    size_t index
) {
    return mylite_execution_child_at(regexp_string_function_arguments(expression), index);
}

static int evaluate_find_in_set_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "FIND_IN_SET() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "FIND_IN_SET() supports only string literals",
                "FIND_IN_SET() arguments do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_strcmp_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "STRCMP() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "STRCMP() supports only string literals",
                "STRCMP() arguments do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_regexp_like_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool allow_session_scalar,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    int rc = MYLITE_OK;

    if (messages == NULL || inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }
    if (!allow_session_scalar && !regexp_like_literal_or_unary_expression_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        rc = evaluate_regexp_like_literal_text_argument(
            database,
            expression,
            messages,
            inout_cell,
            out_owned_text,
            out_text,
            out_text_length,
            out_is_null
        );
    } else if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
        if (rc == MYLITE_OK) {
            rc = regexp_like_cell_text_result(inout_cell, out_text, out_text_length, out_is_null);
        }
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
        if (rc == MYLITE_OK) {
            rc = regexp_like_cell_text_result(inout_cell, out_text, out_text_length, out_is_null);
        }
    }
    if (rc != MYLITE_OK || *out_is_null) {
        return rc;
    }
    if (!mylite_execution_text_value_is_supported_string_key(*out_text, *out_text_length)) {
        mylite_execution_set_unsupported_error(database, messages->non_ascii);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool regexp_like_literal_or_unary_expression_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return true;
    }
    return false;
}

static int evaluate_regexp_like_literal_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(expression);
    int rc = MYLITE_OK;

    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = mylite_execution_decode_sql_string_literal(
            database,
            expression,
            messages->string_unsupported,
            messages->embedded_nul,
            out_owned_text,
            out_text_length
        );
        if (rc == MYLITE_OK) {
            *out_text = *out_owned_text;
        }
        return rc;
    }

    rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    if (rc == MYLITE_OK) {
        rc = regexp_like_cell_text_result(inout_cell, out_text, out_text_length, out_is_null);
    }
    return rc;
}

static int regexp_like_cell_text_result(
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_regexp_like_match_type_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_null,
    bool *out_case_sensitive
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_is_null == NULL || out_case_sensitive == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null = false;
    *out_case_sensitive = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() match_type supports only string and NULL literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() match_type supports only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        "REGEXP_LIKE() match_type supports only string literals",
        "REGEXP_LIKE() match_type literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        rc = regexp_like_case_sensitive_from_match_type(
            database,
            text,
            text_length,
            out_case_sensitive
        );
    }

    free(text);
    return rc;
}

static int regexp_like_case_sensitive_from_match_type(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool *out_case_sensitive
) {
    if (text == NULL || out_case_sensitive == NULL) {
        return MYLITE_MISUSE;
    }
    *out_case_sensitive = false;
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] == 'c') {
            *out_case_sensitive = true;
        } else if (text[index] == 'i') {
            *out_case_sensitive = false;
        } else {
            mylite_execution_set_unsupported_error(
                database,
                "REGEXP_LIKE() match_type supports only c and i flags"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int validate_regexp_like_pattern(
    struct mylite_db *database,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    const char *unsupported_message
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status status = MYLITE_REGEXP_COMPILE_OK;

    if (!mylite_execution_text_value_is_supported_string_key(pattern, pattern_length)) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() pattern arguments support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    if (case_sensitive) {
        status = mylite_regexp_compile_ascii_cs(pattern, pattern_length, &program);
    } else {
        status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    }
    mylite_regexp_program_free(program);
    if (status != MYLITE_REGEXP_COMPILE_OK) {
        return set_regexp_like_compile_error(database, status, unsupported_message);
    }
    return MYLITE_OK;
}

static int set_regexp_like_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status,
    const char *unsupported_message
) {
    if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
        mylite_execution_set_regexp_error(
            database,
            "The regular expression contains an unclosed bracket expression."
        );
        return MYLITE_ERROR;
    }
    if (status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
        mylite_execution_set_regexp_character_range_error(
            database,
            "The regular expression contains an invalid character range."
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int match_regexp_like_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    bool *out_matches
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (case_sensitive) {
        compile_status = mylite_regexp_compile_ascii_cs(pattern, pattern_length, &program);
    } else {
        compile_status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    }
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        mylite_regexp_program_free(program);
        return set_regexp_like_compile_error(
            database,
            compile_status,
            "REGEXP_LIKE() patterns support only MyLite's baseline ASCII regular expression subset"
        );
    }
    if (case_sensitive) {
        match_status =
            mylite_regexp_program_match_ascii_cs(program, value, value_length, out_matches);
    } else {
        match_status =
            mylite_regexp_program_match_ascii_ci(program, value, value_length, out_matches);
    }
    mylite_regexp_program_free(program);
    if (match_status == MYLITE_REGEXP_MATCH_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (match_status == MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() value arguments support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() value arguments are too large"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int regexp_string_find_match(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_match *out_match
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (out_match == NULL) {
        return MYLITE_MISUSE;
    }
    *out_match = (struct mylite_regexp_match){
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    compile_status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        mylite_regexp_program_free(program);
        return regexp_string_compile_error(database, compile_status);
    }

    match_status = mylite_regexp_program_find_ascii_ci(program, value, value_length, 0U, out_match);
    mylite_regexp_program_free(program);
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        return regexp_string_match_error(database, match_status);
    }
    return MYLITE_OK;
}

static int regexp_string_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status
) {
    if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
        mylite_execution_set_regexp_error(
            database,
            "The regular expression contains an unclosed bracket expression."
        );
        return MYLITE_ERROR;
    }
    if (status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
        mylite_execution_set_regexp_character_range_error(
            database,
            "The regular expression contains an invalid character range."
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(
        database,
        "REGEXP string function patterns support only MyLite's baseline ASCII regular expression "
        "subset"
    );
    return MYLITE_ERROR;
}

static int regexp_string_match_error(
    struct mylite_db *database,
    enum mylite_regexp_match_status status
) {
    if (status == MYLITE_REGEXP_MATCH_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (status == MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP string function values support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, "REGEXP string function values are too large");
    return MYLITE_ERROR;
}

static int regexp_string_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    bool value_is_null,
    const char *pattern,
    size_t pattern_length,
    bool pattern_is_null,
    const char *replacement,
    size_t replacement_length,
    bool replacement_is_null,
    struct session_scalar_cell *out_cell
) {
    bool any_argument_is_null = false;

    if (value_is_null || pattern_is_null || replacement_is_null) {
        any_argument_is_null = true;
    }

    if (any_argument_is_null) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    if (pattern_length == 0U) {
        mylite_execution_set_regexp_illegal_argument_error(database);
        return MYLITE_ERROR;
    }
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_INSTR ||
        kind == PLANNED_REGEXP_STRING_FUNCTION_SUBSTR) {
        return regexp_instr_or_substr_result_value(
            database,
            kind,
            value,
            value_length,
            pattern,
            pattern_length,
            out_cell
        );
    }
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        return regexp_replace_result_value(
            database,
            value,
            value_length,
            pattern,
            pattern_length,
            replacement,
            replacement_length,
            out_cell
        );
    }
    return MYLITE_MISUSE;
}

static int regexp_instr_or_substr_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct session_scalar_cell *out_cell
) {
    struct mylite_regexp_match match = {
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    int rc = MYLITE_OK;

    rc = regexp_string_find_match(database, value, value_length, pattern, pattern_length, &match);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_INSTR) {
        uint64_t position = 0U;

        if (match.matched) {
            position = (uint64_t)match.start + 1U;
        }
        return mylite_execution_format_session_scalar_uint64_value(database, position, out_cell);
    }
    if (!match.matched) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    return regexp_substr_result_value(database, value, &match, out_cell);
}

static int regexp_substr_result_value(
    struct mylite_db *database,
    const char *value,
    const struct mylite_regexp_match *match,
    struct session_scalar_cell *out_cell
) {
    size_t length = 0U;

    if (value == NULL || match == NULL || out_cell == NULL || match->end < match->start) {
        return MYLITE_MISUSE;
    }
    length = match->end - match->start;
    out_cell->owned_text = (char *)malloc(length + 1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length != 0U) {
        memcpy(out_cell->owned_text, value + match->start, length);
    }
    out_cell->owned_text[length] = '\0';
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = length;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int regexp_replace_result_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    const char *replacement,
    size_t replacement_length,
    struct session_scalar_cell *out_cell
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;
    size_t append_offset = 0U;
    size_t search_offset = 0U;
    int rc = MYLITE_OK;

    compile_status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        mylite_regexp_program_free(program);
        return regexp_string_compile_error(database, compile_status);
    }
    if (value_length == 0U) {
        mylite_regexp_program_free(program);
        return regexp_replace_append(database, out_cell, "", 0U);
    }

    while (search_offset <= value_length) {
        struct mylite_regexp_match match = {
            .matched = false,
            .start = 0U,
            .end = 0U,
        };

        match_status = mylite_regexp_program_find_ascii_ci(
            program,
            value,
            value_length,
            search_offset,
            &match
        );
        if (match_status != MYLITE_REGEXP_MATCH_OK) {
            mylite_regexp_program_free(program);
            return regexp_string_match_error(database, match_status);
        }
        if (!match.matched) {
            break;
        }
        rc = regexp_replace_append(
            database,
            out_cell,
            value + append_offset,
            match.start - append_offset
        );
        if (rc == MYLITE_OK) {
            rc = regexp_replace_append(database, out_cell, replacement, replacement_length);
        }
        if (rc != MYLITE_OK) {
            mylite_regexp_program_free(program);
            return rc;
        }

        append_offset = match.end;
        search_offset = match.end;
        if (match.start == match.end) {
            if (search_offset >= value_length) {
                break;
            }
            rc = regexp_replace_append(database, out_cell, value + search_offset, 1U);
            if (rc != MYLITE_OK) {
                mylite_regexp_program_free(program);
                return rc;
            }
            ++search_offset;
            append_offset = search_offset;
        }
    }

    mylite_regexp_program_free(program);
    rc = regexp_replace_append(
        database,
        out_cell,
        value + append_offset,
        value_length - append_offset
    );
    if (rc == MYLITE_OK && out_cell->owned_text == NULL) {
        rc = regexp_replace_append(database, out_cell, "", 0U);
    }
    return rc;
}

static int regexp_replace_append(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell,
    const char *text,
    size_t text_length
) {
    char *buffer = NULL;
    size_t current_length = 0U;

    if (out_cell == NULL || (text == NULL && text_length != 0U)) {
        return MYLITE_MISUSE;
    }
    if (out_cell->has_value_size) {
        current_length = out_cell->value_size;
    }
    if (current_length > SIZE_MAX - text_length - 1U) {
        mylite_execution_set_unsupported_error(database, "REGEXP_REPLACE() result is too large");
        return MYLITE_ERROR;
    }
    buffer = (char *)realloc(out_cell->owned_text, current_length + text_length + 1U);
    if (buffer == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_cell->owned_text = buffer;
    if (text_length != 0U) {
        memcpy(out_cell->owned_text + current_length, text, text_length);
    }
    out_cell->value_size = current_length + text_length;
    out_cell->owned_text[out_cell->value_size] = '\0';
    out_cell->has_value_size = true;
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static int charset_collation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_charset_collation_function_kind function_kind =
        PLANNED_CHARSET_COLLATION_FUNCTION_NONE;
    const char *result = NULL;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_CHARSET_COLLATION_FUNCTION_NONE
                                       : charset_collation_function_kind(expression->kind);
    if (function_kind == PLANNED_CHARSET_COLLATION_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "CHARSET(), COLLATION(), and COERCIBILITY() support exactly one argument"
        );
        return MYLITE_ERROR;
    }

    rc = charset_collation_scalar_result(
        database,
        function_kind,
        mylite_execution_child_at(expression, 0U),
        &result
    );
    if (rc == MYLITE_OK) {
        out_cell->value = result;
    }
    return rc;
}

static int charset_collation_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const char *charset = "binary";
    const char *collation = "binary";

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (function_kind == PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY) {
        return coercibility_scalar_result(database, expression, out_result);
    }

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "CHARSET(), COLLATION(), and COERCIBILITY() support only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return set_unknown_column_for_reference(database, expression);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING) {
            charset = database->session.character_set_connection;
            collation = database->session.collation_connection;
        }
        return charset_collation_select_result(function_kind, charset, collation, out_result);
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return charset_collation_select_result(function_kind, charset, collation, out_result);
        }
        break;
    }
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        return charset_collation_select_result(
            function_kind,
            mylite_execution_national_character_set_name(),
            mylite_execution_national_collation_name(),
            out_result
        );
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION: {
        int rc = charset_collation_rand_result(database, expression, &charset, &collation);

        if (rc != MYLITE_OK) {
            return rc;
        }
        return charset_collation_select_result(function_kind, charset, collation, out_result);
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return charset_collation_select_result(function_kind, charset, collation, out_result);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return charset_collation_convert_using_charset_result(
            database,
            function_kind,
            expression,
            out_result
        );
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
        return charset_collation_collate_expression_result(
            database,
            function_kind,
            expression,
            out_result
        );
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
        return charset_collation_concat_scalar_result(
            database,
            function_kind,
            expression,
            out_result
        );
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "CHARSET(), COLLATION(), and COERCIBILITY() support only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int coercibility_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_CONCAT_FUNCTION) {
        return coercibility_concat_scalar_result(database, expression, out_result);
    }
    return coercibility_non_concat_scalar_result(database, expression, out_result);
}

static int coercibility_non_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return coercibility_literal_result(database, expression, out_result);
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            *out_result = "5";
            return MYLITE_OK;
        }
        break;
    }
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        *out_result = "3";
        return MYLITE_OK;
    case MYLITE_SQL_AST_UUID_FUNCTION:
        *out_result = "4";
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION: {
        const char *charset = NULL;
        const char *collation = NULL;
        int rc = charset_collation_rand_result(database, expression, &charset, &collation);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_result = "5";
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return coercibility_binary_wrapper_result(database, expression, out_result);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION: {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_convert_using_charset_value(database, expression, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_result = "2";
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_COLLATE_EXPRESSION: {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_collate_expression_value(database, expression, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_result = "0";
        return MYLITE_OK;
    }
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "COERCIBILITY() supports only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int coercibility_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const struct mylite_sql_ast_node *arguments = mylite_execution_child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    const char *result = "6";
    bool has_non_null_argument = false;
    const struct mylite_execution_catalog_scalar_collation *explicit_collation = NULL;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    for (size_t argument_index = 0U; argument_index < argument_count && argument != NULL;
         ++argument_index) {
        int rc = apply_coercibility_concat_argument(
            database,
            argument,
            &result,
            &has_non_null_argument,
            &explicit_collation
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        argument = argument->next_sibling;
    }
    if (argument != NULL) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }

    *out_result = result;
    return MYLITE_OK;
}

static int apply_coercibility_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char **inout_result,
    bool *inout_has_non_null_argument,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation
) {
    const struct mylite_sql_ast_node *unwrapped_argument =
        mylite_execution_unwrap_parenthesized_expression(argument);
    const char *argument_result = NULL;
    int rc = MYLITE_OK;

    if (inout_result == NULL || inout_has_non_null_argument == NULL ||
        inout_explicit_collation == NULL) {
        return MYLITE_MISUSE;
    }

    if (unwrapped_argument != NULL &&
        (unwrapped_argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
         unwrapped_argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }

    rc = coercibility_non_concat_scalar_result(database, argument, &argument_result);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (unwrapped_argument != NULL &&
        unwrapped_argument->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        const struct mylite_execution_catalog_scalar_collation *argument_explicit_collation = NULL;

        rc = scalar_collation_info_for_expression(
            database,
            unwrapped_argument,
            &argument_explicit_collation
        );
        if (rc == MYLITE_OK) {
            rc = merge_concat_explicit_collation(
                database,
                inout_explicit_collation,
                argument_explicit_collation
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (argument_result == NULL || strcmp(argument_result, "6") == 0) {
        return MYLITE_OK;
    }
    argument_result = coercibility_concat_argument_result(argument_result);
    if (!*inout_has_non_null_argument || argument_result[0] < (*inout_result)[0]) {
        *inout_result = argument_result;
    }
    *inout_has_non_null_argument = true;
    return MYLITE_OK;
}

static int coercibility_binary_wrapper_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const struct mylite_sql_ast_node *argument = NULL;
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return set_unknown_column_for_reference(database, argument);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        rc = coercibility_validate_binary_wrapper_argument(database, argument);
        break;
    default:
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_result = "2";
    return MYLITE_OK;
}

static int coercibility_validate_binary_wrapper_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument
) {
    const char *ignored_result = NULL;

    argument = mylite_execution_unwrap_parenthesized_expression(argument);
    if (argument == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL) {
        return coercibility_literal_result(database, argument, &ignored_result);
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(argument, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_VERSION_FUNCTION) {
        return MYLITE_OK;
    }
    if (argument->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        const char *charset = NULL;
        const char *collation = NULL;

        return charset_collation_rand_result(database, argument, &charset, &collation);
    }
    if (argument->kind == MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION) {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_convert_using_charset_value(database, argument, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        return rc;
    }

    mylite_execution_set_unsupported_error(
        database,
        "COERCIBILITY() supports only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int coercibility_literal_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    switch (mylite_sql_ast_node_literal_kind(expression)) {
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
        *out_result = "4";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_result = "5";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_result = "6";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NONE:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        break;
    }
    mylite_execution_set_unsupported_error(
        database,
        "COERCIBILITY() supports only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static const char *coercibility_concat_argument_result(const char *argument_result) {
    if (argument_result != NULL && strcmp(argument_result, "5") == 0) {
        return "4";
    }
    return argument_result;
}

static bool coercibility_binary_wrapper_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_column_reference
) {
    const struct mylite_sql_ast_node *argument = NULL;

    if (out_column_reference == NULL) {
        return false;
    }
    *out_column_reference = NULL;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        break;
    default:
        return false;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (argument == NULL || (argument->kind != MYLITE_SQL_AST_IDENTIFIER &&
                             argument->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return false;
    }
    *out_column_reference = argument;
    return true;
}

static int set_unknown_column_for_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_set_unknown_column_reference_error(database, expression);
}

static int charset_collation_concat_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const struct mylite_sql_ast_node *arguments = mylite_execution_child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    bool has_binary_argument = false;
    const struct mylite_execution_catalog_scalar_collation *explicit_collation = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    for (size_t argument_index = 0U; argument_index < argument_count && argument != NULL;
         ++argument_index) {
        bool is_binary_argument = false;
        const struct mylite_execution_catalog_scalar_collation *argument_explicit_collation = NULL;
        int rc = MYLITE_OK;

        rc = validate_charset_collation_concat_argument(
            database,
            argument,
            &is_binary_argument,
            &argument_explicit_collation
        );
        if (rc == MYLITE_OK) {
            rc = merge_concat_explicit_collation(
                database,
                &explicit_collation,
                argument_explicit_collation
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (is_binary_argument) {
            has_binary_argument = true;
        }
        argument = argument->next_sibling;
    }
    if (argument != NULL) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }

    if (explicit_collation != NULL) {
        return charset_collation_select_result(
            function_kind,
            explicit_collation->charset,
            explicit_collation->collation,
            out_result
        );
    }
    if (has_binary_argument) {
        return charset_collation_select_result(function_kind, "binary", "binary", out_result);
    }
    return charset_collation_select_result(
        function_kind,
        database->session.character_set_connection,
        database->session.collation_connection,
        out_result
    );
}

static int validate_charset_collation_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_binary_string_argument,
    const struct mylite_execution_catalog_scalar_collation **out_explicit_collation
) {
    const struct mylite_sql_ast_node *literal = NULL;

    if (out_is_binary_string_argument == NULL || out_explicit_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_binary_string_argument = false;
    *out_explicit_collation = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "CHARSET() and COLLATION() support only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        *out_is_binary_string_argument =
            mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_HEX;
        return MYLITE_OK;
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        break;
    }
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION: {
        const char *charset = NULL;
        const char *collation = NULL;

        return charset_collation_rand_result(database, expression, &charset, &collation);
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        *out_is_binary_string_argument = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION: {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_convert_using_charset_value(database, expression, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        return rc;
    }
    case MYLITE_SQL_AST_COLLATE_EXPRESSION: {
        const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
        struct session_scalar_cell cell = {0};
        int rc = scalar_collation_info_for_expression(database, expression, &collation_info);

        if (rc == MYLITE_OK) {
            rc = mylite_execution_collate_expression_value(database, expression, &cell);
        }
        mylite_execution_session_scalar_cell_deinit(&cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_explicit_collation = collation_info;
        return MYLITE_OK;
    }
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "CHARSET() and COLLATION() support only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int merge_concat_explicit_collation(
    struct mylite_db *database,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation,
    const struct mylite_execution_catalog_scalar_collation *argument_collation
) {
    if (inout_explicit_collation == NULL) {
        return MYLITE_MISUSE;
    }
    if (argument_collation == NULL) {
        return MYLITE_OK;
    }
    if (*inout_explicit_collation == NULL) {
        *inout_explicit_collation = argument_collation;
        return MYLITE_OK;
    }
    if (mylite_execution_text_equals_ascii_case_insensitive(
            (*inout_explicit_collation)->collation,
            argument_collation->collation
        )) {
        return MYLITE_OK;
    }

    mylite_execution_set_illegal_mix_of_collations_error(
        database,
        (*inout_explicit_collation)->collation,
        argument_collation->collation,
        "concat"
    );
    return MYLITE_ERROR;
}

static int charset_collation_convert_using_charset_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    struct session_scalar_cell cell = {0};
    struct scalar_convert_charset_info info = {0};
    int rc =
        mylite_execution_scalar_convert_charset_info_for_expression(database, expression, &info);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_convert_using_charset_value(database, expression, &cell);
    }
    mylite_execution_session_scalar_cell_deinit(&cell);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return charset_collation_select_result(function_kind, info.charset, info.collation, out_result);
}

static int charset_collation_collate_expression_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const char *charset = NULL;
    const char *collation = NULL;
    struct session_scalar_cell cell = {0};
    int rc =
        scalar_expression_charset_collation_metadata(database, expression, &charset, &collation);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_collate_expression_value(database, expression, &cell);
    }
    mylite_execution_session_scalar_cell_deinit(&cell);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return charset_collation_select_result(function_kind, charset, collation, out_result);
}

static int charset_collation_rand_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    if (out_charset == NULL || out_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_charset = "binary";
    *out_collation = "binary";
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "RAND() supports only RAND() and RAND(seed)"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) == 0U) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) == 1U) {
        uint32_t seed = 0U;

        return mylite_execution_rand_seed_value(
            database,
            mylite_execution_child_at(expression, 0U),
            &seed
        );
    }

    mylite_execution_set_unsupported_error(database, "RAND() supports only RAND() and RAND(seed)");
    return MYLITE_ERROR;
}

static int scalar_expression_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
    int rc = MYLITE_OK;

    if (out_charset == NULL || out_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_charset = "binary";
    *out_collation = "binary";

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COLLATE supports only scalar values with known character set metadata"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        return scalar_expression_base_charset_collation_metadata(
            database,
            expression,
            out_charset,
            out_collation
        );
    }

    rc = scalar_collation_info_for_expression(database, expression, &collation_info);
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_charset = collation_info->charset;
    *out_collation = collation_info->collation;
    return MYLITE_OK;
}

static int scalar_collation_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_execution_catalog_scalar_collation **out_info
) {
    const struct mylite_sql_ast_node *collation = NULL;
    const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
    const char *charset = NULL;
    const char *current_collation = NULL;
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "COLLATE supports only scalar postfix collation"
        );
        return MYLITE_ERROR;
    }

    rc = scalar_expression_base_charset_collation_metadata(
        database,
        mylite_execution_child_at(expression, 0U),
        &charset,
        &current_collation
    );
    (void)current_collation;
    if (rc != MYLITE_OK) {
        return rc;
    }

    collation = mylite_execution_child_at(expression, 1U);
    rc = mylite_execution_copy_table_option_name_text(
        database,
        collation,
        collation_name,
        sizeof(collation_name),
        "collation",
        "COLLATE names do not support NUL bytes"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    collation_info = scalar_collation_info_by_name(collation_name);
    if (collation_info == NULL) {
        mylite_execution_set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (!mylite_execution_text_equals_ascii_case_insensitive(charset, collation_info->charset)) {
        mylite_execution_set_collation_not_valid_for_charset_error(
            database,
            collation_info->collation,
            charset
        );
        return MYLITE_ERROR;
    }

    *out_info = collation_info;
    return MYLITE_OK;
}

static int scalar_expression_base_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    if (out_charset == NULL || out_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_charset = "binary";
    *out_collation = "binary";

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COLLATE supports only scalar values with known character set metadata"
        );
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        switch (mylite_sql_ast_node_literal_kind(expression)) {
        case MYLITE_SQL_AST_LITERAL_STRING:
        case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        case MYLITE_SQL_AST_LITERAL_INTEGER:
        case MYLITE_SQL_AST_LITERAL_TRUE:
        case MYLITE_SQL_AST_LITERAL_FALSE:
            *out_charset = database->session.character_set_connection;
            *out_collation = database->session.collation_connection;
            break;
        case MYLITE_SQL_AST_LITERAL_NULL:
        case MYLITE_SQL_AST_LITERAL_HEX:
        case MYLITE_SQL_AST_LITERAL_BIT:
            *out_charset = "binary";
            *out_collation = "binary";
            break;
        case MYLITE_SQL_AST_LITERAL_NONE:
        case MYLITE_SQL_AST_LITERAL_DECIMAL:
        case MYLITE_SQL_AST_LITERAL_FLOAT:
            mylite_execution_set_unsupported_error(
                database,
                "COLLATE supports only scalar values with known character set metadata"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            *out_charset = database->session.character_set_connection;
            *out_collation = database->session.collation_connection;
            return MYLITE_OK;
        }
        break;
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        *out_charset = "binary";
        *out_collation = "binary";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION: {
        struct scalar_convert_charset_info info = {0};
        int rc = mylite_execution_scalar_convert_charset_info_for_expression(
            database,
            expression,
            &info
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_charset = info.charset;
        *out_collation = info.collation;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
        *out_charset = mylite_execution_national_character_set_name();
        *out_collation = mylite_execution_national_collation_name();
        return MYLITE_OK;
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "COLLATE supports only scalar values with known character set metadata"
    );
    return MYLITE_ERROR;
}

static const struct mylite_execution_catalog_scalar_collation *scalar_collation_info_by_name(
    const char *collation_name
) {
    return mylite_execution_catalog_scalar_collation_info_by_name(collation_name);
}

static enum planned_charset_collation_function_kind charset_collation_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_CHARSET_FUNCTION:
        return PLANNED_CHARSET_COLLATION_FUNCTION_CHARSET;
    case MYLITE_SQL_AST_COLLATION_FUNCTION:
        return PLANNED_CHARSET_COLLATION_FUNCTION_COLLATION;
    case MYLITE_SQL_AST_COERCIBILITY_FUNCTION:
        return PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY;
    default:
        return PLANNED_CHARSET_COLLATION_FUNCTION_NONE;
    }
}

static bool is_charset_collation_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return charset_collation_function_kind(ast_kind) != PLANNED_CHARSET_COLLATION_FUNCTION_NONE;
}

static int charset_collation_select_result(
    enum planned_charset_collation_function_kind function_kind,
    const char *charset,
    const char *collation,
    const char **out_result
) {
    if (out_result == NULL || charset == NULL || collation == NULL) {
        return MYLITE_MISUSE;
    }
    switch (function_kind) {
    case PLANNED_CHARSET_COLLATION_FUNCTION_CHARSET:
        *out_result = charset;
        return MYLITE_OK;
    case PLANNED_CHARSET_COLLATION_FUNCTION_COLLATION:
        *out_result = collation;
        return MYLITE_OK;
    case PLANNED_CHARSET_COLLATION_FUNCTION_NONE:
    case PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY:
        break;
    }
    return MYLITE_ERROR;
}

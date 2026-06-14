#include "mylite_parser.h"

#include "mylite_parse.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"
#include "mylite_parser_token_map.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *mylite_sql_lemonAlloc(void *(*malloc_proc)(size_t));
void mylite_sql_lemon(
    void *parser,
    int parser_token,
    struct mylite_sql_token token,
    struct mylite_sql_parser_state *state
);
void mylite_sql_lemonFree(void *parser, void (*free_proc)(void *));

struct parenthesized_row_constructor_injection {
    bool enabled;
    const struct mylite_sql_lexer *lexer;
    const struct mylite_sql_token *left_paren;
    const struct mylite_sql_token *previous_token;
    bool has_previous_token;
};

struct mylite_sql_parser_feed_context {
    void *parser;
    struct mylite_sql_parser_state *state;
    struct mylite_sql_parser_token_history *token_history;
    bool *previous_token_was_dot;
    struct mylite_sql_token *previous_token;
    bool *has_previous_token;
    bool inject_parenthesized_row_constructors;
};

struct version_comment_payload {
    const char *text;
    size_t length;
    bool active;
};

struct mylite_sql_parse_error {
    enum mylite_sql_parse_status status;
    int parser_token;
    struct mylite_sql_token token;
};

enum placeholder_statement_kind {
    PLACEHOLDER_STATEMENT_NONE = 0,
    PLACEHOLDER_STATEMENT_ADMIN_NOOP = 1,
    PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM = 2,
    PLACEHOLDER_STATEMENT_UTILITY_NOOP = 3,
    PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY = 4,
    PLACEHOLDER_STATEMENT_EXPLAIN = 5,
    PLACEHOLDER_STATEMENT_ALTER_TABLE_MULTI_ACTION_UNSUPPORTED = 6,
};

struct placeholder_statement_scan {
    const struct mylite_sql_token *tokens;
    size_t token_count;
    bool has_non_trailing_semicolon;
};

enum {
    placeholder_initial_token_capacity = 16,
    placeholder_create_scan_token_limit = 12,
    create_table_partition_min_token_count = 6,
    create_table_select_min_token_count = 5,
    cte_placeholder_min_token_count = 5,
    alter_table_partition_min_token_count = 5,
    mylite_mysql_version_comment_gate = 80409,
    version_comment_min_token_length = 5,
    version_comment_decimal_radix = 10,
    version_comment_lexer_stack_limit = 16,
};

static enum mylite_sql_parse_status parse_sql_with_lemon(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
);
static enum mylite_sql_parse_status parse_sql_with_lemon_options(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result,
    bool inject_parenthesized_row_constructors
);
static void reset_parse_result(struct mylite_sql_parse_result *out_result);
static enum mylite_sql_parse_status feed_lexer_tokens(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    bool feed_eof
);
static enum mylite_sql_parse_status feed_lexer_token(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token
);
static bool parse_version_comment_payload(
    const struct mylite_sql_token *token,
    struct version_comment_payload *out_payload
);
static enum mylite_sql_parse_status push_version_comment_payload_lexer(
    const struct mylite_sql_token *token,
    unsigned int modes,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count
);
static bool ascii_byte_is_digit(char byte);
static enum mylite_sql_parse_status try_parse_select_result_option_before_duplicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
static bool feed_parenthesized_row_constructor_if_needed(
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct parenthesized_row_constructor_injection *injection
);
static bool should_inject_parenthesized_row_constructor(
    const struct parenthesized_row_constructor_injection *injection
);
static bool token_can_name_immediate_function(const struct mylite_sql_token *token);
static bool lexer_parenthesized_expression_has_top_level_comma(const struct mylite_sql_lexer *lexer
);
static struct mylite_sql_token make_synthetic_row_constructor_token(
    const struct mylite_sql_token *left_paren
);
static void update_parser_token_history(
    struct mylite_sql_parser_token_history *history,
    int parser_token
);
static void record_parse_error(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parse_error error
);
static enum mylite_sql_parse_status try_parse_parenthesized_row_constructor_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
static bool scan_can_retry_parenthesized_row_constructors(
    const struct placeholder_statement_scan *scan
);
static enum mylite_sql_parse_status try_parse_tableless_select_limit_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
static bool scan_can_retry_tableless_select_limit(
    const struct placeholder_statement_scan *scan,
    size_t *out_limit_index
);
static bool tableless_select_limit_tail_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t limit_index
);
static bool select_limit_tail_token_is_integer(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool parsed_select_accepts_tableless_limit(struct mylite_sql_ast_node *statement);
static struct mylite_sql_ast_node *make_select_limit_clause_from_tail(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t limit_index
);
static enum mylite_sql_parse_status try_parse_repeated_select_locking_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
static bool scan_can_retry_repeated_select_locking(
    const struct placeholder_statement_scan *scan,
    size_t *out_prefix_length
);
static bool scan_select_locking_clause_sequence(
    const struct placeholder_statement_scan *scan,
    size_t first_index,
    size_t *out_second_index
);
static bool scan_select_locking_clause_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_end_index
);
static bool placeholder_scan_token_starts_select_locking_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_lock_target_list_end(
    const struct placeholder_statement_scan *scan,
    size_t target_index,
    size_t *out_end_index
);
static bool placeholder_scan_token_starts_select_lock_wait(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum mylite_sql_parse_status try_parse_legacy_create_index_type_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
static bool scan_can_retry_legacy_create_index_type(
    const struct placeholder_statement_scan *scan,
    size_t *out_type_index
);
static bool scan_legacy_create_index_type_prefix(
    const struct placeholder_statement_scan *scan,
    size_t after_index_name,
    size_t *out_type_index
);
static bool scan_legacy_create_index_type_suffix(
    const struct placeholder_statement_scan *scan,
    size_t after_index_name,
    size_t *out_type_index
);
static enum mylite_sql_parse_status parse_legacy_create_index_type_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t type_index,
    struct mylite_sql_parse_result *out_result
);
static enum mylite_sql_parse_status try_parse_placeholder_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
static enum mylite_sql_parse_status scan_placeholder_statement_tokens(
    struct mylite_sql_parse_config config,
    struct mylite_sql_token **out_tokens,
    size_t *out_token_count,
    bool *out_has_non_trailing_semicolon
);
static enum mylite_sql_parse_status scan_placeholder_lexer_tokens(
    struct mylite_sql_lexer *lexer,
    const char *root_input,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity,
    bool *saw_semicolon,
    bool *out_has_non_trailing_semicolon
);
static enum mylite_sql_parse_status scan_placeholder_lexer_token(
    struct mylite_sql_token *token,
    struct mylite_sql_lexer *current_lexer,
    const char *root_input,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity,
    bool *saw_semicolon,
    bool *out_has_non_trailing_semicolon,
    bool *out_done
);
static bool scan_can_retry_select_result_option_before_duplicate(
    const struct placeholder_statement_scan *scan,
    size_t *out_duplicate_index
);
static bool placeholder_scan_token_is_select_result_option(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_select_duplicate_modifier(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum mylite_sql_parse_status parse_select_result_option_before_duplicate_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t duplicate_index,
    struct mylite_sql_parse_result *out_result
);
static enum mylite_sql_parse_status feed_select_modifier_reordered_token(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token
);
static enum mylite_sql_parse_status feed_token_with_parser_token_override(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token,
    int parser_token_override
);
static bool append_placeholder_statement_token(
    const struct mylite_sql_token *token,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity
);
static enum mylite_sql_parse_status try_parse_alter_table_algorithm_lock_tail_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
);
static bool scan_alter_table_algorithm_lock_tail(
    const struct placeholder_statement_scan *scan,
    size_t *out_option_index,
    size_t *out_prefix_length,
    bool *out_actionless
);
static size_t alter_table_placeholder_name_end_index(const struct placeholder_statement_scan *scan);
static bool parse_alter_table_algorithm_lock_options(
    const struct placeholder_statement_scan *scan,
    size_t option_index,
    struct mylite_sql_alter_table_options *out_options
);
static bool parse_alter_table_algorithm_lock_option(
    const struct placeholder_statement_scan *scan,
    size_t *index,
    struct mylite_sql_alter_table_options *options
);
static bool alter_table_statement_accepts_prefix_option_tail(
    const struct mylite_sql_ast_node *statement
);
static bool alter_table_algorithm_lock_options_are_known(
    struct mylite_sql_alter_table_options options
);
static enum mylite_sql_parse_status try_parse_create_table_partition_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
);
static bool find_open_version_comment_start_before_token(
    struct mylite_sql_parse_config config,
    const struct mylite_sql_token *token,
    size_t *out_offset
);
static bool scan_is_create_table_partition_statement(
    const struct placeholder_statement_scan *scan,
    size_t *out_partition_index
);
static bool create_table_partition_suffix_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t partition_index
);
static bool create_table_select_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool create_table_select_scan_has_query_source(
    const struct placeholder_statement_scan *scan,
    size_t start_index
);
static bool create_table_select_query_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool create_table_select_parenthesized_query_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool create_table_select_with_query_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool create_table_select_plain_query_keyword_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool create_table_select_query_keyword_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool create_table_select_scan_has_balanced_parentheses(
    const struct placeholder_statement_scan *scan,
    size_t start_index
);
static bool token_is_left_paren(const struct mylite_sql_token *token);
static bool token_is_right_paren(const struct mylite_sql_token *token);
static bool token_is_comma(const struct mylite_sql_token *token);
static bool token_is_equal_sign(const struct mylite_sql_token *token);
static bool token_is_string_literal(const struct mylite_sql_token *token);
static enum placeholder_statement_kind classify_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_schema_security_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_stored_program_script_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_create_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_alter_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_drop_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_show_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_stored_program_object_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum placeholder_statement_kind classify_utility_admin_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_unsupported_utility_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_utility_noop_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_admin_noop_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool flush_placeholder_statement_is_supported(const struct placeholder_statement_scan *scan);
static bool flush_placeholder_option_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static bool flush_placeholder_table_option_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t table_index
);
static bool flush_placeholder_table_name_list_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static bool flush_placeholder_table_name_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static bool flush_placeholder_table_name_part_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum placeholder_statement_kind classify_query_surface_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_cte_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_cte_clause_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *out_query_index
);
static bool placeholder_scan_cte_definition_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static bool placeholder_scan_cte_column_list_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static bool placeholder_scan_cte_body_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static bool placeholder_scan_cte_query_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum placeholder_statement_kind classify_query_scalar_expression_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_query_expression_clause_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_query_function_subquery_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_query_table_reference_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_dml_variant_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_starts_query_statement(const struct placeholder_statement_scan *scan);
static bool placeholder_scan_parenthesized_start_is_query_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_parenthesized_table_reference_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_parenthesized_table_reference_body_has_surface(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_parenthesized_group_has_table_reference_name(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
);
static bool placeholder_scan_contains_odbc_table_reference_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_odbc_escape_starts_table_reference(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_mixed_comma_explicit_join_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_delayed_join_condition_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_from_clause_contains_mixed_comma_explicit_join(
    const struct placeholder_statement_scan *scan,
    size_t from_index
);
static bool placeholder_scan_from_clause_contains_delayed_join_condition(
    const struct placeholder_statement_scan *scan,
    size_t from_index
);
static bool placeholder_scan_token_stops_table_reference_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_parenthesized_table_reference_body_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
);
static bool placeholder_scan_find_matching_right_paren(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index
);
static bool placeholder_scan_query_expression_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_starts_parenthesized_table_reference_context(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_join_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_table_reference_name(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_query_expression_clause_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_query_final_tail_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_compound_query_final_tail_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_select_limit_into_locking_permutation(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_top_level_token_starts_set_operation(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_top_level_token_starts_final_query_tail(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_into_user_variable_list_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t into_index
);
static bool placeholder_scan_contains_json_arrow_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_json_arrow_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_odbc_expression_escape_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_token_is_odbc_expression_escape_name(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_find_matching_right_brace(
    const struct placeholder_statement_scan *scan,
    size_t left_brace_index,
    size_t *out_right_brace_index
);
static bool placeholder_scan_contains_postfix_is_predicate_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_between_expression_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_between_expression_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t between_index
);
static bool placeholder_scan_between_bounds_contain_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t lower_start_index,
    size_t and_index
);
static bool placeholder_scan_contains_descriptor_in_list_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_in_predicate_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t in_index
);
static bool placeholder_scan_contains_literal_left_predicate_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_literal_left_comparison_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
);
static bool placeholder_scan_literal_left_between_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
);
static bool placeholder_scan_literal_left_in_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
);
static bool placeholder_scan_literal_predicate_has_clause_context(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
);
static bool placeholder_scan_tokens_contain_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_tokens_contain_identifier_like(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_contains_row_constructor_predicate_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_row_constructor_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t row_index,
    size_t *out_right_paren_index
);
static bool placeholder_scan_parenthesized_list_contains_top_level_comma(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
);
static bool placeholder_scan_contains_string_order_key_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_constant_order_key_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_token_is_constant_order_key_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_having_residual_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_having_clause_contains_residual(
    const struct placeholder_statement_scan *scan,
    size_t start_index
);
static bool placeholder_scan_having_in_predicate_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t in_index
);
static bool placeholder_scan_contains_query_function_subquery_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_any_function_call_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_quoted_function_call_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_quoted_function_call_has_expression_context(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index
);
static bool placeholder_scan_contains_parameter_marker(const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_token_is_generic_function_placeholder_name(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_nested_function_call_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_named_window_function_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_nonempty_function_call_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index,
    size_t left_paren_index
);
static bool placeholder_scan_function_call_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index,
    size_t left_paren_index,
    bool allow_empty_arguments
);
static bool placeholder_scan_function_call_contains_nested_call(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
);
static bool placeholder_scan_function_call_arguments_are_well_formed(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
);
static bool placeholder_scan_token_can_follow_function_call_surface(
    const struct placeholder_statement_scan *scan,
    size_t right_paren_index
);
static bool placeholder_scan_token_is_aggregate_window_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_builtin_window_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_parenthesized_query_expression_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_has_table_backed_or_dml_context(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_expression_operator_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_expression_residual_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_select_symbolic_not_arithmetic_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_targeted_comparison_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_comparison_operator_has_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_parenthesized_left_operand_contains_comparison(
    const struct placeholder_statement_scan *scan,
    size_t comparison_index
);
static bool placeholder_scan_parenthesized_range_contains_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
);
static bool placeholder_scan_contains_identifier_between_residual_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_identifier_in_list_residual_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_insert_keyword_group_function_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_group_by_user_variable_assignment_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_function_call_has_four_arguments(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
);
static bool placeholder_scan_contains_charset_introducer_string_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_current_temporal_keyword_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_malformed_function_argument_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_expression_clause_contains_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t start_index
);
static bool placeholder_scan_token_is_expression_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_expression_operator_surface_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_unary_expression_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_can_start_expression_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_can_end_expression_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_statement_tail_is_obviously_incomplete(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_token_is_incomplete_statement_tail(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_stops_expression_clause_search(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_starts_duplicate_update_assignment_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_row_tuple_predicate_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_row_tuple_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index
);
static bool placeholder_scan_left_paren_starts_function_arguments(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
);
static bool placeholder_scan_token_can_name_immediate_function(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_dml_variant_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_delete_variant_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_delete_ignore_modifier_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_delete_multitable_target_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_delete_using_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_multikey_dml_order_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_user_variable_assignment_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_update_variant_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_update_has_joined_source_before_set(
    const struct placeholder_statement_scan *scan,
    size_t set_index
);
static bool placeholder_scan_update_has_using_join_before_set(
    const struct placeholder_statement_scan *scan,
    size_t set_index
);
static bool placeholder_scan_contains_insert_replace_variant_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_insert_row_alias_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_insert_row_constructor_list_end(
    const struct placeholder_statement_scan *scan,
    size_t values_index,
    size_t *out_end_index
);
static bool placeholder_scan_insert_row_alias_tail_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t alias_index
);
static bool placeholder_scan_token_can_name_insert_row_alias(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_insert_row_alias_column_list_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_end_index
);
static bool placeholder_scan_contains_insert_identifier_value_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_parenthesized_values_contain_identifier(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
);
static bool placeholder_scan_contains_insert_set_identifier_value_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_duplicate_identifier_assignment_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_duplicate_qualified_assignment_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_qualified_identifier_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_end_index
);
static bool placeholder_scan_contains_replace_compound_select_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_find_top_level_keyword(
    const struct placeholder_statement_scan *scan,
    const char *keyword,
    size_t *out_index
);
static bool placeholder_scan_find_top_level_keyword_after(
    const struct placeholder_statement_scan *scan,
    const char *keyword,
    size_t start_index,
    size_t *out_index
);
static bool placeholder_scan_token_is_assignment_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static size_t placeholder_scan_delete_modifier_prefix_end(
    const struct placeholder_statement_scan *scan,
    bool *out_saw_ignore
);
static bool placeholder_scan_token_is_delete_modifier_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_can_start_dml_identifier_value(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_bare_truth_clause_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_token_can_start_bare_truth_expression(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_bare_truth_expression_is_simple(
    const struct placeholder_statement_scan *scan,
    size_t expression_index
);
static bool placeholder_scan_bare_truth_expression_is_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t expression_index,
    size_t stop_index
);
static bool placeholder_scan_contains_lateral_derived_table(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_grouping_function(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_sounds_like_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_deprecated_logical_operator_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_like_surface(const struct placeholder_statement_scan *scan);
static bool placeholder_scan_contains_like_escape_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_not_like_surface(const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_typed_temporal_literal_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_interval_expression_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_interval_expression_follows_parenthesized_separator(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_interval_value_binary_expression_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_date_interval_unit(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_scalar_in_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_scalar_in_is_descriptor_predicate(
    const struct placeholder_statement_scan *scan,
    size_t in_index
);
static bool placeholder_scan_token_starts_predicate_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_stops_predicate_clause_search(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_quantified_subquery_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_fulltext_match_against_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_match_column_list_without_parentheses_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t match_index
);
static bool placeholder_scan_match_column_list_with_parentheses_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t match_index
);
static bool placeholder_scan_match_column_name_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_next_index
);
static bool placeholder_scan_token_can_name_loose_identifier(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_parentheses_are_balanced(
    const struct placeholder_statement_scan *scan,
    size_t start_index
);
static bool placeholder_scan_parenthesized_operand_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
);
static bool placeholder_scan_token_is_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum placeholder_statement_kind classify_create_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_alter_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool foreign_server_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool foreign_server_create_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool foreign_server_alter_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool foreign_server_drop_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool foreign_server_drop_name_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool alter_schema_unsupported_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool alter_schema_unsupported_encryption_option_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool alter_schema_unsupported_read_only_option_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool alter_schema_read_only_value_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool alter_table_engine_first_multi_action_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool alter_table_engine_first_multi_action_starter_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool create_view_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool alter_view_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool view_placeholder_statement_has_query_body(
    const struct placeholder_statement_scan *scan,
    size_t view_index
);
static bool undo_tablespace_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool ddl_zerofill_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool ddl_extended_option_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool ddl_extended_option_placeholder_statement_is_candidate(
    const struct placeholder_statement_scan *scan
);
static bool ddl_extended_option_scan_has_marker(const struct placeholder_statement_scan *scan);
static bool ddl_residual_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool ddl_residual_placeholder_statement_is_candidate(
    const struct placeholder_statement_scan *scan
);
static bool ddl_residual_scan_has_fulltext_parser_clause(
    const struct placeholder_statement_scan *scan
);
static bool ddl_residual_scan_has_generated_column_surface(
    const struct placeholder_statement_scan *scan
);
static bool ddl_generated_column_clause_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t generated_index
);
static bool ddl_residual_scan_has_foreign_key_set_default_action(
    const struct placeholder_statement_scan *scan
);
static bool ddl_residual_scan_has_alter_table_order_by_action(
    const struct placeholder_statement_scan *scan
);
static bool ddl_residual_scan_has_create_table_start_transaction_option(
    const struct placeholder_statement_scan *scan
);
static bool ddl_check_expression_placeholder_scan_has_marker(
    const struct placeholder_statement_scan *scan
);
static bool ddl_check_expression_placeholder_clause_has_marker(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
);
static bool ddl_check_expression_in_list_marker_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
);
static bool placeholder_scan_starts_create_table_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_starts_alter_table_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_starts_create_index_statement(
    const struct placeholder_statement_scan *scan
);
static bool alter_table_partition_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool alter_table_tablespace_file_operation_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool alter_table_tablespace_file_operation_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t operation_index
);
static size_t alter_table_partition_operation_start_index(
    const struct placeholder_statement_scan *scan
);
static bool alter_table_partition_scan_has_operation(
    const struct placeholder_statement_scan *scan,
    size_t start_index
);
static bool alter_table_partition_scan_has_balanced_parentheses(
    const struct placeholder_statement_scan *scan,
    size_t start_index
);
static bool placeholder_scan_token_text_equals_any(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *const *texts,
    size_t text_count
);
static bool load_xml_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool select_into_file_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool import_table_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool help_placeholder_statement_is_supported(const struct placeholder_statement_scan *scan);
static bool lock_instance_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool unlock_instance_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool change_replication_source_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static enum mylite_sql_parse_status try_parse_admin_set_residual_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
);
static enum mylite_sql_parse_status try_parse_plural_table_maintenance_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
);
static enum mylite_sql_parse_status try_parse_describe_explain_table_filter_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
);
static enum mylite_sql_parse_status try_parse_show_extended_metadata_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
);
static bool scan_show_extended_metadata_statement(
    const struct placeholder_statement_scan *scan,
    bool *out_full_columns,
    bool *out_columns_statement,
    bool *out_index_statement,
    size_t *out_index
);
static enum mylite_sql_parse_status parse_show_extended_metadata_tail(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    bool columns_statement,
    size_t *index,
    struct mylite_sql_ast_node **out_schema_name,
    struct mylite_sql_ast_node **out_filter
);
static struct mylite_sql_ast_node *make_show_extended_metadata_statement(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    bool full_columns,
    bool index_statement,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter
);
static enum mylite_sql_parse_status try_parse_set_system_variable_assign_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
);
static enum mylite_sql_parse_status init_scanned_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parser_state *out_state
);
static enum mylite_sql_parse_status finish_scanned_statement_parse(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement
);
static struct mylite_sql_ast_node *placeholder_parse_table_name(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static struct mylite_sql_ast_node *placeholder_parse_table_name_list(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static struct mylite_sql_ast_node *placeholder_parse_show_like_filter(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
);
static struct mylite_sql_ast_node *placeholder_parse_set_system_variable_value(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_set_system_variable_scalar_value(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_dot(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_show_columns_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_show_index_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_schema_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum placeholder_statement_kind classify_drop_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_set_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_set_leading_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool set_placeholder_scan_contains_unsupported_residual(
    const struct placeholder_statement_scan *scan
);
static bool set_placeholder_token_requires_unsupported_utility(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool saw_assignment_operator
);
static bool set_placeholder_assignment_value_starts_unsupported_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool set_placeholder_assigned_token_is_unsupported_surface(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool saw_assignment_operator
);
static enum placeholder_statement_kind classify_show_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_describe_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool show_engine_logs_mutex_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_explain_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_explain_statement_start_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_contains_text(
    const struct placeholder_statement_scan *scan,
    const char *text
);
static bool placeholder_scan_token_is_identifier_like(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_text_equals(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *text
);
static bool placeholder_scan_token_text_starts_with(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *prefix
);
static bool placeholder_token_is_semicolon(const struct mylite_sql_token *token);
static enum mylite_sql_ast_node_kind ast_kind_for_placeholder_statement(
    enum placeholder_statement_kind kind
);
static enum mylite_sql_parse_status finish_placeholder_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum placeholder_statement_kind kind
);
static enum mylite_sql_parse_status finish_explain_placeholder_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan
);

enum mylite_sql_parse_status mylite_sql_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
) {
    enum mylite_sql_parse_status status = parse_sql_with_lemon(config, out_result);

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status modifier_status =
            try_parse_select_result_option_before_duplicate_statement(config, out_result, &handled);

        if (handled) {
            status = modifier_status;
            out_result->status = modifier_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status row_constructor_status =
            try_parse_parenthesized_row_constructor_statement(config, out_result, &handled);

        if (handled) {
            status = row_constructor_status;
            out_result->status = row_constructor_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status tableless_limit_status =
            try_parse_tableless_select_limit_statement(config, out_result, &handled);

        if (handled) {
            status = tableless_limit_status;
            out_result->status = tableless_limit_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status locking_status =
            try_parse_repeated_select_locking_statement(config, out_result, &handled);

        if (handled) {
            status = locking_status;
            out_result->status = locking_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status legacy_index_status =
            try_parse_legacy_create_index_type_statement(config, out_result, &handled);

        if (handled) {
            status = legacy_index_status;
            out_result->status = legacy_index_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;
        enum mylite_sql_parse_status placeholder_status =
            try_parse_placeholder_statement(config, out_result, &handled);

        if (handled) {
            status = placeholder_status;
            out_result->status = placeholder_status;
        }
    }

    return status;
}

static enum mylite_sql_parse_status parse_sql_with_lemon(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
) {
    return parse_sql_with_lemon_options(config, out_result, false);
}

static enum mylite_sql_parse_status parse_sql_with_lemon_options(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result,
    bool inject_parenthesized_row_constructors
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_lexer lexer;
    void *parser = NULL;
    bool previous_token_was_dot = false;
    struct mylite_sql_parser_token_history token_history = {0};
    struct mylite_sql_token previous_token = {0};
    bool has_previous_token = false;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_result == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    reset_parse_result(out_result);

    if (config.input == NULL && config.length != 0U) {
        out_result->status = MYLITE_SQL_PARSE_MISUSE;
        return out_result->status;
    }

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .modes = config.modes,
        .accepted = false,
    };

    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        out_result->status = MYLITE_SQL_PARSE_NOMEM;
        return out_result->status;
    }

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = config.input,
            .length = config.length,
            .modes = config.modes,
        }
    );
    status = feed_lexer_tokens(
        &(struct mylite_sql_parser_feed_context){
            .parser = parser,
            .state = &state,
            .token_history = &token_history,
            .previous_token_was_dot = &previous_token_was_dot,
            .previous_token = &previous_token,
            .has_previous_token = &has_previous_token,
            .inject_parenthesized_row_constructors = inject_parenthesized_row_constructors,
        },
        &lexer,
        true
    );

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return out_result->status;
}

static void reset_parse_result(struct mylite_sql_parse_result *out_result) {
    out_result->root = NULL;
    out_result->status = MYLITE_SQL_PARSE_OK;
    out_result->error_token = (struct mylite_sql_token){0};
    out_result->parser_token = 0;
    mylite_sql_ast_init(&out_result->ast);
}

static enum mylite_sql_parse_status feed_lexer_tokens(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    bool feed_eof
) {
    struct mylite_sql_lexer lexer_stack[version_comment_lexer_stack_limit];
    size_t lexer_count = 1U;

    if (context == NULL || lexer == NULL || context->state == NULL ||
        context->state->result == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    lexer_stack[0] = *lexer;
    for (;;) {
        struct mylite_sql_token token;
        enum mylite_sql_parse_status status;
        struct mylite_sql_lexer *current_lexer = &lexer_stack[lexer_count - 1U];

        if (mylite_sql_lexer_next(current_lexer, &token) != 0) {
            return MYLITE_SQL_PARSE_MISUSE;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            if (lexer_count > 1U) {
                --lexer_count;
                continue;
            }
            if (!feed_eof) {
                return MYLITE_SQL_PARSE_OK;
            }
        } else if (token.kind == MYLITE_SQL_TOKEN_VERSION_COMMENT) {
            status = push_version_comment_payload_lexer(
                &token,
                context->state->modes,
                lexer_stack,
                &lexer_count
            );
            if (status != MYLITE_SQL_PARSE_OK) {
                return status;
            }
            continue;
        }

        status = feed_lexer_token(context, current_lexer, &token);
        if (status != MYLITE_SQL_PARSE_OK || token.kind == MYLITE_SQL_TOKEN_EOF) {
            return status;
        }
    }
}

static enum mylite_sql_parse_status feed_lexer_token(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token
) {
    struct mylite_sql_parser_token_map token_map;

    if (context == NULL || context->parser == NULL || context->state == NULL ||
        context->state->result == NULL || context->token_history == NULL ||
        context->previous_token_was_dot == NULL || context->previous_token == NULL ||
        context->has_previous_token == NULL || lexer == NULL || token == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    if (mylite_sql_parser_token_is_comment(token->kind)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (token->kind == MYLITE_SQL_TOKEN_ERROR) {
        record_parse_error(
            context->state->result,
            (struct mylite_sql_parse_error){
                .status = MYLITE_SQL_PARSE_LEXER_ERROR,
                .parser_token = 0,
                .token = *token,
            }
        );
        return context->state->result->status;
    }

    /* Lock targets do not affect MyLite's embedded no-op locking behavior. */
    if (mylite_sql_parser_should_skip_select_lock_target_list(token, context->token_history)) {
        enum mylite_sql_parse_status status =
            mylite_sql_parser_skip_select_lock_target_list(lexer, token);

        if (status != MYLITE_SQL_PARSE_OK) {
            record_parse_error(
                context->state->result,
                (struct mylite_sql_parse_error){
                    .status = status,
                    .parser_token = 0,
                    .token = *token,
                }
            );
            return context->state->result->status;
        }
    }

    if (!feed_parenthesized_row_constructor_if_needed(
            context->parser,
            context->state,
            context->token_history,
            context->previous_token_was_dot,
            &(struct parenthesized_row_constructor_injection){
                .enabled = context->inject_parenthesized_row_constructors,
                .lexer = lexer,
                .left_paren = token,
                .previous_token = context->previous_token,
                .has_previous_token = *context->has_previous_token,
            }
        )) {
        return context->state->result->status;
    }

    if (!mylite_sql_parser_map_lexer_token(
            context->state,
            lexer,
            token,
            *context->previous_token_was_dot,
            context->token_history,
            &token_map
        )) {
        record_parse_error(
            context->state->result,
            (struct mylite_sql_parse_error){
                .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                .parser_token = -1,
                .token = *token,
            }
        );
        return context->state->result->status;
    }

    mylite_sql_lemon(context->parser, token_map.parser_token, *token, context->state);
    *context->previous_token_was_dot = token_map.previous_token_was_dot;
    update_parser_token_history(context->token_history, token_map.parser_token);
    if (context->inject_parenthesized_row_constructors) {
        *context->previous_token = *token;
        *context->has_previous_token = token->kind != MYLITE_SQL_TOKEN_EOF;
    }

    return context->state->result->status;
}

static bool parse_version_comment_payload(
    const struct mylite_sql_token *token,
    struct version_comment_payload *out_payload
) {
    const char *payload_start;
    const char *payload_end;
    const char *cursor;
    unsigned int version = 0U;
    bool has_version = false;

    if (out_payload == NULL) {
        return false;
    }
    *out_payload = (struct version_comment_payload){0};

    if (token == NULL || token->kind != MYLITE_SQL_TOKEN_VERSION_COMMENT || token->text == NULL ||
        token->length < (size_t)version_comment_min_token_length) {
        return false;
    }

    payload_start = token->text + 3U;
    payload_end = token->text + token->length - 2U;
    if (payload_end < payload_start) {
        return false;
    }

    cursor = payload_start;
    while (cursor < payload_end && ascii_byte_is_digit(*cursor)) {
        has_version = true;
        if (version <= (unsigned int)mylite_mysql_version_comment_gate) {
            version = (version * (unsigned int)version_comment_decimal_radix) +
                      (unsigned int)(*cursor - '0');
        }
        ++cursor;
    }

    out_payload->text = cursor;
    out_payload->length = (size_t)(payload_end - cursor);
    out_payload->active =
        !has_version || version <= (unsigned int)mylite_mysql_version_comment_gate;
    return true;
}

static enum mylite_sql_parse_status push_version_comment_payload_lexer(
    const struct mylite_sql_token *token,
    unsigned int modes,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count
) {
    struct version_comment_payload payload;

    if (lexer_stack == NULL || lexer_count == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (!parse_version_comment_payload(token, &payload)) {
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (!payload.active || payload.length == 0U) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (*lexer_count >= (size_t)version_comment_lexer_stack_limit) {
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    mylite_sql_lexer_init(
        &lexer_stack[*lexer_count],
        (struct mylite_sql_lexer_config){
            .input = payload.text,
            .length = payload.length,
            .modes = modes,
        }
    );
    ++*lexer_count;
    return MYLITE_SQL_PARSE_OK;
}

static bool ascii_byte_is_digit(char byte) {
    return byte >= '0' && byte <= '9';
}

void mylite_sql_parse_result_deinit(struct mylite_sql_parse_result *result) {
    if (result == NULL) {
        return;
    }

    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
}

const char *mylite_sql_parse_status_name(enum mylite_sql_parse_status status) {
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return "ok";
    case MYLITE_SQL_PARSE_MISUSE:
        return "misuse";
    case MYLITE_SQL_PARSE_NOMEM:
        return "nomem";
    case MYLITE_SQL_PARSE_LEXER_ERROR:
        return "lexer_error";
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
        return "syntax_error";
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        return "stack_overflow";
    }

    return "unknown";
}

static enum mylite_sql_parse_status try_parse_select_result_option_before_duplicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t duplicate_index = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_select_result_option_before_duplicate(&scan, &duplicate_index)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status = parse_select_result_option_before_duplicate_tokens(
        config,
        &scan,
        duplicate_index,
        &retry_result
    );
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    free(tokens);
    if (status != MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        result->status = status;
        *out_handled = true;
        return status;
    }
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_select_result_option_before_duplicate(
    const struct placeholder_statement_scan *scan,
    size_t *out_duplicate_index
) {
    size_t index = 1U;

    if (out_duplicate_index != NULL) {
        *out_duplicate_index = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count < 3U || !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_token_is_select_result_option(scan, index)) {
        return false;
    }
    while (index < scan->token_count) {
        if (!placeholder_scan_token_is_select_result_option(scan, index)) {
            break;
        }
        ++index;
    }
    if (index >= scan->token_count ||
        !placeholder_scan_token_is_select_duplicate_modifier(scan, index)) {
        return false;
    }
    if (out_duplicate_index != NULL) {
        *out_duplicate_index = index;
    }
    return true;
}

static bool placeholder_scan_token_is_select_result_option(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SQL_SMALL_RESULT") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_BIG_RESULT") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_BUFFER_RESULT") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_NO_CACHE") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_CALC_FOUND_ROWS");
}

static bool placeholder_scan_token_is_select_duplicate_modifier(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "ALL") ||
           placeholder_scan_token_text_equals(scan, index, "DISTINCT") ||
           placeholder_scan_token_text_equals(scan, index, "DISTINCTROW");
}

static enum mylite_sql_parse_status parse_select_result_option_before_duplicate_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t duplicate_index,
    struct mylite_sql_parse_result *out_result
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_parser_token_history token_history = {0};
    void *parser = NULL;
    bool previous_token_was_dot = false;
    struct mylite_sql_token eof_token = {
        .kind = MYLITE_SQL_TOKEN_EOF,
        .text = config.input == NULL ? NULL : config.input + config.length,
        .offset = config.length,
    };
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_result == NULL || scan == NULL || scan->tokens == NULL ||
        duplicate_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    reset_parse_result(out_result);

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .modes = config.modes,
        .accepted = false,
    };
    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        out_result->status = MYLITE_SQL_PARSE_NOMEM;
        return out_result->status;
    }

    status = feed_select_modifier_reordered_token(
        config,
        parser,
        &state,
        &token_history,
        &previous_token_was_dot,
        &scan->tokens[0]
    );
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[duplicate_index]
        );
    }
    for (size_t index = 1U; status == MYLITE_SQL_PARSE_OK && index < duplicate_index; ++index) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[index]
        );
    }
    for (size_t index = duplicate_index + 1U;
         status == MYLITE_SQL_PARSE_OK && index < scan->token_count;
         ++index) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[index]
        );
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &eof_token
        );
    }

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return out_result->status;
}

static enum mylite_sql_parse_status feed_select_modifier_reordered_token(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token
) {
    return feed_token_with_parser_token_override(
        config,
        parser,
        state,
        history,
        previous_token_was_dot,
        token,
        0
    );
}

static enum mylite_sql_parse_status feed_token_with_parser_token_override(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token,
    int parser_token_override
) {
    struct mylite_sql_parser_token_map token_map;
    struct mylite_sql_lexer token_lexer;

    if (parser == NULL || state == NULL || state->result == NULL || history == NULL ||
        previous_token_was_dot == NULL || token == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    if (parser_token_override > 0) {
        mylite_sql_lemon(parser, parser_token_override, *token, state);
        *previous_token_was_dot = false;
        update_parser_token_history(history, parser_token_override);
        return state->result->status;
    }

    token_lexer = (struct mylite_sql_lexer){
        .input = config.input,
        .length = config.length,
        .offset = token->offset + token->length,
        .modes = config.modes,
    };
    if (token_lexer.offset > config.length) {
        token_lexer.offset = config.length;
    }
    if (!mylite_sql_parser_map_lexer_token(
            state,
            &token_lexer,
            token,
            *previous_token_was_dot,
            history,
            &token_map
        )) {
        record_parse_error(
            state->result,
            (struct mylite_sql_parse_error){
                .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                .parser_token = -1,
                .token = *token,
            }
        );
        return state->result->status;
    }

    mylite_sql_lemon(parser, token_map.parser_token, *token, state);
    *previous_token_was_dot = token_map.previous_token_was_dot;
    update_parser_token_history(history, token_map.parser_token);
    return state->result->status;
}

static enum mylite_sql_parse_status try_parse_parenthesized_row_constructor_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_parenthesized_row_constructors(&scan)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status = parse_sql_with_lemon_options(config, &retry_result, true);
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_parenthesized_row_constructors(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count == 0U || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_row_tuple_starts_at(scan, index, NULL)) {
            return true;
        }
    }
    return false;
}

static enum mylite_sql_parse_status try_parse_tableless_select_limit_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result prefix_result;
    struct mylite_sql_ast_node *limit_clause = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_parser_state state;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t limit_index = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_tableless_select_limit(&scan, &limit_index)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&prefix_result, 0, sizeof(prefix_result));
    status = parse_sql_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = scan.tokens[limit_index].offset,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    statement = mylite_sql_parser_child_at(prefix_result.root, 0U);
    if (!parsed_select_accepts_tableless_limit(statement)) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    state = (struct mylite_sql_parser_state){
        .result = &prefix_result,
        .modes = config.modes,
        .accepted = true,
    };
    limit_clause = make_select_limit_clause_from_tail(&state, &scan, limit_index);
    if (limit_clause == NULL) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_NOMEM;
    }
    mylite_sql_ast_node_append_child(statement, limit_clause);
    mylite_sql_ast_node_set_span(
        statement,
        mylite_sql_parser_span_join(statement->span, limit_clause->span)
    );
    mylite_sql_ast_node_set_span(prefix_result.root, statement->span);

    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_tableless_select_limit(
    const struct placeholder_statement_scan *scan,
    size_t *out_limit_index
) {
    size_t limit_index = 0U;

    if (out_limit_index != NULL) {
        *out_limit_index = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        !placeholder_scan_find_top_level_keyword(scan, "LIMIT", &limit_index) ||
        !tableless_select_limit_tail_is_supported(scan, limit_index)) {
        return false;
    }
    if (out_limit_index != NULL) {
        *out_limit_index = limit_index;
    }
    return true;
}

static bool tableless_select_limit_tail_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t limit_index
) {
    size_t tail_count = 0U;

    if (scan == NULL || limit_index >= scan->token_count) {
        return false;
    }
    tail_count = scan->token_count - limit_index;
    if (tail_count == 2U) {
        return select_limit_tail_token_is_integer(scan, limit_index + 1U);
    }
    if (tail_count != 4U || !select_limit_tail_token_is_integer(scan, limit_index + 1U)) {
        return false;
    }
    return (placeholder_scan_token_text_equals(scan, limit_index + 2U, "OFFSET") &&
            select_limit_tail_token_is_integer(scan, limit_index + 3U)) ||
           (token_is_comma(&scan->tokens[limit_index + 2U]) &&
            select_limit_tail_token_is_integer(scan, limit_index + 3U));
}

static bool select_limit_tail_token_is_integer(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return scan != NULL && index < scan->token_count &&
           scan->tokens[index].kind == MYLITE_SQL_TOKEN_INTEGER;
}

static bool parsed_select_accepts_tableless_limit(struct mylite_sql_ast_node *statement) {
    struct mylite_sql_ast_node *source = NULL;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return false;
    }
    source = mylite_sql_parser_child_at(statement, 1U);
    return source == NULL || source->kind == MYLITE_SQL_AST_FROM_DUAL;
}

static struct mylite_sql_ast_node *make_select_limit_clause_from_tail(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t limit_index
) {
    struct mylite_sql_ast_node *first = NULL;
    struct mylite_sql_ast_node *second = NULL;

    if (state == NULL || scan == NULL ||
        !tableless_select_limit_tail_is_supported(scan, limit_index)) {
        return NULL;
    }
    first = mylite_sql_parser_make_literal(
        state,
        scan->tokens[limit_index + 1U],
        MYLITE_SQL_AST_LITERAL_INTEGER
    );
    if (first == NULL) {
        return NULL;
    }
    if (scan->token_count - limit_index == 2U) {
        return mylite_sql_parser_make_limit_clause(state, scan->tokens[limit_index], first, NULL);
    }
    second = mylite_sql_parser_make_literal(
        state,
        scan->tokens[limit_index + 3U],
        MYLITE_SQL_AST_LITERAL_INTEGER
    );
    if (second == NULL) {
        return NULL;
    }
    if (placeholder_scan_token_text_equals(scan, limit_index + 2U, "OFFSET")) {
        return mylite_sql_parser_make_limit_clause(state, scan->tokens[limit_index], first, second);
    }
    return mylite_sql_parser_make_limit_clause(state, scan->tokens[limit_index], second, first);
}

static enum mylite_sql_parse_status try_parse_repeated_select_locking_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result prefix_result;
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t prefix_length = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_repeated_select_locking(&scan, &prefix_length)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&prefix_result, 0, sizeof(prefix_result));
    status = parse_sql_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = prefix_length,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    statement = mylite_sql_parser_child_at(prefix_result.root, 0U);
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    mylite_sql_ast_node_set_span(
        statement,
        (struct mylite_sql_source_span){
            .text = config.input,
            .length = config.length,
            .offset = 0U,
        }
    );
    mylite_sql_ast_node_set_span(prefix_result.root, statement->span);

    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_repeated_select_locking(
    const struct placeholder_statement_scan *scan,
    size_t *out_prefix_length
) {
    int paren_depth = 0;

    if (out_prefix_length != NULL) {
        *out_prefix_length = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        size_t second_index = 0U;

        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0 || !placeholder_scan_token_starts_select_locking_clause(scan, index)) {
            continue;
        }
        if (scan_select_locking_clause_sequence(scan, index, &second_index)) {
            if (out_prefix_length != NULL) {
                *out_prefix_length = scan->tokens[second_index].offset;
            }
            return true;
        }
    }
    return false;
}

static bool scan_select_locking_clause_sequence(
    const struct placeholder_statement_scan *scan,
    size_t first_index,
    size_t *out_second_index
) {
    size_t index = first_index;
    size_t clause_count = 0U;

    if (out_second_index != NULL) {
        *out_second_index = 0U;
    }
    while (index < scan->token_count) {
        size_t end_index = 0U;

        if (!placeholder_scan_token_starts_select_locking_clause(scan, index) ||
            !scan_select_locking_clause_end(scan, index, &end_index) || end_index <= index) {
            return false;
        }
        ++clause_count;
        if (clause_count == 2U && out_second_index != NULL) {
            *out_second_index = index;
        }
        index = end_index;
    }
    return clause_count > 1U;
}

static bool scan_select_locking_clause_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_end_index
) {
    size_t index = start_index;

    if (scan == NULL || out_end_index == NULL || start_index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "FOR") &&
        (placeholder_scan_token_text_equals(scan, index + 1U, "UPDATE") ||
         placeholder_scan_token_text_equals(scan, index + 1U, "SHARE"))) {
        index += 2U;
        if (placeholder_scan_token_text_equals(scan, index, "OF") &&
            !placeholder_scan_lock_target_list_end(scan, index + 1U, &index)) {
            return false;
        }
        if (placeholder_scan_token_text_equals(scan, index, "NOWAIT")) {
            ++index;
        } else if (placeholder_scan_token_text_equals(scan, index, "SKIP") &&
                   placeholder_scan_token_text_equals(scan, index + 1U, "LOCKED")) {
            index += 2U;
        }
        *out_end_index = index;
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "LOCK") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "IN") &&
        placeholder_scan_token_text_equals(scan, index + 2U, "SHARE") &&
        placeholder_scan_token_text_equals(scan, index + 3U, "MODE")) {
        *out_end_index = index + 4U;
        return true;
    }
    return false;
}

static bool placeholder_scan_token_starts_select_locking_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return (placeholder_scan_token_text_equals(scan, index, "FOR") &&
            (placeholder_scan_token_text_equals(scan, index + 1U, "UPDATE") ||
             placeholder_scan_token_text_equals(scan, index + 1U, "SHARE"))) ||
           (placeholder_scan_token_text_equals(scan, index, "LOCK") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "IN") &&
            placeholder_scan_token_text_equals(scan, index + 2U, "SHARE") &&
            placeholder_scan_token_text_equals(scan, index + 3U, "MODE"));
}

static bool placeholder_scan_lock_target_list_end(
    const struct placeholder_statement_scan *scan,
    size_t target_index,
    size_t *out_end_index
) {
    bool expecting_identifier = true;
    size_t index = target_index;

    if (scan == NULL || out_end_index == NULL || target_index >= scan->token_count) {
        return false;
    }
    for (; index < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index];

        if (!expecting_identifier &&
            (placeholder_scan_token_starts_select_locking_clause(scan, index) ||
             placeholder_scan_token_starts_select_lock_wait(scan, index))) {
            *out_end_index = index;
            return true;
        }
        if (expecting_identifier) {
            if (!mylite_sql_parser_token_can_be_select_lock_target_identifier(token)) {
                return false;
            }
            expecting_identifier = false;
            continue;
        }
        if (token_is_comma(token)) {
            expecting_identifier = true;
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, index, ".")) {
            expecting_identifier = true;
            continue;
        }
        *out_end_index = index;
        return true;
    }
    if (expecting_identifier) {
        return false;
    }
    *out_end_index = index;
    return true;
}

static bool placeholder_scan_token_starts_select_lock_wait(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "NOWAIT") ||
           (placeholder_scan_token_text_equals(scan, index, "SKIP") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "LOCKED"));
}

static enum mylite_sql_parse_status try_parse_legacy_create_index_type_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t type_index = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_legacy_create_index_type(&scan, &type_index)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status = parse_legacy_create_index_type_tokens(config, &scan, type_index, &retry_result);
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_legacy_create_index_type(
    const struct placeholder_statement_scan *scan,
    size_t *out_type_index
) {
    enum { legacy_create_index_type_min_token_count = 7U };

    size_t index = 1U;
    size_t after_index_name = 0U;

    if (out_type_index != NULL) {
        *out_type_index = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count < legacy_create_index_type_min_token_count ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "UNIQUE")) {
        ++index;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "INDEX") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        return false;
    }

    after_index_name = index + 2U;
    return scan_legacy_create_index_type_prefix(scan, after_index_name, out_type_index) ||
           scan_legacy_create_index_type_suffix(scan, after_index_name, out_type_index);
}

static bool scan_legacy_create_index_type_prefix(
    const struct placeholder_statement_scan *scan,
    size_t after_index_name,
    size_t *out_type_index
) {
    if (!placeholder_scan_token_text_equals(scan, after_index_name, "TYPE") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, after_index_name + 1U) ||
        !placeholder_scan_token_text_equals(scan, after_index_name + 2U, "ON")) {
        return false;
    }
    if (out_type_index != NULL) {
        *out_type_index = after_index_name;
    }
    return true;
}

static bool scan_legacy_create_index_type_suffix(
    const struct placeholder_statement_scan *scan,
    size_t after_index_name,
    size_t *out_type_index
) {
    size_t index = after_index_name;
    size_t right_paren_index = 0U;

    if (!placeholder_scan_token_text_equals(scan, index, "ON")) {
        return false;
    }
    ++index;
    if (!placeholder_scan_match_column_name_starts_at(scan, index, &index) ||
        !token_is_left_paren(&scan->tokens[index]) ||
        !placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index)) {
        return false;
    }
    index = right_paren_index + 1U;
    if (!placeholder_scan_token_text_equals(scan, index, "TYPE") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        return false;
    }
    if (out_type_index != NULL) {
        *out_type_index = index;
    }
    return true;
}

static enum mylite_sql_parse_status parse_legacy_create_index_type_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t type_index,
    struct mylite_sql_parse_result *out_result
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_parser_token_history token_history = {0};
    void *parser = NULL;
    bool previous_token_was_dot = false;
    struct mylite_sql_token eof_token = {
        .kind = MYLITE_SQL_TOKEN_EOF,
        .text = config.input == NULL ? NULL : config.input + config.length,
        .offset = config.length,
    };
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_result == NULL || scan == NULL || scan->tokens == NULL ||
        type_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    reset_parse_result(out_result);

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .modes = config.modes,
        .accepted = false,
    };
    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        out_result->status = MYLITE_SQL_PARSE_NOMEM;
        return out_result->status;
    }

    for (size_t index = 0U; status == MYLITE_SQL_PARSE_OK && index < scan->token_count; ++index) {
        status = feed_token_with_parser_token_override(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[index],
            index == type_index ? MYLITE_SQL_PARSE_USING : 0
        );
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_token_with_parser_token_override(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &eof_token,
            0
        );
    }

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return out_result->status;
}

static enum mylite_sql_parse_status try_parse_placeholder_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    enum placeholder_statement_kind kind = PLACEHOLDER_STATEMENT_NONE;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    status =
        try_parse_alter_table_algorithm_lock_tail_statement(config, result, &scan, out_handled);
    if (*out_handled) {
        free(tokens);
        return status;
    }
    status = try_parse_create_table_partition_statement(config, result, &scan, out_handled);
    if (*out_handled) {
        free(tokens);
        return status;
    }
    status = try_parse_admin_set_residual_statement(config, result, &scan, out_handled);
    if (*out_handled) {
        free(tokens);
        return status;
    }

    kind = classify_placeholder_statement(&scan);
    if (kind == PLACEHOLDER_STATEMENT_NONE ||
        ((kind == PLACEHOLDER_STATEMENT_ADMIN_NOOP || kind == PLACEHOLDER_STATEMENT_UTILITY_NOOP) &&
         scan.has_non_trailing_semicolon)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    if (kind == PLACEHOLDER_STATEMENT_EXPLAIN) {
        status = finish_explain_placeholder_statement_parse(config, result, &scan);
    } else {
        status = finish_placeholder_statement_parse(config, result, kind);
    }
    *out_handled = true;
    free(tokens);
    return status;
}

static enum mylite_sql_parse_status try_parse_admin_set_residual_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    status = try_parse_plural_table_maintenance_statement(config, result, scan, out_handled);
    if (*out_handled) {
        return status;
    }
    status = try_parse_describe_explain_table_filter_statement(config, result, scan, out_handled);
    if (*out_handled) {
        return status;
    }
    status = try_parse_show_extended_metadata_statement(config, result, scan, out_handled);
    if (*out_handled) {
        return status;
    }
    return try_parse_set_system_variable_assign_statement(config, result, scan, out_handled);
}

static enum mylite_sql_parse_status try_parse_plural_table_maintenance_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *table_names = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_ast_node_kind statement_kind;
    size_t index = 1U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        (!placeholder_scan_token_text_equals(scan, 0U, "ANALYZE") &&
         !placeholder_scan_token_text_equals(scan, 0U, "OPTIMIZE"))) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (placeholder_scan_token_text_equals(scan, index, "NO_WRITE_TO_BINLOG") ||
        placeholder_scan_token_text_equals(scan, index, "LOCAL")) {
        ++index;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "TABLES")) {
        return MYLITE_SQL_PARSE_OK;
    }
    ++index;

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    table_names = placeholder_parse_table_name_list(&state, scan, &index);
    if (table_names == NULL || index != scan->token_count) {
        result->status = table_names == NULL && result->status != MYLITE_SQL_PARSE_OK
                             ? result->status
                             : MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }

    statement_kind = placeholder_scan_token_text_equals(scan, 0U, "ANALYZE")
                         ? MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT
                         : MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT;
    statement = mylite_sql_parser_make_table_maintenance_statement(
        &state,
        statement_kind,
        scan->tokens[0],
        table_names
    );
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static enum mylite_sql_parse_status try_parse_describe_explain_table_filter_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *table_name = NULL;
    struct mylite_sql_ast_node *filter = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    size_t index = 1U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        (!placeholder_scan_token_text_equals(scan, 0U, "DESCRIBE") &&
         !placeholder_scan_token_text_equals(scan, 0U, "DESC") &&
         !placeholder_scan_token_text_equals(scan, 0U, "EXPLAIN"))) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (!placeholder_scan_token_is_identifier_like(scan, index)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    table_name = placeholder_parse_table_name(&state, scan, &index);
    if (table_name == NULL || index >= scan->token_count) {
        result->status = table_name == NULL && result->status != MYLITE_SQL_PARSE_OK
                             ? result->status
                             : MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }
    if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_STRING) {
        filter = mylite_sql_parser_make_literal(
            &state,
            scan->tokens[index],
            MYLITE_SQL_AST_LITERAL_STRING
        );
    } else if (placeholder_scan_token_is_identifier_like(scan, index)) {
        filter = mylite_sql_parser_make_identifier(&state, scan->tokens[index]);
    } else {
        result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }
    ++index;
    if (filter == NULL || index != scan->token_count) {
        if (filter == NULL) {
            result->status =
                result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        } else {
            result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
        }
        *out_handled = true;
        return result->status;
    }

    statement = mylite_sql_parser_make_show_columns_statement(
        &state,
        scan->tokens[0],
        table_name,
        NULL,
        filter
    );
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static enum mylite_sql_parse_status try_parse_show_extended_metadata_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *table_name = NULL;
    struct mylite_sql_ast_node *schema_name = NULL;
    struct mylite_sql_ast_node *filter = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    bool full_columns = false;
    bool index_statement = false;
    bool columns_statement = false;
    size_t index = 0U;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (!scan_show_extended_metadata_statement(
            scan,
            &full_columns,
            &columns_statement,
            &index_statement,
            &index
        )) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    table_name = placeholder_parse_table_name(&state, scan, &index);
    if (table_name == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_SYNTAX_ERROR : result->status;
        *out_handled = true;
        return result->status;
    }
    status = parse_show_extended_metadata_tail(
        &state,
        scan,
        columns_statement,
        &index,
        &schema_name,
        &filter
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return status;
    }
    if (index != scan->token_count) {
        result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }

    statement = make_show_extended_metadata_statement(
        &state,
        scan,
        full_columns,
        index_statement,
        table_name,
        schema_name,
        filter
    );
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static bool scan_show_extended_metadata_statement(
    const struct placeholder_statement_scan *scan,
    bool *out_full_columns,
    bool *out_columns_statement,
    bool *out_index_statement,
    size_t *out_index
) {
    enum {
        show_extended_min_token_count = 5,
    };

    bool full_columns = false;
    bool columns_statement = false;
    bool index_statement = false;
    size_t keyword_index = 2U;

    if (out_full_columns == NULL || out_columns_statement == NULL || out_index_statement == NULL ||
        out_index == NULL) {
        return false;
    }
    *out_full_columns = false;
    *out_columns_statement = false;
    *out_index_statement = false;
    *out_index = 0U;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count < show_extended_min_token_count ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "SHOW") ||
        !placeholder_scan_token_text_equals(scan, 1U, "EXTENDED")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, keyword_index, "FULL")) {
        full_columns = true;
        ++keyword_index;
    }
    columns_statement = placeholder_scan_token_is_show_columns_keyword(scan, keyword_index);
    index_statement =
        !full_columns && placeholder_scan_token_is_show_index_keyword(scan, keyword_index);
    if ((!columns_statement && !index_statement) ||
        !placeholder_scan_token_is_schema_keyword(scan, keyword_index + 1U)) {
        return false;
    }

    *out_full_columns = full_columns;
    *out_columns_statement = columns_statement;
    *out_index_statement = index_statement;
    *out_index = keyword_index + 2U;
    return true;
}

static enum mylite_sql_parse_status parse_show_extended_metadata_tail(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    bool columns_statement,
    size_t *index,
    struct mylite_sql_ast_node **out_schema_name,
    struct mylite_sql_ast_node **out_filter
) {
    struct mylite_sql_parse_result *result = state == NULL ? NULL : state->result;

    if (state == NULL || result == NULL || scan == NULL || index == NULL ||
        out_schema_name == NULL || out_filter == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_schema_name = NULL;
    *out_filter = NULL;
    if (placeholder_scan_token_is_schema_keyword(scan, *index)) {
        ++*index;
        if (!placeholder_scan_token_is_identifier_like(scan, *index)) {
            result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
            return result->status;
        }
        *out_schema_name = mylite_sql_parser_make_identifier(state, scan->tokens[*index]);
        if (*out_schema_name == NULL) {
            result->status =
                result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
            return result->status;
        }
        ++*index;
    }
    if (columns_statement) {
        size_t filter_index = *index;
        *out_filter = placeholder_parse_show_like_filter(state, scan, index);
        if (result->status != MYLITE_SQL_PARSE_OK) {
            return result->status;
        }
        if (*out_filter == NULL && placeholder_scan_token_text_equals(scan, filter_index, "LIKE")) {
            result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
            return result->status;
        }
    }
    return result->status;
}

static struct mylite_sql_ast_node *make_show_extended_metadata_statement(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    bool full_columns,
    bool index_statement,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter
) {
    if (index_statement) {
        return mylite_sql_parser_make_show_index_statement(
            state,
            scan->tokens[0],
            table_name,
            schema_name,
            NULL
        );
    }
    if (full_columns) {
        return mylite_sql_parser_make_show_full_columns_statement(
            state,
            scan->tokens[0],
            table_name,
            schema_name,
            filter
        );
    }
    return mylite_sql_parser_make_show_columns_statement(
        state,
        scan->tokens[0],
        table_name,
        schema_name,
        filter
    );
}

static enum mylite_sql_parse_status try_parse_set_system_variable_assign_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *target = NULL;
    struct mylite_sql_ast_node *value = NULL;
    struct mylite_sql_ast_node *assignment = NULL;
    struct mylite_sql_ast_node *assignment_list = NULL;
    struct mylite_sql_ast_node *statement = NULL;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon || scan->token_count != 4U ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "SET") ||
        scan->tokens[1].kind != MYLITE_SQL_TOKEN_SYSTEM_VARIABLE ||
        !placeholder_scan_token_is_assignment_operator(scan, 2U) ||
        !placeholder_scan_token_is_set_system_variable_scalar_value(scan, 3U)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    target = mylite_sql_parser_make_set_system_variable_target(
        &state,
        NULL,
        mylite_sql_parser_make_system_variable(&state, scan->tokens[1])
    );
    if (target == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    value = placeholder_parse_set_system_variable_value(&state, scan, 3U);
    if (value == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    assignment = mylite_sql_parser_make_set_assignment(&state, target, scan->tokens[2], value);
    if (assignment == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    assignment_list = mylite_sql_parser_make_set_assignment_list(&state, assignment);
    if (assignment_list == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    statement = mylite_sql_parser_make_set_statement(&state, scan->tokens[0], assignment_list);
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static enum mylite_sql_parse_status init_scanned_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parser_state *out_state
) {
    if (result == NULL || out_state == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
    result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&result->ast);
    *out_state = (struct mylite_sql_parser_state){
        .result = result,
        .modes = config.modes,
        .accepted = true,
    };
    return result->status;
}

static enum mylite_sql_parse_status finish_scanned_statement_parse(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement
) {
    struct mylite_sql_ast_node *script = NULL;

    if (result == NULL || state == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (statement == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }
    script = mylite_sql_parser_make_script_with_statement(state, statement);
    if (script == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }
    mylite_sql_parser_state_set_root(state, script);
    return result->status;
}

static struct mylite_sql_ast_node *placeholder_parse_table_name(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    struct mylite_sql_ast_node *table_name = NULL;
    struct mylite_sql_ast_node *schema_name = NULL;

    if (state == NULL || scan == NULL || index == NULL ||
        !placeholder_scan_token_is_identifier_like(scan, *index)) {
        return NULL;
    }
    table_name = mylite_sql_parser_make_identifier(state, scan->tokens[*index]);
    ++*index;
    if (placeholder_scan_token_is_dot(scan, *index)) {
        ++*index;
        if (!placeholder_scan_token_is_identifier_like(scan, *index)) {
            return NULL;
        }
        schema_name = table_name;
        table_name = mylite_sql_parser_make_qualified_identifier(
            state,
            schema_name,
            mylite_sql_parser_make_identifier(state, scan->tokens[*index])
        );
        ++*index;
    }
    return table_name;
}

static struct mylite_sql_ast_node *placeholder_parse_table_name_list(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    struct mylite_sql_ast_node *table_name = placeholder_parse_table_name(state, scan, index);
    struct mylite_sql_ast_node *table_names = NULL;

    if (table_name == NULL) {
        return NULL;
    }
    table_names = mylite_sql_parser_make_table_name_list(state, table_name);
    while (index != NULL && *index < scan->token_count) {
        if (!token_is_comma(&scan->tokens[*index])) {
            return NULL;
        }
        ++*index;
        table_name = placeholder_parse_table_name(state, scan, index);
        if (table_name == NULL) {
            return NULL;
        }
        table_names = mylite_sql_parser_append_table_name(state, table_names, table_name);
    }
    return table_names;
}

static struct mylite_sql_ast_node *placeholder_parse_show_like_filter(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL || *index >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, *index, "LIKE")) {
        return NULL;
    }
    ++*index;
    if (*index >= scan->token_count || scan->tokens[*index].kind != MYLITE_SQL_TOKEN_STRING) {
        return NULL;
    }
    ++*index;
    return mylite_sql_parser_make_literal(
        state,
        scan->tokens[*index - 1U],
        MYLITE_SQL_AST_LITERAL_STRING
    );
}

static struct mylite_sql_ast_node *placeholder_parse_set_system_variable_value(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (state == NULL || scan == NULL || index >= scan->token_count) {
        return NULL;
    }
    token = &scan->tokens[index];
    if (placeholder_scan_token_text_equals(scan, index, "DEFAULT")) {
        return mylite_sql_parser_make_set_default_value(state, *token);
    }
    if (placeholder_scan_token_text_equals(scan, index, "TRUE") ||
        placeholder_scan_token_text_equals(scan, index, "ON")) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_TRUE);
    }
    if (placeholder_scan_token_text_equals(scan, index, "FALSE") ||
        placeholder_scan_token_text_equals(scan, index, "OFF")) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_FALSE);
    }
    if (placeholder_scan_token_text_equals(scan, index, "NULL")) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_NULL);
    }
    if (token->kind == MYLITE_SQL_TOKEN_INTEGER) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_INTEGER);
    }
    if (token->kind == MYLITE_SQL_TOKEN_DECIMAL) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_DECIMAL);
    }
    if (token->kind == MYLITE_SQL_TOKEN_FLOAT) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_FLOAT);
    }
    if (token->kind == MYLITE_SQL_TOKEN_STRING) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_STRING);
    }
    if (token->kind == MYLITE_SQL_TOKEN_HEX_LITERAL) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_HEX);
    }
    if (token->kind == MYLITE_SQL_TOKEN_BIT_LITERAL) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_BIT);
    }
    if (placeholder_scan_token_is_identifier_like(scan, index) ||
        placeholder_scan_token_text_equals(scan, index, "UTC") ||
        placeholder_scan_token_text_equals(scan, index, "SYSTEM") ||
        placeholder_scan_token_text_equals(scan, index, "SERIALIZABLE") ||
        placeholder_scan_token_text_equals(scan, index, "BINARY")) {
        return mylite_sql_parser_make_identifier(state, *token);
    }
    return NULL;
}

static bool placeholder_scan_token_is_set_system_variable_scalar_value(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return placeholder_scan_token_text_equals(scan, index, "DEFAULT") ||
           placeholder_scan_token_text_equals(scan, index, "TRUE") ||
           placeholder_scan_token_text_equals(scan, index, "ON") ||
           placeholder_scan_token_text_equals(scan, index, "FALSE") ||
           placeholder_scan_token_text_equals(scan, index, "OFF") ||
           placeholder_scan_token_text_equals(scan, index, "NULL") ||
           token->kind == MYLITE_SQL_TOKEN_INTEGER || token->kind == MYLITE_SQL_TOKEN_DECIMAL ||
           token->kind == MYLITE_SQL_TOKEN_FLOAT || token->kind == MYLITE_SQL_TOKEN_STRING ||
           token->kind == MYLITE_SQL_TOKEN_HEX_LITERAL ||
           token->kind == MYLITE_SQL_TOKEN_BIT_LITERAL ||
           placeholder_scan_token_is_identifier_like(scan, index) ||
           placeholder_scan_token_text_equals(scan, index, "UTC") ||
           placeholder_scan_token_text_equals(scan, index, "SYSTEM") ||
           placeholder_scan_token_text_equals(scan, index, "SERIALIZABLE") ||
           placeholder_scan_token_text_equals(scan, index, "BINARY");
}

static bool placeholder_scan_token_is_dot(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == '.';
}

static bool placeholder_scan_token_is_show_columns_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "COLUMNS") ||
           placeholder_scan_token_text_equals(scan, index, "FIELDS");
}

static bool placeholder_scan_token_is_show_index_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "INDEX") ||
           placeholder_scan_token_text_equals(scan, index, "INDEXES") ||
           placeholder_scan_token_text_equals(scan, index, "KEYS");
}

static bool placeholder_scan_token_is_schema_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "IN");
}

static enum mylite_sql_parse_status scan_placeholder_statement_tokens(
    struct mylite_sql_parse_config config,
    struct mylite_sql_token **out_tokens,
    size_t *out_token_count,
    bool *out_has_non_trailing_semicolon
) {
    struct mylite_sql_lexer lexer;
    struct mylite_sql_token *tokens = NULL;
    size_t token_count = 0U;
    size_t token_capacity = 0U;
    bool saw_semicolon = false;

    if (out_tokens == NULL || out_token_count == NULL || out_has_non_trailing_semicolon == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_tokens = NULL;
    *out_token_count = 0U;
    *out_has_non_trailing_semicolon = false;

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = config.input,
            .length = config.length,
            .modes = config.modes,
        }
    );
    {
        enum mylite_sql_parse_status status = scan_placeholder_lexer_tokens(
            &lexer,
            config.input,
            &tokens,
            &token_count,
            &token_capacity,
            &saw_semicolon,
            out_has_non_trailing_semicolon
        );

        if (status != MYLITE_SQL_PARSE_OK) {
            free(tokens);
            return status;
        }
    }

    *out_tokens = tokens;
    *out_token_count = token_count;
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status scan_placeholder_lexer_tokens(
    struct mylite_sql_lexer *lexer,
    const char *root_input,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity,
    bool *saw_semicolon,
    bool *out_has_non_trailing_semicolon
) {
    struct mylite_sql_lexer lexer_stack[version_comment_lexer_stack_limit];
    size_t lexer_count = 1U;

    if (lexer == NULL || tokens == NULL || token_count == NULL || token_capacity == NULL ||
        saw_semicolon == NULL || out_has_non_trailing_semicolon == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    lexer_stack[0] = *lexer;
    for (;;) {
        struct mylite_sql_token token;
        struct mylite_sql_lexer *current_lexer = &lexer_stack[lexer_count - 1U];
        bool done = false;
        enum mylite_sql_parse_status status;

        if (mylite_sql_lexer_next(current_lexer, &token) != 0) {
            return MYLITE_SQL_PARSE_MISUSE;
        }
        status = scan_placeholder_lexer_token(
            &token,
            current_lexer,
            root_input,
            lexer_stack,
            &lexer_count,
            tokens,
            token_count,
            token_capacity,
            saw_semicolon,
            out_has_non_trailing_semicolon,
            &done
        );

        if (status != MYLITE_SQL_PARSE_OK || done) {
            return status;
        }
    }
}

static enum mylite_sql_parse_status scan_placeholder_lexer_token(
    struct mylite_sql_token *token,
    struct mylite_sql_lexer *current_lexer,
    const char *root_input,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity,
    bool *saw_semicolon,
    bool *out_has_non_trailing_semicolon,
    bool *out_done
) {
    if (token == NULL || current_lexer == NULL || lexer_stack == NULL || lexer_count == NULL ||
        tokens == NULL || token_count == NULL || token_capacity == NULL || saw_semicolon == NULL ||
        out_has_non_trailing_semicolon == NULL || out_done == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_done = false;

    if (root_input != NULL && token->text != NULL && token->text >= root_input) {
        token->offset = (size_t)(token->text - root_input);
    }
    if (token->kind == MYLITE_SQL_TOKEN_EOF && *lexer_count > 1U) {
        --*lexer_count;
        return MYLITE_SQL_PARSE_OK;
    }
    if (token->kind == MYLITE_SQL_TOKEN_VERSION_COMMENT) {
        return push_version_comment_payload_lexer(
            token,
            current_lexer->modes,
            lexer_stack,
            lexer_count
        );
    }
    if (mylite_sql_parser_token_is_comment(token->kind)) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (token->kind == MYLITE_SQL_TOKEN_ERROR) {
        return MYLITE_SQL_PARSE_LEXER_ERROR;
    }
    if (token->kind == MYLITE_SQL_TOKEN_EOF) {
        *out_done = true;
        return MYLITE_SQL_PARSE_OK;
    }
    if (placeholder_token_is_semicolon(token)) {
        *saw_semicolon = true;
        return MYLITE_SQL_PARSE_OK;
    }
    if (*saw_semicolon) {
        *out_has_non_trailing_semicolon = true;
    }
    if (!append_placeholder_statement_token(token, tokens, token_count, token_capacity)) {
        return MYLITE_SQL_PARSE_NOMEM;
    }

    return MYLITE_SQL_PARSE_OK;
}

static bool append_placeholder_statement_token(
    const struct mylite_sql_token *token,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity
) {
    struct mylite_sql_token *grown = NULL;
    size_t new_capacity = 0U;

    if (*token_count == *token_capacity) {
        new_capacity =
            *token_capacity == 0U ? placeholder_initial_token_capacity : *token_capacity * 2U;
        if (new_capacity < *token_capacity) {
            return false;
        }
        grown = (struct mylite_sql_token *)realloc(*tokens, new_capacity * sizeof(**tokens));
        if (grown == NULL) {
            return false;
        }
        *tokens = grown;
        *token_capacity = new_capacity;
    }

    (*tokens)[*token_count] = *token;
    ++*token_count;
    return true;
}

static enum mylite_sql_parse_status try_parse_alter_table_algorithm_lock_tail_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();
    struct mylite_sql_parse_result prefix_result;
    struct mylite_sql_ast_node *statement = NULL;
    size_t option_index = 0U;
    size_t prefix_length = 0U;
    bool actionless = false;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !scan_alter_table_algorithm_lock_tail(scan, &option_index, &prefix_length, &actionless) ||
        !parse_alter_table_algorithm_lock_options(scan, option_index, &options)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (actionless) {
        if (!alter_table_algorithm_lock_options_are_known(options)) {
            return MYLITE_SQL_PARSE_OK;
        }
        *out_handled = true;
        return finish_placeholder_statement_parse(
            config,
            result,
            PLACEHOLDER_STATEMENT_UTILITY_NOOP
        );
    }

    memset(&prefix_result, 0, sizeof(prefix_result));
    status = parse_sql_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = prefix_length,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    statement = mylite_sql_parser_child_at(prefix_result.root, 0U);
    if (!alter_table_statement_accepts_prefix_option_tail(statement)) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    mylite_sql_parser_apply_alter_table_options(statement, options);
    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_alter_table_algorithm_lock_tail(
    const struct placeholder_statement_scan *scan,
    size_t *out_option_index,
    size_t *out_prefix_length,
    bool *out_actionless
) {
    size_t table_name_end = 0U;
    int paren_depth = 0;

    if (scan == NULL || out_option_index == NULL || out_prefix_length == NULL ||
        out_actionless == NULL || scan->token_count < 4U ||
        !placeholder_scan_token_text_equals(scan, 0U, "ALTER") ||
        !placeholder_scan_token_text_equals(scan, 1U, "TABLE")) {
        return false;
    }

    table_name_end = alter_table_placeholder_name_end_index(scan);
    if (table_name_end >= scan->token_count) {
        return false;
    }

    for (size_t index = table_name_end; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0 || (!placeholder_scan_token_text_equals(scan, index, "ALGORITHM") &&
                                 !placeholder_scan_token_text_equals(scan, index, "LOCK"))) {
            continue;
        }
        if (index == table_name_end) {
            *out_option_index = index;
            *out_prefix_length = 0U;
            *out_actionless = true;
            return true;
        }
        if (index > 0U && token_is_comma(&scan->tokens[index - 1U])) {
            *out_option_index = index;
            *out_prefix_length = scan->tokens[index - 1U].offset;
            *out_actionless = false;
            return true;
        }
    }
    return false;
}

static size_t alter_table_placeholder_name_end_index(const struct placeholder_statement_scan *scan
) {
    size_t index = 2U;

    if (scan == NULL) {
        return index;
    }
    if (index < scan->token_count) {
        ++index;
    }
    while (index + 1U < scan->token_count && placeholder_scan_token_text_equals(scan, index, ".")) {
        index += 2U;
    }
    return index;
}

static bool parse_alter_table_algorithm_lock_options(
    const struct placeholder_statement_scan *scan,
    size_t option_index,
    struct mylite_sql_alter_table_options *out_options
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();
    size_t index = option_index;

    if (scan == NULL || out_options == NULL || index >= scan->token_count) {
        return false;
    }

    for (;;) {
        if (!parse_alter_table_algorithm_lock_option(scan, &index, &options)) {
            return false;
        }
        if (index == scan->token_count) {
            *out_options = options;
            return true;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            return false;
        }
        ++index;
        if (index == scan->token_count) {
            return false;
        }
    }
}

static bool parse_alter_table_algorithm_lock_option(
    const struct placeholder_statement_scan *scan,
    size_t *index,
    struct mylite_sql_alter_table_options *options
) {
    struct mylite_sql_alter_table_options option;
    const struct mylite_sql_token *option_token = NULL;
    const struct mylite_sql_token *value_token = NULL;
    bool option_is_algorithm = false;

    if (scan == NULL || index == NULL || options == NULL || *index >= scan->token_count) {
        return false;
    }
    option_token = &scan->tokens[*index];
    option_is_algorithm = placeholder_scan_token_text_equals(scan, *index, "ALGORITHM");
    if (!option_is_algorithm && !placeholder_scan_token_text_equals(scan, *index, "LOCK")) {
        return false;
    }
    *index += 1U;
    if (*index < scan->token_count && token_is_equal_sign(&scan->tokens[*index])) {
        ++*index;
    }
    if (*index >= scan->token_count) {
        return false;
    }
    value_token = &scan->tokens[*index];
    if (!placeholder_scan_token_is_identifier_like(scan, *index) &&
        !placeholder_scan_token_text_equals(scan, *index, "DEFAULT") &&
        !placeholder_scan_token_text_equals(scan, *index, "NONE")) {
        return false;
    }

    if (option_is_algorithm) {
        option = mylite_sql_parser_make_alter_table_algorithm_option(
            *option_token,
            mylite_sql_parser_make_alter_algorithm_value(*value_token)
        );
    } else {
        option = mylite_sql_parser_make_alter_table_lock_option(
            *option_token,
            mylite_sql_parser_make_alter_lock_value(*value_token)
        );
    }
    *options = mylite_sql_parser_append_alter_table_option(*options, option);
    ++*index;
    return true;
}

static bool alter_table_statement_accepts_prefix_option_tail(
    const struct mylite_sql_ast_node *statement
) {
    if (statement == NULL) {
        return false;
    }
    switch (statement->kind) {
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT:
        return true;
    default:
        return false;
    }
}

static bool alter_table_algorithm_lock_options_are_known(
    struct mylite_sql_alter_table_options options
) {
    return options.algorithm != MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN &&
           options.lock != MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;
}

static enum mylite_sql_parse_status try_parse_create_table_partition_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parse_result prefix_result;
    size_t partition_index = 0U;
    size_t prefix_length = 0U;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->tokens == NULL ||
        !scan_is_create_table_partition_statement(scan, &partition_index)) {
        return MYLITE_SQL_PARSE_OK;
    }

    prefix_length = scan->tokens[partition_index].offset;
    status = parse_sql_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = prefix_length,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK && find_open_version_comment_start_before_token(
                                             config,
                                             &scan->tokens[partition_index],
                                             &prefix_length
                                         )) {
        mylite_sql_parse_result_deinit(&prefix_result);
        status = parse_sql_with_lemon(
            (struct mylite_sql_parse_config){
                .input = config.input,
                .length = prefix_length,
                .modes = config.modes,
            },
            &prefix_result
        );
    }
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool find_open_version_comment_start_before_token(
    struct mylite_sql_parse_config config,
    const struct mylite_sql_token *token,
    size_t *out_offset
) {
    const char *end = NULL;
    const char *found = NULL;

    if (config.input == NULL || token == NULL || token->text == NULL || out_offset == NULL ||
        token->text < config.input || token->text > config.input + config.length) {
        return false;
    }

    end = token->text;
    for (const char *cursor = config.input; cursor + 2 < end; ++cursor) {
        if (cursor[0] == '/' && cursor[1] == '*' && cursor[2] == '!') {
            found = cursor;
            cursor += 2;
            continue;
        }
        if (cursor[0] == '*' && cursor[1] == '/') {
            found = NULL;
            ++cursor;
        }
    }

    if (found == NULL) {
        return false;
    }
    *out_offset = (size_t)(found - config.input);
    return true;
}

static bool scan_is_create_table_partition_statement(
    const struct placeholder_statement_scan *scan,
    size_t *out_partition_index
) {
    size_t table_index = 1U;
    int paren_depth = 0;

    if (scan == NULL || scan->tokens == NULL || out_partition_index == NULL ||
        scan->token_count < create_table_partition_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, table_index, "TEMPORARY")) {
        ++table_index;
    }
    if (!placeholder_scan_token_text_equals(scan, table_index, "TABLE")) {
        return false;
    }

    for (size_t index = table_index + 1U; index + 1U < scan->token_count; ++index) {
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "PARTITION") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            create_table_partition_suffix_is_supported(scan, index)) {
            *out_partition_index = index;
            return true;
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return false;
}

static bool create_table_partition_suffix_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t partition_index
) {
    int paren_depth = 0;
    bool saw_method = false;
    bool saw_method_paren = false;

    if (partition_index + 2U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, partition_index, "PARTITION") ||
        !placeholder_scan_token_text_equals(scan, partition_index + 1U, "BY")) {
        return false;
    }
    for (size_t index = partition_index + 2U; index < scan->token_count; ++index) {
        if (paren_depth == 0 && (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
                                 placeholder_scan_token_text_equals(scan, index, "AS") ||
                                 placeholder_scan_token_text_equals(scan, index, "IGNORE") ||
                                 placeholder_scan_token_text_equals(scan, index, "REPLACE"))) {
            return false;
        }
        if (!saw_method && (placeholder_scan_token_text_equals(scan, index, "HASH") ||
                            placeholder_scan_token_text_equals(scan, index, "KEY") ||
                            placeholder_scan_token_text_equals(scan, index, "RANGE") ||
                            placeholder_scan_token_text_equals(scan, index, "LIST"))) {
            saw_method = true;
        }
        if (saw_method && token_is_left_paren(&scan->tokens[index])) {
            saw_method_paren = true;
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return saw_method && saw_method_paren && paren_depth == 0;
}

static bool create_table_select_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t table_index = 1U;

    if (scan == NULL || scan->tokens == NULL ||
        scan->token_count < create_table_select_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, table_index, "TEMPORARY")) {
        ++table_index;
    }
    if (!placeholder_scan_token_text_equals(scan, table_index, "TABLE") ||
        !create_table_select_scan_has_balanced_parentheses(scan, table_index + 1U)) {
        return false;
    }
    return create_table_select_scan_has_query_source(scan, table_index + 1U);
}

static bool create_table_select_scan_has_query_source(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (paren_depth == 0) {
            if (create_table_select_query_keyword_is_complete(scan, index)) {
                return true;
            }
            if (placeholder_scan_token_text_equals(scan, index, "AS") &&
                create_table_select_query_starts_at(scan, index + 1U)) {
                return true;
            }
            if (create_table_select_parenthesized_query_starts_at(scan, index)) {
                return true;
            }
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
        }
    }
    return false;
}

static bool create_table_select_query_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return create_table_select_query_keyword_is_complete(scan, index) ||
           create_table_select_parenthesized_query_starts_at(scan, index);
}

static bool create_table_select_parenthesized_query_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t query_start_index = index;
    int paren_depth = 0;

    if (scan == NULL || index + 1U >= scan->token_count ||
        !token_is_left_paren(&scan->tokens[index])) {
        return false;
    }
    while (query_start_index + 1U < scan->token_count &&
           token_is_left_paren(&scan->tokens[query_start_index + 1U])) {
        ++query_start_index;
    }
    if (query_start_index + 1U >= scan->token_count ||
        !create_table_select_query_keyword_is_complete(scan, query_start_index + 1U)) {
        return false;
    }
    for (size_t scan_index = index; scan_index < scan->token_count; ++scan_index) {
        if (token_is_left_paren(&scan->tokens[scan_index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[scan_index])) {
            --paren_depth;
            if (paren_depth == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool create_table_select_with_query_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    int paren_depth = 0;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, index, "WITH")) {
        return false;
    }
    for (size_t scan_index = index + 1U; scan_index < scan->token_count; ++scan_index) {
        if (paren_depth == 0 &&
            create_table_select_plain_query_keyword_is_complete(scan, scan_index)) {
            return scan_index + 1U < scan->token_count;
        }
        if (token_is_left_paren(&scan->tokens[scan_index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[scan_index])) {
            --paren_depth;
        }
    }
    return false;
}

static bool create_table_select_plain_query_keyword_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
            placeholder_scan_token_text_equals(scan, index, "TABLE") ||
            placeholder_scan_token_text_equals(scan, index, "VALUES")) &&
           index + 1U < scan->token_count;
}

static bool create_table_select_query_keyword_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    if (create_table_select_plain_query_keyword_is_complete(scan, index)) {
        return true;
    }
    return create_table_select_with_query_is_complete(scan, index);
}

static bool create_table_select_scan_has_balanced_parentheses(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    if (scan == NULL) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return paren_depth == 0;
}

static bool token_is_left_paren(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == '(';
}

static bool token_is_right_paren(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == ')';
}

static bool token_is_comma(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == ',';
}

static bool token_is_equal_sign(const struct mylite_sql_token *token) {
    return (token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
            token->text != NULL && token->text[0] == '=') ||
           (token != NULL && token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
            token->operator_kind == MYLITE_SQL_OPERATOR_EQUAL);
}

static bool token_is_string_literal(const struct mylite_sql_token *token) {
    return token != NULL && (token->kind == MYLITE_SQL_TOKEN_STRING ||
                             token->kind == MYLITE_SQL_TOKEN_NATIONAL_STRING);
}

static enum placeholder_statement_kind classify_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    enum placeholder_statement_kind kind;

    if (scan == NULL || scan->token_count == 0U) {
        return PLACEHOLDER_STATEMENT_NONE;
    }

    kind = classify_schema_security_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }

    kind = classify_stored_program_script_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }

    return classify_utility_admin_placeholder_statement(scan);
}

static enum placeholder_statement_kind classify_schema_security_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "CALL")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return classify_create_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "ALTER")) {
        return classify_alter_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "DROP")) {
        return classify_drop_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "RENAME") &&
        placeholder_scan_token_text_equals(scan, 1U, "USER")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "GRANT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REVOKE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }

    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_stored_program_script_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count == 0U ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_stored_program_statement_starts_at(scan, index)) {
            return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
        }
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool placeholder_scan_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "SIGNAL") ||
        placeholder_scan_token_text_equals(scan, index, "RESIGNAL")) {
        return true;
    }
    return placeholder_scan_create_stored_program_statement_starts_at(scan, index) ||
           placeholder_scan_alter_stored_program_statement_starts_at(scan, index) ||
           placeholder_scan_drop_stored_program_statement_starts_at(scan, index) ||
           placeholder_scan_show_stored_program_statement_starts_at(scan, index);
}

static bool placeholder_scan_create_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t limit = 0U;

    if (!placeholder_scan_token_text_equals(scan, index, "CREATE")) {
        return false;
    }
    limit = index + placeholder_create_scan_token_limit;
    if (limit > scan->token_count) {
        limit = scan->token_count;
    }
    for (size_t scan_index = index + 1U; scan_index < limit; ++scan_index) {
        if (placeholder_scan_token_is_stored_program_object_keyword(scan, scan_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_alter_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t limit = 0U;

    if (!placeholder_scan_token_text_equals(scan, index, "ALTER")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index + 1U, "PROCEDURE") ||
        placeholder_scan_token_text_equals(scan, index + 1U, "FUNCTION") ||
        placeholder_scan_token_text_equals(scan, index + 1U, "EVENT")) {
        return true;
    }
    if (!placeholder_scan_token_text_equals(scan, index + 1U, "DEFINER")) {
        return false;
    }
    limit = index + placeholder_create_scan_token_limit;
    if (limit > scan->token_count) {
        limit = scan->token_count;
    }
    for (size_t scan_index = index + 2U; scan_index < limit; ++scan_index) {
        if (placeholder_scan_token_text_equals(scan, scan_index, "EVENT")) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_drop_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "DROP") &&
           placeholder_scan_token_is_stored_program_object_keyword(scan, index + 1U);
}

static bool placeholder_scan_show_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SHOW") &&
           placeholder_scan_token_text_equals(scan, index + 1U, "CREATE") &&
           placeholder_scan_token_is_stored_program_object_keyword(scan, index + 2U);
}

static bool placeholder_scan_token_is_stored_program_object_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
           placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
           placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
           placeholder_scan_token_text_equals(scan, index, "EVENT");
}

static enum placeholder_statement_kind classify_utility_admin_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    enum placeholder_statement_kind kind;

    kind = classify_unsupported_utility_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    kind = classify_utility_noop_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    kind = classify_admin_noop_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "SET")) {
        return classify_set_placeholder_statement(scan);
    }
    kind = classify_query_surface_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "SHOW")) {
        return classify_show_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "DESCRIBE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DESC")) {
        return classify_describe_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "EXPLAIN")) {
        return classify_explain_placeholder_statement(scan);
    }

    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_unsupported_utility_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "XA") ||
        placeholder_scan_token_text_equals(scan, 0U, "HANDLER")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "GET") &&
        placeholder_scan_token_text_equals(scan, 1U, "DIAGNOSTICS")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (help_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "LOAD") &&
        placeholder_scan_token_text_equals(scan, 1U, "DATA")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (load_xml_placeholder_statement_is_supported(scan) ||
        select_into_file_placeholder_statement_is_supported(scan) ||
        import_table_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_utility_noop_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "ANALYZE") &&
        placeholder_scan_token_text_equals(scan, 1U, "TABLE") &&
        placeholder_scan_contains_text(scan, "HISTOGRAM")) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "INSTALL") &&
        (placeholder_scan_token_text_equals(scan, 1U, "COMPONENT") ||
         placeholder_scan_token_text_equals(scan, 1U, "PLUGIN"))) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "UNINSTALL") &&
        (placeholder_scan_token_text_equals(scan, 1U, "COMPONENT") ||
         placeholder_scan_token_text_equals(scan, 1U, "PLUGIN"))) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_admin_noop_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (lock_instance_placeholder_statement_is_supported(scan) ||
        unlock_instance_placeholder_statement_is_supported(scan) ||
        change_replication_source_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (flush_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "RESET") ||
        placeholder_scan_token_text_equals(scan, 0U, "PURGE") ||
        placeholder_scan_token_text_equals(scan, 0U, "KILL") ||
        placeholder_scan_token_text_equals(scan, 0U, "CACHE") ||
        placeholder_scan_token_text_equals(scan, 0U, "RESTART") ||
        placeholder_scan_token_text_equals(scan, 0U, "SHUTDOWN") ||
        placeholder_scan_token_text_equals(scan, 0U, "CLONE") ||
        placeholder_scan_token_text_equals(scan, 0U, "BINLOG")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "LOAD") &&
        placeholder_scan_token_text_equals(scan, 1U, "INDEX")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool flush_placeholder_statement_is_supported(const struct placeholder_statement_scan *scan
) {
    size_t index = 1U;

    if (scan == NULL || scan->token_count < 2U ||
        !placeholder_scan_token_text_equals(scan, 0U, "FLUSH")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "NO_WRITE_TO_BINLOG") ||
        placeholder_scan_token_text_equals(scan, index, "LOCAL")) {
        ++index;
    }
    if (index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "TABLE") ||
        placeholder_scan_token_text_equals(scan, index, "TABLES")) {
        return flush_placeholder_table_option_is_supported(scan, index);
    }
    for (;;) {
        if (!flush_placeholder_option_is_supported(scan, &index)) {
            return false;
        }
        if (index == scan->token_count) {
            return true;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            return false;
        }
        ++index;
        if (index == scan->token_count) {
            return false;
        }
    }
}

static bool flush_placeholder_option_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL || *index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, *index, "BINARY") ||
        placeholder_scan_token_text_equals(scan, *index, "ENGINE") ||
        placeholder_scan_token_text_equals(scan, *index, "ERROR") ||
        placeholder_scan_token_text_equals(scan, *index, "GENERAL") ||
        placeholder_scan_token_text_equals(scan, *index, "SLOW")) {
        if (!placeholder_scan_token_text_equals(scan, *index + 1U, "LOGS")) {
            return false;
        }
        *index += 2U;
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, *index, "RELAY")) {
        if (!placeholder_scan_token_text_equals(scan, *index + 1U, "LOGS")) {
            return false;
        }
        *index += 2U;
        if (placeholder_scan_token_text_equals(scan, *index, "FOR") &&
            placeholder_scan_token_text_equals(scan, *index + 1U, "CHANNEL")) {
            size_t channel_index = *index + 2U;

            if (channel_index >= scan->token_count ||
                (!placeholder_scan_token_is_identifier_like(scan, channel_index) &&
                 !token_is_string_literal(&scan->tokens[channel_index]))) {
                return false;
            }
            *index += 3U;
        }
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, *index, "LOGS") ||
        placeholder_scan_token_text_equals(scan, *index, "PRIVILEGES") ||
        placeholder_scan_token_text_equals(scan, *index, "OPTIMIZER_COSTS") ||
        placeholder_scan_token_text_equals(scan, *index, "STATUS") ||
        placeholder_scan_token_text_equals(scan, *index, "USER_RESOURCES")) {
        ++*index;
        return true;
    }
    return false;
}

static bool flush_placeholder_table_option_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t table_index
) {
    size_t index = table_index + 1U;

    if (scan == NULL || table_index >= scan->token_count) {
        return false;
    }
    if (index == scan->token_count) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "WITH") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "READ") &&
        placeholder_scan_token_text_equals(scan, index + 2U, "LOCK")) {
        return index + 3U == scan->token_count;
    }
    if (!flush_placeholder_table_name_list_is_supported(scan, &index)) {
        return false;
    }
    if (index == scan->token_count) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "WITH") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "READ") &&
        placeholder_scan_token_text_equals(scan, index + 2U, "LOCK")) {
        return index + 3U == scan->token_count;
    }
    if (placeholder_scan_token_text_equals(scan, index, "FOR") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "EXPORT")) {
        return index + 2U == scan->token_count;
    }
    return false;
}

static bool flush_placeholder_table_name_list_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (!flush_placeholder_table_name_is_supported(scan, index)) {
        return false;
    }
    while (*index < scan->token_count && token_is_comma(&scan->tokens[*index])) {
        ++*index;
        if (!flush_placeholder_table_name_is_supported(scan, index)) {
            return false;
        }
    }
    return true;
}

static bool flush_placeholder_table_name_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL ||
        !flush_placeholder_table_name_part_is_supported(scan, *index)) {
        return false;
    }
    ++*index;
    while (placeholder_scan_token_text_equals(scan, *index, ".") &&
           flush_placeholder_table_name_part_is_supported(scan, *index + 1U)) {
        *index += 2U;
    }
    return true;
}

static bool flush_placeholder_table_name_part_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_text_equals(scan, index, ".") ||
        placeholder_scan_token_text_equals(scan, index, "WITH") ||
        placeholder_scan_token_text_equals(scan, index, "FOR")) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
           token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER ||
           token->kind == MYLITE_SQL_TOKEN_KEYWORD;
}

static enum placeholder_statement_kind classify_query_surface_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (classify_cte_placeholder_statement(scan) != PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_scalar_expression_placeholder_statement(scan) !=
        PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_expression_clause_placeholder_statement(scan) !=
        PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_function_subquery_placeholder_statement(scan) !=
        PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_table_reference_placeholder_statement(scan) != PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_dml_variant_placeholder_statement(scan) != PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_cte_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t query_index = 0U;

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "WITH") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (!placeholder_scan_cte_clause_is_supported(scan, &query_index) ||
        !placeholder_scan_cte_query_statement_starts_at(scan, query_index)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
}

static bool placeholder_scan_cte_clause_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *out_query_index
) {
    size_t index = 1U;

    if (out_query_index != NULL) {
        *out_query_index = 0U;
    }
    if (scan == NULL || scan->token_count < cte_placeholder_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 0U, "WITH")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "RECURSIVE")) {
        ++index;
    }

    while (index < scan->token_count) {
        if (!placeholder_scan_cte_definition_is_supported(scan, &index)) {
            return false;
        }
        if (index >= scan->token_count || !token_is_comma(&scan->tokens[index])) {
            break;
        }
        ++index;
    }

    if (out_query_index != NULL) {
        *out_query_index = index;
    }
    return index < scan->token_count;
}

static bool placeholder_scan_cte_definition_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL || *index >= scan->token_count ||
        !placeholder_scan_token_is_identifier_like(scan, *index)) {
        return false;
    }
    ++*index;

    if (!placeholder_scan_cte_column_list_is_supported(scan, index)) {
        return false;
    }
    if (*index + 1U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, *index, "AS") ||
        !token_is_left_paren(&scan->tokens[*index + 1U])) {
        return false;
    }
    return placeholder_scan_cte_body_is_supported(scan, index);
}

static bool placeholder_scan_cte_column_list_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    size_t column_list_right_index = 0U;

    if (scan == NULL || index == NULL) {
        return false;
    }
    if (*index >= scan->token_count || !token_is_left_paren(&scan->tokens[*index])) {
        return true;
    }
    if (!placeholder_scan_find_matching_right_paren(scan, *index, &column_list_right_index)) {
        return false;
    }
    *index = column_list_right_index + 1U;
    return true;
}

static bool placeholder_scan_cte_body_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    size_t body_left_index = 0U;
    size_t body_right_index = 0U;

    if (scan == NULL || index == NULL || *index + 1U >= scan->token_count) {
        return false;
    }
    body_left_index = *index + 1U;
    if (!placeholder_scan_find_matching_right_paren(scan, body_left_index, &body_right_index) ||
        body_right_index <= body_left_index + 1U) {
        return false;
    }
    if (!placeholder_scan_query_expression_starts_at(scan, body_left_index + 1U) &&
        !create_table_select_parenthesized_query_starts_at(scan, body_left_index + 1U)) {
        return false;
    }
    *index = body_right_index + 1U;
    return true;
}

static bool placeholder_scan_cte_query_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_query_expression_starts_at(scan, index) ||
        create_table_select_parenthesized_query_starts_at(scan, index)) {
        return true;
    }
    return placeholder_scan_token_text_equals(scan, index, "UPDATE") ||
           placeholder_scan_token_text_equals(scan, index, "DELETE");
}

static enum placeholder_statement_kind classify_query_scalar_expression_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_lateral_derived_table(scan) ||
        placeholder_scan_contains_grouping_function(scan) ||
        placeholder_scan_contains_sounds_like_surface(scan) ||
        placeholder_scan_contains_deprecated_logical_operator_surface(scan) ||
        placeholder_scan_contains_like_surface(scan) ||
        placeholder_scan_contains_like_escape_surface(scan) ||
        placeholder_scan_contains_not_like_surface(scan) ||
        placeholder_scan_contains_typed_temporal_literal_surface(scan) ||
        placeholder_scan_contains_interval_expression_surface(scan) ||
        placeholder_scan_contains_scalar_in_surface(scan) ||
        placeholder_scan_contains_quantified_subquery_surface(scan) ||
        placeholder_scan_contains_fulltext_match_against_surface(scan) ||
        placeholder_scan_contains_expression_residual_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_query_expression_clause_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_query_expression_clause_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_query_function_subquery_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_query_function_subquery_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_query_table_reference_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_parenthesized_table_reference_surface(scan) ||
        placeholder_scan_contains_odbc_table_reference_surface(scan) ||
        placeholder_scan_contains_mixed_comma_explicit_join_surface(scan) ||
        placeholder_scan_contains_delayed_join_condition_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_dml_variant_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_dml_variant_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool placeholder_scan_starts_query_statement(const struct placeholder_statement_scan *scan) {
    if (scan == NULL || scan->token_count == 0U) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        placeholder_scan_token_text_equals(scan, 0U, "WITH") ||
        placeholder_scan_token_text_equals(scan, 0U, "TABLE") ||
        placeholder_scan_token_text_equals(scan, 0U, "VALUES") ||
        placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REPLACE") ||
        placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DELETE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DO")) {
        return true;
    }
    return placeholder_scan_parenthesized_start_is_query_statement(scan);
}

static bool placeholder_scan_parenthesized_start_is_query_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t index = 0U;

    if (scan == NULL) {
        return false;
    }
    while (index < scan->token_count && token_is_left_paren(&scan->tokens[index])) {
        ++index;
    }
    return index > 0U && (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
                          placeholder_scan_token_text_equals(scan, index, "WITH") ||
                          placeholder_scan_token_text_equals(scan, index, "TABLE") ||
                          placeholder_scan_token_text_equals(scan, index, "VALUES"));
}

static bool placeholder_scan_contains_parenthesized_table_reference_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_starts_parenthesized_table_reference_context(scan, index) &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_table_reference_body_starts_at(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_odbc_table_reference_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 3U < scan->token_count; ++index) {
        bool saw_join = false;

        if (!placeholder_scan_odbc_escape_starts_table_reference(scan, index) ||
            !placeholder_scan_token_text_equals(scan, index, "{") ||
            !placeholder_scan_token_text_equals(scan, index + 1U, "OJ")) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index < scan->token_count; ++scan_index) {
            if (placeholder_scan_token_is_join_keyword(scan, scan_index)) {
                saw_join = true;
            }
            if (placeholder_scan_token_text_equals(scan, scan_index, "}")) {
                if (saw_join) {
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

static bool placeholder_scan_odbc_escape_starts_table_reference(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t previous = 0U;

    if (scan == NULL || index == 0U) {
        return false;
    }
    previous = index - 1U;
    return placeholder_scan_token_text_equals(scan, previous, "FROM") ||
           placeholder_scan_token_is_join_keyword(scan, previous) ||
           token_is_comma(&scan->tokens[previous]);
}

static bool placeholder_scan_contains_mixed_comma_explicit_join_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;

    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "FROM") &&
            placeholder_scan_from_clause_contains_mixed_comma_explicit_join(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_delayed_join_condition_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;

    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "FROM") &&
            placeholder_scan_from_clause_contains_delayed_join_condition(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_from_clause_contains_mixed_comma_explicit_join(
    const struct placeholder_statement_scan *scan,
    size_t from_index
) {
    int paren_depth = 0;
    bool saw_comma = false;
    bool saw_join = false;

    if (scan == NULL || from_index + 1U >= scan->token_count) {
        return false;
    }
    for (size_t index = from_index + 1U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_stops_table_reference_clause(scan, index)) {
            break;
        }
        if (token_is_comma(&scan->tokens[index])) {
            saw_comma = true;
        } else if (placeholder_scan_token_is_join_keyword(scan, index)) {
            saw_join = true;
        }
        if (saw_comma && saw_join) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_from_clause_contains_delayed_join_condition(
    const struct placeholder_statement_scan *scan,
    size_t from_index
) {
    int paren_depth = 0;
    size_t joins_before_condition = 0U;

    if (scan == NULL || from_index + 1U >= scan->token_count) {
        return false;
    }
    for (size_t index = from_index + 1U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_stops_table_reference_clause(scan, index)) {
            break;
        }
        if (placeholder_scan_token_text_equals(scan, index, "ON") ||
            placeholder_scan_token_text_equals(scan, index, "USING")) {
            joins_before_condition = 0U;
            continue;
        }
        if (placeholder_scan_token_is_join_keyword(scan, index)) {
            ++joins_before_condition;
            if (joins_before_condition > 1U) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_token_stops_table_reference_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "WHERE") ||
           placeholder_scan_token_text_equals(scan, index, "GROUP") ||
           placeholder_scan_token_text_equals(scan, index, "HAVING") ||
           placeholder_scan_token_text_equals(scan, index, "WINDOW") ||
           placeholder_scan_token_text_equals(scan, index, "ORDER") ||
           placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "FOR") ||
           placeholder_scan_token_text_equals(scan, index, "LOCK") ||
           placeholder_scan_token_text_equals(scan, index, "INTO");
}

static bool placeholder_scan_parenthesized_table_reference_body_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    size_t right_paren_index = 0U;
    size_t first_index = left_paren_index + 1U;

    if (!placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= first_index ||
        placeholder_scan_query_expression_starts_at(scan, first_index)) {
        return false;
    }
    return placeholder_scan_parenthesized_table_reference_body_has_surface(
        scan,
        first_index,
        right_paren_index
    );
}

static bool placeholder_scan_parenthesized_table_reference_body_has_surface(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    bool saw_reference_name = false;

    if (scan == NULL || start_index >= end_index || end_index > scan->token_count ||
        placeholder_scan_query_expression_starts_at(scan, start_index)) {
        return false;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            size_t right_paren_index = 0U;

            if (!placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index) ||
                right_paren_index >= end_index) {
                return false;
            }
            if (!placeholder_scan_parenthesized_group_has_table_reference_name(
                    scan,
                    index,
                    right_paren_index
                )) {
                return false;
            }
            saw_reference_name = true;
            index = right_paren_index;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            return false;
        }
        if (placeholder_scan_token_is_table_reference_name(scan, index)) {
            saw_reference_name = true;
        }
        if (saw_reference_name && (token_is_comma(&scan->tokens[index]) ||
                                   placeholder_scan_token_is_join_keyword(scan, index))) {
            return true;
        }
    }
    return saw_reference_name;
}

static bool placeholder_scan_parenthesized_group_has_table_reference_name(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    size_t start_index = left_paren_index + 1U;
    size_t end_index = right_paren_index;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        right_paren_index > scan->token_count || right_paren_index <= left_paren_index ||
        !token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }

    while (start_index < end_index && token_is_left_paren(&scan->tokens[start_index])) {
        size_t nested_right_index = 0U;

        if (!placeholder_scan_find_matching_right_paren(scan, start_index, &nested_right_index) ||
            nested_right_index != end_index - 1U) {
            break;
        }
        ++start_index;
        --end_index;
    }

    if (start_index >= end_index ||
        placeholder_scan_query_expression_starts_at(scan, start_index)) {
        return false;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (placeholder_scan_token_is_table_reference_name(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_find_matching_right_paren(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            if (paren_depth == 0) {
                if (out_right_paren_index != NULL) {
                    *out_right_paren_index = index;
                }
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_query_expression_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, index, "WITH") ||
           placeholder_scan_token_text_equals(scan, index, "TABLE") ||
           placeholder_scan_token_text_equals(scan, index, "VALUES");
}

static bool placeholder_scan_token_starts_parenthesized_table_reference_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "JOIN") ||
           placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN");
}

static bool placeholder_scan_token_is_join_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "JOIN") ||
           placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN");
}

static bool placeholder_scan_token_is_table_reference_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_is_identifier_like(scan, index) ||
           placeholder_scan_token_text_equals(scan, index, "DUAL");
}

static bool placeholder_scan_contains_query_function_subquery_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    return placeholder_scan_contains_any_function_call_surface(scan) ||
           placeholder_scan_contains_quoted_function_call_surface(scan) ||
           placeholder_scan_contains_nested_function_call_surface(scan) ||
           placeholder_scan_contains_named_window_function_surface(scan) ||
           placeholder_scan_contains_parenthesized_query_expression_surface(scan);
}

static bool placeholder_scan_contains_any_function_call_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_contains_parameter_marker(scan) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index]) &&
            placeholder_scan_left_paren_starts_function_arguments(scan, index) &&
            placeholder_scan_token_is_generic_function_placeholder_name(scan, index - 1U) &&
            placeholder_scan_function_call_is_complete(scan, index - 1U, index, true)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_quoted_function_call_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_contains_parameter_marker(scan) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index]) && index > 0U &&
            scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER &&
            placeholder_scan_quoted_function_call_has_expression_context(scan, index - 1U) &&
            placeholder_scan_function_call_is_complete(scan, index - 1U, index, true)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_quoted_function_call_has_expression_context(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index
) {
    size_t previous_index = 0U;

    if (scan == NULL || function_name_index == 0U) {
        return false;
    }
    previous_index = function_name_index - 1U;
    return placeholder_scan_token_text_equals(scan, previous_index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, previous_index, "DISTINCT") ||
           placeholder_scan_token_text_equals(scan, previous_index, "DISTINCTROW") ||
           placeholder_scan_token_text_equals(scan, previous_index, "ALL") ||
           placeholder_scan_token_text_equals(scan, previous_index, "BY") ||
           placeholder_scan_token_starts_predicate_clause(scan, previous_index) ||
           token_is_comma(&scan->tokens[previous_index]) ||
           token_is_left_paren(&scan->tokens[previous_index]) ||
           placeholder_scan_token_is_assignment_operator(scan, previous_index) ||
           scan->tokens[previous_index].kind == MYLITE_SQL_TOKEN_OPERATOR;
}

static bool placeholder_scan_contains_parameter_marker(const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_PARAMETER) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_generic_function_placeholder_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "ROW")) {
        return false;
    }
    return !placeholder_scan_token_is_builtin_window_function_name(scan, index);
}

static bool placeholder_scan_contains_nested_function_call_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || !placeholder_scan_has_table_backed_or_dml_context(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index]) &&
            placeholder_scan_left_paren_starts_function_arguments(scan, index) &&
            placeholder_scan_nonempty_function_call_is_complete(scan, index - 1U, index) &&
            placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index) &&
            placeholder_scan_function_call_contains_nested_call(scan, index, right_paren_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_named_window_function_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || !placeholder_scan_has_table_backed_or_dml_context(scan) ||
        !placeholder_scan_contains_text(scan, "WINDOW")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (!token_is_left_paren(&scan->tokens[index]) ||
            !placeholder_scan_left_paren_starts_function_arguments(scan, index) ||
            !placeholder_scan_token_is_aggregate_window_function_name(scan, index - 1U) ||
            !placeholder_scan_nonempty_function_call_is_complete(scan, index - 1U, index) ||
            !placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index)) {
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "OVER") &&
            placeholder_scan_token_is_identifier_like(scan, right_paren_index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_nonempty_function_call_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index,
    size_t left_paren_index
) {
    return placeholder_scan_function_call_is_complete(
        scan,
        function_name_index,
        left_paren_index,
        false
    );
}

static bool placeholder_scan_function_call_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index,
    size_t left_paren_index,
    bool allow_empty_arguments
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || function_name_index + 1U != left_paren_index ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index)) {
        return false;
    }
    if (right_paren_index == left_paren_index + 1U) {
        if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "OVER") &&
            placeholder_scan_token_is_aggregate_window_function_name(scan, function_name_index)) {
            return true;
        }
        return allow_empty_arguments &&
               placeholder_scan_token_can_follow_function_call_surface(scan, right_paren_index);
    }
    if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "OVER") &&
        placeholder_scan_token_is_aggregate_window_function_name(scan, function_name_index)) {
        return true;
    }
    return placeholder_scan_function_call_arguments_are_well_formed(
               scan,
               left_paren_index,
               right_paren_index
           ) &&
           placeholder_scan_token_can_follow_function_call_surface(scan, right_paren_index);
}

static bool placeholder_scan_function_call_contains_nested_call(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    if (scan == NULL || left_paren_index + 1U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        size_t nested_right_paren_index = 0U;

        if (!token_is_left_paren(&scan->tokens[index]) ||
            !placeholder_scan_left_paren_starts_function_arguments(scan, index) ||
            !placeholder_scan_find_matching_right_paren(scan, index, &nested_right_paren_index) ||
            nested_right_paren_index >= right_paren_index ||
            nested_right_paren_index == index + 1U) {
            continue;
        }
        if (placeholder_scan_function_call_arguments_are_well_formed(
                scan,
                index,
                nested_right_paren_index
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_function_call_arguments_are_well_formed(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    int paren_depth = 0;
    bool previous_top_level_token_was_separator = true;

    if (scan == NULL || left_paren_index + 1U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            previous_top_level_token_was_separator = false;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            if (paren_depth == 0) {
                previous_top_level_token_was_separator = false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            previous_top_level_token_was_separator = false;
            continue;
        }
        if (previous_top_level_token_was_separator) {
            return false;
        }
        previous_top_level_token_was_separator = true;
    }
    return paren_depth == 0 && !previous_top_level_token_was_separator;
}

static bool placeholder_scan_token_can_follow_function_call_surface(
    const struct placeholder_statement_scan *scan,
    size_t right_paren_index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || right_paren_index + 1U >= scan->token_count) {
        return true;
    }
    if (token_is_comma(&scan->tokens[right_paren_index + 1U]) ||
        token_is_right_paren(&scan->tokens[right_paren_index + 1U]) ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "AS") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "FROM") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "WHERE") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "GROUP") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "HAVING") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "ORDER") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "WINDOW") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "LIMIT") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "WITH") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "AND") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "ASC") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "DESC") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "UNION") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "ON") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IS") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IN") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "LIKE") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "REGEXP") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "RLIKE") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "BETWEEN")) {
        return true;
    }
    if (placeholder_scan_token_is_comparison_operator(scan, right_paren_index + 1U)) {
        return true;
    }
    token = &scan->tokens[right_paren_index + 1U];
    if (token->kind != MYLITE_SQL_TOKEN_OPERATOR) {
        return false;
    }
    return right_paren_index + 2U < scan->token_count &&
           !placeholder_scan_token_stops_expression_clause_search(scan, right_paren_index + 2U);
}

static bool placeholder_scan_token_is_aggregate_window_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "AVG") ||
        placeholder_scan_token_text_equals(scan, index, "BIT_AND") ||
        placeholder_scan_token_text_equals(scan, index, "BIT_OR") ||
        placeholder_scan_token_text_equals(scan, index, "BIT_XOR") ||
        placeholder_scan_token_text_equals(scan, index, "COUNT") ||
        placeholder_scan_token_text_equals(scan, index, "GROUP_CONCAT") ||
        placeholder_scan_token_text_equals(scan, index, "JSON_ARRAYAGG") ||
        placeholder_scan_token_text_equals(scan, index, "JSON_OBJECTAGG") ||
        placeholder_scan_token_text_equals(scan, index, "MAX") ||
        placeholder_scan_token_text_equals(scan, index, "MIN") ||
        placeholder_scan_token_text_equals(scan, index, "STDDEV") ||
        placeholder_scan_token_text_equals(scan, index, "STDDEV_POP") ||
        placeholder_scan_token_text_equals(scan, index, "STDDEV_SAMP") ||
        placeholder_scan_token_text_equals(scan, index, "SUM") ||
        placeholder_scan_token_text_equals(scan, index, "VAR_POP") ||
        placeholder_scan_token_text_equals(scan, index, "VAR_SAMP") ||
        placeholder_scan_token_text_equals(scan, index, "VARIANCE")) {
        return true;
    }
    return false;
}

static bool placeholder_scan_token_is_builtin_window_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "CUME_DIST") ||
        placeholder_scan_token_text_equals(scan, index, "DENSE_RANK") ||
        placeholder_scan_token_text_equals(scan, index, "FIRST_VALUE") ||
        placeholder_scan_token_text_equals(scan, index, "LAG") ||
        placeholder_scan_token_text_equals(scan, index, "LAST_VALUE") ||
        placeholder_scan_token_text_equals(scan, index, "LEAD") ||
        placeholder_scan_token_text_equals(scan, index, "NTH_VALUE") ||
        placeholder_scan_token_text_equals(scan, index, "NTILE") ||
        placeholder_scan_token_text_equals(scan, index, "PERCENT_RANK") ||
        placeholder_scan_token_text_equals(scan, index, "RANK") ||
        placeholder_scan_token_text_equals(scan, index, "ROW_NUMBER")) {
        return true;
    }
    return false;
}

static bool placeholder_scan_contains_parenthesized_query_expression_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "REPLACE") ||
        placeholder_scan_token_text_equals(scan, 0U, "WITH")) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index]) &&
            placeholder_scan_query_expression_starts_at(scan, index + 1U) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_query_expression_clause_surface(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_contains_query_final_tail_surface(scan) ||
        placeholder_scan_contains_odbc_expression_escape_surface(scan) ||
        placeholder_scan_contains_postfix_is_predicate_surface(scan) ||
        placeholder_scan_contains_between_expression_surface(scan) ||
        placeholder_scan_contains_row_constructor_predicate_surface(scan) ||
        placeholder_scan_contains_string_order_key_surface(scan) ||
        placeholder_scan_contains_constant_order_key_surface(scan) ||
        placeholder_scan_contains_having_residual_surface(scan)) {
        return true;
    }
    if (!placeholder_scan_has_table_backed_or_dml_context(scan)) {
        return false;
    }
    return placeholder_scan_contains_json_arrow_surface(scan) ||
           placeholder_scan_contains_expression_operator_surface(scan) ||
           placeholder_scan_contains_row_tuple_predicate_surface(scan) ||
           placeholder_scan_contains_descriptor_in_list_surface(scan) ||
           placeholder_scan_contains_literal_left_predicate_surface(scan) ||
           placeholder_scan_contains_group_by_user_variable_assignment_surface(scan) ||
           placeholder_scan_contains_bare_truth_clause_surface(scan);
}

static bool placeholder_scan_contains_query_final_tail_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_starts_query_statement(scan)) {
        return false;
    }
    return placeholder_scan_contains_compound_query_final_tail_surface(scan) ||
           placeholder_scan_contains_select_limit_into_locking_permutation(scan);
}

static bool placeholder_scan_contains_compound_query_final_tail_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;
    bool saw_set_operation = false;

    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_top_level_token_starts_set_operation(scan, index)) {
            saw_set_operation = true;
            continue;
        }
        if (saw_set_operation &&
            placeholder_scan_top_level_token_starts_final_query_tail(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_select_limit_into_locking_permutation(
    const struct placeholder_statement_scan *scan
) {
    size_t limit_index = 0U;
    size_t into_index = 0U;
    size_t locking_index = 0U;
    size_t locking_end_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_find_top_level_keyword(scan, "LIMIT", &limit_index)) {
        return false;
    }
    if (placeholder_scan_find_top_level_keyword_after(
            scan,
            "INTO",
            limit_index + 1U,
            &into_index
        ) &&
        placeholder_scan_into_user_variable_list_is_complete(scan, into_index)) {
        return true;
    }
    for (size_t index = limit_index + 1U; index < scan->token_count; ++index) {
        if (!placeholder_scan_token_starts_select_locking_clause(scan, index) ||
            !scan_select_locking_clause_end(scan, index, &locking_end_index)) {
            continue;
        }
        locking_index = index;
        if (locking_end_index < scan->token_count &&
            placeholder_scan_token_text_equals(scan, locking_end_index, "INTO") &&
            placeholder_scan_into_user_variable_list_is_complete(scan, locking_end_index)) {
            return true;
        }
        if (placeholder_scan_find_top_level_keyword_after(
                scan,
                "INTO",
                locking_index + 1U,
                &into_index
            ) &&
            placeholder_scan_into_user_variable_list_is_complete(scan, into_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_top_level_token_starts_set_operation(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "EXCEPT") ||
           placeholder_scan_token_text_equals(scan, index, "INTERSECT");
}

static bool placeholder_scan_top_level_token_starts_final_query_tail(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t locking_end_index = 0U;

    if (placeholder_scan_token_text_equals(scan, index, "ORDER") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
        !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 2U)) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "LIMIT") &&
        !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U)) {
        return true;
    }
    if (placeholder_scan_into_user_variable_list_is_complete(scan, index)) {
        return true;
    }
    if (!scan_select_locking_clause_end(scan, index, &locking_end_index) ||
        locking_end_index <= index) {
        return false;
    }
    return locking_end_index >= scan->token_count ||
           placeholder_scan_into_user_variable_list_is_complete(scan, locking_end_index);
}

static bool placeholder_scan_into_user_variable_list_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t into_index
) {
    bool need_variable = true;
    bool saw_variable = false;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, into_index, "INTO")) {
        return false;
    }
    for (size_t index = into_index + 1U; index < scan->token_count; ++index) {
        if (!need_variable && (placeholder_scan_token_starts_select_locking_clause(scan, index) ||
                               placeholder_scan_token_text_equals(scan, index, "ORDER") ||
                               placeholder_scan_token_text_equals(scan, index, "LIMIT"))) {
            return saw_variable;
        }
        if (need_variable) {
            if (scan->tokens[index].kind != MYLITE_SQL_TOKEN_USER_VARIABLE) {
                return false;
            }
            need_variable = false;
            saw_variable = true;
            continue;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            return false;
        }
        need_variable = true;
    }
    return saw_variable && !need_variable;
}

static bool placeholder_scan_contains_json_arrow_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index];

        if (token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
            (token->operator_kind == MYLITE_SQL_OPERATOR_JSON_EXTRACT ||
             token->operator_kind == MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT) &&
            placeholder_scan_json_arrow_has_operand_context(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_json_arrow_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return scan != NULL && index > 0U && index + 1U < scan->token_count &&
           placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
           scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_STRING;
}

static bool placeholder_scan_contains_odbc_expression_escape_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        size_t right_brace_index = 0U;

        if (!placeholder_scan_token_text_equals(scan, index, "{") ||
            placeholder_scan_token_text_equals(scan, index + 1U, "OJ") ||
            !placeholder_scan_token_is_odbc_expression_escape_name(scan, index + 1U) ||
            !placeholder_scan_find_matching_right_brace(scan, index, &right_brace_index) ||
            right_brace_index <= index + 2U ||
            placeholder_scan_token_is_incomplete_statement_tail(scan, right_brace_index - 1U)) {
            continue;
        }
        return true;
    }
    return false;
}

static bool placeholder_scan_token_is_odbc_expression_escape_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "FN") ||
           placeholder_scan_token_text_equals(scan, index, "D") ||
           placeholder_scan_token_text_equals(scan, index, "T") ||
           placeholder_scan_token_text_equals(scan, index, "TS");
}

static bool placeholder_scan_find_matching_right_brace(
    const struct placeholder_statement_scan *scan,
    size_t left_brace_index,
    size_t *out_right_brace_index
) {
    int brace_depth = 0;

    if (scan == NULL || left_brace_index >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, left_brace_index, "{")) {
        return false;
    }
    for (size_t index = left_brace_index; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "{")) {
            ++brace_depth;
        } else if (placeholder_scan_token_text_equals(scan, index, "}")) {
            --brace_depth;
            if (brace_depth < 0) {
                return false;
            }
            if (brace_depth == 0) {
                if (out_right_brace_index != NULL) {
                    *out_right_brace_index = index;
                }
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_postfix_is_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        size_t value_index = index + 1U;

        if (!placeholder_scan_token_text_equals(scan, index, "IS") ||
            !placeholder_scan_token_can_end_expression_operand(scan, index - 1U)) {
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, value_index, "NOT")) {
            ++value_index;
        }
        if ((placeholder_scan_token_text_equals(scan, value_index, "NULL") ||
             placeholder_scan_token_text_equals(scan, value_index, "TRUE") ||
             placeholder_scan_token_text_equals(scan, value_index, "FALSE") ||
             placeholder_scan_token_text_equals(scan, value_index, "UNKNOWN")) &&
            !placeholder_scan_token_text_equals(scan, value_index + 1U, "IS") &&
            !(index >= 2U && placeholder_scan_token_text_equals(scan, index - 2U, "IS")) &&
            !(index >= 3U && placeholder_scan_token_text_equals(scan, index - 2U, "NOT") &&
              placeholder_scan_token_text_equals(scan, index - 3U, "IS"))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_between_expression_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "BETWEEN") &&
            placeholder_scan_between_expression_has_operand_context(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_between_expression_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t between_index
) {
    int paren_depth = 0;
    bool left_operand_is_complete = false;

    if (scan == NULL || between_index == 0U || between_index + 2U >= scan->token_count ||
        !placeholder_scan_token_can_start_expression_operand(scan, between_index + 1U)) {
        return false;
    }
    left_operand_is_complete =
        placeholder_scan_token_can_end_expression_operand(scan, between_index - 1U) ||
        (between_index > 1U &&
         placeholder_scan_token_text_equals(scan, between_index - 1U, "NOT") &&
         placeholder_scan_token_can_end_expression_operand(scan, between_index - 2U));
    if (!left_operand_is_complete) {
        return false;
    }
    for (size_t index = between_index + 1U; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > between_index + 1U &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "AND")) {
            return index > between_index + 1U &&
                   placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
                   placeholder_scan_token_can_start_expression_operand(scan, index + 1U) &&
                   placeholder_scan_between_bounds_contain_qualified_identifier(
                       scan,
                       between_index + 1U,
                       index
                   );
        }
    }
    return false;
}

static bool placeholder_scan_between_bounds_contain_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t lower_start_index,
    size_t and_index
) {
    int paren_depth = 0;
    size_t upper_end_index = 0U;

    if (scan == NULL || lower_start_index >= and_index || and_index + 1U >= scan->token_count) {
        return false;
    }
    upper_end_index = scan->token_count;
    for (size_t index = and_index + 1U; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > and_index + 1U &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            upper_end_index = index;
            break;
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return placeholder_scan_tokens_contain_qualified_identifier(
               scan,
               lower_start_index,
               and_index
           ) ||
           placeholder_scan_tokens_contain_qualified_identifier(
               scan,
               and_index + 1U,
               upper_end_index
           );
}

static bool placeholder_scan_contains_descriptor_in_list_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (placeholder_scan_token_text_equals(scan, index, "IN") &&
            placeholder_scan_scalar_in_is_descriptor_predicate(scan, index) &&
            placeholder_scan_in_predicate_has_operand_context(scan, index) &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_find_matching_right_paren(scan, index + 1U, &right_paren_index) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U) &&
            placeholder_scan_tokens_contain_qualified_identifier(
                scan,
                index + 2U,
                right_paren_index
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_in_predicate_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t in_index
) {
    if (scan == NULL || in_index == 0U) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, in_index - 1U, "NOT")) {
        return in_index > 1U &&
               placeholder_scan_token_can_end_expression_operand(scan, in_index - 2U);
    }
    return placeholder_scan_token_can_end_expression_operand(scan, in_index - 1U);
}

static bool placeholder_scan_contains_literal_left_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        if (!token_is_string_literal(&scan->tokens[index]) ||
            !placeholder_scan_literal_predicate_has_clause_context(scan, index)) {
            continue;
        }
        if (placeholder_scan_literal_left_comparison_is_complete(scan, index) ||
            placeholder_scan_literal_left_between_is_complete(scan, index) ||
            placeholder_scan_literal_left_in_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_literal_left_comparison_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    size_t right_index = literal_index + 2U;

    if (scan == NULL || right_index >= scan->token_count ||
        !placeholder_scan_token_is_comparison_operator(scan, literal_index + 1U) ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, right_index) ||
        placeholder_scan_token_stops_expression_clause_search(scan, right_index)) {
        return false;
    }
    return placeholder_scan_token_can_name_loose_identifier(scan, right_index);
}

static bool placeholder_scan_literal_left_between_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    int paren_depth = 0;
    size_t lower_start_index = literal_index + 2U;

    if (scan == NULL || lower_start_index >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, literal_index + 1U, "BETWEEN") ||
        !placeholder_scan_token_can_start_expression_operand(scan, lower_start_index)) {
        return false;
    }
    for (size_t index = lower_start_index + 1U; index + 1U < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (placeholder_scan_token_text_equals(scan, index, "AND") &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U) &&
            (placeholder_scan_tokens_contain_identifier_like(scan, lower_start_index, index) ||
             placeholder_scan_token_can_name_loose_identifier(scan, index + 1U))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_literal_left_in_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    size_t left_paren_index = literal_index + 2U;
    size_t right_paren_index = 0U;

    return scan != NULL && left_paren_index < scan->token_count &&
           placeholder_scan_token_text_equals(scan, literal_index + 1U, "IN") &&
           token_is_left_paren(&scan->tokens[left_paren_index]) &&
           placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, left_paren_index) &&
           placeholder_scan_tokens_contain_identifier_like(
               scan,
               left_paren_index + 1U,
               right_paren_index
           );
}

static bool placeholder_scan_literal_predicate_has_clause_context(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    if (scan == NULL || literal_index == 0U) {
        return false;
    }
    for (size_t scan_index = literal_index; scan_index > 0U; --scan_index) {
        size_t previous_index = scan_index - 1U;

        if (placeholder_scan_token_starts_predicate_clause(scan, previous_index)) {
            return true;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, previous_index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_tokens_contain_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    if (scan == NULL || start_index >= end_index || end_index > scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index + 2U < end_index; ++index) {
        if (placeholder_scan_token_can_name_loose_identifier(scan, index) &&
            placeholder_scan_token_text_equals(scan, index + 1U, ".") &&
            placeholder_scan_token_can_name_loose_identifier(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_tokens_contain_identifier_like(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    if (scan == NULL || start_index >= end_index || end_index > scan->token_count) {
        return false;
    }
    if (placeholder_scan_tokens_contain_qualified_identifier(scan, start_index, end_index)) {
        return true;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (placeholder_scan_token_is_identifier_like(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_row_constructor_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_row_constructor_starts_at(scan, index, &right_paren_index)) {
            continue;
        }
        if (placeholder_scan_token_is_comparison_operator(scan, right_paren_index + 1U) &&
            placeholder_scan_token_can_start_expression_operand(scan, right_paren_index + 2U)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IN") ||
            (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NOT") &&
             placeholder_scan_token_text_equals(scan, right_paren_index + 2U, "IN"))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_row_constructor_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t row_index,
    size_t *out_right_paren_index
) {
    size_t left_paren_index = row_index + 1U;
    size_t right_paren_index = 0U;

    if (!placeholder_scan_token_text_equals(scan, row_index, "ROW") ||
        left_paren_index >= scan->token_count ||
        !token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        !placeholder_scan_parenthesized_list_contains_top_level_comma(
            scan,
            left_paren_index,
            right_paren_index
        )) {
        return false;
    }
    if (out_right_paren_index != NULL) {
        *out_right_paren_index = right_paren_index;
    }
    return true;
}

static bool placeholder_scan_parenthesized_list_contains_top_level_comma(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= right_paren_index ||
        right_paren_index >= scan->token_count) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && token_is_comma(&scan->tokens[index])) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_string_order_key_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    if (!placeholder_scan_token_text_equals(scan, 0U, "VALUES") &&
        !placeholder_scan_token_text_equals(scan, 0U, "VALUE")) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index + 2U];

        if (placeholder_scan_token_text_equals(scan, index, "ORDER") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            (token->kind == MYLITE_SQL_TOKEN_STRING ||
             token->kind == MYLITE_SQL_TOKEN_NATIONAL_STRING)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_constant_order_key_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;

    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_starts_query_statement(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "ORDER") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            placeholder_scan_token_is_constant_order_key_surface(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_constant_order_key_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_STRING ||
           token->kind == MYLITE_SQL_TOKEN_NATIONAL_STRING ||
           token->kind == MYLITE_SQL_TOKEN_USER_VARIABLE ||
           token->kind == MYLITE_SQL_TOKEN_SYSTEM_VARIABLE ||
           placeholder_scan_token_text_equals(scan, index, "NULL") ||
           placeholder_scan_token_text_equals(scan, index, "TRUE") ||
           placeholder_scan_token_text_equals(scan, index, "FALSE");
}

static bool placeholder_scan_contains_having_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t having_index = 0U;

    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_find_top_level_keyword(scan, "HAVING", &having_index)) {
        return false;
    }
    return placeholder_scan_having_clause_contains_residual(scan, having_index + 1U);
}

static bool placeholder_scan_having_clause_contains_residual(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    if (scan == NULL || start_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > start_index &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_is_comparison_operator(scan, index) && index > start_index &&
            placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            return true;
        }
        if (placeholder_scan_having_in_predicate_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_having_in_predicate_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t in_index
) {
    size_t operand_index = 0U;
    size_t right_paren_index = 0U;

    if (scan == NULL || in_index == 0U || in_index + 1U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, in_index, "IN")) {
        return false;
    }
    operand_index = in_index - 1U;
    if (placeholder_scan_token_text_equals(scan, operand_index, "NOT")) {
        if (operand_index == 0U) {
            return false;
        }
        --operand_index;
    }
    return placeholder_scan_token_can_end_expression_operand(scan, operand_index) &&
           token_is_left_paren(&scan->tokens[in_index + 1U]) &&
           placeholder_scan_find_matching_right_paren(scan, in_index + 1U, &right_paren_index) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, in_index + 1U);
}

static bool placeholder_scan_has_table_backed_or_dml_context(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REPLACE") ||
        placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return true;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "FROM") ||
            placeholder_scan_token_text_equals(scan, index, "JOIN")) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_expression_operator_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_starts_predicate_clause(scan, index) &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
        if ((placeholder_scan_token_text_equals(scan, index, "GROUP") ||
             placeholder_scan_token_text_equals(scan, index, "ORDER")) &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 2U)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "SET") &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
        if ((placeholder_scan_token_text_equals(scan, index, "VALUES") ||
             placeholder_scan_token_text_equals(scan, index, "VALUE")) &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
        if (placeholder_scan_token_starts_duplicate_update_assignment_clause(scan, index) &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_expression_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_contains_parameter_marker(scan) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        placeholder_scan_contains_malformed_function_argument_surface(scan)) {
        return false;
    }
    return placeholder_scan_contains_select_symbolic_not_arithmetic_surface(scan) ||
           placeholder_scan_contains_targeted_comparison_surface(scan) ||
           placeholder_scan_contains_identifier_between_residual_surface(scan) ||
           placeholder_scan_contains_identifier_in_list_residual_surface(scan) ||
           placeholder_scan_contains_insert_keyword_group_function_surface(scan) ||
           placeholder_scan_contains_group_by_user_variable_assignment_surface(scan) ||
           placeholder_scan_contains_charset_introducer_string_surface(scan) ||
           placeholder_scan_contains_current_temporal_keyword_surface(scan);
}

static bool placeholder_scan_contains_select_symbolic_not_arithmetic_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    if (!placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        bool saw_arithmetic = false;

        if (placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (scan->tokens[index].kind != MYLITE_SQL_TOKEN_OPERATOR ||
            scan->tokens[index].operator_kind != MYLITE_SQL_OPERATOR_NOT ||
            !placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index < scan->token_count; ++scan_index) {
            if (placeholder_scan_token_text_equals(scan, scan_index, "FROM")) {
                return saw_arithmetic &&
                       placeholder_scan_token_text_equals(scan, scan_index + 1U, "DUAL");
            }
            if (scan->tokens[scan_index].kind == MYLITE_SQL_TOKEN_OPERATOR &&
                scan->tokens[scan_index].operator_kind == MYLITE_SQL_OPERATOR_STAR &&
                placeholder_scan_expression_operator_surface_has_operand_context(
                    scan,
                    scan_index
                )) {
                saw_arithmetic = true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_targeted_comparison_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (!placeholder_scan_token_is_comparison_operator(scan, index) ||
            !placeholder_scan_comparison_operator_has_predicate_context(scan, index) ||
            index + 1U >= scan->token_count ||
            !placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            continue;
        }
        if (scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_DECIMAL &&
            index + 4U <= scan->token_count &&
            placeholder_scan_tokens_contain_qualified_identifier(scan, index + 1U, index + 4U)) {
            return true;
        }
        if (placeholder_scan_parenthesized_left_operand_contains_comparison(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_comparison_operator_has_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index == 0U) {
        return false;
    }
    for (size_t scan_index = index; scan_index > 0U; --scan_index) {
        size_t previous_index = scan_index - 1U;

        if (placeholder_scan_token_starts_predicate_clause(scan, previous_index)) {
            return true;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, previous_index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_left_operand_contains_comparison(
    const struct placeholder_statement_scan *scan,
    size_t comparison_index
) {
    int paren_depth = 1;

    if (scan == NULL || comparison_index < 2U ||
        !token_is_right_paren(&scan->tokens[comparison_index - 1U])) {
        return false;
    }
    for (size_t scan_index = comparison_index - 1U; scan_index > 0U; --scan_index) {
        size_t previous_index = scan_index - 1U;

        if (token_is_right_paren(&scan->tokens[previous_index])) {
            ++paren_depth;
            continue;
        }
        if (!token_is_left_paren(&scan->tokens[previous_index])) {
            continue;
        }
        --paren_depth;
        if (paren_depth == 0) {
            return placeholder_scan_parenthesized_range_contains_comparison_operator(
                scan,
                previous_index,
                comparison_index - 1U
            );
        }
        if (paren_depth < 0) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_range_contains_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    if (scan == NULL || left_paren_index + 2U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index + 1U < right_paren_index; ++index) {
        if (placeholder_scan_token_is_comparison_operator(scan, index) &&
            placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_identifier_between_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 2U; index + 2U < scan->token_count; ++index) {
        if (!placeholder_scan_token_text_equals(scan, index, "BETWEEN") ||
            !placeholder_scan_token_is_identifier_like(scan, index - 1U) ||
            !placeholder_scan_token_is_identifier_like(scan, index + 1U)) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index + 1U < scan->token_count; ++scan_index) {
            if (placeholder_scan_token_stops_expression_clause_search(scan, scan_index)) {
                break;
            }
            if (placeholder_scan_token_text_equals(scan, scan_index, "AND") &&
                placeholder_scan_token_is_identifier_like(scan, scan_index + 1U)) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_identifier_in_list_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 2U; index + 3U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_token_text_equals(scan, index, "IN") ||
            !placeholder_scan_token_is_identifier_like(scan, index - 1U) ||
            !token_is_left_paren(&scan->tokens[index + 1U]) ||
            !placeholder_scan_find_matching_right_paren(scan, index + 1U, &right_paren_index) ||
            !placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U) ||
            !placeholder_scan_tokens_contain_identifier_like(scan, index + 2U, right_paren_index)) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index < right_paren_index; ++scan_index) {
            if (token_is_comma(&scan->tokens[scan_index])) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_insert_keyword_group_function_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 1U; index + 3U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_token_text_equals(scan, index, "GROUP") ||
            !placeholder_scan_token_text_equals(scan, index + 1U, "BY") ||
            !placeholder_scan_token_text_equals(scan, index + 2U, "INSERT") ||
            !token_is_left_paren(&scan->tokens[index + 3U]) ||
            !placeholder_scan_find_matching_right_paren(scan, index + 3U, &right_paren_index) ||
            !placeholder_scan_function_call_arguments_are_well_formed(
                scan,
                index + 3U,
                right_paren_index
            ) ||
            !placeholder_scan_function_call_has_four_arguments(
                scan,
                index + 3U,
                right_paren_index
            ) ||
            !placeholder_scan_token_can_follow_function_call_surface(scan, right_paren_index)) {
            continue;
        }
        return true;
    }
    return false;
}

static bool placeholder_scan_contains_group_by_user_variable_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t group_index = 0U;
    int paren_depth = 0;

    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_find_top_level_keyword(scan, "GROUP", &group_index) ||
        !placeholder_scan_token_text_equals(scan, group_index + 1U, "BY")) {
        return false;
    }
    for (size_t index = group_index + 2U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, index, "HAVING") ||
            placeholder_scan_token_text_equals(scan, index, "ORDER") ||
            placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
            placeholder_scan_token_text_equals(scan, index, "UNION")) {
            return false;
        }
        if (placeholder_scan_token_is_assignment_operator(scan, index) &&
            index > group_index + 2U &&
            scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_USER_VARIABLE &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_function_call_has_four_arguments(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    int paren_depth = 0;
    size_t argument_count = 1U;

    if (scan == NULL || left_paren_index + 1U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && token_is_comma(&scan->tokens[index])) {
            ++argument_count;
        }
    }
    return paren_depth == 0 && argument_count == 4U;
}

static bool placeholder_scan_contains_charset_introducer_string_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_CHARSET_INTRODUCER &&
            (scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_STRING ||
             scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_HEX_LITERAL)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_current_temporal_keyword_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "CURRENT_DATE") ||
             placeholder_scan_token_text_equals(scan, index, "CURRENT_TIME")) &&
            (index + 1U >= scan->token_count || !token_is_left_paren(&scan->tokens[index + 1U])) &&
            index > 0U && placeholder_scan_token_is_comparison_operator(scan, index - 1U) &&
            placeholder_scan_comparison_operator_has_predicate_context(scan, index - 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_malformed_function_argument_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!token_is_left_paren(&scan->tokens[index]) ||
            !placeholder_scan_left_paren_starts_function_arguments(scan, index) ||
            !placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index) ||
            right_paren_index == index + 1U) {
            continue;
        }
        if (!placeholder_scan_function_call_arguments_are_well_formed(
                scan,
                index,
                right_paren_index
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_expression_clause_contains_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;
    bool waiting_for_between_and = false;

    if (scan == NULL || start_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (paren_depth == 0 &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "BETWEEN")) {
            waiting_for_between_and = true;
            continue;
        }
        if (paren_depth == 0 && waiting_for_between_and &&
            placeholder_scan_token_text_equals(scan, index, "AND")) {
            waiting_for_between_and = false;
            continue;
        }
        if (placeholder_scan_token_is_expression_operator_surface(scan, index) &&
            placeholder_scan_expression_operator_surface_has_operand_context(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_expression_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "AND") ||
        placeholder_scan_token_text_equals(scan, index, "OR") ||
        placeholder_scan_token_text_equals(scan, index, "XOR") ||
        placeholder_scan_token_text_equals(scan, index, "DIV") ||
        placeholder_scan_token_text_equals(scan, index, "MOD") ||
        placeholder_scan_token_text_equals(scan, index, "NOT")) {
        return true;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
           (token->operator_kind == MYLITE_SQL_OPERATOR_PLUS ||
            token->operator_kind == MYLITE_SQL_OPERATOR_MINUS ||
            token->operator_kind == MYLITE_SQL_OPERATOR_STAR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_SLASH ||
            token->operator_kind == MYLITE_SQL_OPERATOR_PERCENT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_NOT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_AND ||
            token->operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_OR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_XOR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_AND ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_OR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_LEFT_SHIFT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_RIGHT_SHIFT);
}

static bool placeholder_scan_expression_operator_surface_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        !placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "NOT") &&
        (index == 0U || !placeholder_scan_token_can_end_expression_operand(scan, index - 1U))) {
        return true;
    }
    if (placeholder_scan_token_is_unary_expression_operator_surface(scan, index) &&
        (index == 0U || !placeholder_scan_token_can_end_expression_operand(scan, index - 1U))) {
        return true;
    }
    return index > 0U && placeholder_scan_token_can_end_expression_operand(scan, index - 1U);
}

static bool placeholder_scan_token_is_unary_expression_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count ||
        scan->tokens[index].kind != MYLITE_SQL_TOKEN_OPERATOR) {
        return false;
    }
    token = &scan->tokens[index];
    return token->operator_kind == MYLITE_SQL_OPERATOR_PLUS ||
           token->operator_kind == MYLITE_SQL_OPERATOR_MINUS ||
           token->operator_kind == MYLITE_SQL_OPERATOR_NOT ||
           token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT;
}

static bool placeholder_scan_token_can_start_expression_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count || token_is_comma(&scan->tokens[index]) ||
        token_is_right_paren(&scan->tokens[index]) ||
        placeholder_scan_token_stops_expression_clause_search(scan, index)) {
        return false;
    }
    token = &scan->tokens[index];
    if (token->kind == MYLITE_SQL_TOKEN_OPERATOR) {
        return placeholder_scan_token_is_unary_expression_operator_surface(scan, index);
    }
    return !placeholder_scan_token_is_incomplete_statement_tail(scan, index) ||
           token_is_left_paren(token);
}

static bool placeholder_scan_token_can_end_expression_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count || token_is_comma(&scan->tokens[index]) ||
        token_is_left_paren(&scan->tokens[index]) ||
        placeholder_scan_token_stops_expression_clause_search(scan, index) ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index)) {
        return false;
    }
    return scan->tokens[index].kind != MYLITE_SQL_TOKEN_OPERATOR;
}

static bool placeholder_scan_statement_tail_is_obviously_incomplete(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count == 0U) {
        return true;
    }
    return placeholder_scan_token_is_incomplete_statement_tail(scan, scan->token_count - 1U);
}

static bool placeholder_scan_token_is_incomplete_statement_tail(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return true;
    }
    if (token_is_comma(&scan->tokens[index]) || token_is_left_paren(&scan->tokens[index])) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
        placeholder_scan_token_text_equals(scan, index, "FROM") ||
        placeholder_scan_token_text_equals(scan, index, "WHERE") ||
        placeholder_scan_token_text_equals(scan, index, "GROUP") ||
        placeholder_scan_token_text_equals(scan, index, "ORDER") ||
        placeholder_scan_token_text_equals(scan, index, "BY") ||
        placeholder_scan_token_text_equals(scan, index, "HAVING") ||
        placeholder_scan_token_text_equals(scan, index, "ON") ||
        placeholder_scan_token_text_equals(scan, index, "JOIN") ||
        placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
        placeholder_scan_token_text_equals(scan, index, "UNION") ||
        placeholder_scan_token_text_equals(scan, index, "VALUES") ||
        placeholder_scan_token_text_equals(scan, index, "SET") ||
        placeholder_scan_token_text_equals(scan, index, "AS") ||
        placeholder_scan_token_text_equals(scan, index, "COLLATE") ||
        placeholder_scan_token_text_equals(scan, index, "USING") ||
        placeholder_scan_token_text_equals(scan, index, "ESCAPE") ||
        placeholder_scan_token_text_equals(scan, index, "INTERVAL") ||
        placeholder_scan_token_text_equals(scan, index, "BETWEEN") ||
        placeholder_scan_token_text_equals(scan, index, "AND") ||
        placeholder_scan_token_text_equals(scan, index, "OR") ||
        placeholder_scan_token_text_equals(scan, index, "XOR") ||
        placeholder_scan_token_text_equals(scan, index, "DIV") ||
        placeholder_scan_token_text_equals(scan, index, "MOD") ||
        placeholder_scan_token_text_equals(scan, index, "NOT") ||
        placeholder_scan_token_text_equals(scan, index, "IN") ||
        placeholder_scan_token_text_equals(scan, index, "IS") ||
        placeholder_scan_token_text_equals(scan, index, "LIKE") ||
        placeholder_scan_token_text_equals(scan, index, "REGEXP") ||
        placeholder_scan_token_text_equals(scan, index, "RLIKE")) {
        return true;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR;
}

static bool placeholder_scan_token_stops_expression_clause_search(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "WHERE") ||
           placeholder_scan_token_text_equals(scan, index, "HAVING") ||
           placeholder_scan_token_text_equals(scan, index, "ORDER") ||
           placeholder_scan_token_text_equals(scan, index, "GROUP") ||
           placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "VALUES") ||
           placeholder_scan_token_text_equals(scan, index, "SET");
}

static bool placeholder_scan_token_starts_duplicate_update_assignment_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "UPDATE") && index >= 3U &&
           placeholder_scan_token_text_equals(scan, index - 1U, "KEY") &&
           placeholder_scan_token_text_equals(scan, index - 2U, "DUPLICATE") &&
           placeholder_scan_token_text_equals(scan, index - 3U, "ON");
}

static bool placeholder_scan_contains_row_tuple_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_row_tuple_starts_at(scan, index, &right_paren_index)) {
            continue;
        }
        if (placeholder_scan_token_is_comparison_operator(scan, right_paren_index + 1U) ||
            placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IN") ||
            (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NOT") &&
             placeholder_scan_token_text_equals(scan, right_paren_index + 2U, "IN"))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_row_tuple_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index
) {
    size_t right_paren_index = 0U;
    int paren_depth = 0;

    if (!placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 2U ||
        placeholder_scan_left_paren_starts_function_arguments(scan, left_paren_index) ||
        placeholder_scan_query_expression_starts_at(scan, left_paren_index + 1U)) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && token_is_comma(&scan->tokens[index])) {
            if (out_right_paren_index != NULL) {
                *out_right_paren_index = right_paren_index;
            }
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_left_paren_starts_function_arguments(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    if (scan == NULL || left_paren_index == 0U || left_paren_index >= scan->token_count ||
        (scan->tokens[left_paren_index].flags & MYLITE_SQL_TOKEN_HAS_LEADING_SPACE) != 0U) {
        return false;
    }
    return placeholder_scan_token_can_name_immediate_function(scan, left_paren_index - 1U);
}

static bool placeholder_scan_token_can_name_immediate_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    if (token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
        token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind != MYLITE_SQL_TOKEN_KEYWORD) {
        return false;
    }
    return !placeholder_scan_token_text_equals(scan, index, "SELECT") &&
           !placeholder_scan_token_text_equals(scan, index, "FROM") &&
           !placeholder_scan_token_text_equals(scan, index, "WHERE") &&
           !placeholder_scan_token_text_equals(scan, index, "HAVING") &&
           !placeholder_scan_token_text_equals(scan, index, "ON") &&
           !placeholder_scan_token_text_equals(scan, index, "GROUP") &&
           !placeholder_scan_token_text_equals(scan, index, "ORDER") &&
           !placeholder_scan_token_text_equals(scan, index, "BY") &&
           !placeholder_scan_token_text_equals(scan, index, "LIMIT") &&
           !placeholder_scan_token_text_equals(scan, index, "UNION") &&
           !placeholder_scan_token_text_equals(scan, index, "IN") &&
           !placeholder_scan_token_text_equals(scan, index, "NOT") &&
           !placeholder_scan_token_text_equals(scan, index, "VALUES") &&
           !placeholder_scan_token_text_equals(scan, index, "SET") &&
           !placeholder_scan_token_text_equals(scan, index, "UPDATE") &&
           !placeholder_scan_token_text_equals(scan, index, "DELETE") &&
           !placeholder_scan_token_text_equals(scan, index, "INSERT") &&
           !placeholder_scan_token_text_equals(scan, index, "REPLACE") &&
           !placeholder_scan_token_text_equals(scan, index, "JOIN");
}

static bool placeholder_scan_contains_dml_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return placeholder_scan_contains_delete_variant_surface(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "UPDATE")) {
        return placeholder_scan_contains_update_variant_surface(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REPLACE")) {
        return placeholder_scan_contains_insert_replace_variant_surface(scan);
    }
    return false;
}

static bool placeholder_scan_contains_delete_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    return placeholder_scan_contains_delete_ignore_modifier_surface(scan) ||
           placeholder_scan_contains_delete_multitable_target_surface(scan) ||
           placeholder_scan_contains_delete_using_surface(scan) ||
           placeholder_scan_contains_multikey_dml_order_surface(scan) ||
           placeholder_scan_contains_user_variable_assignment_surface(scan);
}

static bool placeholder_scan_contains_delete_ignore_modifier_surface(
    const struct placeholder_statement_scan *scan
) {
    bool saw_ignore = false;
    size_t prefix_end = 0U;
    size_t from_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return false;
    }
    prefix_end = placeholder_scan_delete_modifier_prefix_end(scan, &saw_ignore);
    return saw_ignore && prefix_end < scan->token_count &&
           !placeholder_scan_token_is_delete_modifier_keyword(scan, prefix_end) &&
           placeholder_scan_find_top_level_keyword_after(scan, "FROM", prefix_end, &from_index);
}

static bool placeholder_scan_contains_delete_multitable_target_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t target_start = 0U;
    size_t from_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE") ||
        !placeholder_scan_find_top_level_keyword(scan, "FROM", &from_index)) {
        return false;
    }
    target_start = placeholder_scan_delete_modifier_prefix_end(scan, NULL);
    if (from_index <= target_start || from_index + 1U >= scan->token_count ||
        placeholder_scan_token_is_delete_modifier_keyword(scan, target_start)) {
        return false;
    }
    return target_start < scan->token_count;
}

static bool placeholder_scan_contains_delete_using_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t from_index = 0U;
    size_t using_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return false;
    }
    from_index = placeholder_scan_delete_modifier_prefix_end(scan, NULL);
    return placeholder_scan_token_text_equals(scan, from_index, "FROM") &&
           placeholder_scan_find_top_level_keyword(scan, "USING", &using_index) &&
           using_index > from_index + 1U && using_index + 1U < scan->token_count;
}

static bool placeholder_scan_contains_multikey_dml_order_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t order_index = 0U;
    int paren_depth = 0;

    if (scan == NULL ||
        (!placeholder_scan_token_text_equals(scan, 0U, "DELETE") &&
         !placeholder_scan_token_text_equals(scan, 0U, "UPDATE")) ||
        !placeholder_scan_find_top_level_keyword(scan, "ORDER", &order_index) ||
        !placeholder_scan_token_text_equals(scan, order_index + 1U, "BY")) {
        return false;
    }
    for (size_t index = order_index + 2U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "LIMIT")) {
            return false;
        }
        if (paren_depth == 0 && token_is_comma(&scan->tokens[index])) {
            return index > order_index + 2U && index + 1U < scan->token_count &&
                   !placeholder_scan_token_text_equals(scan, index + 1U, "LIMIT");
        }
    }
    return false;
}

static bool placeholder_scan_contains_user_variable_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_is_assignment_operator(scan, index) && index > 0U &&
            scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_USER_VARIABLE) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_update_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t set_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
        !placeholder_scan_find_top_level_keyword(scan, "SET", &set_index) || set_index <= 1U ||
        set_index + 1U >= scan->token_count) {
        return false;
    }
    return placeholder_scan_update_has_joined_source_before_set(scan, set_index) ||
           placeholder_scan_update_has_using_join_before_set(scan, set_index) ||
           placeholder_scan_contains_multikey_dml_order_surface(scan);
}

static bool placeholder_scan_update_has_joined_source_before_set(
    const struct placeholder_statement_scan *scan,
    size_t set_index
) {
    size_t source_start = 1U;

    if (placeholder_scan_token_text_equals(scan, source_start, "LOW_PRIORITY")) {
        ++source_start;
    }
    if (placeholder_scan_token_text_equals(scan, source_start, "IGNORE")) {
        ++source_start;
    }
    for (size_t index = source_start + 1U; index < set_index; ++index) {
        if (token_is_comma(&scan->tokens[index])) {
            return index > source_start && index + 1U < set_index &&
                   !token_is_comma(&scan->tokens[index - 1U]) &&
                   !token_is_comma(&scan->tokens[index + 1U]);
        }
        if (placeholder_scan_token_text_equals(scan, index, "JOIN") ||
            placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN")) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_update_has_using_join_before_set(
    const struct placeholder_statement_scan *scan,
    size_t set_index
) {
    bool saw_join = false;

    for (size_t index = 1U; index < set_index; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "JOIN") ||
            placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN")) {
            saw_join = true;
        }
        if (saw_join && placeholder_scan_token_text_equals(scan, index, "USING")) {
            return index + 1U < set_index;
        }
    }
    return false;
}

static bool placeholder_scan_contains_insert_replace_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    return placeholder_scan_contains_insert_row_alias_surface(scan) ||
           placeholder_scan_contains_insert_identifier_value_surface(scan) ||
           placeholder_scan_contains_insert_set_identifier_value_surface(scan) ||
           placeholder_scan_contains_duplicate_identifier_assignment_surface(scan) ||
           placeholder_scan_contains_duplicate_qualified_assignment_surface(scan) ||
           placeholder_scan_contains_replace_compound_select_surface(scan);
}

static bool placeholder_scan_contains_insert_row_alias_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        size_t alias_index = 0U;
        size_t row_list_end_index = 0U;

        if ((!placeholder_scan_token_text_equals(scan, index, "VALUES") &&
             !placeholder_scan_token_text_equals(scan, index, "VALUE")) ||
            !placeholder_scan_insert_row_constructor_list_end(scan, index, &row_list_end_index)) {
            continue;
        }
        alias_index = row_list_end_index;
        if (placeholder_scan_token_text_equals(scan, alias_index, "AS")) {
            ++alias_index;
        }
        if (placeholder_scan_insert_row_alias_tail_is_complete(scan, alias_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_insert_row_constructor_list_end(
    const struct placeholder_statement_scan *scan,
    size_t values_index,
    size_t *out_end_index
) {
    bool saw_constructor = false;
    size_t index = values_index + 1U;

    if (scan == NULL || out_end_index == NULL || index >= scan->token_count) {
        return false;
    }
    for (;;) {
        size_t left_paren_index = index;
        size_t right_paren_index = 0U;

        if (placeholder_scan_token_text_equals(scan, index, "ROW")) {
            left_paren_index = index + 1U;
        }
        if (left_paren_index >= scan->token_count ||
            !token_is_left_paren(&scan->tokens[left_paren_index]) ||
            !placeholder_scan_find_matching_right_paren(
                scan,
                left_paren_index,
                &right_paren_index
            )) {
            return false;
        }
        saw_constructor = true;
        index = right_paren_index + 1U;
        if (index >= scan->token_count) {
            break;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            break;
        }
        ++index;
        if (index >= scan->token_count) {
            return false;
        }
    }

    *out_end_index = index;
    return saw_constructor;
}

static bool placeholder_scan_insert_row_alias_tail_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t alias_index
) {
    size_t tail_index = alias_index + 1U;

    if (!placeholder_scan_token_can_name_insert_row_alias(scan, alias_index)) {
        return false;
    }
    if (tail_index >= scan->token_count) {
        return true;
    }
    if (token_is_left_paren(&scan->tokens[tail_index]) &&
        !placeholder_scan_insert_row_alias_column_list_is_complete(scan, tail_index, &tail_index)) {
        return false;
    }
    if (tail_index >= scan->token_count) {
        return true;
    }
    return placeholder_scan_token_starts_duplicate_update_assignment_clause(scan, tail_index + 3U);
}

static bool placeholder_scan_token_can_name_insert_row_alias(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index)) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "ON") ||
        placeholder_scan_token_text_equals(scan, index, "WHERE") ||
        placeholder_scan_token_text_equals(scan, index, "ORDER") ||
        placeholder_scan_token_text_equals(scan, index, "LIMIT")) {
        return false;
    }
    return placeholder_scan_token_can_name_loose_identifier(scan, index);
}

static bool placeholder_scan_insert_row_alias_column_list_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_end_index
) {
    size_t right_paren_index = 0U;
    bool need_column = true;

    if (scan == NULL || out_end_index == NULL ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (need_column) {
            if (!placeholder_scan_token_can_name_loose_identifier(scan, index)) {
                return false;
            }
            need_column = false;
            continue;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            return false;
        }
        need_column = true;
    }
    if (need_column) {
        return false;
    }
    *out_end_index = right_paren_index + 1U;
    return true;
}

static bool placeholder_scan_contains_insert_identifier_value_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "VALUES") ||
             placeholder_scan_token_text_equals(scan, index, "VALUE")) &&
            index + 1U < scan->token_count && token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_values_contain_identifier(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_values_contain_identifier(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    size_t right_paren_index = 0U;
    int paren_depth = 0;

    if (!placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U ||
        token_is_comma(&scan->tokens[right_paren_index - 1U])) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && token_is_comma(&scan->tokens[index]) &&
            index + 1U == right_paren_index) {
            return false;
        }
        if (paren_depth == 0 &&
            placeholder_scan_token_can_start_dml_identifier_value(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_insert_set_identifier_value_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t set_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        !placeholder_scan_find_top_level_keyword(scan, "SET", &set_index)) {
        return false;
    }
    for (size_t index = set_index + 1U; index + 1U < scan->token_count; ++index) {
        if (token_is_equal_sign(&scan->tokens[index]) &&
            placeholder_scan_token_can_start_dml_identifier_value(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_duplicate_identifier_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t update_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        !placeholder_scan_find_top_level_keyword(scan, "UPDATE", &update_index) ||
        update_index < 3U || !placeholder_scan_token_text_equals(scan, update_index - 1U, "KEY") ||
        !placeholder_scan_token_text_equals(scan, update_index - 2U, "DUPLICATE") ||
        !placeholder_scan_token_text_equals(scan, update_index - 3U, "ON")) {
        return false;
    }
    for (size_t index = update_index + 1U; index + 1U < scan->token_count; ++index) {
        if (token_is_equal_sign(&scan->tokens[index]) &&
            placeholder_scan_token_can_start_dml_identifier_value(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_duplicate_qualified_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t update_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        !placeholder_scan_find_top_level_keyword(scan, "UPDATE", &update_index) ||
        update_index < 3U || !placeholder_scan_token_text_equals(scan, update_index - 1U, "KEY") ||
        !placeholder_scan_token_text_equals(scan, update_index - 2U, "DUPLICATE") ||
        !placeholder_scan_token_text_equals(scan, update_index - 3U, "ON")) {
        return false;
    }
    for (size_t index = update_index + 1U; index + 1U < scan->token_count; ++index) {
        size_t lhs_start_index = update_index + 1U;
        size_t lhs_end_index = 0U;
        size_t rhs_end_index = 0U;

        if (!token_is_equal_sign(&scan->tokens[index])) {
            continue;
        }
        for (size_t scan_index = index; scan_index > update_index + 1U; --scan_index) {
            size_t previous_index = scan_index - 1U;

            if (token_is_comma(&scan->tokens[previous_index])) {
                lhs_start_index = scan_index;
                break;
            }
        }
        if (index > update_index + 1U &&
            placeholder_scan_qualified_identifier_starts_at(
                scan,
                lhs_start_index,
                &lhs_end_index
            ) &&
            lhs_end_index == index) {
            return true;
        }
        if (placeholder_scan_qualified_identifier_starts_at(scan, index + 1U, &rhs_end_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_qualified_identifier_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_end_index
) {
    if (out_end_index != NULL) {
        *out_end_index = index;
    }
    if (scan == NULL || index + 2U >= scan->token_count ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index) ||
        !placeholder_scan_token_text_equals(scan, index + 1U, ".") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index + 2U)) {
        return false;
    }
    if (out_end_index != NULL) {
        *out_end_index = index + 3U;
    }
    return true;
}

static bool placeholder_scan_contains_replace_compound_select_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t select_index = 0U;
    size_t union_index = 0U;

    return scan != NULL && placeholder_scan_token_text_equals(scan, 0U, "REPLACE") &&
           placeholder_scan_find_top_level_keyword(scan, "SELECT", &select_index) &&
           placeholder_scan_find_top_level_keyword_after(
               scan,
               "UNION",
               select_index + 1U,
               &union_index
           ) &&
           union_index + 1U < scan->token_count;
}

static bool placeholder_scan_find_top_level_keyword(
    const struct placeholder_statement_scan *scan,
    const char *keyword,
    size_t *out_index
) {
    return placeholder_scan_find_top_level_keyword_after(scan, keyword, 0U, out_index);
}

static bool placeholder_scan_find_top_level_keyword_after(
    const struct placeholder_statement_scan *scan,
    const char *keyword,
    size_t start_index,
    size_t *out_index
) {
    int paren_depth = 0;

    if (scan == NULL || keyword == NULL || out_index == NULL || start_index > scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, keyword)) {
            *out_index = index;
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_assignment_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
           token->operator_kind == MYLITE_SQL_OPERATOR_ASSIGN;
}

static size_t placeholder_scan_delete_modifier_prefix_end(
    const struct placeholder_statement_scan *scan,
    bool *out_saw_ignore
) {
    size_t index = 1U;

    if (out_saw_ignore != NULL) {
        *out_saw_ignore = false;
    }
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return 0U;
    }
    if (placeholder_scan_token_text_equals(scan, index, "LOW_PRIORITY")) {
        ++index;
    }
    if (placeholder_scan_token_text_equals(scan, index, "QUICK")) {
        ++index;
    }
    if (placeholder_scan_token_text_equals(scan, index, "IGNORE")) {
        if (out_saw_ignore != NULL) {
            *out_saw_ignore = true;
        }
        ++index;
    }
    return index;
}

static bool placeholder_scan_token_is_delete_modifier_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "LOW_PRIORITY") ||
           placeholder_scan_token_text_equals(scan, index, "QUICK") ||
           placeholder_scan_token_text_equals(scan, index, "IGNORE");
}

static bool placeholder_scan_token_can_start_dml_identifier_value(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || !placeholder_scan_token_is_identifier_like(scan, index)) {
        return false;
    }
    if (index + 1U >= scan->token_count) {
        return true;
    }
    return !token_is_left_paren(&scan->tokens[index + 1U]);
}

static bool placeholder_scan_contains_bare_truth_clause_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_starts_predicate_clause(scan, index) &&
            placeholder_scan_token_can_start_bare_truth_expression(scan, index + 1U) &&
            placeholder_scan_bare_truth_expression_is_simple(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_can_start_bare_truth_expression(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_stops_predicate_clause_search(scan, index)) {
        return false;
    }
    token = &scan->tokens[index];
    return placeholder_scan_token_is_identifier_like(scan, index) ||
           token->kind == MYLITE_SQL_TOKEN_INTEGER || token->kind == MYLITE_SQL_TOKEN_DECIMAL ||
           token->kind == MYLITE_SQL_TOKEN_FLOAT || token->kind == MYLITE_SQL_TOKEN_STRING ||
           token->kind == MYLITE_SQL_TOKEN_HEX_LITERAL ||
           token->kind == MYLITE_SQL_TOKEN_BIT_LITERAL ||
           token->kind == MYLITE_SQL_TOKEN_USER_VARIABLE ||
           token->kind == MYLITE_SQL_TOKEN_SYSTEM_VARIABLE ||
           placeholder_scan_token_text_equals(scan, index, "TRUE") ||
           placeholder_scan_token_text_equals(scan, index, "FALSE") ||
           placeholder_scan_token_text_equals(scan, index, "NULL") || token_is_left_paren(token);
}

static bool placeholder_scan_bare_truth_expression_is_simple(
    const struct placeholder_statement_scan *scan,
    size_t expression_index
) {
    int paren_depth = 0;
    size_t expression_token_count = 0U;

    if (scan == NULL || expression_index >= scan->token_count) {
        return false;
    }
    if (expression_index + 1U < scan->token_count &&
        token_is_left_paren(&scan->tokens[expression_index + 1U])) {
        return false;
    }
    for (size_t index = expression_index; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > expression_index &&
            placeholder_scan_token_stops_predicate_clause_search(scan, index)) {
            return expression_token_count == 1U ||
                   placeholder_scan_bare_truth_expression_is_qualified_identifier(
                       scan,
                       expression_index,
                       index
                   );
        }
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
        if (paren_depth == 0 && (placeholder_scan_token_is_comparison_operator(scan, index) ||
                                 placeholder_scan_token_text_equals(scan, index, "BETWEEN") ||
                                 placeholder_scan_token_text_equals(scan, index, "IN") ||
                                 placeholder_scan_token_text_equals(scan, index, "IS") ||
                                 placeholder_scan_token_text_equals(scan, index, "LIKE") ||
                                 placeholder_scan_token_text_equals(scan, index, "REGEXP") ||
                                 placeholder_scan_token_text_equals(scan, index, "RLIKE"))) {
            return false;
        }
        ++expression_token_count;
    }
    return expression_token_count == 1U ||
           placeholder_scan_bare_truth_expression_is_qualified_identifier(
               scan,
               expression_index,
               scan->token_count
           );
}

static bool placeholder_scan_bare_truth_expression_is_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t expression_index,
    size_t stop_index
) {
    bool need_identifier = true;
    size_t token_count = 0U;

    if (scan == NULL || stop_index <= expression_index + 2U || stop_index > scan->token_count) {
        return false;
    }
    for (size_t index = expression_index; index < stop_index; ++index) {
        if (need_identifier) {
            if (!placeholder_scan_token_is_identifier_like(scan, index)) {
                return false;
            }
            need_identifier = false;
        } else {
            if (!placeholder_scan_token_text_equals(scan, index, ".")) {
                return false;
            }
            need_identifier = true;
        }
        ++token_count;
    }
    return token_count >= 3U && !need_identifier;
}

static bool placeholder_scan_contains_lateral_derived_table(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "LATERAL") &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_grouping_function(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "GROUPING") &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_sounds_like_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "SOUNDS") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "LIKE") &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 2U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_deprecated_logical_operator_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_token_text_equals(scan, 0U, "DO") ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 1U < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_OPERATOR &&
            (scan->tokens[index].operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_AND ||
             scan->tokens[index].operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_OR) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_like_surface(const struct placeholder_statement_scan *scan) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 1U < scan->token_count; ++index) {
        bool right_operand_is_parenthesized =
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U);
        bool right_operand_is_plain =
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 1U);

        if (placeholder_scan_token_text_equals(scan, index, "LIKE") &&
            !placeholder_scan_token_text_equals(scan, index - 1U, "SOUNDS") &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            (right_operand_is_plain || right_operand_is_parenthesized)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_like_escape_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 1U < scan->token_count; ++index) {
        if (!placeholder_scan_token_text_equals(scan, index, "ESCAPE") ||
            placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) ||
            placeholder_scan_token_stops_expression_clause_search(scan, index + 1U)) {
            continue;
        }
        for (size_t scan_index = index; scan_index > 0U; --scan_index) {
            size_t previous_index = scan_index - 1U;

            if (placeholder_scan_token_stops_expression_clause_search(scan, previous_index)) {
                break;
            }
            if (placeholder_scan_token_text_equals(scan, previous_index, "LIKE") &&
                previous_index > 1U && previous_index + 1U < index &&
                !placeholder_scan_token_is_incomplete_statement_tail(scan, previous_index - 1U) &&
                !placeholder_scan_token_stops_expression_clause_search(scan, previous_index - 1U) &&
                !placeholder_scan_token_is_incomplete_statement_tail(scan, previous_index + 1U) &&
                !placeholder_scan_token_stops_expression_clause_search(scan, previous_index + 1U)) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_not_like_surface(const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "NOT") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "LIKE") &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 2U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_typed_temporal_literal_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER &&
            scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_STRING) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_interval_expression_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") &&
            !token_is_left_paren(&scan->tokens[index + 1U]) &&
            !placeholder_scan_interval_expression_follows_parenthesized_separator(scan, index) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            placeholder_scan_token_is_date_interval_unit(scan, index + 2U)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") &&
            index + 4U < scan->token_count && !token_is_left_paren(&scan->tokens[index + 1U]) &&
            !placeholder_scan_interval_expression_follows_parenthesized_separator(scan, index) &&
            placeholder_scan_interval_value_binary_expression_is_complete(scan, index)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_find_matching_right_paren(scan, index + 1U, &right_paren_index) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U) &&
            right_paren_index + 1U < scan->token_count &&
            placeholder_scan_token_is_date_interval_unit(scan, right_paren_index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_interval_expression_follows_parenthesized_separator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    int paren_depth = 0;

    if (scan == NULL || index == 0U || !token_is_comma(&scan->tokens[index - 1U])) {
        return false;
    }
    for (size_t scan_index = 0U; scan_index < index; ++scan_index) {
        if (token_is_left_paren(&scan->tokens[scan_index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[scan_index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return paren_depth > 0;
}

static bool placeholder_scan_interval_value_binary_expression_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *operator_token = NULL;

    if (scan == NULL || index + 4U >= scan->token_count ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index + 3U) ||
        !placeholder_scan_token_is_date_interval_unit(scan, index + 4U)) {
        return false;
    }

    operator_token = &scan->tokens[index + 2U];
    if (operator_token->kind != MYLITE_SQL_TOKEN_OPERATOR) {
        return false;
    }
    return operator_token->operator_kind == MYLITE_SQL_OPERATOR_PLUS ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_MINUS ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_XOR ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_LEFT_SHIFT ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_RIGHT_SHIFT;
}

static bool placeholder_scan_token_is_date_interval_unit(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "YEAR") ||
           placeholder_scan_token_text_equals(scan, index, "QUARTER") ||
           placeholder_scan_token_text_equals(scan, index, "MONTH") ||
           placeholder_scan_token_text_equals(scan, index, "WEEK") ||
           placeholder_scan_token_text_equals(scan, index, "DAY") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR") ||
           placeholder_scan_token_text_equals(scan, index, "MINUTE") ||
           placeholder_scan_token_text_equals(scan, index, "SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "YEAR_MONTH") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_HOUR") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_MINUTE") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR_MINUTE") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR_SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "MINUTE_SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR_MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "MINUTE_MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "SECOND_MICROSECOND");
}

static bool placeholder_scan_contains_scalar_in_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "IN") &&
            !placeholder_scan_scalar_in_is_descriptor_predicate(scan, index) &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_scalar_in_is_descriptor_predicate(
    const struct placeholder_statement_scan *scan,
    size_t in_index
) {
    if (scan == NULL || in_index == 0U || token_is_right_paren(&scan->tokens[in_index - 1U])) {
        return false;
    }
    for (size_t index = in_index; index > 0U; --index) {
        size_t previous_index = index - 1U;

        if (placeholder_scan_token_starts_predicate_clause(scan, previous_index)) {
            return true;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, previous_index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_token_starts_predicate_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "WHERE") ||
           placeholder_scan_token_text_equals(scan, index, "HAVING") ||
           placeholder_scan_token_text_equals(scan, index, "ON");
}

static bool placeholder_scan_token_stops_predicate_clause_search(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "ORDER") ||
           placeholder_scan_token_text_equals(scan, index, "GROUP") ||
           placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "VALUES") ||
           placeholder_scan_token_text_equals(scan, index, "SET");
}

static bool placeholder_scan_contains_quantified_subquery_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "ANY") ||
             placeholder_scan_token_text_equals(scan, index, "SOME") ||
             placeholder_scan_token_text_equals(scan, index, "ALL")) &&
            placeholder_scan_token_is_comparison_operator(scan, index - 1U) &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_fulltext_match_against_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "MATCH") &&
            (placeholder_scan_match_column_list_without_parentheses_is_supported(scan, index) ||
             placeholder_scan_match_column_list_with_parentheses_is_supported(scan, index))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_match_column_list_without_parentheses_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t match_index
) {
    size_t index = match_index + 1U;
    bool saw_column = false;
    bool need_column = true;

    if (scan == NULL || index >= scan->token_count || token_is_left_paren(&scan->tokens[index])) {
        return false;
    }
    while (index < scan->token_count) {
        size_t next_index = index;

        if (need_column) {
            if (!placeholder_scan_match_column_name_starts_at(scan, index, &next_index)) {
                return false;
            }
            saw_column = true;
            need_column = false;
            index = next_index;
            continue;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            break;
        }
        need_column = true;
        ++index;
    }
    return saw_column && !need_column && index + 1U < scan->token_count &&
           placeholder_scan_token_text_equals(scan, index, "AGAINST") &&
           token_is_left_paren(&scan->tokens[index + 1U]) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U);
}

static bool placeholder_scan_match_column_list_with_parentheses_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t match_index
) {
    size_t left_paren_index = match_index + 1U;
    size_t right_paren_index = 0U;
    size_t index = left_paren_index + 1U;
    bool need_column = true;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U) {
        return false;
    }
    while (index < right_paren_index) {
        size_t next_index = index;

        if (need_column) {
            if (!placeholder_scan_match_column_name_starts_at(scan, index, &next_index) ||
                next_index > right_paren_index) {
                return false;
            }
            need_column = false;
            index = next_index;
            continue;
        }
        if (!token_is_comma(&scan->tokens[index])) {
            return false;
        }
        need_column = true;
        ++index;
    }
    return !need_column && right_paren_index + 2U < scan->token_count &&
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "AGAINST") &&
           token_is_left_paren(&scan->tokens[right_paren_index + 2U]) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, right_paren_index + 2U);
}

static bool placeholder_scan_match_column_name_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_next_index
) {
    if (out_next_index != NULL) {
        *out_next_index = index;
    }
    if (!placeholder_scan_token_can_name_loose_identifier(scan, index)) {
        return false;
    }
    ++index;
    while (placeholder_scan_token_text_equals(scan, index, ".") &&
           placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        index += 2U;
    }
    if (out_next_index != NULL) {
        *out_next_index = index;
    }
    return true;
}

static bool placeholder_scan_token_can_name_loose_identifier(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    return placeholder_scan_token_is_identifier_like(scan, index) ||
           scan->tokens[index].kind == MYLITE_SQL_TOKEN_KEYWORD;
}

static bool placeholder_scan_parentheses_are_balanced(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    if (scan == NULL || start_index > scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return paren_depth == 0;
}

static bool placeholder_scan_parenthesized_operand_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (!token_is_right_paren(&scan->tokens[index])) {
            continue;
        }
        --paren_depth;
        if (paren_depth < 0) {
            return false;
        }
        if (paren_depth == 0) {
            return index > left_paren_index + 1U && !token_is_comma(&scan->tokens[index - 1U]);
        }
    }
    return false;
}

static bool placeholder_scan_token_is_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token_is_equal_sign(token) ||
           (token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
            (token->operator_kind == MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL ||
             token->operator_kind == MYLITE_SQL_OPERATOR_NOT_EQUAL ||
             token->operator_kind == MYLITE_SQL_OPERATOR_LESS ||
             token->operator_kind == MYLITE_SQL_OPERATOR_LESS_EQUAL ||
             token->operator_kind == MYLITE_SQL_OPERATOR_GREATER ||
             token->operator_kind == MYLITE_SQL_OPERATOR_GREATER_EQUAL));
}

static enum placeholder_statement_kind classify_create_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "USER") ||
        placeholder_scan_token_text_equals(scan, 1U, "ROLE") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (foreign_server_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLESPACE")) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (undo_tablespace_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (create_view_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_zerofill_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_extended_option_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_residual_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (create_table_select_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }

    for (size_t index = 1U;
         index < scan->token_count && index < placeholder_create_scan_token_limit;
         ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
        }
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_alter_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "USER") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE") ||
        placeholder_scan_token_text_equals(scan, 1U, "INSTANCE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (foreign_server_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLESPACE")) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (undo_tablespace_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (alter_view_placeholder_statement_is_supported(scan) ||
        alter_table_tablespace_file_operation_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_zerofill_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (alter_table_engine_first_multi_action_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ALTER_TABLE_MULTI_ACTION_UNSUPPORTED;
    }
    if (ddl_extended_option_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_residual_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLE") &&
        alter_table_partition_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (alter_schema_unsupported_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "PROCEDURE") ||
        placeholder_scan_token_text_equals(scan, 1U, "FUNCTION") ||
        placeholder_scan_token_text_equals(scan, 1U, "EVENT")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool foreign_server_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 1U, "SERVER")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return foreign_server_create_placeholder_statement_is_supported(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "ALTER")) {
        return foreign_server_alter_placeholder_statement_is_supported(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "DROP")) {
        return foreign_server_drop_placeholder_statement_is_supported(scan);
    }
    return false;
}

static bool foreign_server_create_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { foreign_server_create_min_tokens = 8 };

    bool saw_foreign_data_wrapper = false;
    bool saw_options = false;

    if (scan == NULL || scan->token_count < foreign_server_create_min_tokens) {
        return false;
    }
    for (size_t index = 3U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "FOREIGN") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "DATA") &&
            placeholder_scan_token_text_equals(scan, index + 2U, "WRAPPER")) {
            saw_foreign_data_wrapper = true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "OPTIONS")) {
            saw_options = true;
        }
    }
    return saw_foreign_data_wrapper && saw_options;
}

static bool foreign_server_alter_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { foreign_server_alter_min_tokens = 5 };

    if (scan == NULL || scan->token_count < foreign_server_alter_min_tokens) {
        return false;
    }
    for (size_t index = 3U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "OPTIONS")) {
            return true;
        }
    }
    return false;
}

static bool foreign_server_drop_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { foreign_server_drop_if_exists_tokens = 5 };

    if (scan == NULL || scan->token_count < 3U) {
        return false;
    }
    if (scan->token_count == 3U) {
        return foreign_server_drop_name_is_supported(scan, 2U);
    }
    return scan->token_count == foreign_server_drop_if_exists_tokens &&
           placeholder_scan_token_text_equals(scan, 2U, "IF") &&
           placeholder_scan_token_text_equals(scan, 3U, "EXISTS") &&
           foreign_server_drop_name_is_supported(scan, 4U);
}

static bool foreign_server_drop_name_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_text_equals(scan, index, "IF") ||
        placeholder_scan_token_text_equals(scan, index, "EXISTS")) {
        return false;
    }
    return placeholder_scan_token_can_name_loose_identifier(scan, index) ||
           scan->tokens[index].kind == MYLITE_SQL_TOKEN_STRING;
}

static bool alter_schema_unsupported_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "ALTER") ||
        (!placeholder_scan_token_text_equals(scan, 1U, "DATABASE") &&
         !placeholder_scan_token_text_equals(scan, 1U, "SCHEMA"))) {
        return false;
    }
    for (size_t index = 2U; index < scan->token_count; ++index) {
        if (alter_schema_unsupported_encryption_option_starts_at(scan, index) ||
            alter_schema_unsupported_read_only_option_starts_at(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool alter_schema_unsupported_encryption_option_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t value_index = index + 1U;

    if (scan == NULL || index + 1U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, index, "ENCRYPTION")) {
        return false;
    }
    if (token_is_equal_sign(&scan->tokens[value_index])) {
        ++value_index;
    }
    if (value_index >= scan->token_count) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, value_index, "DEFAULT") ||
           scan->tokens[value_index].kind == MYLITE_SQL_TOKEN_STRING;
}

static bool alter_schema_unsupported_read_only_option_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t value_index = index + 2U;

    if (scan == NULL || index + 2U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, index, "READ") ||
        !placeholder_scan_token_text_equals(scan, index + 1U, "ONLY")) {
        return false;
    }
    if (token_is_equal_sign(&scan->tokens[value_index])) {
        ++value_index;
    }
    return alter_schema_read_only_value_is_supported(scan, value_index);
}

static bool alter_schema_read_only_value_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, index, "DEFAULT") ||
           placeholder_scan_token_text_equals(scan, index, "0") ||
           placeholder_scan_token_text_equals(scan, index, "1");
}

static bool alter_table_engine_first_multi_action_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t index = 2U;

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_starts_alter_table_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index)) {
        return false;
    }
    ++index;
    if (placeholder_scan_token_text_equals(scan, index, ".") &&
        placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        index += 2U;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "ENGINE")) {
        return false;
    }
    ++index;
    if (index < scan->token_count && token_is_equal_sign(&scan->tokens[index])) {
        ++index;
    }
    if (index >= scan->token_count ||
        (!placeholder_scan_token_can_name_loose_identifier(scan, index) &&
         scan->tokens[index].kind != MYLITE_SQL_TOKEN_STRING)) {
        return false;
    }
    ++index;
    if (index >= scan->token_count || !token_is_comma(&scan->tokens[index])) {
        return false;
    }
    return alter_table_engine_first_multi_action_starter_is_supported(scan, index + 1U);
}

static bool alter_table_engine_first_multi_action_starter_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    static const char *const starters[] = {
        "ADD",
        "ALTER",
        "CHANGE",
        "DROP",
        "MODIFY",
        "RENAME",
    };

    return placeholder_scan_token_text_equals_any(
        scan,
        index,
        starters,
        sizeof(starters) / sizeof(starters[0])
    );
}

static bool create_view_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { create_view_scan_limit = 16 };

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count && index < create_view_scan_limit; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "VIEW")) {
            return view_placeholder_statement_has_query_body(scan, index);
        }
        if (placeholder_scan_token_text_equals(scan, index, "TABLE") ||
            placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return false;
        }
    }
    return false;
}

static bool alter_view_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { alter_view_scan_limit = 16 };

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "ALTER") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count && index < alter_view_scan_limit; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "VIEW")) {
            return view_placeholder_statement_has_query_body(scan, index);
        }
        if (placeholder_scan_token_text_equals(scan, index, "TABLE") ||
            placeholder_scan_token_text_equals(scan, index, "USER") ||
            placeholder_scan_token_text_equals(scan, index, "INSTANCE") ||
            placeholder_scan_token_text_equals(scan, index, "TABLESPACE") ||
            placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return false;
        }
    }
    return false;
}

static bool view_placeholder_statement_has_query_body(
    const struct placeholder_statement_scan *scan,
    size_t view_index
) {
    if (scan == NULL || view_index >= scan->token_count) {
        return false;
    }
    for (size_t index = view_index + 1U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "AS") &&
            create_table_select_query_starts_at(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool undo_tablespace_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && scan->token_count > 3U &&
           placeholder_scan_token_text_equals(scan, 1U, "UNDO") &&
           placeholder_scan_token_text_equals(scan, 2U, "TABLESPACE") &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
}

static bool ddl_zerofill_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && placeholder_scan_contains_text(scan, "ZEROFILL") &&
           placeholder_scan_parentheses_are_balanced(scan, 0U) &&
           (placeholder_scan_starts_create_table_statement(scan) ||
            placeholder_scan_starts_alter_table_statement(scan));
}

static bool ddl_extended_option_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (!ddl_extended_option_placeholder_statement_is_candidate(scan)) {
        return false;
    }
    return ddl_extended_option_scan_has_marker(scan) ||
           ddl_check_expression_placeholder_scan_has_marker(scan);
}

static bool ddl_extended_option_placeholder_statement_is_candidate(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
           placeholder_scan_parentheses_are_balanced(scan, 0U) &&
           (placeholder_scan_starts_create_table_statement(scan) ||
            placeholder_scan_starts_alter_table_statement(scan));
}

static bool ddl_extended_option_scan_has_marker(const struct placeholder_statement_scan *scan) {
    static const char *const column_storage_values[] = {"DISK", "MEMORY"};
    static const char *const column_format_values[] = {"FIXED", "DYNAMIC", "DEFAULT"};

    if (placeholder_scan_contains_text(scan, "ENCRYPTION") ||
        placeholder_scan_contains_text(scan, "SECONDARY_ENGINE") ||
        placeholder_scan_contains_text(scan, "ENGINE_ATTRIBUTE") ||
        placeholder_scan_contains_text(scan, "SECONDARY_ENGINE_ATTRIBUTE")) {
        return true;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "DATA") ||
             placeholder_scan_token_text_equals(scan, index, "INDEX")) &&
            placeholder_scan_token_text_equals(scan, index + 1U, "DIRECTORY")) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "COLUMN_FORMAT") &&
            placeholder_scan_token_text_equals_any(
                scan,
                index + 1U,
                column_format_values,
                sizeof(column_format_values) / sizeof(column_format_values[0])
            )) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "STORAGE") &&
            placeholder_scan_token_text_equals_any(
                scan,
                index + 1U,
                column_storage_values,
                sizeof(column_storage_values) / sizeof(column_storage_values[0])
            )) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "NOT") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "SECONDARY")) {
            return true;
        }
    }
    return false;
}

static bool ddl_residual_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (!ddl_residual_placeholder_statement_is_candidate(scan)) {
        return false;
    }
    return ddl_residual_scan_has_fulltext_parser_clause(scan) ||
           ddl_residual_scan_has_generated_column_surface(scan) ||
           ddl_residual_scan_has_foreign_key_set_default_action(scan) ||
           ddl_residual_scan_has_alter_table_order_by_action(scan) ||
           ddl_residual_scan_has_create_table_start_transaction_option(scan);
}

static bool ddl_residual_placeholder_statement_is_candidate(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
           placeholder_scan_parentheses_are_balanced(scan, 0U) &&
           (placeholder_scan_starts_create_table_statement(scan) ||
            placeholder_scan_starts_alter_table_statement(scan) ||
            placeholder_scan_starts_create_index_statement(scan));
}

static bool ddl_residual_scan_has_fulltext_parser_clause(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_contains_text(scan, "FULLTEXT")) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "WITH") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "PARSER") &&
            placeholder_scan_token_can_name_loose_identifier(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool ddl_residual_scan_has_generated_column_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_starts_create_table_statement(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 3U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "GENERATED") &&
            ddl_generated_column_clause_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool ddl_generated_column_clause_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t generated_index
) {
    size_t left_paren_index = generated_index + 3U;
    size_t right_paren_index = 0U;

    if (scan == NULL || generated_index + 4U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, generated_index + 1U, "ALWAYS") ||
        !placeholder_scan_token_text_equals(scan, generated_index + 2U, "AS") ||
        !token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, right_paren_index - 1U)) {
        return false;
    }
    return right_paren_index + 1U >= scan->token_count ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "VIRTUAL") ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "STORED") ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NOT") ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NULL") ||
           token_is_comma(&scan->tokens[right_paren_index + 1U]) ||
           token_is_right_paren(&scan->tokens[right_paren_index + 1U]);
}

static bool ddl_residual_scan_has_foreign_key_set_default_action(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_contains_text(scan, "FOREIGN")) {
        return false;
    }
    for (size_t index = 0U; index + 3U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "ON") &&
            (placeholder_scan_token_text_equals(scan, index + 1U, "DELETE") ||
             placeholder_scan_token_text_equals(scan, index + 1U, "UPDATE")) &&
            placeholder_scan_token_text_equals(scan, index + 2U, "SET") &&
            placeholder_scan_token_text_equals(scan, index + 3U, "DEFAULT")) {
            return true;
        }
    }
    return false;
}

static bool ddl_residual_scan_has_alter_table_order_by_action(
    const struct placeholder_statement_scan *scan
) {
    size_t start_index = 0U;
    int paren_depth = 0;

    if (scan == NULL || !placeholder_scan_starts_alter_table_statement(scan)) {
        return false;
    }

    start_index = alter_table_partition_operation_start_index(scan);
    for (size_t index = start_index; index + 3U < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0 || !token_is_comma(&scan->tokens[index]) ||
            !placeholder_scan_token_text_equals(scan, index + 1U, "ORDER") ||
            !placeholder_scan_token_text_equals(scan, index + 2U, "BY")) {
            continue;
        }
        return placeholder_scan_token_can_name_loose_identifier(scan, index + 3U);
    }
    return false;
}

static bool ddl_residual_scan_has_create_table_start_transaction_option(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_starts_create_table_statement(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (index > 0U && token_is_right_paren(&scan->tokens[index - 1U]) &&
            placeholder_scan_token_text_equals(scan, index, "START") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "TRANSACTION")) {
            return true;
        }
    }
    return false;
}

static bool ddl_check_expression_placeholder_scan_has_marker(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_contains_text(scan, "CHECK")) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "CHECK") &&
            token_is_left_paren(&scan->tokens[index + 1U]) &&
            ddl_check_expression_placeholder_clause_has_marker(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool ddl_check_expression_placeholder_clause_has_marker(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            if (paren_depth == 0) {
                return false;
            }
            --paren_depth;
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") ||
            placeholder_scan_token_text_equals(scan, index, "&&") ||
            placeholder_scan_token_text_equals(scan, index, "||")) {
            return true;
        }
        if (index + 1U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index, "IN") &&
            ddl_check_expression_in_list_marker_is_supported(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool ddl_check_expression_in_list_marker_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    int paren_depth = 0;
    bool saw_item = false;
    bool need_item = true;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            saw_item = true;
            need_item = false;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            if (paren_depth > 0) {
                --paren_depth;
                continue;
            }
            return saw_item && !need_item;
        }
        if (paren_depth == 0 && token_is_comma(&scan->tokens[index])) {
            if (need_item) {
                return false;
            }
            need_item = true;
            continue;
        }
        saw_item = true;
        need_item = false;
    }
    return false;
}

static bool placeholder_scan_starts_create_table_statement(
    const struct placeholder_statement_scan *scan
) {
    enum { create_table_scan_limit = 6 };

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count && index < create_table_scan_limit; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "TABLE")) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "VIEW") ||
            placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_starts_alter_table_statement(
    const struct placeholder_statement_scan *scan
) {
    return placeholder_scan_token_text_equals(scan, 0U, "ALTER") &&
           placeholder_scan_token_text_equals(scan, 1U, "TABLE");
}

static bool placeholder_scan_starts_create_index_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t index = 1U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "UNIQUE") ||
        placeholder_scan_token_text_equals(scan, index, "FULLTEXT") ||
        placeholder_scan_token_text_equals(scan, index, "SPATIAL")) {
        ++index;
    }
    return placeholder_scan_token_text_equals(scan, index, "INDEX");
}

static bool alter_table_partition_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t operation_start_index = 0U;

    if (scan == NULL || scan->token_count < alter_table_partition_min_token_count ||
        !alter_table_partition_scan_has_balanced_parentheses(scan, 2U)) {
        return false;
    }
    operation_start_index = alter_table_partition_operation_start_index(scan);
    if (operation_start_index >= scan->token_count) {
        return false;
    }
    return alter_table_partition_scan_has_operation(scan, operation_start_index);
}

static bool alter_table_tablespace_file_operation_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t operation_start_index = 0U;

    if (scan == NULL || scan->token_count < alter_table_partition_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 1U, "TABLE") ||
        !alter_table_partition_scan_has_balanced_parentheses(scan, 2U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    operation_start_index = alter_table_partition_operation_start_index(scan);
    for (size_t index = operation_start_index; index + 1U < scan->token_count; ++index) {
        if (alter_table_tablespace_file_operation_starts_at(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool alter_table_tablespace_file_operation_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t operation_index
) {
    size_t index = operation_index + 1U;

    if (scan == NULL || operation_index + 1U >= scan->token_count ||
        (!placeholder_scan_token_text_equals(scan, operation_index, "DISCARD") &&
         !placeholder_scan_token_text_equals(scan, operation_index, "IMPORT"))) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "TABLESPACE")) {
        return true;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "PARTITION") &&
        !placeholder_scan_token_text_equals(scan, index, "SUBPARTITION")) {
        return false;
    }
    ++index;
    while (placeholder_scan_token_can_name_loose_identifier(scan, index)) {
        ++index;
        if (!token_is_comma(&scan->tokens[index])) {
            break;
        }
        ++index;
    }
    return index < scan->token_count &&
           placeholder_scan_token_text_equals(scan, index, "TABLESPACE");
}

static size_t alter_table_partition_operation_start_index(
    const struct placeholder_statement_scan *scan
) {
    size_t table_name_index = 2U;

    if (scan == NULL) {
        return table_name_index;
    }
    if (table_name_index + 2U < scan->token_count &&
        placeholder_scan_token_text_equals(scan, table_name_index + 1U, ".")) {
        return table_name_index + 3U;
    }
    return table_name_index + 1U;
}

static bool alter_table_partition_scan_has_operation(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    static const char *const partition_action_prefixes[] = {
        "ADD",
        "DROP",
        "REORGANIZE",
        "REBUILD",
        "COALESCE",
        "TRUNCATE",
        "EXCHANGE",
        "ANALYZE",
        "CHECK",
        "OPTIMIZE",
        "REPAIR",
    };
    int paren_depth = 0;

    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (index + 2U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index, "PARTITION") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY")) {
            return true;
        }
        if (index + 1U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index, "REMOVE") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "PARTITIONING")) {
            return true;
        }
        if (index + 1U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index + 1U, "PARTITION") &&
            placeholder_scan_token_text_equals_any(
                scan,
                index,
                partition_action_prefixes,
                sizeof(partition_action_prefixes) / sizeof(partition_action_prefixes[0])
            )) {
            if (placeholder_scan_token_text_equals(scan, index, "ADD") ||
                placeholder_scan_token_text_equals(scan, index, "REORGANIZE")) {
                return true;
            }
            return index + 2U < scan->token_count;
        }
    }
    return false;
}

static bool alter_table_partition_scan_has_balanced_parentheses(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return paren_depth == 0;
}

static bool placeholder_scan_token_text_equals_any(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *const *texts,
    size_t text_count
) {
    if (texts == NULL) {
        return false;
    }
    for (size_t text_index = 0U; text_index < text_count; ++text_index) {
        if (placeholder_scan_token_text_equals(scan, index, texts[text_index])) {
            return true;
        }
    }
    return false;
}

static bool load_xml_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "LOAD") ||
        !placeholder_scan_token_text_equals(scan, 1U, "XML") ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, 2U, "INFILE") ||
           (placeholder_scan_token_text_equals(scan, 2U, "LOCAL") &&
            placeholder_scan_token_text_equals(scan, 3U, "INFILE"));
}

static bool select_into_file_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "INTO") &&
            (placeholder_scan_token_text_equals(scan, index + 1U, "OUTFILE") ||
             placeholder_scan_token_text_equals(scan, index + 1U, "DUMPFILE")) &&
            scan->tokens[index + 2U].kind == MYLITE_SQL_TOKEN_STRING) {
            return true;
        }
    }
    return false;
}

static bool import_table_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           placeholder_scan_token_text_equals(scan, 0U, "IMPORT") &&
           placeholder_scan_token_text_equals(scan, 1U, "TABLE") &&
           placeholder_scan_token_text_equals(scan, 2U, "FROM") && scan->token_count > 3U &&
           scan->tokens[3U].kind == MYLITE_SQL_TOKEN_STRING &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
}

static bool help_placeholder_statement_is_supported(const struct placeholder_statement_scan *scan) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           placeholder_scan_token_text_equals(scan, 0U, "HELP") && scan->token_count > 1U &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
}

static bool lock_instance_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon && scan->token_count == 4U &&
           placeholder_scan_token_text_equals(scan, 0U, "LOCK") &&
           placeholder_scan_token_text_equals(scan, 1U, "INSTANCE") &&
           placeholder_scan_token_text_equals(scan, 2U, "FOR") &&
           placeholder_scan_token_text_equals(scan, 3U, "BACKUP");
}

static bool unlock_instance_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon && scan->token_count == 2U &&
           placeholder_scan_token_text_equals(scan, 0U, "UNLOCK") &&
           placeholder_scan_token_text_equals(scan, 1U, "INSTANCE");
}

static bool change_replication_source_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon && scan->token_count > 4U &&
           placeholder_scan_token_text_equals(scan, 0U, "CHANGE") &&
           placeholder_scan_token_text_equals(scan, 1U, "REPLICATION") &&
           placeholder_scan_token_text_equals(scan, 2U, "SOURCE") &&
           placeholder_scan_token_text_equals(scan, 3U, "TO") &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
           placeholder_scan_parentheses_are_balanced(scan, 0U);
}

static enum placeholder_statement_kind classify_drop_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "USER") ||
        placeholder_scan_token_text_equals(scan, 1U, "ROLE") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (foreign_server_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLESPACE")) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (undo_tablespace_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "FUNCTION") ||
        placeholder_scan_token_text_equals(scan, 1U, "TRIGGER") ||
        placeholder_scan_token_text_equals(scan, 1U, "EVENT")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_set_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    enum placeholder_statement_kind leading_kind = classify_set_leading_placeholder_statement(scan);
    bool statement_is_complete = false;

    if (leading_kind != PLACEHOLDER_STATEMENT_NONE) {
        return leading_kind;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TRANSACTION")) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    statement_is_complete = !scan->has_non_trailing_semicolon &&
                            placeholder_scan_parentheses_are_balanced(scan, 0U) &&
                            !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
    if (!statement_is_complete) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_expression_operator_surface(scan) ||
        set_placeholder_scan_contains_unsupported_residual(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_set_leading_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "PASSWORD") ||
        placeholder_scan_token_text_equals(scan, 1U, "ROLE") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE") ||
        placeholder_scan_token_text_equals(scan, 1U, "PERSIST") ||
        placeholder_scan_token_text_equals(scan, 1U, "PERSIST_ONLY") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@PERSIST.") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@PERSIST_ONLY.")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "GLOBAL") ||
        placeholder_scan_token_text_equals(scan, 1U, "SESSION") ||
        placeholder_scan_token_text_equals(scan, 1U, "LOCAL") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@GLOBAL.") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@SESSION.") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@LOCAL.")) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "DEFAULT") &&
        placeholder_scan_token_text_equals(scan, 2U, "ROLE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool set_placeholder_scan_contains_unsupported_residual(
    const struct placeholder_statement_scan *scan
) {
    bool saw_assignment_operator = false;

    for (size_t index = 1U; index < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index];
        bool is_assignment_operator = token_is_equal_sign(token) ||
                                      placeholder_scan_token_is_assignment_operator(scan, index);

        if (set_placeholder_token_requires_unsupported_utility(
                scan,
                index,
                saw_assignment_operator
            )) {
            return true;
        }
        if (is_assignment_operator) {
            saw_assignment_operator = true;
        }
    }
    return false;
}

static bool set_placeholder_token_requires_unsupported_utility(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool saw_assignment_operator
) {
    const struct mylite_sql_token *token = &scan->tokens[index];

    if (placeholder_scan_token_text_equals(scan, index, "PERSIST") ||
        placeholder_scan_token_text_equals(scan, index, "PERSIST_ONLY")) {
        return true;
    }
    if (token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind == MYLITE_SQL_TOKEN_STRING && index > 1U &&
        scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_STRING) {
        return true;
    }
    if (token->kind == MYLITE_SQL_TOKEN_SYSTEM_VARIABLE && index > 1U &&
        (token_is_equal_sign(&scan->tokens[index - 1U]) ||
         placeholder_scan_token_is_assignment_operator(scan, index - 1U))) {
        return true;
    }
    if (set_placeholder_assignment_value_starts_unsupported_surface(scan, index)) {
        return true;
    }
    return set_placeholder_assigned_token_is_unsupported_surface(
        scan,
        index,
        saw_assignment_operator
    );
}

static bool set_placeholder_assignment_value_starts_unsupported_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (!token_is_equal_sign(&scan->tokens[index]) &&
        !placeholder_scan_token_is_assignment_operator(scan, index)) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, index + 1U, "EXISTS") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "FROM_UNIXTIME") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "CHAR") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "LEFT") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "DEBUG");
}

static bool set_placeholder_assigned_token_is_unsupported_surface(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool saw_assignment_operator
) {
    const struct mylite_sql_token *token = &scan->tokens[index];

    if (!saw_assignment_operator) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
        placeholder_scan_token_text_equals(scan, index, "IN") ||
        placeholder_scan_token_text_equals(scan, index, "EXISTS") ||
        placeholder_scan_token_text_equals(scan, index, "FROM_UNIXTIME")) {
        return true;
    }
    return token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == '.';
}

static enum placeholder_statement_kind classify_show_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (show_engine_logs_mutex_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (!scan->has_non_trailing_semicolon && placeholder_scan_parentheses_are_balanced(scan, 0U) &&
        !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
        placeholder_scan_contains_text(scan, "WHERE") &&
        (placeholder_scan_token_text_equals(scan, 1U, "TRIGGERS") ||
         (placeholder_scan_token_text_equals(scan, 1U, "OPEN") &&
          placeholder_scan_token_text_equals(scan, 2U, "TABLES")))) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "CREATE")) {
        if (placeholder_scan_token_text_equals(scan, 2U, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, 2U, "TRIGGER") ||
            placeholder_scan_token_text_equals(scan, 2U, "EVENT")) {
            return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
        }
        if (placeholder_scan_token_text_equals(scan, 2U, "USER")) {
            return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
        }
    }
    if ((placeholder_scan_token_text_equals(scan, 1U, "PROCEDURE") ||
         placeholder_scan_token_text_equals(scan, 1U, "FUNCTION")) &&
        placeholder_scan_token_text_equals(scan, 2U, "CODE")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "PROFILE") ||
        placeholder_scan_token_text_equals(scan, 1U, "PROFILES")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_describe_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count < 2U || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_explain_statement_start_is_supported(scan, 1U)) {
        return PLACEHOLDER_STATEMENT_EXPLAIN;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool show_engine_logs_mutex_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count != 4U || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "SHOW") ||
        !placeholder_scan_token_text_equals(scan, 1U, "ENGINE") ||
        !placeholder_scan_token_is_identifier_like(scan, 2U) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, 3U, "LOGS") ||
           placeholder_scan_token_text_equals(scan, 3U, "MUTEX");
}

static enum placeholder_statement_kind classify_explain_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t statement_index = 1U;

    if (scan == NULL || scan->token_count < 2U) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_token_text_equals(scan, statement_index, "ANALYZE")) {
        ++statement_index;
    }
    if (placeholder_scan_token_text_equals(scan, statement_index, "FORMAT")) {
        statement_index += 3U;
        if (!placeholder_scan_token_text_equals(scan, statement_index - 2U, "=") ||
            !placeholder_scan_token_is_identifier_like(scan, statement_index - 1U)) {
            return PLACEHOLDER_STATEMENT_NONE;
        }
    }
    if (statement_index >= scan->token_count) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (!placeholder_explain_statement_start_is_supported(scan, statement_index)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    return PLACEHOLDER_STATEMENT_EXPLAIN;
}

static bool placeholder_explain_statement_start_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
        placeholder_scan_token_text_equals(scan, index, "TABLE") ||
        placeholder_scan_token_text_equals(scan, index, "WITH") ||
        placeholder_scan_token_text_equals(scan, index, "(")) {
        return true;
    }
    return placeholder_scan_token_text_equals(scan, index, "VALUES") ||
           placeholder_scan_token_text_equals(scan, index, "INSERT") ||
           placeholder_scan_token_text_equals(scan, index, "REPLACE") ||
           placeholder_scan_token_text_equals(scan, index, "UPDATE") ||
           placeholder_scan_token_text_equals(scan, index, "DELETE");
}

static bool placeholder_scan_contains_text(
    const struct placeholder_statement_scan *scan,
    const char *text
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, text)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_identifier_like(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
           token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER ||
           placeholder_scan_token_text_equals(scan, index, "CURRENT") ||
           placeholder_scan_token_text_equals(scan, index, "LOCKED") ||
           placeholder_scan_token_text_equals(scan, index, "NOWAIT") ||
           placeholder_scan_token_text_equals(scan, index, "SESSION_USER") ||
           placeholder_scan_token_text_equals(scan, index, "SKIP") ||
           placeholder_scan_token_text_equals(scan, index, "SYSTEM_USER") ||
           placeholder_scan_token_text_equals(scan, index, "TRADITIONAL") ||
           placeholder_scan_token_text_equals(scan, index, "USER") ||
           placeholder_scan_token_text_equals(scan, index, "JSON") ||
           placeholder_scan_token_text_equals(scan, index, "TREE") ||
           placeholder_scan_token_text_equals(scan, index, "CSV");
}

static bool placeholder_scan_token_text_equals(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *text
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    return mylite_sql_parser_token_text_equals(&scan->tokens[index], text);
}

static bool placeholder_scan_token_text_starts_with(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *prefix
) {
    const struct mylite_sql_token *token = NULL;
    size_t prefix_size = strlen(prefix);

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    if (token->length < prefix_size) {
        return false;
    }
    for (size_t offset = 0U; offset < prefix_size; ++offset) {
        if (mylite_sql_parser_ascii_upper((unsigned char)token->text[offset]) !=
            mylite_sql_parser_ascii_upper((unsigned char)prefix[offset])) {
            return false;
        }
    }
    return true;
}

static bool placeholder_token_is_semicolon(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == ';';
}

static enum mylite_sql_ast_node_kind ast_kind_for_placeholder_statement(
    enum placeholder_statement_kind kind
) {
    switch (kind) {
    case PLACEHOLDER_STATEMENT_ADMIN_NOOP:
        return MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT;
    case PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM:
        return MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT;
    case PLACEHOLDER_STATEMENT_UTILITY_NOOP:
        return MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT;
    case PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY:
        return MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT;
    case PLACEHOLDER_STATEMENT_EXPLAIN:
        return MYLITE_SQL_AST_EXPLAIN_STATEMENT;
    case PLACEHOLDER_STATEMENT_ALTER_TABLE_MULTI_ACTION_UNSUPPORTED:
        return MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT;
    case PLACEHOLDER_STATEMENT_NONE:
        break;
    }
    return MYLITE_SQL_AST_SCRIPT;
}

static enum mylite_sql_parse_status finish_placeholder_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum placeholder_statement_kind kind
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_ast_node *script = NULL;

    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
    result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&result->ast);

    state = (struct mylite_sql_parser_state){
        .result = result,
        .modes = config.modes,
        .accepted = true,
    };
    statement = mylite_sql_parser_make_raw_statement(
        &state,
        ast_kind_for_placeholder_statement(kind),
        (struct mylite_sql_source_span){
            .text = config.input,
            .length = config.length,
            .offset = 0U,
        }
    );
    script = mylite_sql_parser_make_script_with_statement(&state, statement);
    if (statement == NULL || script == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }

    mylite_sql_parser_state_set_root(&state, script);
    return result->status;
}

static enum mylite_sql_parse_status finish_explain_placeholder_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *format = NULL;
    struct mylite_sql_ast_node *analyze = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_ast_node *script = NULL;
    size_t token_index = 1U;

    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
    result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&result->ast);

    state = (struct mylite_sql_parser_state){
        .result = result,
        .modes = config.modes,
        .accepted = true,
    };

    if (placeholder_scan_token_text_equals(scan, token_index, "ANALYZE")) {
        analyze = mylite_sql_parser_make_explain_analyze(&state, scan->tokens[token_index]);
        ++token_index;
    }
    if (placeholder_scan_token_text_equals(scan, token_index, "FORMAT")) {
        struct mylite_sql_ast_node *format_name =
            mylite_sql_parser_make_identifier(&state, scan->tokens[token_index + 2U]);
        format =
            mylite_sql_parser_make_explain_format(&state, scan->tokens[token_index], format_name);
    }

    statement =
        mylite_sql_parser_make_explain_statement(&state, scan->tokens[0], format, analyze, NULL);
    script = mylite_sql_parser_make_script_with_statement(&state, statement);
    if (statement == NULL || script == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }

    mylite_sql_parser_state_set_root(&state, script);
    return result->status;
}

void mylite_sql_parser_state_set_root(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *root
) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    state->result->root = root;
}

void mylite_sql_parser_state_syntax_error(
    struct mylite_sql_parser_state *state,
    int parser_token,
    struct mylite_sql_token token
) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    record_parse_error(
        state->result,
        (struct mylite_sql_parse_error){
            .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
            .parser_token = parser_token,
            .token = token,
        }
    );
}

void mylite_sql_parser_state_parse_failed(struct mylite_sql_parser_state *state) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
}

void mylite_sql_parser_state_accept(struct mylite_sql_parser_state *state) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    state->accepted = true;
}

void mylite_sql_parser_state_stack_overflow(struct mylite_sql_parser_state *state) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_STACK_OVERFLOW);
}

static bool feed_parenthesized_row_constructor_if_needed(
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct parenthesized_row_constructor_injection *injection
) {
    struct mylite_sql_token synthetic_row;

    if (!should_inject_parenthesized_row_constructor(injection)) {
        return true;
    }

    synthetic_row = make_synthetic_row_constructor_token(injection->left_paren);
    mylite_sql_lemon(parser, MYLITE_SQL_PARSE_ROW, synthetic_row, state);
    if (previous_token_was_dot != NULL) {
        *previous_token_was_dot = false;
    }
    update_parser_token_history(history, MYLITE_SQL_PARSE_ROW);

    return state != NULL && state->result != NULL && state->result->status == MYLITE_SQL_PARSE_OK;
}

static bool should_inject_parenthesized_row_constructor(
    const struct parenthesized_row_constructor_injection *injection
) {
    const struct mylite_sql_token *previous_token = NULL;

    if (injection == NULL || !injection->enabled || !token_is_left_paren(injection->left_paren)) {
        return false;
    }
    if (injection->has_previous_token) {
        previous_token = injection->previous_token;
    }
    if (token_can_name_immediate_function(previous_token)) {
        return false;
    }
    return lexer_parenthesized_expression_has_top_level_comma(injection->lexer);
}

static bool token_can_name_immediate_function(const struct mylite_sql_token *token) {
    if (token == NULL) {
        return false;
    }
    if (token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
        token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind != MYLITE_SQL_TOKEN_KEYWORD) {
        return false;
    }
    return !mylite_sql_parser_token_text_equals(token, "SELECT") &&
           !mylite_sql_parser_token_text_equals(token, "FROM") &&
           !mylite_sql_parser_token_text_equals(token, "WHERE") &&
           !mylite_sql_parser_token_text_equals(token, "HAVING") &&
           !mylite_sql_parser_token_text_equals(token, "ON") &&
           !mylite_sql_parser_token_text_equals(token, "GROUP") &&
           !mylite_sql_parser_token_text_equals(token, "ORDER") &&
           !mylite_sql_parser_token_text_equals(token, "BY") &&
           !mylite_sql_parser_token_text_equals(token, "LIMIT") &&
           !mylite_sql_parser_token_text_equals(token, "UNION") &&
           !mylite_sql_parser_token_text_equals(token, "IN") &&
           !mylite_sql_parser_token_text_equals(token, "NOT") &&
           !mylite_sql_parser_token_text_equals(token, "VALUES") &&
           !mylite_sql_parser_token_text_equals(token, "SET") &&
           !mylite_sql_parser_token_text_equals(token, "UPDATE") &&
           !mylite_sql_parser_token_text_equals(token, "DELETE") &&
           !mylite_sql_parser_token_text_equals(token, "INSERT") &&
           !mylite_sql_parser_token_text_equals(token, "REPLACE") &&
           !mylite_sql_parser_token_text_equals(token, "JOIN");
}

static bool lexer_parenthesized_expression_has_top_level_comma(const struct mylite_sql_lexer *lexer
) {
    struct mylite_sql_lexer lookahead;
    int paren_depth = 1;
    bool has_top_level_comma = false;

    if (lexer == NULL) {
        return false;
    }

    lookahead = *lexer;
    for (;;) {
        struct mylite_sql_token token;

        if (mylite_sql_lexer_next(&lookahead, &token) != 0) {
            return false;
        }
        if (mylite_sql_parser_token_is_comment(token.kind)) {
            continue;
        }
        if (token.kind == MYLITE_SQL_TOKEN_ERROR || token.kind == MYLITE_SQL_TOKEN_EOF) {
            return false;
        }
        if (token_is_left_paren(&token)) {
            ++paren_depth;
            continue;
        }
        if (token_is_right_paren(&token)) {
            --paren_depth;
            if (paren_depth <= 0) {
                return has_top_level_comma;
            }
            continue;
        }
        if (paren_depth == 1 && token_is_comma(&token)) {
            has_top_level_comma = true;
        }
    }
}

static struct mylite_sql_token make_synthetic_row_constructor_token(
    const struct mylite_sql_token *left_paren
) {
    struct mylite_sql_token token = *left_paren;

    token.flags |= MYLITE_SQL_TOKEN_SYNTHETIC_ROW_CONSTRUCTOR;
    return token;
}

static void update_parser_token_history(
    struct mylite_sql_parser_token_history *history,
    int parser_token
) {
    if (history == NULL) {
        return;
    }
    history->token_before_previous_parser_token = history->previous_parser_token;
    history->previous_parser_token = parser_token;
}

static void record_parse_error(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parse_error error
) {
    if (result == NULL || result->status != MYLITE_SQL_PARSE_OK) {
        return;
    }

    result->status = error.status;
    result->parser_token = error.parser_token;
    result->error_token = error.token;
}

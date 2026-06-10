#include "mylite_parser.h"

#include "mylite_parse.h"
#include "mylite_parser_internal.h"

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

struct mylite_sql_parser_token_map {
    int parser_token;
    bool previous_token_was_dot;
};

struct mylite_sql_parser_token_history {
    int previous_parser_token;
    int token_before_previous_parser_token;
};

struct parenthesized_row_constructor_injection {
    bool enabled;
    const struct mylite_sql_lexer *lexer;
    const struct mylite_sql_token *left_paren;
    const struct mylite_sql_token *previous_token;
    bool has_previous_token;
};

struct mylite_sql_token_kind_mapping {
    enum mylite_sql_token_kind kind;
    int parser_token;
};

struct mylite_sql_punctuation_mapping {
    char punctuation;
    int parser_token;
};

struct mylite_sql_operator_mapping {
    enum mylite_sql_operator_kind operator_kind;
    int parser_token;
};

struct mylite_sql_parse_error {
    enum mylite_sql_parse_status status;
    int parser_token;
    struct mylite_sql_token token;
};

struct column_attribute_positions {
    size_t charset;
    size_t collation;
    size_t binary_collation;
    size_t comment;
    size_t nullability;
    size_t default_value;
    size_t primary_key;
    size_t unique_key;
    size_t auto_increment;
    size_t generated;
    size_t visibility;
    size_t srid;
};

enum placeholder_statement_kind {
    PLACEHOLDER_STATEMENT_NONE = 0,
    PLACEHOLDER_STATEMENT_ADMIN_NOOP = 1,
    PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM = 2,
    PLACEHOLDER_STATEMENT_UTILITY_NOOP = 3,
    PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY = 4,
    PLACEHOLDER_STATEMENT_EXPLAIN = 5,
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
    alter_table_partition_min_token_count = 5,
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
static enum mylite_sql_parse_status try_parse_select_result_option_before_duplicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
static bool map_lexer_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
    struct mylite_sql_parser_token_map *out_map
);
static bool should_skip_select_lock_target_list(
    const struct mylite_sql_token *token,
    const struct mylite_sql_parser_token_history *history
);
static enum mylite_sql_parse_status skip_select_lock_target_list(
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_next_token
);
static bool token_can_be_select_lock_target_identifier(const struct mylite_sql_token *token);
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
static bool map_direct_lexer_token(enum mylite_sql_token_kind kind, int *out_parser_token);
static bool lexer_token_has_immediate_left_paren(
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token
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
static enum placeholder_statement_kind classify_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_schema_security_placeholder_statement(
    const struct placeholder_statement_scan *scan
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
static enum placeholder_statement_kind classify_query_surface_placeholder_statement(
    const struct placeholder_statement_scan *scan
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
static bool placeholder_scan_starts_query_statement(const struct placeholder_statement_scan *scan);
static bool placeholder_scan_parenthesized_start_is_query_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_parenthesized_table_reference_surface(
    const struct placeholder_statement_scan *scan
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
static bool placeholder_scan_contains_query_function_subquery_surface(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_scan_contains_any_function_call_surface(
    const struct placeholder_statement_scan *scan
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
static enum placeholder_statement_kind classify_drop_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_set_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_show_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static enum placeholder_statement_kind classify_explain_placeholder_statement(
    const struct placeholder_statement_scan *scan
);
static bool placeholder_explain_statement_start_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool analyze
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
static bool is_comment_token(enum mylite_sql_token_kind kind);
static bool map_keyword_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
    bool has_immediate_left_paren,
    int *out_parser_token
);
static bool token_text_is_count_function_name(const struct mylite_sql_token *token);
static bool token_text_is_generic_aggregate_window_function_name(
    const struct mylite_sql_token *token
);
static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token);
static bool map_operator_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    int *out_parser_token
);
static struct mylite_sql_ast_node *parser_child_at(struct mylite_sql_ast_node *node, size_t index);
static bool previous_token_allows_select_noop_modifier(int previous_parser_token);
static bool token_text_equals(const struct mylite_sql_token *token, const char *text);
static char ascii_upper(unsigned char byte);
static bool is_parse_ok(const struct mylite_sql_parser_state *state);
static bool parser_sql_mode_has(
    const struct mylite_sql_parser_state *state,
    enum mylite_sql_mode mode
);
static bool create_table_name_is_no_space_function_identifier(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *table_name,
    const struct mylite_sql_token *left_paren
);
static void set_state_status(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_parse_status status
);
static struct mylite_sql_ast_node *make_node(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
);
static struct mylite_sql_source_span span_from_token(const struct mylite_sql_token *token);
static struct mylite_sql_source_span span_join(
    struct mylite_sql_source_span left,
    struct mylite_sql_source_span right
);
static void apply_alter_table_options(
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_alter_table_options options
);
static const struct mylite_sql_ast_node *last_identifier_component(
    const struct mylite_sql_ast_node *identifier
);
static bool span_text_equals(const struct mylite_sql_source_span *span, const char *text);
static bool span_text_matches_ignore_space_function_name(const struct mylite_sql_source_span *span);
static int scan_column_attribute_positions(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *attributes,
    struct column_attribute_positions *out_positions
);
static int record_column_attribute_position(
    struct mylite_sql_parser_state *state,
    size_t *slot,
    size_t position
);
static int validate_legacy_column_attribute_order(
    struct mylite_sql_parser_state *state,
    const struct column_attribute_positions *positions
);
static size_t column_charset_collation_position_limit(
    const struct column_attribute_positions *positions
);
static bool column_attribute_charset_order_is_valid(
    const struct column_attribute_positions *positions
);
static bool legacy_column_attribute_precedes_charset_collation(
    const struct column_attribute_positions *positions,
    size_t charset_collation_limit
);
static bool column_attribute_position_is_set(size_t position);

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

    if (out_result == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    memset(out_result, 0, sizeof(*out_result));
    out_result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&out_result->ast);

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
    for (;;) {
        struct mylite_sql_token token;
        struct mylite_sql_parser_token_map token_map;

        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            out_result->status = MYLITE_SQL_PARSE_MISUSE;
            break;
        }

        if (is_comment_token(token.kind)) {
            continue;
        }

        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            record_parse_error(
                out_result,
                (struct mylite_sql_parse_error){
                    .status = MYLITE_SQL_PARSE_LEXER_ERROR,
                    .parser_token = 0,
                    .token = token,
                }
            );
            break;
        }

        /* Lock targets do not affect MyLite's embedded no-op locking behavior. */
        if (should_skip_select_lock_target_list(&token, &token_history)) {
            enum mylite_sql_parse_status status = skip_select_lock_target_list(&lexer, &token);

            if (status != MYLITE_SQL_PARSE_OK) {
                record_parse_error(
                    out_result,
                    (struct mylite_sql_parse_error){
                        .status = status,
                        .parser_token = 0,
                        .token = token,
                    }
                );
                break;
            }
        }

        if (!feed_parenthesized_row_constructor_if_needed(
                parser,
                &state,
                &token_history,
                &previous_token_was_dot,
                &(struct parenthesized_row_constructor_injection){
                    .enabled = inject_parenthesized_row_constructors,
                    .lexer = &lexer,
                    .left_paren = &token,
                    .previous_token = &previous_token,
                    .has_previous_token = has_previous_token,
                }
            )) {
            break;
        }

        if (!map_lexer_token(
                &state,
                &lexer,
                &token,
                previous_token_was_dot,
                token_history.previous_parser_token,
                &token_map
            )) {
            record_parse_error(
                out_result,
                (struct mylite_sql_parse_error){
                    .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                    .parser_token = -1,
                    .token = token,
                }
            );
            break;
        }

        mylite_sql_lemon(parser, token_map.parser_token, token, &state);
        previous_token_was_dot = token_map.previous_token_was_dot;
        update_parser_token_history(&token_history, token_map.parser_token);
        previous_token = token;
        has_previous_token = token.kind != MYLITE_SQL_TOKEN_EOF;

        if (out_result->status != MYLITE_SQL_PARSE_OK || token.kind == MYLITE_SQL_TOKEN_EOF) {
            break;
        }
    }

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return out_result->status;
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

    memset(out_result, 0, sizeof(*out_result));
    out_result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&out_result->ast);

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
    struct mylite_sql_parser_token_map token_map;
    struct mylite_sql_lexer token_lexer;

    if (parser == NULL || state == NULL || state->result == NULL || history == NULL ||
        previous_token_was_dot == NULL || token == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
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
    if (!map_lexer_token(
            state,
            &token_lexer,
            token,
            *previous_token_was_dot,
            history->previous_parser_token,
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
    for (;;) {
        struct mylite_sql_token token;

        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            free(tokens);
            return MYLITE_SQL_PARSE_MISUSE;
        }
        if (is_comment_token(token.kind)) {
            continue;
        }
        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            free(tokens);
            return MYLITE_SQL_PARSE_LEXER_ERROR;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            break;
        }
        if (placeholder_token_is_semicolon(&token)) {
            saw_semicolon = true;
            continue;
        }
        if (saw_semicolon) {
            *out_has_non_trailing_semicolon = true;
        }
        if (!append_placeholder_statement_token(&token, &tokens, &token_count, &token_capacity)) {
            free(tokens);
            return MYLITE_SQL_PARSE_NOMEM;
        }
    }

    *out_tokens = tokens;
    *out_token_count = token_count;
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

    statement = parser_child_at(prefix_result.root, 0U);
    if (!alter_table_statement_accepts_prefix_option_tail(statement)) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    apply_alter_table_options(statement, options);
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
    ++*index;
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
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->tokens == NULL ||
        !scan_is_create_table_partition_statement(scan, &partition_index)) {
        return MYLITE_SQL_PARSE_OK;
    }

    status = parse_sql_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = scan->tokens[partition_index].offset,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    return MYLITE_SQL_PARSE_OK;
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
    if (placeholder_scan_token_text_equals(scan, 0U, "RESET") ||
        placeholder_scan_token_text_equals(scan, 0U, "FLUSH") ||
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

static enum placeholder_statement_kind classify_query_surface_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
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
    return PLACEHOLDER_STATEMENT_NONE;
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
        placeholder_scan_contains_fulltext_match_against_surface(scan)) {
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
    if (placeholder_scan_contains_parenthesized_table_reference_surface(scan)) {
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
        placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
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

static bool placeholder_scan_parenthesized_table_reference_body_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    size_t right_paren_index = 0U;
    size_t first_index = left_paren_index + 1U;
    int paren_depth = 0;
    bool saw_reference_name = false;

    if (!placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= first_index ||
        placeholder_scan_query_expression_starts_at(scan, first_index)) {
        return false;
    }
    for (size_t index = first_index; index < right_paren_index; ++index) {
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
    if (!placeholder_scan_has_table_backed_or_dml_context(scan)) {
        return false;
    }
    return placeholder_scan_contains_expression_operator_surface(scan) ||
           placeholder_scan_contains_row_tuple_predicate_surface(scan) ||
           placeholder_scan_contains_bare_truth_clause_surface(scan);
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
        placeholder_scan_token_text_equals(scan, index, "MOD")) {
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
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_OR);
}

static bool placeholder_scan_expression_operator_surface_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        !placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
        return false;
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
            return expression_token_count == 1U;
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
    return expression_token_count == 1U;
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
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
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
        if (placeholder_scan_token_text_equals(scan, index, "LIKE") &&
            !placeholder_scan_token_text_equals(scan, index - 1U, "SOUNDS") &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 1U)) {
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
        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") &&
            !token_is_left_paren(&scan->tokens[index + 1U]) &&
            !placeholder_scan_interval_expression_follows_parenthesized_separator(scan, index) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            placeholder_scan_token_is_date_interval_unit(scan, index + 2U)) {
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
            placeholder_scan_match_column_list_without_parentheses_is_supported(scan, index)) {
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
    if (ddl_extended_option_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLE") &&
        alter_table_partition_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "PROCEDURE") ||
        placeholder_scan_token_text_equals(scan, 1U, "FUNCTION") ||
        placeholder_scan_token_text_equals(scan, 1U, "EVENT")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    return PLACEHOLDER_STATEMENT_NONE;
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
        if (index + 2U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index + 1U, "PARTITION") &&
            placeholder_scan_token_text_equals_any(
                scan,
                index,
                partition_action_prefixes,
                sizeof(partition_action_prefixes) / sizeof(partition_action_prefixes[0])
            )) {
            return true;
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
    if (placeholder_scan_token_text_equals(scan, 1U, "TRANSACTION")) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (!scan->has_non_trailing_semicolon && placeholder_scan_parentheses_are_balanced(scan, 0U) &&
        !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
        placeholder_scan_contains_expression_operator_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_show_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
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

static enum placeholder_statement_kind classify_explain_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t statement_index = 1U;
    bool analyze = false;

    if (scan == NULL || scan->token_count < 2U) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_token_text_equals(scan, statement_index, "ANALYZE")) {
        analyze = true;
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
    if (!placeholder_explain_statement_start_is_supported(scan, statement_index, analyze)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    return PLACEHOLDER_STATEMENT_EXPLAIN;
}

static bool placeholder_explain_statement_start_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool analyze
) {
    if (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
        placeholder_scan_token_text_equals(scan, index, "TABLE") ||
        placeholder_scan_token_text_equals(scan, index, "WITH") ||
        placeholder_scan_token_text_equals(scan, index, "(")) {
        return true;
    }
    if (analyze) {
        return false;
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
           placeholder_scan_token_text_equals(scan, index, "TRADITIONAL") ||
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
    return token_text_equals(&scan->tokens[index], text);
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
        if (ascii_upper((unsigned char)token->text[offset]) !=
            ascii_upper((unsigned char)prefix[offset])) {
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
            .line = 1U,
            .column = 1U,
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
    if (!is_parse_ok(state)) {
        return;
    }

    state->result->root = root;
}

void mylite_sql_parser_state_syntax_error(
    struct mylite_sql_parser_state *state,
    int parser_token,
    struct mylite_sql_token token
) {
    if (!is_parse_ok(state)) {
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
    if (!is_parse_ok(state)) {
        return;
    }

    set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
}

void mylite_sql_parser_state_accept(struct mylite_sql_parser_state *state) {
    if (!is_parse_ok(state)) {
        return;
    }

    state->accepted = true;
}

void mylite_sql_parser_state_stack_overflow(struct mylite_sql_parser_state *state) {
    if (!is_parse_ok(state)) {
        return;
    }

    set_state_status(state, MYLITE_SQL_PARSE_STACK_OVERFLOW);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_script(struct mylite_sql_parser_state *state) {
    return make_node(state, MYLITE_SQL_AST_SCRIPT, (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *mylite_sql_parser_make_script_with_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement
) {
    struct mylite_sql_ast_node *script = mylite_sql_parser_make_script(state);
    if (script == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(script, statement);
    if (statement != NULL) {
        mylite_sql_ast_node_set_span(script, statement->span);
    }
    return script;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *script,
    struct mylite_sql_ast_node *statement
) {
    if (!is_parse_ok(state) || script == NULL) {
        return script;
    }

    mylite_sql_ast_node_append_child(script, statement);
    if (statement != NULL) {
        mylite_sql_ast_node_set_span(script, span_join(script->span, statement->span));
    }
    return script;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token table_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    return mylite_sql_parser_make_select_statement(
        state,
        table_token,
        mylite_sql_parser_make_wildcard_select_list(state, table_token),
        mylite_sql_parser_make_from_table(state, table_token, table_name, NULL, NULL),
        NULL,
        NULL,
        NULL,
        order_clause,
        limit_clause
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_explain_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token explain_token,
    struct mylite_sql_ast_node *format,
    struct mylite_sql_ast_node *analyze,
    struct mylite_sql_ast_node *statement
) {
    struct mylite_sql_source_span span = span_from_token(&explain_token);
    struct mylite_sql_ast_node *explain = NULL;

    if (format != NULL) {
        span = span_join(span, format->span);
    }
    if (analyze != NULL) {
        span = span_join(span, analyze->span);
    }
    if (statement != NULL) {
        span = span_join(span, statement->span);
    }

    explain = make_node(state, MYLITE_SQL_AST_EXPLAIN_STATEMENT, span);
    if (explain == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(explain, format);
    mylite_sql_ast_node_append_child(explain, analyze);
    mylite_sql_ast_node_append_child(explain, statement);
    return explain;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_explain_format(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token format_token,
    struct mylite_sql_ast_node *format_name
) {
    struct mylite_sql_source_span span = span_from_token(&format_token);
    struct mylite_sql_ast_node *format = NULL;

    if (format_name != NULL) {
        span = span_join(span, format_name->span);
    }
    format = make_node(state, MYLITE_SQL_AST_EXPLAIN_FORMAT, span);
    if (format == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(format, format_name);
    return format;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_explain_analyze(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token analyze_token
) {
    return make_node(state, MYLITE_SQL_AST_EXPLAIN_ANALYZE, span_from_token(&analyze_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement_with_modifiers(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_select_modifiers modifiers,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause,
    struct mylite_sql_select_locking_clause locking_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        group_clause,
        having_clause,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_modifier(statement, modifiers.duplicate_modifier);
    mylite_sql_ast_node_set_select_options(statement, modifiers.options);
    mylite_sql_ast_node_set_select_calc_found_rows(statement, modifiers.calc_found_rows);
    mylite_sql_ast_node_set_select_locking_clause(statement, locking_clause.kind);
    if (statement != NULL && locking_clause.kind != MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        mylite_sql_ast_node_set_span(statement, span_join(statement->span, locking_clause.span));
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_select_window_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *window_clause
) {
    (void)state;

    if (statement == NULL || window_clause == NULL) {
        return statement;
    }
    mylite_sql_ast_node_append_child(statement, window_clause);
    mylite_sql_ast_node_set_span(statement, span_join(statement->span, window_clause->span));
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_select_into_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *into_clause
) {
    (void)state;

    if (statement == NULL || into_clause == NULL) {
        return statement;
    }
    mylite_sql_ast_node_append_child(statement, into_clause);
    mylite_sql_ast_node_set_span(statement, span_join(statement->span, into_clause->span));
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_distinct_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        group_clause,
        having_clause,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_modifier(statement, MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_calc_found_rows_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        NULL,
        NULL,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_calc_found_rows(statement, 1);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&select_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_list != NULL) {
        span = span_join(span, select_list->span);
    }
    if (from_clause != NULL) {
        span = span_join(span, from_clause->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (group_clause != NULL) {
        span = span_join(span, group_clause->span);
    }
    if (having_clause != NULL) {
        span = span_join(span, having_clause->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, select_list);
    mylite_sql_ast_node_append_child(statement, from_clause);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, group_clause);
    mylite_sql_ast_node_append_child(statement, having_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_with_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_ast_node *union_terms,
    struct mylite_sql_ast_node *order_clause
) {
    struct mylite_sql_source_span span = span_from_token(&with_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    } else if (union_terms != NULL) {
        span = span_join(span, union_terms->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, NULL);
    mylite_sql_ast_node_append_child(statement, NULL);
    mylite_sql_ast_node_append_child(statement, NULL);
    mylite_sql_ast_node_append_child(statement, NULL);
    mylite_sql_ast_node_append_child(statement, NULL);
    mylite_sql_ast_node_append_child(statement, NULL);
    mylite_sql_ast_node_append_child(statement, NULL);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_compound_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *first_select,
    struct mylite_sql_ast_node *terms
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *statement = NULL;

    if (first_select != NULL) {
        span = first_select->span;
    }
    if (terms != NULL) {
        span = first_select == NULL ? terms->span : span_join(span, terms->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, first_select);
    mylite_sql_ast_node_append_child(statement, terms);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_query_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_parenthesis,
    struct mylite_sql_ast_node *inner_statement,
    struct mylite_sql_token right_parenthesis,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&left_parenthesis), span_from_token(&right_parenthesis));
    struct mylite_sql_ast_node *expression = NULL;

    if (inner_statement != NULL) {
        span = span_join(span, inner_statement->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_PARENTHESIZED_QUERY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, inner_statement);
    if (order_clause != NULL) {
        mylite_sql_ast_node_append_child(expression, order_clause);
    }
    if (limit_clause != NULL) {
        mylite_sql_ast_node_append_child(expression, limit_clause);
    }
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&values_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (rows != NULL) {
        span = span_join(span, rows->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_VALUES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_union_term_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *term
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *list = NULL;

    if (term != NULL) {
        span = term->span;
    }
    list = make_node(state, MYLITE_SQL_AST_UNION_TERM_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, term);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *terms,
    struct mylite_sql_ast_node *term
) {
    (void)state;

    if (terms == NULL || term == NULL) {
        return terms;
    }

    mylite_sql_ast_node_append_child(terms, term);
    mylite_sql_ast_node_set_span(terms, span_join(terms->span, term->span));
    return terms;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    enum mylite_sql_ast_union_modifier modifier,
    struct mylite_sql_ast_node *select_statement
) {
    return mylite_sql_parser_make_set_operation_term(
        state,
        union_token,
        MYLITE_SQL_AST_SET_OPERATOR_UNION,
        modifier,
        select_statement
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_operation_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_set_operator operator_kind,
    enum mylite_sql_ast_union_modifier modifier,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = span_from_token(&operator_token);
    struct mylite_sql_ast_node *term = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    }

    term = make_node(state, MYLITE_SQL_AST_UNION_TERM, span);
    if (term == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_set_operator_kind(term, operator_kind);
    mylite_sql_ast_node_set_union_modifier(term, modifier);
    mylite_sql_ast_node_append_child(term, select_statement);
    return term;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_do_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token do_token,
    struct mylite_sql_ast_node *expression_list
) {
    struct mylite_sql_source_span span = span_from_token(&do_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (expression_list != NULL) {
        span = span_join(span, expression_list->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DO_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, expression_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_do_expression_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression
) {
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_DO_EXPRESSION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, expression);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_do_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *expression
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, expression);
    if (expression != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, expression->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_use_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token use_token,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = span_from_token(&use_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_USE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_begin_immediate_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token begin_token,
    struct mylite_sql_token immediate_token
) {
    if (!token_text_equals(&immediate_token, "IMMEDIATE")) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_IDENTIFIER, immediate_token);
        return NULL;
    }

    return mylite_sql_parser_make_transaction_control_statement(
        state,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        begin_token,
        immediate_token,
        NULL
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    struct mylite_sql_ast_node *characteristics
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&first_token), span_from_token(&last_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (characteristics != NULL) {
        span = span_join(span, characteristics->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, characteristics);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_transaction_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *characteristics
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (characteristics != NULL) {
        span = span_join(span, characteristics->span);
    } else if (scope != NULL) {
        span = span_join(span, scope->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, characteristics);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *characteristic
) {
    struct mylite_sql_source_span span =
        characteristic == NULL ? (struct mylite_sql_source_span){0} : characteristic->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *characteristic
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    if (characteristic != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, characteristic->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&first_token), span_from_token(&last_token));

    return make_node(state, kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_savepoint_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *savepoint_name
) {
    struct mylite_sql_source_span span = span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (savepoint_name != NULL) {
        span = span_join(span, savepoint_name->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, savepoint_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_maintenance_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_source_span span = span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_names != NULL) {
        span = span_join(span, table_names->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_names);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token lock_token,
    struct mylite_sql_ast_node *targets
) {
    struct mylite_sql_source_span span = span_from_token(&lock_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (targets != NULL) {
        span = span_join(span, targets->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, targets);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unlock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unlock_token,
    struct mylite_sql_token table_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&unlock_token), span_from_token(&table_token));

    return make_node(state, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target
) {
    struct mylite_sql_source_span span =
        target == NULL ? (struct mylite_sql_source_span){0} : target->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, target);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *target
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, target);
    if (target != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, target->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *lock_type
) {
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *target = NULL;

    if (lock_type != NULL) {
        span = span_join(span, lock_type->span);
    } else if (alias != NULL) {
        span = span_join(span, alias->span);
    }

    target = make_node(state, MYLITE_SQL_AST_LOCK_TABLE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, table_name);
    mylite_sql_ast_node_append_child(target, alias);
    mylite_sql_ast_node_append_child(target, lock_type);
    return target;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_type(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    const struct mylite_sql_source_span span =
        span_join(span_from_token(&first_token), span_from_token(&last_token));

    return make_node(state, kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_names_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (collation_name != NULL) {
        span = span_join(span, collation_name->span);
    } else if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_NAMES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, charset_name);
    mylite_sql_ast_node_append_child(statement, collation_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_set_tail_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *assignment_list
) {
    (void)state;

    if (statement == NULL || assignment_list == NULL) {
        return statement;
    }
    mylite_sql_ast_node_append_child(statement, assignment_list);
    mylite_sql_ast_node_set_span(statement, span_join(statement->span, assignment_list->span));
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, charset_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_default_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET,
        span_from_token(&default_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *assignments
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, assignments);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_SET_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_set_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, assignment->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&operator_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_SET_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_system_variable_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_source_span span =
        scope == NULL ? (struct mylite_sql_source_span){0} : scope->span;
    struct mylite_sql_ast_node *target = NULL;

    if (name != NULL) {
        span = scope == NULL ? name->span : span_join(span, name->span);
    }

    target = make_node(state, MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, scope);
    mylite_sql_ast_node_append_child(target, name);
    return target;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
) {
    return make_node(state, MYLITE_SQL_AST_SET_DEFAULT_VALUE, span_from_token(&default_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_user_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_USER_VARIABLE, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_user_variable_assignment_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&operator_token) : target->span;
    struct mylite_sql_ast_node *expression = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, target);
    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_into_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *variable
) {
    struct mylite_sql_source_span span =
        variable == NULL ? (struct mylite_sql_source_span){0} : variable->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_SELECT_INTO_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, variable);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_select_into_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *variable
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, variable);
    if (variable != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, variable->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token prepare_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *source
) {
    struct mylite_sql_source_span span = span_from_token(&prepare_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (source != NULL) {
        span = span_join(span, source->span);
    } else if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_PREPARE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    mylite_sql_ast_node_append_child(statement, source);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_execute_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token execute_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *using_list
) {
    struct mylite_sql_source_span span = span_from_token(&execute_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (using_list != NULL) {
        span = span_join(span, using_list->span);
    } else if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_EXECUTE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    mylite_sql_ast_node_append_child(statement, using_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_execute_using_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *variable
) {
    struct mylite_sql_source_span span =
        variable == NULL ? (struct mylite_sql_source_span){0} : variable->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_EXECUTE_USING_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, variable);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_execute_using_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *variable
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, variable);
    if (variable != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, variable->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_deallocate_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_source_span span = span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&create_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *statement = NULL;

    if (create_table_name_is_no_space_function_identifier(state, table_name, &left_paren)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (table_options != NULL) {
        mylite_sql_ast_node_append_child(statement, table_options);
        mylite_sql_ast_node_set_span(statement, span_join(statement->span, table_options->span));
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        left_paren,
        columns,
        right_paren,
        table_options
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (source_table != NULL) {
        span = span_join(span, source_table->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, source_table);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_like_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        source_table
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (table_options != NULL) {
        mylite_sql_ast_node_append_child(statement, table_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_select_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        table_options,
        select_statement
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *or_replace_clause,
    struct mylite_sql_ast_node *view_options,
    struct mylite_sql_ast_node *view_name,
    struct mylite_sql_ast_node *column_names,
    struct mylite_sql_ast_node *check_option,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    } else if (view_name != NULL) {
        span = span_join(span, view_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_VIEW_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, view_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    if (or_replace_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, or_replace_clause);
    }
    if (view_options != NULL) {
        mylite_sql_ast_node_append_child(statement, view_options);
    }
    if (column_names != NULL) {
        mylite_sql_ast_node_append_child(statement, column_names);
    }
    if (check_option != NULL) {
        mylite_sql_ast_node_append_child(statement, check_option);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *view_options,
    struct mylite_sql_ast_node *view_name,
    struct mylite_sql_ast_node *column_names,
    struct mylite_sql_ast_node *check_option,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    } else if (view_name != NULL) {
        span = span_join(span, view_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_VIEW_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, view_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    if (view_options != NULL) {
        mylite_sql_ast_node_append_child(statement, view_options);
    }
    if (column_names != NULL) {
        mylite_sql_ast_node_append_child(statement, column_names);
    }
    if (check_option != NULL) {
        mylite_sql_ast_node_append_child(statement, check_option);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_or_replace_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token or_token,
    struct mylite_sql_token replace_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CREATE_OR_REPLACE_CLAUSE,
        span_join(span_from_token(&or_token), span_from_token(&replace_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_VIEW_OPTION_LIST, span);

    if (list != NULL) {
        mylite_sql_ast_node_append_child(list, option);
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_view_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    if (list == NULL) {
        return mylite_sql_parser_make_view_option_list(state, option);
    }
    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_algorithm_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token algorithm_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&algorithm_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }
    option = make_node(state, MYLITE_SQL_AST_VIEW_ALGORITHM_OPTION, span);
    if (option != NULL) {
        mylite_sql_ast_node_append_child(option, value);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_definer_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token definer_token,
    struct mylite_sql_ast_node *account
) {
    struct mylite_sql_source_span span = span_from_token(&definer_token);
    struct mylite_sql_ast_node *option = NULL;

    if (account != NULL) {
        span = span_join(span, account->span);
    }
    option = make_node(state, MYLITE_SQL_AST_VIEW_DEFINER_OPTION, span);
    if (option != NULL) {
        mylite_sql_ast_node_append_child(option, account);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_security_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token sql_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&sql_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }
    option = make_node(state, MYLITE_SQL_AST_VIEW_SECURITY_OPTION, span);
    if (option != NULL) {
        mylite_sql_ast_node_append_child(option, value);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_definer_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *user,
    struct mylite_sql_ast_node *host
) {
    struct mylite_sql_source_span span =
        user == NULL ? (struct mylite_sql_source_span){0} : user->span;
    struct mylite_sql_ast_node *account = NULL;

    if (host != NULL) {
        span = span_join(span, host->span);
    }
    account = make_node(state, MYLITE_SQL_AST_VIEW_DEFINER_ACCOUNT, span);
    if (account != NULL) {
        mylite_sql_ast_node_append_child(account, user);
        mylite_sql_ast_node_append_child(account, host);
    }
    return account;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_view_definer_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token,
    struct mylite_sql_token end_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_VIEW_DEFINER_ACCOUNT,
        span_join(span_from_token(&current_user_token), span_from_token(&end_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_check_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_token option_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_VIEW_CHECK_OPTION,
        span_join(span_from_token(&with_token), span_from_token(&option_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *procedure_name,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&create_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *statement =
        make_node(state, MYLITE_SQL_AST_CREATE_PROCEDURE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, procedure_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    bool is_unique,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_ast_node_kind statement_kind = MYLITE_SQL_AST_CREATE_INDEX_STATEMENT;

    if (index_options != NULL) {
        span = span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_type != NULL) {
        span = span_join(span, index_type->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    if (is_unique) {
        statement_kind = MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT;
    }
    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(statement, index_type);
    }
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_fulltext_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_spatial_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_TABLE_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_table_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_engine_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token engine_token,
    struct mylite_sql_ast_node *engine_name
) {
    struct mylite_sql_source_span span = span_from_token(&engine_token);
    struct mylite_sql_ast_node *option = NULL;

    if (engine_name != NULL) {
        span = span_join(span, engine_name->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_ENGINE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, engine_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_charset_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = span_from_token(&charset_token);
    struct mylite_sql_ast_node *option = NULL;

    if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_CHARSET_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, charset_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_collation_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = span_from_token(&collate_token);
    struct mylite_sql_ast_node *option = NULL;

    if (collation_name != NULL) {
        span = span_join(span, collation_name->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_COLLATION_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, collation_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_auto_increment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&auto_increment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_row_format_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_format_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&row_format_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_key_block_size_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token key_block_size_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&key_block_size_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_pack_keys_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token pack_keys_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&pack_keys_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_checksum_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token checksum_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&checksum_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_persistent_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_persistent_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&stats_persistent_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_auto_recalc_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_auto_recalc_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&stats_auto_recalc_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_sample_pages_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_sample_pages_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&stats_sample_pages_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_min_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token min_rows_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&min_rows_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_MIN_ROWS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_max_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token max_rows_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&max_rows_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_MAX_ROWS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_avg_row_length_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token avg_row_length_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&avg_row_length_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_AVG_ROW_LENGTH_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_delay_key_write_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delay_key_write_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&delay_key_write_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_DELAY_KEY_WRITE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_tablespace_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token tablespace_token,
    struct mylite_sql_ast_node *tablespace_name,
    struct mylite_sql_ast_node *storage
) {
    struct mylite_sql_source_span span = span_from_token(&tablespace_token);
    struct mylite_sql_ast_node *option = NULL;

    if (storage != NULL) {
        span = span_join(span, storage->span);
    } else if (tablespace_name != NULL) {
        span = span_join(span, tablespace_name->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_TABLESPACE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, tablespace_name);
    if (storage != NULL) {
        mylite_sql_ast_node_append_child(option, storage);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_union_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    struct mylite_sql_ast_node *table_names,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&union_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *option = make_node(state, MYLITE_SQL_AST_TABLE_UNION_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, table_names);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_insert_method_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_method_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&insert_method_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_INSERT_METHOD_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_storage_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token storage_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&storage_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_STORAGE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INDEX_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_index_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    (void)state;
    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_type_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *type_name
) {
    struct mylite_sql_source_span span = span_from_token(&using_token);
    struct mylite_sql_ast_node *option = NULL;

    if (type_name != NULL) {
        span = span_join(span, type_name->span);
    }
    option = make_node(state, MYLITE_SQL_AST_INDEX_TYPE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, type_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }
    option = make_node(state, MYLITE_SQL_AST_INDEX_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_visibility_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_ast_node *option = make_node(
        state,
        MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION,
        span_from_token(&visibility_token)
    );
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(option, visibility);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (if_not_exists_clause != NULL) {
        span = span_join(span, if_not_exists_clause->span);
    }
    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }
    if (schema_options != NULL) {
        span = span_join(span, schema_options->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (schema_options != NULL) {
        mylite_sql_ast_node_append_child(statement, schema_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_schema_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_options != NULL) {
        span = span_join(span, schema_options->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement =
        make_node(state, MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    if (schema_name != NULL) {
        mylite_sql_ast_node_append_child(statement, schema_name);
    }
    mylite_sql_ast_node_append_child(statement, schema_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_names != NULL) {
        span = span_join(span, table_names->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_names);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_drop_table_statement(
        state,
        drop_token,
        if_exists_clause,
        table_names
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *view_names
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_drop_table_statement(
        state,
        drop_token,
        if_exists_clause,
        view_names
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_DROP_VIEW_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *procedure_name
) {
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (procedure_name != NULL) {
        span = span_join(span, procedure_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_PROCEDURE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, procedure_name);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_name_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_TABLE_NAME_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_table_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *table_name
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    if (table_name != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, table_name->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (if_exists_clause != NULL) {
        span = span_join(span, if_exists_clause->span);
    }
    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_truncate_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token truncate_token,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = span_from_token(&truncate_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    int is_full,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&tables_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_show_tables_full(statement, is_full);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_variables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token variables_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&variables_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = span_join(span, filter->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&status_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = span_join(span, filter->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_table_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&status_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = span_join(span, filter->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token collation_token,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&collation_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_triggers_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token triggers_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&triggers_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_events_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token events_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&events_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_open_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&tables_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_routine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&status_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_processlist_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token processlist_token,
    enum mylite_sql_ast_node_kind statement_kind
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&processlist_token));

    return make_node(state, statement_kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&end_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_for_target_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_ast_node *role_list
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (role_list != NULL) {
        span = span_join(span, role_list->span);
    } else if (target != NULL) {
        span = span_join(span, target->span);
    }
    statement = make_node(state, MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, role_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *user,
    struct mylite_sql_ast_node *host
) {
    struct mylite_sql_source_span span =
        user == NULL ? (struct mylite_sql_source_span){0} : user->span;
    struct mylite_sql_ast_node *account = NULL;

    if (host != NULL) {
        span = span_join(span, host->span);
    }
    account = make_node(state, MYLITE_SQL_AST_SHOW_GRANTS_ACCOUNT, span);
    if (account != NULL) {
        mylite_sql_ast_node_append_child(account, user);
        mylite_sql_ast_node_append_child(account, host);
    }
    return account;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_show_grants_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token,
    struct mylite_sql_token end_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        span_join(span_from_token(&current_user_token), span_from_token(&end_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_role_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *role
) {
    struct mylite_sql_source_span span =
        role == NULL ? (struct mylite_sql_source_span){0} : role->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_SHOW_GRANTS_ROLE_LIST, span);

    if (list != NULL) {
        mylite_sql_ast_node_append_child(list, role);
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_show_grants_role(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *role
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, role);
    if (role != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, role->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token warnings_token,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&warnings_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_warnings_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.show), span_from_token(&tokens.warnings));

    if (tokens.left_paren.offset != tokens.count.offset + tokens.count.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, tokens.left_paren);
        return NULL;
    }

    return make_node(state, MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token errors_token,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&errors_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_errors_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.show), span_from_token(&tokens.errors));

    if (tokens.left_paren.offset != tokens.count.offset + tokens.count.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, tokens.left_paren);
        return NULL;
    }

    return make_node(state, MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_columns_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_full_columns_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *where_clause
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, where_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_databases_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token databases_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&databases_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = span_join(span, filter->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *view_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, view_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_VIEW_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *procedure_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, procedure_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_PROCEDURE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_database_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_call_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token call_token,
    struct mylite_sql_ast_node *procedure_name,
    struct mylite_sql_ast_node *arguments
) {
    struct mylite_sql_source_span span = span_from_token(&call_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (procedure_name != NULL) {
        span = span_join(span, procedure_name->span);
    }
    if (arguments != NULL) {
        span = span_join(span, arguments->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CALL_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, procedure_name);
    mylite_sql_ast_node_append_child(statement, arguments);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_raw_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_source_span span
) {
    return make_node(state, statement_kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_engines_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token engines_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&engines_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_engine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *engine_name,
    struct mylite_sql_token status_token
) {
    struct mylite_sql_ast_node *statement = make_node(
        state,
        MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&status_token))
    );

    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, engine_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_plugins_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token plugins_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_PLUGINS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&plugins_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_privileges_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token privileges_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&privileges_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_log_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&status_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_logs_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token logs_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&logs_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_replica_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_REPLICA_STATUS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&status_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_replicas_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token replicas_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_REPLICAS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&replicas_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token rename_token,
    struct mylite_sql_ast_node *pairs
) {
    struct mylite_sql_source_span span = span_from_token(&rename_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (pairs != NULL) {
        span = span_join(span, pairs->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_RENAME_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, pairs);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *pair
) {
    struct mylite_sql_source_span span =
        pair == NULL ? (struct mylite_sql_source_span){0} : pair->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, pair);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *pair
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, pair);
    if (pair != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, pair->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_token to_token,
    struct mylite_sql_ast_node *target_name
) {
    struct mylite_sql_source_span span =
        source_name == NULL ? span_from_token(&to_token) : source_name->span;
    struct mylite_sql_ast_node *pair = NULL;

    if (target_name != NULL) {
        span = span_join(span, target_name->span);
    }

    pair = make_node(state, MYLITE_SQL_AST_RENAME_TABLE_PAIR, span);
    if (pair == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(pair, source_name);
    mylite_sql_ast_node_append_child(pair, target_name);
    return pair;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_ast_node *target_name
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target_name != NULL) {
        span = span_join(span, target_name->span);
    } else if (source_name != NULL) {
        span = span_join(span, source_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, source_name);
    mylite_sql_ast_node_append_child(statement, target_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = span_join(span, position->span);
    } else if (column != NULL) {
        span = span_join(span, column->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column);
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
) {
    struct mylite_sql_source_span span =
        action == NULL ? (struct mylite_sql_source_span){0} : action->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, action);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *column_definitions
) {
    struct mylite_sql_ast_node *list = NULL;
    struct mylite_sql_ast_node *column = NULL;

    if (column_definitions == NULL ||
        column_definitions->kind != MYLITE_SQL_AST_COLUMN_DEFINITION_LIST) {
        return NULL;
    }

    column = parser_child_at(column_definitions, 0U);
    while (column != NULL) {
        struct mylite_sql_ast_node *next_column = column->next_sibling;
        struct mylite_sql_ast_node *action =
            mylite_sql_parser_make_alter_table_add_column_statement(
                state,
                add_token,
                NULL,
                column,
                NULL,
                mylite_sql_parser_empty_alter_table_options()
            );

        list = mylite_sql_parser_append_alter_table_action(state, list, action);
        column = next_column;
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_alter_table_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
) {
    if (list == NULL) {
        return mylite_sql_parser_make_alter_table_action_list(state, action);
    }
    if (action != NULL) {
        mylite_sql_ast_node_append_child(list, action);
        mylite_sql_ast_node_set_span(list, span_join(list->span, action->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_multi_action_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *actions,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (actions != NULL) {
        span = span_join(span, actions->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, actions);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *primary_key,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (primary_key != NULL) {
        span = span_join(span, primary_key->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, primary_key);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *secondary_index,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (secondary_index != NULL) {
        span = span_join(span, secondary_index->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, secondary_index);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (foreign_key != NULL) {
        span = span_join(span, foreign_key->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, foreign_key);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (foreign_key_name != NULL) {
        span = span_join(span, foreign_key_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, foreign_key_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_constraint_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (constraint_name != NULL) {
        span = span_join(span, constraint_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, constraint_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_name != NULL) {
        span = span_join(span, index_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, index_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_index_name,
    struct mylite_sql_ast_node *new_index_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (new_index_name != NULL) {
        span = span_join(span, new_index_name->span);
    } else if (old_index_name != NULL) {
        span = span_join(span, old_index_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_index_name);
    mylite_sql_ast_node_append_child(statement, new_index_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_index_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = span_join(span, span_from_token(&visibility_token));

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, index_name);
    apply_alter_table_options(statement, options);
    mylite_sql_ast_node_set_column_visibility(statement, visibility);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_constraint
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (check_constraint != NULL) {
        span = span_join(span, check_constraint->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_constraint);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (check_name != NULL) {
        span = span_join(span, check_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_alter_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name,
    struct mylite_sql_ast_node *enforcement
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (enforcement != NULL) {
        span = span_join(span, enforcement->span);
    } else if (check_name != NULL) {
        span = span_join(span, check_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_name);
    mylite_sql_ast_node_append_child(statement, enforcement);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token key_token,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (key_token.text != NULL) {
        span = span_join(span, span_from_token(&key_token));
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_auto_increment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *auto_increment_option
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (auto_increment_option != NULL) {
        span = span_join(span, auto_increment_option->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, auto_increment_option);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (column_name != NULL) {
        span = span_join(span, column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *new_column_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (new_column_name != NULL) {
        span = span_join(span, new_column_name->span);
    } else if (old_column_name != NULL) {
        span = span_join(span, old_column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_column_name);
    mylite_sql_ast_node_append_child(statement, new_column_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_modify_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = span_join(span, position->span);
    } else if (column != NULL) {
        span = span_join(span, column->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column);
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_change_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = span_join(span, position->span);
    } else if (column != NULL) {
        span = span_join(span, column->span);
    } else if (old_column_name != NULL) {
        span = span_join(span, old_column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_column_name);
    mylite_sql_ast_node_append_child(statement, column);
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_first(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token
) {
    return make_node(state, MYLITE_SQL_AST_COLUMN_POSITION_FIRST, span_from_token(&first_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_after(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token after_token,
    struct mylite_sql_ast_node *column_name
) {
    struct mylite_sql_source_span span = span_from_token(&after_token);
    struct mylite_sql_ast_node *position = NULL;

    if (column_name != NULL) {
        span = span_join(span, column_name->span);
    }

    position = make_node(state, MYLITE_SQL_AST_COLUMN_POSITION_AFTER, span);
    if (position == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(position, column_name);
    return position;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_set_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_ast_node *default_node
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (default_node != NULL) {
        span = span_join(span, default_node->span);
    } else if (column_name != NULL) {
        span = span_join(span, column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    mylite_sql_ast_node_append_child(statement, default_node);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token default_token
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = span_join(span, span_from_token(&default_token));

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_column_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = span_join(span, span_from_token(&visibility_token));

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(statement, visibility);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement =
        make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_convert_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_comment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *comment_option,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (comment_option != NULL) {
        span = span_join(span, comment_option->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, comment_option);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_storage_statistics_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_STORAGE_STATISTICS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_order_by_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_items
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (order_items != NULL) {
        span = span_join(span, order_items->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, order_items);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_force_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_disable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_enable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_alter_table_options mylite_sql_parser_empty_alter_table_options(void) {
    return (struct mylite_sql_alter_table_options){
        .algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED,
        .lock = MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED,
        .span = {0},
        .has_span = 0,
    };
}

struct mylite_sql_alter_algorithm_value mylite_sql_parser_make_alter_algorithm_value(
    struct mylite_sql_token token
) {
    enum mylite_sql_ast_alter_algorithm kind = MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN;

    if (token_text_equals(&token, "DEFAULT")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_DEFAULT;
    } else if (token_text_equals(&token, "INSTANT")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT;
    } else if (token_text_equals(&token, "INPLACE")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE;
    } else if (token_text_equals(&token, "COPY")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_COPY;
    }

    return (struct mylite_sql_alter_algorithm_value){
        .kind = kind,
        .span = span_from_token(&token),
    };
}

struct mylite_sql_alter_lock_value mylite_sql_parser_make_alter_lock_value(
    struct mylite_sql_token token
) {
    enum mylite_sql_ast_alter_lock kind = MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;

    if (token_text_equals(&token, "DEFAULT")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_DEFAULT;
    } else if (token_text_equals(&token, "NONE")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_NONE;
    } else if (token_text_equals(&token, "SHARED")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_SHARED;
    } else if (token_text_equals(&token, "EXCLUSIVE")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE;
    }

    return (struct mylite_sql_alter_lock_value){
        .kind = kind,
        .span = span_from_token(&token),
    };
}

struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_algorithm_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_algorithm_value value
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();

    options.algorithm = value.kind;
    options.span = span_join(span_from_token(&option_token), value.span);
    options.has_span = 1;
    return options;
}

struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_lock_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_lock_value value
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();

    options.lock = value.kind;
    options.span = span_join(span_from_token(&option_token), value.span);
    options.has_span = 1;
    return options;
}

struct mylite_sql_alter_table_options mylite_sql_parser_append_alter_table_option(
    struct mylite_sql_alter_table_options list,
    struct mylite_sql_alter_table_options option
) {
    if (option.algorithm != MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED) {
        if (list.algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN ||
            option.algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN) {
            list.algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN;
        } else {
            list.algorithm = option.algorithm;
        }
    }
    if (option.lock != MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED) {
        if (list.lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN ||
            option.lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN) {
            list.lock = MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;
        } else {
            list.lock = option.lock;
        }
    }
    if (!list.has_span) {
        list.span = option.span;
        list.has_span = option.has_span;
    } else if (option.has_span) {
        list.span = span_join(list.span, option.span);
    }
    return list;
}

static void apply_alter_table_options(
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_alter_table_options options
) {
    if (statement == NULL) {
        return;
    }

    mylite_sql_ast_node_set_alter_table_options(statement, options.algorithm, options.lock);
    if (options.has_span) {
        mylite_sql_ast_node_set_span(statement, span_join(statement->span, options.span));
    }
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = span_join(span, duplicate_update->span);
    } else if (rows != NULL) {
        span = span_join(span, rows->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = span_join(span, duplicate_update->span);
    } else if (select != NULL) {
        span = span_join(span, select->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, select);
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_infile_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token load_token,
    struct mylite_sql_ast_node *file_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *ignore_lines,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *local_modifier
) {
    struct mylite_sql_source_span span = span_from_token(&load_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (ignore_lines != NULL) {
        span = span_join(span, ignore_lines->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (file_name != NULL) {
        span = span_join(span, file_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, file_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, ignore_lines);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, local_modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_local_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_high_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_ignore_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_ignore_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select != NULL) {
        span = span_join(span, select->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, select);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (rows != NULL) {
        span = span_join(span, rows->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = span_join(span, duplicate_update->span);
    } else if (assignments != NULL) {
        span = span_join(span, assignments->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, assignment->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&equals_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_INSERT_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_update_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *assignments
) {
    struct mylite_sql_source_span span = span_from_token(&on_token);
    struct mylite_sql_ast_node *clause = NULL;

    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }

    clause = make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, assignments);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_duplicate_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, assignment->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&equals_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_values_reference(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_token close_token
) {
    struct mylite_sql_source_span span = span_from_token(&values_token);
    struct mylite_sql_ast_node *reference = NULL;

    if (column != NULL) {
        span = span_join(span, column->span);
    }
    span = span_join(span, span_from_token(&close_token));

    reference = make_node(state, MYLITE_SQL_AST_INSERT_VALUES_REFERENCE, span);
    if (reference == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(reference, column);
    return reference;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&delete_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DELETE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_joined_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *where_clause
) {
    struct mylite_sql_source_span span = span_from_token(&delete_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL) {
        span = span_join(span, target->span);
    }
    if (from_join != NULL) {
        span = span_join(span, from_join->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_JOINED_DELETE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, from_join);
    mylite_sql_ast_node_append_child(statement, where_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_update_statement_parts parts
) {
    struct mylite_sql_source_span span = span_from_token(&update_token);
    struct mylite_sql_ast_node *target = parts.target_table;
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL && target->kind == MYLITE_SQL_AST_FROM_TABLE &&
        target->first_child != NULL && target->first_child->next_sibling == NULL) {
        target = target->first_child;
    }

    if (target != NULL) {
        span = span_join(span, target->span);
    }
    if (parts.assignment_list != NULL) {
        span = span_join(span, parts.assignment_list->span);
    }
    if (parts.where_clause != NULL) {
        span = span_join(span, parts.where_clause->span);
    }
    if (parts.order_clause != NULL) {
        span = span_join(span, parts.order_clause->span);
    }
    if (parts.limit_clause != NULL) {
        span = span_join(span, parts.limit_clause->span);
    }
    if (parts.low_priority_modifier != NULL) {
        span = span_join(span, parts.low_priority_modifier->span);
    }
    if (parts.ignore_modifier != NULL) {
        span = span_join(span, parts.ignore_modifier->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, parts.assignment_list);
    mylite_sql_ast_node_append_child(statement, parts.where_clause);
    mylite_sql_ast_node_append_child(statement, parts.order_clause);
    mylite_sql_ast_node_append_child(statement, parts.limit_clause);
    mylite_sql_ast_node_append_child(statement, parts.low_priority_modifier);
    mylite_sql_ast_node_append_child(statement, parts.ignore_modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_joined_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&update_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (from_join != NULL) {
        span = span_join(span, from_join->span);
    }
    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, from_join);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_update_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, assignment->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&equals_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_UPDATE_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token wildcard_token
) {
    return mylite_sql_parser_make_select_list(
        state,
        mylite_sql_parser_make_select_item(
            state,
            mylite_sql_parser_make_wildcard(state, wildcard_token),
            NULL
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
) {
    struct mylite_sql_source_span span =
        item == NULL ? (struct mylite_sql_source_span){0} : item->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_SELECT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *item
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, item);
    if (item != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, item->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_ast_node *alias
) {
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *item = make_node(state, MYLITE_SQL_AST_SELECT_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    if (alias != NULL) {
        mylite_sql_ast_node_set_span(item, span_join(span, alias->span));
    }
    mylite_sql_ast_node_append_child(item, expression);
    mylite_sql_ast_node_append_child(item, alias);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_token dual_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_FROM_DUAL,
        span_join(span_from_token(&from_token), span_from_token(&dual_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_table(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
) {
    struct mylite_sql_source_span span = span_from_token(&from_token);
    struct mylite_sql_ast_node *from_table = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (alias != NULL) {
        span = span_join(span, alias->span);
    }
    if (index_hints != NULL) {
        span = span_join(span, index_hints->span);
    }

    from_table = make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(from_table, alias);
    }
    if (index_hints != NULL) {
        mylite_sql_ast_node_append_child(from_table, index_hints);
    }
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
) {
    struct mylite_sql_source_span span =
        table_name != NULL ? table_name->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *from_table = NULL;

    if (alias != NULL) {
        span = span_join(span, alias->span);
    }
    if (index_hints != NULL) {
        span = span_join(span, index_hints->span);
    }

    from_table = make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(from_table, alias);
    }
    if (index_hints != NULL) {
        mylite_sql_ast_node_append_child(from_table, index_hints);
    }
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_derived_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_parenthesis,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_parenthesis,
    struct mylite_sql_ast_node *alias
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&left_parenthesis), span_from_token(&right_parenthesis));
    struct mylite_sql_ast_node *derived = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    }
    if (alias != NULL) {
        span = span_join(span, alias->span);
    }

    derived = make_node(state, MYLITE_SQL_AST_FROM_DERIVED, span);
    if (derived == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(derived, select_statement);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(derived, alias);
    }
    return derived;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_join(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
) {
    struct mylite_sql_source_span span = span_from_token(&from_token);
    struct mylite_sql_ast_node *join = NULL;

    if (left != NULL) {
        span = span_join(span, left->span);
    }
    if (right != NULL) {
        span = span_join(span, right->span);
    }
    if (condition != NULL) {
        span = span_join(span, condition->span);
    }

    join = make_node(state, MYLITE_SQL_AST_FROM_JOIN, span);
    if (join == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_join_kind(join, join_kind);
    mylite_sql_ast_node_append_child(join, left);
    mylite_sql_ast_node_append_child(join, right);
    mylite_sql_ast_node_append_child(join, condition);
    return join;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_join_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
) {
    struct mylite_sql_source_span span =
        left != NULL ? left->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *join = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }
    if (condition != NULL) {
        span = span_join(span, condition->span);
    }

    join = make_node(state, MYLITE_SQL_AST_FROM_JOIN, span);
    if (join == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_join_kind(join, join_kind);
    mylite_sql_ast_node_append_child(join, left);
    mylite_sql_ast_node_append_child(join, right);
    mylite_sql_ast_node_append_child(join, condition);
    return join;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_join_using_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_parenthesis
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&using_token), span_from_token(&right_parenthesis));
    struct mylite_sql_ast_node *clause = NULL;

    if (columns != NULL) {
        span = span_join(span, columns->span);
    }

    clause = make_node(state, MYLITE_SQL_AST_JOIN_USING_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, columns);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *hint
) {
    struct mylite_sql_source_span span =
        hint != NULL ? hint->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INDEX_HINT_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, hint);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_index_hint(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *hint
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, hint);
    if (hint != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, hint->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *names,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *hint = make_node(
        state,
        kind,
        span_join(span_from_token(&start_token), span_from_token(&right_paren))
    );

    if (hint == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(hint, scope);
    mylite_sql_ast_node_append_child(hint, names);
    return hint;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_scope(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token for_token,
    struct mylite_sql_token last_token
) {
    return make_node(
        state,
        kind,
        span_join(span_from_token(&for_token), span_from_token(&last_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_where_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token where_token,
    struct mylite_sql_ast_node *predicate
) {
    struct mylite_sql_source_span span = span_from_token(&where_token);
    struct mylite_sql_ast_node *where_clause = NULL;

    if (predicate != NULL) {
        span = span_join(span, predicate->span);
    }

    where_clause = make_node(state, MYLITE_SQL_AST_WHERE_CLAUSE, span);
    if (where_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(where_clause, predicate);
    return where_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_key_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *group_key
) {
    struct mylite_sql_source_span span =
        group_key == NULL ? (struct mylite_sql_source_span){0} : group_key->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_GROUP_BY_ITEM_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, group_key);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_group_by_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *group_key
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, group_key);
    if (group_key != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, group_key->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token group_token,
    struct mylite_sql_ast_node *group_keys
) {
    struct mylite_sql_source_span span = span_from_token(&group_token);
    struct mylite_sql_ast_node *group_clause = NULL;

    if (group_keys != NULL) {
        span = span_join(span, group_keys->span);
    }

    group_clause = make_node(state, MYLITE_SQL_AST_GROUP_BY_CLAUSE, span);
    if (group_clause == NULL) {
        return NULL;
    }

    if (group_keys != NULL && group_keys->kind == MYLITE_SQL_AST_GROUP_BY_ITEM_LIST) {
        struct mylite_sql_ast_node *group_key = group_keys->first_child;

        while (group_key != NULL) {
            struct mylite_sql_ast_node *next = group_key->next_sibling;
            mylite_sql_ast_node_append_child(group_clause, group_key);
            group_key = next;
        }
    } else {
        mylite_sql_ast_node_append_child(group_clause, group_keys);
    }

    return group_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_rollup_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_token rollup_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_GROUP_BY_ROLLUP_MODIFIER,
        span_join(span_from_token(&with_token), span_from_token(&rollup_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_having_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token having_token,
    struct mylite_sql_ast_node *predicate
) {
    struct mylite_sql_source_span span = span_from_token(&having_token);
    struct mylite_sql_ast_node *having_clause = NULL;

    if (predicate != NULL) {
        span = span_join(span, predicate->span);
    }

    having_clause = make_node(state, MYLITE_SQL_AST_HAVING_CLAUSE, span);
    if (having_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(having_clause, predicate);
    return having_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_comparison_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_COMPARISON_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_like_comparison_predicate(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_parser_like_comparison_predicate_request *request
) {
    struct mylite_sql_ast_node *predicate = NULL;

    if (request == NULL) {
        return NULL;
    }

    predicate = mylite_sql_parser_make_comparison_predicate(
        state,
        request->left,
        request->operator_token,
        request->operator_kind,
        request->right
    );

    if (predicate == NULL || request->escape == NULL) {
        return predicate;
    }

    predicate->span = span_join(predicate->span, request->escape->span);
    mylite_sql_ast_node_append_child(predicate, request->escape);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_is_null_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token null_token
) {
    struct mylite_sql_source_span span = left == NULL ? span_from_token(&is_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&null_token));
    predicate = make_node(state, MYLITE_SQL_AST_IS_NULL_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_is_boolean_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token truth_token
) {
    struct mylite_sql_source_span span = left == NULL ? span_from_token(&is_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&truth_token));
    predicate = make_node(state, MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_between_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token between_token,
    struct mylite_sql_ast_node *lower,
    struct mylite_sql_ast_node *upper
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&between_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (upper != NULL) {
        span = span_join(span, upper->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_BETWEEN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, lower);
    mylite_sql_ast_node_append_child(predicate, upper);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_in_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token in_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = left == NULL ? span_from_token(&in_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&right_paren));
    predicate = make_node(state, MYLITE_SQL_AST_IN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, values);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_exists_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token exists_token,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = span_from_token(&exists_token);
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&right_paren));
    predicate = make_node(state, MYLITE_SQL_AST_EXISTS_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, select_statement);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_predicate_value_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_PREDICATE_VALUE_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, value);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_predicate_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *value
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, value->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_and_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_AND_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_or_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_OR_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_xor_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_XOR_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_not_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *child
) {
    struct mylite_sql_source_span span = span_from_token(&operator_token);
    struct mylite_sql_ast_node *predicate = NULL;

    if (child != NULL) {
        span = span_join(span, child->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_NOT_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT);
    mylite_sql_ast_node_append_child(predicate, child);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span = span_from_token(&order_token);
    struct mylite_sql_ast_node *order_clause = NULL;

    if (direction != NULL) {
        span = span_join(span, direction->span);
    } else if (order_key != NULL) {
        span = span_join(span, order_key->span);
    }

    order_clause = make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(order_clause, order_key);
    mylite_sql_ast_node_append_child(order_clause, direction);
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause_from_item_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *item_list
) {
    struct mylite_sql_source_span span = span_from_token(&order_token);
    struct mylite_sql_ast_node *order_clause = NULL;
    struct mylite_sql_ast_node *order_child = item_list;

    if (item_list != NULL) {
        span = span_join(span, item_list->span);
    }
    if (item_list != NULL && item_list->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST &&
        mylite_sql_ast_node_child_count(item_list) == 1U) {
        order_child = parser_child_at(item_list, 0U);
    }
    order_clause = make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }
    if (order_child != NULL && order_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM) {
        struct mylite_sql_ast_node *key = parser_child_at(order_child, 0U);
        struct mylite_sql_ast_node *direction = parser_child_at(order_child, 1U);

        mylite_sql_ast_node_append_child(order_clause, key);
        mylite_sql_ast_node_append_child(order_clause, direction);
    } else {
        mylite_sql_ast_node_append_child(order_clause, order_child);
    }
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_parser_select_order_by_parts parts
) {
    struct mylite_sql_source_span span = span_from_token(&order_token);
    struct mylite_sql_ast_node *first_item = NULL;
    struct mylite_sql_ast_node *list = NULL;
    struct mylite_sql_ast_node *order_clause = NULL;
    struct mylite_sql_ast_node *item = NULL;

    if (parts.tail_items == NULL) {
        return mylite_sql_parser_make_order_by_clause(
            state,
            order_token,
            parts.first_order_key,
            parts.first_direction
        );
    }

    first_item =
        mylite_sql_parser_make_order_by_item(state, parts.first_order_key, parts.first_direction);
    list = mylite_sql_parser_make_order_by_item_list(state, first_item);
    if (list == NULL) {
        return NULL;
    }

    item = parts.tail_items->first_child;
    while (item != NULL) {
        struct mylite_sql_ast_node *next = item->next_sibling;
        mylite_sql_parser_append_order_by_item(state, list, item);
        item = next;
    }

    span = span_join(span, list->span);
    order_clause = make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(order_clause, list);
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
) {
    struct mylite_sql_source_span span =
        item == NULL ? (struct mylite_sql_source_span){0} : item->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_ORDER_BY_ITEM_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_order_by_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *item
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, item);
    if (item != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, item->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        order_key == NULL ? (struct mylite_sql_source_span){0} : order_key->span;
    struct mylite_sql_ast_node *item = NULL;

    if (direction != NULL) {
        span = span_join(span, direction->span);
    }

    item = make_node(state, MYLITE_SQL_AST_ORDER_BY_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(item, order_key);
    mylite_sql_ast_node_append_child(item, direction);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_direction(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token direction_token,
    enum mylite_sql_ast_order_direction direction
) {
    struct mylite_sql_ast_node *direction_node =
        make_node(state, MYLITE_SQL_AST_ORDER_DIRECTION, span_from_token(&direction_token));
    if (direction_node == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_order_direction(direction_node, direction);
    return direction_node;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_limit_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token limit_token,
    struct mylite_sql_ast_node *row_count,
    struct mylite_sql_ast_node *offset
) {
    struct mylite_sql_source_span span = span_from_token(&limit_token);
    struct mylite_sql_ast_node *limit_clause = NULL;

    if (offset != NULL) {
        span = span_join(span, offset->span);
    }
    if (row_count != NULL) {
        span = span_join(span, row_count->span);
    }

    limit_clause = make_node(state, MYLITE_SQL_AST_LIMIT_CLAUSE, span);
    if (limit_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(limit_clause, row_count);
    mylite_sql_ast_node_append_child(limit_clause, offset);
    return limit_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_IDENTIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_ignore_space_sensitive_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    if (parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_IDENTIFIER, token);
        return NULL;
    }
    return mylite_sql_parser_make_identifier(state, token);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? (struct mylite_sql_source_span){0} : left->span;
    struct mylite_sql_ast_node *identifier = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    identifier = make_node(state, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, span);
    if (identifier == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(identifier, left);
    mylite_sql_ast_node_append_child(identifier, right);
    return identifier;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *qualifier,
    struct mylite_sql_token token
) {
    struct mylite_sql_ast_node *wildcard = NULL;
    struct mylite_sql_source_span span =
        qualifier == NULL ? span_from_token(&token) : qualifier->span;
    struct mylite_sql_ast_node *qualified = NULL;

    wildcard = mylite_sql_parser_make_wildcard(state, token);
    if (wildcard != NULL) {
        span = span_join(span, wildcard->span);
    }

    qualified = make_node(state, MYLITE_SQL_AST_QUALIFIED_WILDCARD, span);
    if (qualified == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(qualified, qualifier);
    mylite_sql_ast_node_append_child(qualified, wildcard);
    return qualified;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_WILDCARD, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_literal(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_literal_kind literal_kind
) {
    struct mylite_sql_ast_node *literal =
        make_node(state, MYLITE_SQL_AST_LITERAL, span_from_token(&token));
    if (literal == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_literal_kind(literal, literal_kind);
    return literal;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_dml_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_DML_DEFAULT_VALUE, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_system_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_SYSTEM_VARIABLE, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *operand
) {
    struct mylite_sql_source_span span = span_from_token(&operator_token);
    struct mylite_sql_ast_node *expression = NULL;

    if (operand != NULL) {
        span = span_join(span, operand->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_UNARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(expression, operator_kind);
    mylite_sql_ast_node_append_child(expression, operand);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *expression = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_BINARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(expression, operator_kind);
    mylite_sql_ast_node_append_child(expression, left);
    mylite_sql_ast_node_append_child(expression, right);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_cast_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token cast_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&cast_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CAST_BINARY_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unary_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token binary_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&binary_token);
    struct mylite_sql_ast_node *expression = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_CAST_BINARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&convert_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_binary_type_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&convert_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_charset_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *charset,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&convert_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    mylite_sql_ast_node_append_child(expression, charset);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_collate_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation
) {
    struct mylite_sql_source_span span = span_from_token(&collate_token);
    struct mylite_sql_ast_node *expression = NULL;

    if (value != NULL) {
        span = span_join(value->span, span);
    }
    if (collation != NULL) {
        span = span_join(span, collation->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_COLLATE_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    mylite_sql_ast_node_append_child(expression, collation);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *parenthesized = make_node(
        state,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
    if (parenthesized == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(parenthesized, expression);
    return parenthesized;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_scalar_subquery_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *subquery = make_node(
        state,
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
    if (subquery == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(subquery, select_statement);
    return subquery;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_searched_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_ast_node *case_expression = make_node(
        state,
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        span_join(span_from_token(&case_token), span_from_token(&end_token))
    );
    if (case_expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(case_expression, when_list);
    mylite_sql_ast_node_append_child(case_expression, else_clause);
    return case_expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_simple_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *case_value,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_ast_node *case_expression = make_node(
        state,
        MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION,
        span_join(span_from_token(&case_token), span_from_token(&end_token))
    );
    if (case_expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(case_expression, case_value);
    mylite_sql_ast_node_append_child(case_expression, when_list);
    mylite_sql_ast_node_append_child(case_expression, else_clause);
    return case_expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *when_clause
) {
    struct mylite_sql_source_span span =
        when_clause == NULL ? (struct mylite_sql_source_span){0} : when_clause->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_CASE_WHEN_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, when_clause);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_case_when(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *when_clause
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, when_clause);
    if (when_clause != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, when_clause->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token when_token,
    struct mylite_sql_ast_node *condition,
    struct mylite_sql_ast_node *result
) {
    struct mylite_sql_source_span span = span_from_token(&when_token);
    struct mylite_sql_ast_node *when_clause = NULL;

    if (result != NULL) {
        span = span_join(span, result->span);
    } else if (condition != NULL) {
        span = span_join(span, condition->span);
    }

    when_clause = make_node(state, MYLITE_SQL_AST_CASE_WHEN_CLAUSE, span);
    if (when_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(when_clause, condition);
    mylite_sql_ast_node_append_child(when_clause, result);
    return when_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_else_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token else_token,
    struct mylite_sql_ast_node *result
) {
    struct mylite_sql_source_span span = span_from_token(&else_token);
    struct mylite_sql_ast_node *else_clause = NULL;

    if (result != NULL) {
        span = span_join(span, result->span);
    }

    else_clause = make_node(state, MYLITE_SQL_AST_CASE_ELSE_CLAUSE, span);
    if (else_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(else_clause, result);
    return else_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
) {
    return make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_zero_argument_function(
        state,
        function_token,
        function_kind,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_one_argument_function(
        state,
        function_token,
        function_kind,
        argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_concat_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *separator,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    function = make_node(
        state,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, value);
    mylite_sql_ast_node_append_child(function, order_clause);
    mylite_sql_ast_node_append_child(function, separator);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_function_window_clause(
    struct mylite_sql_ast_node *function,
    struct mylite_sql_ast_node *window_clause
) {
    if (function == NULL || window_clause == NULL) {
        return function;
    }
    mylite_sql_ast_node_append_child(function, window_clause);
    mylite_sql_ast_node_set_span(function, span_join(function->span, window_clause->span));
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_row_number_window_function_with_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *window_clause
) {
    return mylite_sql_parser_make_window_function_with_clause(
        state,
        function_token,
        MYLITE_SQL_AST_ROW_NUMBER_FUNCTION,
        (struct mylite_sql_window_function_arguments){0},
        window_clause
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_function_with_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_window_function_arguments arguments,
    struct mylite_sql_ast_node *window_clause
) {
    return mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        function_token,
        function_kind,
        arguments,
        NULL,
        window_clause
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_window_function_arguments arguments,
    struct mylite_sql_ast_node *null_treatment,
    struct mylite_sql_ast_node *window_clause
) {
    struct mylite_sql_ast_node *argument_list = NULL;
    struct mylite_sql_source_span span = span_from_token(&function_token);
    struct mylite_sql_ast_node *function = NULL;

    if (arguments.count > sizeof(arguments.items) / sizeof(arguments.items[0])) {
        return NULL;
    }
    if (arguments.count != 0U) {
        argument_list = mylite_sql_parser_make_function_argument_list(state, arguments.items[0]);
    }
    for (size_t argument_index = 1U; argument_list != NULL && argument_index < arguments.count;
         ++argument_index) {
        argument_list = mylite_sql_parser_append_function_argument(
            state,
            argument_list,
            arguments.items[argument_index]
        );
    }
    if (arguments.count != 0U && argument_list == NULL) {
        return NULL;
    }
    if (null_treatment != NULL) {
        span = span_join(span, null_treatment->span);
    }
    if (window_clause != NULL) {
        span = span_join(span, window_clause->span);
    }

    function = make_node(state, function_kind, span);
    if (function == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(function, argument_list);
    mylite_sql_ast_node_append_child(function, window_clause);
    if (null_treatment != NULL) {
        mylite_sql_ast_node_append_child(function, null_treatment);
    }
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_null_treatment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token treatment_token,
    enum mylite_sql_ast_node_kind treatment_kind,
    struct mylite_sql_token nulls_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&treatment_token), span_from_token(&nulls_token));

    return make_node(state, treatment_kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_empty_window_spec(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token over_token,
    struct mylite_sql_token right_paren
) {
    return make_node(
        state,
        MYLITE_SQL_AST_WINDOW_SPEC,
        span_join(span_from_token(&over_token), span_from_token(&right_paren))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_spec(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *window_reference,
    struct mylite_sql_ast_node *partition_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *frame_clause
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *spec = NULL;

    if (window_reference != NULL) {
        span = window_reference->span;
    }
    if (partition_clause != NULL) {
        span = window_reference == NULL ? partition_clause->span
                                        : span_join(span, partition_clause->span);
    }
    if (order_clause != NULL) {
        span = window_reference == NULL && partition_clause == NULL
                   ? order_clause->span
                   : span_join(span, order_clause->span);
    }
    if (frame_clause != NULL) {
        span = window_reference == NULL && partition_clause == NULL && order_clause == NULL
                   ? frame_clause->span
                   : span_join(span, frame_clause->span);
    }

    spec = make_node(state, MYLITE_SQL_AST_WINDOW_SPEC, span);
    if (spec == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(spec, window_reference);
    mylite_sql_ast_node_append_child(spec, partition_clause);
    mylite_sql_ast_node_append_child(spec, order_clause);
    mylite_sql_ast_node_append_child(spec, frame_clause);
    return spec;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_partition_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token partition_token,
    struct mylite_sql_ast_node *key_list
) {
    struct mylite_sql_source_span span = span_from_token(&partition_token);
    struct mylite_sql_ast_node *clause = NULL;
    struct mylite_sql_ast_node *key = key_list;

    if (key_list != NULL) {
        span = span_join(span, key_list->span);
    }
    if (key_list != NULL && key_list->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST &&
        mylite_sql_ast_node_child_count(key_list) == 1U) {
        key = parser_child_at(key_list, 0U);
    }
    clause = make_node(state, MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(clause, key);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_order_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *order_list
) {
    struct mylite_sql_source_span span = span_from_token(&order_token);
    struct mylite_sql_ast_node *clause = NULL;
    struct mylite_sql_ast_node *order_child = order_list;

    if (order_list != NULL) {
        span = span_join(span, order_list->span);
    }
    if (order_list != NULL && order_list->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST &&
        mylite_sql_ast_node_child_count(order_list) == 1U) {
        order_child = parser_child_at(order_list, 0U);
    }
    clause = make_node(state, MYLITE_SQL_AST_WINDOW_ORDER_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }
    if (order_child != NULL && order_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM) {
        struct mylite_sql_ast_node *key = parser_child_at(order_child, 0U);
        struct mylite_sql_ast_node *direction = parser_child_at(order_child, 1U);

        mylite_sql_ast_node_append_child(clause, key);
        mylite_sql_ast_node_append_child(clause, direction);
    } else {
        mylite_sql_ast_node_append_child(clause, order_child);
    }
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_reference(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_ast_node *reference = NULL;

    if (name == NULL) {
        return NULL;
    }
    reference = make_node(state, MYLITE_SQL_AST_WINDOW_REFERENCE, name->span);
    if (reference == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(reference, name);
    return reference;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_definition_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *definition
) {
    struct mylite_sql_ast_node *list = NULL;

    if (definition == NULL) {
        return NULL;
    }
    list = make_node(state, MYLITE_SQL_AST_WINDOW_DEFINITION_LIST, definition->span);
    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, definition);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_window_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *definition
) {
    (void)state;

    if (list == NULL || definition == NULL) {
        return list;
    }
    mylite_sql_ast_node_append_child(list, definition);
    mylite_sql_ast_node_set_span(list, span_join(list->span, definition->span));
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *spec
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *definition = NULL;

    if (name != NULL) {
        span = name->span;
    }
    if (spec != NULL) {
        span = name == NULL ? spec->span : span_join(span, spec->span);
    }
    definition = make_node(state, MYLITE_SQL_AST_WINDOW_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(definition, name);
    mylite_sql_ast_node_append_child(definition, spec);
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_frame_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token frame_token,
    struct mylite_sql_ast_node *first_bound,
    struct mylite_sql_ast_node *second_bound
) {
    struct mylite_sql_source_span span = span_from_token(&frame_token);
    struct mylite_sql_ast_node *clause = NULL;

    if (first_bound != NULL) {
        span = span_join(span, first_bound->span);
    }
    if (second_bound != NULL) {
        span = span_join(span, second_bound->span);
    }
    clause = make_node(state, MYLITE_SQL_AST_WINDOW_FRAME_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(clause, first_bound);
    mylite_sql_ast_node_append_child(clause, second_bound);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_frame_bound(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token bound_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&bound_token);
    struct mylite_sql_ast_node *bound = NULL;

    if (value != NULL) {
        span = span_join(value->span, span);
    }
    bound = make_node(state, MYLITE_SQL_AST_WINDOW_FRAME_BOUND, span);
    if (bound == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(bound, value);
    return bound;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_two_argument_function(
        state,
        function_token,
        function_kind,
        first_argument,
        second_argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_three_argument_function(
        state,
        function_token,
        function_kind,
        first_argument,
        second_argument,
        third_argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_trim_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *remove_string,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, value);
    mylite_sql_ast_node_append_child(function, remove_string);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_count_error(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind error_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *error = NULL;

    error = make_node(
        state,
        error_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (error == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(error, arguments);
    return error;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_function_argument_count_error(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind error_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_function_argument_count_error(
        state,
        function_token,
        error_kind,
        arguments,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    mylite_sql_ast_node_append_child(function, third_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_four_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_ast_node *fourth_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    mylite_sql_ast_node_append_child(function, third_argument);
    mylite_sql_ast_node_append_child(function, fourth_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_list_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, arguments);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generic_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;
    struct mylite_sql_ast_node *name = NULL;

    name = mylite_sql_parser_make_identifier(state, function_token);
    if (name == NULL) {
        return NULL;
    }

    function = make_node(
        state,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, name);
    mylite_sql_ast_node_append_child(function, arguments);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generic_function_with_window_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *window_clause
) {
    struct mylite_sql_ast_node *function = NULL;

    if (window_clause != NULL &&
        !token_text_is_generic_aggregate_window_function_name(&function_token)) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return NULL;
    }

    if (token_text_equals(&function_token, "ROW") ||
        (function_token.flags & MYLITE_SQL_TOKEN_SYNTHETIC_ROW_CONSTRUCTOR) != 0U) {
        return mylite_sql_parser_make_row_constructor(
            state,
            function_token,
            arguments,
            right_paren
        );
    }

    function =
        mylite_sql_parser_make_generic_function(state, function_token, arguments, right_paren);
    return mylite_sql_parser_attach_function_window_clause(function, window_clause);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_row_constructor(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *constructor = NULL;
    struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;

    for (argument = arguments == NULL ? NULL : arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        ++argument_count;
    }
    if (argument_count < 2U) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return NULL;
    }

    constructor = make_node(
        state,
        MYLITE_SQL_AST_ROW_CONSTRUCTOR,
        span_join(span_from_token(&start_token), span_from_token(&right_paren))
    );
    if (constructor == NULL) {
        return NULL;
    }

    argument = arguments == NULL ? NULL : arguments->first_child;
    while (argument != NULL) {
        struct mylite_sql_ast_node *next = argument->next_sibling;

        mylite_sql_ast_node_append_child(constructor, argument);
        argument = next;
    }
    return constructor;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument
) {
    struct mylite_sql_source_span span =
        argument == NULL ? (struct mylite_sql_source_span){0} : argument->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, argument);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_prepend_function_argument(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_ast_node *list
) {
    if (!is_parse_ok(state)) {
        return list;
    }
    if (list == NULL) {
        return mylite_sql_parser_make_function_argument_list(state, argument);
    }
    if (argument == NULL) {
        return list;
    }

    argument->next_sibling = list->first_child;
    list->first_child = argument;
    if (list->last_child == NULL) {
        list->last_child = argument;
    }
    mylite_sql_ast_node_set_span(list, span_join(argument->span, list->span));
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_function_argument(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *argument
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, argument);
    if (argument != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, argument->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        span_from_token(&current_user_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_timestamp_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE,
        span_from_token(&current_timestamp_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_date_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        span_from_token(&current_date_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_time_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        span_from_token(&current_time_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_temporal_value_with_precision(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_temporal_fractional_precision_tokens precision
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&precision.end_token))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        function,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = precision.has_precision,
            .precision_span = span_from_token(&precision.precision_token),
        }
    );
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_temporal_value_with_precision(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_temporal_fractional_precision_tokens precision
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_temporal_value_with_precision(
        state,
        function_token,
        function_kind,
        precision
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_date_token
) {
    return make_node(state, MYLITE_SQL_AST_UTC_DATE_VALUE, span_from_token(&utc_date_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_time_token
) {
    return make_node(state, MYLITE_SQL_AST_UTC_TIME_VALUE, span_from_token(&utc_time_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_timestamp_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        span_from_token(&utc_timestamp_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column
) {
    struct mylite_sql_source_span span =
        column == NULL ? (struct mylite_sql_source_span){0} : column->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, column);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *column
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, column);
    if (column != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, column->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): grammar order.
    struct mylite_sql_ast_node *index_name,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): grammar order.
    struct mylite_sql_ast_node *index_type,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): grammar order.
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&primary_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *primary_key =
        make_node(state, MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION, span);
    if (primary_key == NULL) {
        return NULL;
    }

    (void)index_name;
    mylite_sql_ast_node_append_child(primary_key, key_parts);
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(primary_key, index_type);
    }
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(primary_key, index_options);
        mylite_sql_ast_node_set_span(
            primary_key,
            span_join(primary_key->span, index_options->span)
        );
    }
    return primary_key;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_primary_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token index_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&index_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *secondary_index =
        make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION, span);
    if (secondary_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_name);
    }
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_type);
    }
    mylite_sql_ast_node_append_child(secondary_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_options);
        mylite_sql_ast_node_set_span(
            secondary_index,
            span_join(secondary_index->span, index_options->span)
        );
    }
    return secondary_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unique_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&unique_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *unique_index =
        make_node(state, MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION, span);
    if (unique_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_name);
    }
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_type);
    }
    mylite_sql_ast_node_append_child(unique_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_options);
        mylite_sql_ast_node_set_span(
            unique_index,
            span_join(unique_index->span, index_options->span)
        );
    }
    return unique_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_fulltext_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token fulltext_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&fulltext_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *fulltext_index =
        make_node(state, MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION, span);
    if (fulltext_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(fulltext_index, index_name);
    }
    mylite_sql_ast_node_append_child(fulltext_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(fulltext_index, index_options);
        mylite_sql_ast_node_set_span(
            fulltext_index,
            span_join(fulltext_index->span, index_options->span)
        );
    }
    return fulltext_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token spatial_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&spatial_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *spatial_index =
        make_node(state, MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION, span);
    if (spatial_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(spatial_index, index_name);
    }
    mylite_sql_ast_node_append_child(spatial_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(spatial_index, index_options);
        mylite_sql_ast_node_set_span(
            spatial_index,
            span_join(spatial_index->span, index_options->span)
        );
    }
    return spatial_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token foreign_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *child_parts,
    struct mylite_sql_ast_node *referenced_table,
    struct mylite_sql_ast_node *referenced_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *actions
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&foreign_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *definition = NULL;

    if (constraint_name != NULL) {
        span = span_join(constraint_name->span, span);
    }
    if (actions != NULL) {
        span = span_join(span, actions->span);
    }

    definition = make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }

    if (constraint_name != NULL) {
        mylite_sql_ast_node_append_child(definition, constraint_name);
    }
    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(definition, index_name);
    }
    mylite_sql_ast_node_append_child(definition, child_parts);
    mylite_sql_ast_node_append_child(definition, referenced_table);
    mylite_sql_ast_node_append_child(definition, referenced_parts);
    if (actions != NULL) {
        mylite_sql_ast_node_append_child(definition, actions);
    }
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_check_constraint_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token check_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *enforcement
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&check_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *definition = NULL;

    if (constraint_name != NULL) {
        span = span_join(constraint_name->span, span);
    }
    if (enforcement != NULL) {
        span = span_join(span, enforcement->span);
    }

    definition = make_node(state, MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(definition, expression);
    if (constraint_name != NULL) {
        mylite_sql_ast_node_append_child(definition, constraint_name);
    }
    if (enforcement != NULL) {
        mylite_sql_ast_node_append_child(definition, enforcement);
    }
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_check_enforcement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
) {
    return make_node(state, kind, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_index_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
) {
    struct mylite_sql_source_span span =
        identifier == NULL ? (struct mylite_sql_source_span){0} : identifier->span;
    struct mylite_sql_ast_node *index_name =
        make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME, span);
    if (index_name == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(index_name, identifier);
    return index_name;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
) {
    struct mylite_sql_source_span span =
        action == NULL ? (struct mylite_sql_source_span){0} : action->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, action);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, action);
    if (action != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, action->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    enum mylite_sql_ast_node_kind kind
) {
    return make_node(
        state,
        kind,
        span_join(span_from_token(&first_token), span_from_token(&last_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *prefix_length,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        column == NULL ? (struct mylite_sql_source_span){0} : column->span;
    struct mylite_sql_ast_node *part = NULL;

    if (prefix_length != NULL) {
        span = span_join(span, prefix_length->span);
    }
    if (direction != NULL) {
        span = span_join(span, direction->span);
    }

    part = make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART, span);
    if (part == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(part, column);
    if (prefix_length != NULL) {
        mylite_sql_ast_node_append_child(part, prefix_length);
    }
    if (direction != NULL) {
        mylite_sql_ast_node_append_child(part, direction);
    }
    return part;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_functional_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&left_paren), span_from_token(&right_paren));
    struct mylite_sql_ast_node *part = NULL;

    if (direction != NULL) {
        span = span_join(span, direction->span);
    }

    part = make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART, span);
    if (part == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(part, expression);
    if (direction != NULL) {
        mylite_sql_ast_node_append_child(part, direction);
    }
    return part;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_multi_valued_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_multi_valued_index_part_tokens tokens,
    struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_node_kind cast_target,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.left_paren), span_from_token(&tokens.right_part_paren));
    struct mylite_sql_ast_node *multi_valued = NULL;
    struct mylite_sql_ast_node *cast_type = NULL;
    struct mylite_sql_ast_node *part = NULL;

    if (direction != NULL) {
        span = span_join(span, direction->span);
    }

    multi_valued = make_node(state, MYLITE_SQL_AST_MULTI_VALUED_INDEX_PART, span);
    if (multi_valued == NULL) {
        return NULL;
    }

    cast_type = make_node(
        state,
        cast_target,
        span_join(span_from_token(&tokens.cast_token), span_from_token(&tokens.right_cast_paren))
    );
    if (cast_type == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(multi_valued, expression);
    mylite_sql_ast_node_append_child(multi_valued, cast_type);

    part = make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART, span);
    if (part == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(part, multi_valued);
    if (direction != NULL) {
        mylite_sql_ast_node_append_child(part, direction);
    }
    return part;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_inline_primary_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    struct mylite_sql_token key_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        span_join(span_from_token(&primary_token), span_from_token(&key_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_inline_unique_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_token end_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        span_join(span_from_token(&unique_token), span_from_token(&end_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_attribute_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *attribute
) {
    struct mylite_sql_source_span span =
        attribute == NULL ? (struct mylite_sql_source_span){0} : attribute->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, attribute);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_column_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *attribute
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, attribute);
    if (attribute != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, attribute->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_auto_increment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        span_from_token(&auto_increment_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_on_update_current_timestamp(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *current_timestamp_value
) {
    struct mylite_sql_source_span span = span_from_token(&on_token);
    struct mylite_sql_ast_node *on_update = NULL;

    if (current_timestamp_value != NULL) {
        span = span_join(span, current_timestamp_value->span);
    }
    on_update = make_node(state, MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP, span);
    if (on_update == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(on_update, current_timestamp_value);
    return on_update;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_charset_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = span_from_token(&charset_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, charset_name);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_collation_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = span_from_token(&collate_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (collation_name != NULL) {
        span = span_join(span, collation_name->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, collation_name);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_binary_collation_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token binary_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_COLUMN_BINARY_COLLATION_ATTRIBUTE,
        span_from_token(&binary_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_comment_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *comment
) {
    struct mylite_sql_source_span span = span_from_token(&comment_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (comment != NULL) {
        span = span_join(span, comment->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, comment);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_visibility_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_ast_node *attribute = make_node(
        state,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_ATTRIBUTE,
        span_from_token(&visibility_token)
    );
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(attribute, visibility);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_srid_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token srid_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&srid_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_SRID_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, value);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token as_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren_token,
    struct mylite_sql_ast_node *storage
) {
    struct mylite_sql_source_span span = span_from_token(&as_token);
    struct mylite_sql_ast_node *clause = NULL;

    span = span_join(span, span_from_token(&right_paren_token));
    if (storage != NULL) {
        span = span_join(span, storage->span);
    }

    clause = make_node(state, MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, expression);
    mylite_sql_ast_node_append_child(clause, storage);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_storage(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
) {
    return make_node(state, kind, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *nullability,
    struct mylite_sql_ast_node *default_null,
    struct mylite_sql_ast_node *primary_key
) {
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct mylite_sql_ast_node *column = NULL;

    if (column_type != NULL) {
        span = span_join(span, column_type->span);
    }
    if (nullability != NULL) {
        span = span_join(span, nullability->span);
    }
    if (default_null != NULL) {
        span = span_join(span, default_null->span);
    }
    if (primary_key != NULL) {
        span = span_join(span, primary_key->span);
    }

    column = make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, column_type);
    mylite_sql_ast_node_append_child(column, nullability);
    mylite_sql_ast_node_append_child(column, default_null);
    mylite_sql_ast_node_append_child(column, primary_key);
    return column;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_with_attributes(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *attributes
) {
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct column_attribute_positions positions = {0};
    struct mylite_sql_ast_node *column = NULL;
    struct mylite_sql_ast_node *attribute = NULL;
    int rc = MYLITE_SQL_PARSE_OK;

    if (column_type != NULL) {
        span = span_join(span, column_type->span);
    }
    if (attributes != NULL) {
        span = span_join(span, attributes->span);
    }

    rc = scan_column_attribute_positions(state, attributes, &positions);
    if (rc == MYLITE_SQL_PARSE_OK) {
        rc = validate_legacy_column_attribute_order(state, &positions);
    }
    if (rc != MYLITE_SQL_PARSE_OK) {
        return NULL;
    }

    column = make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, column_type);
    attribute = attributes == NULL ? NULL : attributes->first_child;
    while (attribute != NULL) {
        struct mylite_sql_ast_node *next = attribute->next_sibling;

        mylite_sql_ast_node_append_child(column, attribute);
        attribute = next;
    }

    return column;
}

static int scan_column_attribute_positions(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *attributes,
    struct column_attribute_positions *out_positions
) {
    const struct mylite_sql_ast_node *attribute = NULL;
    size_t position = 0U;
    int rc = MYLITE_SQL_PARSE_OK;

    *out_positions = (struct column_attribute_positions){
        .charset = (size_t)-1,
        .collation = (size_t)-1,
        .binary_collation = (size_t)-1,
        .comment = (size_t)-1,
        .nullability = (size_t)-1,
        .default_value = (size_t)-1,
        .primary_key = (size_t)-1,
        .unique_key = (size_t)-1,
        .auto_increment = (size_t)-1,
        .generated = (size_t)-1,
        .visibility = (size_t)-1,
        .srid = (size_t)-1,
    };

    attribute = attributes == NULL ? NULL : attributes->first_child;
    while (rc == MYLITE_SQL_PARSE_OK && attribute != NULL) {
        switch (attribute->kind) {
        case MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->charset, position);
            break;
        case MYLITE_SQL_AST_COLUMN_BINARY_COLLATION_ATTRIBUTE:
            rc =
                record_column_attribute_position(state, &out_positions->binary_collation, position);
            if (rc != MYLITE_SQL_PARSE_OK) {
                break;
            }
            rc = record_column_attribute_position(state, &out_positions->collation, position);
            break;
        case MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->collation, position);
            break;
        case MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE:
            if (!column_attribute_position_is_set(out_positions->comment)) {
                out_positions->comment = position;
            }
            break;
        case MYLITE_SQL_AST_NULLABILITY:
            rc = record_column_attribute_position(state, &out_positions->nullability, position);
            break;
        case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
        case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
            rc = record_column_attribute_position(state, &out_positions->default_value, position);
            break;
        case MYLITE_SQL_AST_INLINE_PRIMARY_KEY:
            rc = record_column_attribute_position(state, &out_positions->primary_key, position);
            break;
        case MYLITE_SQL_AST_INLINE_UNIQUE_KEY:
            rc = record_column_attribute_position(state, &out_positions->unique_key, position);
            break;
        case MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT:
            rc = record_column_attribute_position(state, &out_positions->auto_increment, position);
            break;
        case MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE:
            rc = record_column_attribute_position(state, &out_positions->generated, position);
            break;
        case MYLITE_SQL_AST_COLUMN_VISIBILITY_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->visibility, position);
            break;
        case MYLITE_SQL_AST_COLUMN_SRID_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->srid, position);
            break;
        default:
            break;
        }

        ++position;
        attribute = attribute->next_sibling;
    }

    return rc;
}

static int record_column_attribute_position(
    struct mylite_sql_parser_state *state,
    size_t *slot,
    size_t position
) {
    if (column_attribute_position_is_set(*slot)) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    *slot = position;
    return MYLITE_SQL_PARSE_OK;
}

static int validate_legacy_column_attribute_order(
    struct mylite_sql_parser_state *state,
    const struct column_attribute_positions *positions
) {
    bool invalid_order = false;
    size_t charset_collation_limit = column_charset_collation_position_limit(positions);

    if (column_attribute_position_is_set(positions->auto_increment)) {
        if (!column_attribute_position_is_set(positions->charset) &&
            !column_attribute_position_is_set(positions->collation)) {
            return MYLITE_SQL_PARSE_OK;
        }
    }
    if (!column_attribute_charset_order_is_valid(positions)) {
        invalid_order = true;
    }
    if (column_attribute_position_is_set(positions->generated) &&
        ((column_attribute_position_is_set(positions->nullability) &&
          positions->nullability < positions->generated) ||
         (column_attribute_position_is_set(positions->default_value) &&
          positions->default_value < positions->generated) ||
         (column_attribute_position_is_set(positions->primary_key) &&
          positions->primary_key < positions->generated) ||
         (column_attribute_position_is_set(positions->unique_key) &&
          positions->unique_key < positions->generated) ||
         (column_attribute_position_is_set(positions->auto_increment) &&
          positions->auto_increment < positions->generated) ||
         (column_attribute_position_is_set(positions->comment) &&
          positions->comment < positions->generated))) {
        invalid_order = true;
    }
    if (legacy_column_attribute_precedes_charset_collation(positions, charset_collation_limit)) {
        invalid_order = true;
    }
    if (invalid_order) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (column_attribute_position_is_set(positions->auto_increment)) {
        return MYLITE_SQL_PARSE_OK;
    }

    invalid_order = ((column_attribute_position_is_set(positions->nullability) &&
                      column_attribute_position_is_set(positions->default_value) &&
                      positions->nullability > positions->default_value) ||
                     (column_attribute_position_is_set(positions->nullability) &&
                      column_attribute_position_is_set(positions->primary_key) &&
                      positions->nullability > positions->primary_key) ||
                     (column_attribute_position_is_set(positions->default_value) &&
                      column_attribute_position_is_set(positions->primary_key) &&
                      positions->default_value > positions->primary_key)) != 0;
    if (invalid_order) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return MYLITE_SQL_PARSE_OK;
}

static bool column_attribute_charset_order_is_valid(
    const struct column_attribute_positions *positions
) {
    if (!column_attribute_position_is_set(positions->charset) ||
        !column_attribute_position_is_set(positions->collation) ||
        positions->charset <= positions->collation) {
        return true;
    }

    return column_attribute_position_is_set(positions->binary_collation) &&
           positions->binary_collation == positions->collation &&
           positions->charset == positions->collation + 1U;
}

static size_t column_charset_collation_position_limit(
    const struct column_attribute_positions *positions
) {
    size_t limit = (size_t)-1;

    if (column_attribute_position_is_set(positions->charset)) {
        limit = positions->charset;
    }
    if (column_attribute_position_is_set(positions->collation) &&
        (!column_attribute_position_is_set(limit) || positions->collation > limit)) {
        limit = positions->collation;
    }

    return limit;
}

static bool legacy_column_attribute_precedes_charset_collation(
    const struct column_attribute_positions *positions,
    size_t charset_collation_limit
) {
    if (!column_attribute_position_is_set(charset_collation_limit)) {
        return false;
    }

    return ((column_attribute_position_is_set(positions->nullability) &&
             positions->nullability < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->default_value) &&
             positions->default_value < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->primary_key) &&
             positions->primary_key < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->unique_key) &&
             positions->unique_key < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->comment) &&
             positions->comment < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->auto_increment) &&
             positions->auto_increment < charset_collation_limit)) != 0;
}

static bool column_attribute_position_is_set(size_t position) {
    return position != (size_t)-1;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_null(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_token null_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&default_token), span_from_token(&null_token));

    return make_node(state, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&default_token);
    struct mylite_sql_ast_node *default_value = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    default_value = make_node(state, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE, span);
    if (default_value == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(default_value, value);
    return default_value;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_integer_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    enum mylite_sql_ast_integer_type integer_type,
    struct mylite_sql_token display_width_token,
    struct mylite_sql_token display_width_end_token,
    struct mylite_sql_token attribute_token,
    int is_unsigned,
    int is_bool_alias,
    int is_serial_alias
) {
    struct mylite_sql_source_span span = span_from_token(&type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (display_width_end_token.text != NULL) {
        span = span_join(span, span_from_token(&display_width_end_token));
    }
    if (attribute_token.text != NULL) {
        span = span_join(span, span_from_token(&attribute_token));
    }

    type = make_node(state, MYLITE_SQL_AST_INTEGER_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_integer_type(
        type,
        (struct mylite_sql_ast_integer_type_payload){
            .kind = integer_type,
            .is_unsigned = is_unsigned,
            .has_display_width = display_width_token.text != NULL,
            .is_bool_alias = is_bool_alias,
            .is_serial_alias = is_serial_alias,
            .display_width_span = span_from_token(&display_width_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_varchar_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_varchar_type_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_VARCHAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_varchar_type(
        type,
        (struct mylite_sql_ast_varchar_type_payload){
            .is_national = tokens.is_national,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_char_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_char_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.has_explicit_length ||
        (tokens.end_token.text != NULL && tokens.end_token.text != tokens.type_token.text)) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_CHAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_char_type(
        type,
        (struct mylite_sql_ast_char_type_payload){
            .has_explicit_length = tokens.has_explicit_length,
            .is_national = tokens.is_national,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_text_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_text_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_TEXT_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_text_type(
        type,
        (struct mylite_sql_ast_text_type_payload){
            .kind = tokens.text_type,
            .has_length = tokens.has_length,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_json_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token
) {
    return make_node(state, MYLITE_SQL_AST_JSON_TYPE, span_from_token(&type_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_spatial_type_tokens tokens
) {
    struct mylite_sql_ast_node *type =
        make_node(state, MYLITE_SQL_AST_SPATIAL_TYPE, span_from_token(&tokens.type_token));
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_spatial_type(type, tokens.spatial_type);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_enum_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&type_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_ENUM_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(type, label_list);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_enum_label_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *label
) {
    struct mylite_sql_ast_node *list = make_node(
        state,
        MYLITE_SQL_AST_ENUM_LABEL_LIST,
        label == NULL ? (struct mylite_sql_source_span){0} : label->span
    );
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, label);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_enum_label(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_ast_node *label
) {
    if (!is_parse_ok(state) || label_list == NULL) {
        return label_list;
    }

    mylite_sql_ast_node_append_child(label_list, label);
    if (label != NULL) {
        mylite_sql_ast_node_set_span(label_list, span_join(label_list->span, label->span));
    }
    return label_list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&type_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_SET_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(type, member_list);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_member_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *member
) {
    struct mylite_sql_ast_node *list = make_node(
        state,
        MYLITE_SQL_AST_SET_MEMBER_LIST,
        member == NULL ? (struct mylite_sql_source_span){0} : member->span
    );
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, member);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_set_member(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_ast_node *member
) {
    if (!is_parse_ok(state) || member_list == NULL) {
        return member_list;
    }

    mylite_sql_ast_node_append_child(member_list, member);
    if (member != NULL) {
        mylite_sql_ast_node_set_span(member_list, span_join(member_list->span, member->span));
    }
    return member_list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_binary_string_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_binary_string_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_BINARY_STRING_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_binary_string_type(
        type,
        (struct mylite_sql_ast_binary_string_type_payload){
            .kind = tokens.binary_string_type,
            .has_length = tokens.has_length,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_bit_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_bit_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_BIT_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_bit_type(
        type,
        (struct mylite_sql_ast_bit_type_payload){
            .has_length = tokens.has_length,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_year_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_year_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_YEAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_year_type(
        type,
        (struct mylite_sql_ast_year_type_payload){
            .has_width = tokens.has_width,
            .width_span = span_from_token(&tokens.width_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_decimal_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_decimal_type_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_DECIMAL_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_decimal_type(
        type,
        (struct mylite_sql_ast_decimal_type_payload){
            .kind = tokens.decimal_type,
            .has_precision = tokens.has_precision,
            .has_scale = tokens.has_scale,
            .is_unsigned = tokens.is_unsigned,
            .precision_span = span_from_token(&tokens.precision_token),
            .scale_span = span_from_token(&tokens.scale_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_approximate_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_approximate_type_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_APPROXIMATE_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_approximate_type(
        type,
        (struct mylite_sql_ast_approximate_type_payload){
            .kind = tokens.approximate_type,
            .has_precision = tokens.has_precision,
            .has_scale = tokens.has_scale,
            .is_unsigned = tokens.is_unsigned,
            .precision_span = span_from_token(&tokens.precision_token),
            .scale_span = span_from_token(&tokens.scale_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_date_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token date_token
) {
    return make_node(state, MYLITE_SQL_AST_DATE_TYPE, span_from_token(&date_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_datetime_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
) {
    struct mylite_sql_ast_node *type = make_node(
        state,
        MYLITE_SQL_AST_DATETIME_TYPE,
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token))
    );
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        type,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = tokens.has_precision,
            .precision_span = span_from_token(&tokens.precision_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_timestamp_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
) {
    struct mylite_sql_ast_node *type = make_node(
        state,
        MYLITE_SQL_AST_TIMESTAMP_TYPE,
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token))
    );
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        type,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = tokens.has_precision,
            .precision_span = span_from_token(&tokens.precision_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_time_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
) {
    struct mylite_sql_ast_node *type = make_node(
        state,
        MYLITE_SQL_AST_TIME_TYPE,
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token))
    );
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        type,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = tokens.has_precision,
            .precision_span = span_from_token(&tokens.precision_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_nullability(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_nullability nullability,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    struct mylite_sql_ast_node *node = make_node(
        state,
        MYLITE_SQL_AST_NULLABILITY,
        span_join(span_from_token(&first_token), span_from_token(&last_token))
    );
    if (node == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_nullability(node, nullability);
    return node;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
) {
    struct mylite_sql_source_span span =
        identifier == NULL ? (struct mylite_sql_source_span){0} : identifier->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_IDENTIFIER_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, identifier);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_empty_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *list = mylite_sql_parser_make_identifier_list(state, NULL);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_set_span(
        list,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *identifier
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, identifier);
    if (identifier != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, identifier->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
) {
    struct mylite_sql_source_span span =
        row == NULL ? (struct mylite_sql_source_span){0} : row->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INSERT_ROW_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, row);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
) {
    struct mylite_sql_source_span span =
        row == NULL ? (struct mylite_sql_source_span){0} : row->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_VALUES_ROW_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, row);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, row);
    if (row != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, row->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_values_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, row);
    if (row != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, row->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *row = make_node(state, MYLITE_SQL_AST_INSERT_ROW, span);
    if (row == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(row, value);
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *row = make_node(state, MYLITE_SQL_AST_VALUES_ROW, span);
    if (row == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(row, value);
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row,
    struct mylite_sql_ast_node *value
) {
    if (!is_parse_ok(state) || row == NULL) {
        return row;
    }

    mylite_sql_ast_node_append_child(row, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(row, span_join(row->span, value->span));
    }
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_values_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row,
    struct mylite_sql_ast_node *value
) {
    if (!is_parse_ok(state) || row == NULL) {
        return row;
    }

    mylite_sql_ast_node_append_child(row, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(row, span_join(row->span, value->span));
    }
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    if (!is_parse_ok(state) || values == NULL) {
        return values;
    }

    mylite_sql_ast_node_set_span(
        values,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
    return values;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    if (!is_parse_ok(state) || values == NULL) {
        return values;
    }

    mylite_sql_ast_node_set_span(
        values,
        span_join(span_from_token(&row_token), span_from_token(&right_paren))
    );
    return values;
}

static bool map_lexer_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
    struct mylite_sql_parser_token_map *out_map
) {
    int parser_token = 0;

    if (token == NULL || out_map == NULL) {
        return false;
    }

    if (token->kind == MYLITE_SQL_TOKEN_EOF) {
        *out_map = (struct mylite_sql_parser_token_map){
            .parser_token = 0,
            .previous_token_was_dot = false,
        };
        return true;
    }

    if (!map_direct_lexer_token(token->kind, &parser_token)) {
        switch (token->kind) {
        case MYLITE_SQL_TOKEN_KEYWORD:
            if (!map_keyword_token(
                    state,
                    token,
                    previous_token_was_dot,
                    previous_parser_token,
                    lexer_token_has_immediate_left_paren(lexer, token),
                    &parser_token
                )) {
                return false;
            }
            break;
        case MYLITE_SQL_TOKEN_OPERATOR:
            if (!map_operator_token(state, token, &parser_token)) {
                return false;
            }
            break;
        case MYLITE_SQL_TOKEN_PUNCTUATION:
            if (!map_punctuation_token(token, &parser_token)) {
                return false;
            }
            break;
        default:
            return false;
        }
    }

    *out_map = (struct mylite_sql_parser_token_map){
        .parser_token = parser_token,
        .previous_token_was_dot = parser_token == MYLITE_SQL_PARSE_DOT,
    };
    return true;
}

static bool should_skip_select_lock_target_list(
    const struct mylite_sql_token *token,
    const struct mylite_sql_parser_token_history *history
) {
    if (token == NULL || !token_text_equals(token, "OF")) {
        return false;
    }
    if (history == NULL || history->token_before_previous_parser_token != MYLITE_SQL_PARSE_FOR) {
        return false;
    }
    return history->previous_parser_token == MYLITE_SQL_PARSE_UPDATE ||
           history->previous_parser_token == MYLITE_SQL_PARSE_SHARE;
}

static enum mylite_sql_parse_status skip_select_lock_target_list(
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_next_token
) {
    bool expecting_identifier = true;

    for (;;) {
        if (mylite_sql_lexer_next(lexer, out_next_token) != 0) {
            return MYLITE_SQL_PARSE_MISUSE;
        }
        if (is_comment_token(out_next_token->kind)) {
            continue;
        }
        if (out_next_token->kind == MYLITE_SQL_TOKEN_ERROR) {
            return MYLITE_SQL_PARSE_LEXER_ERROR;
        }
        if (out_next_token->kind == MYLITE_SQL_TOKEN_EOF) {
            return expecting_identifier ? MYLITE_SQL_PARSE_SYNTAX_ERROR : MYLITE_SQL_PARSE_OK;
        }

        if (expecting_identifier) {
            if (!token_can_be_select_lock_target_identifier(out_next_token)) {
                return MYLITE_SQL_PARSE_SYNTAX_ERROR;
            }
            expecting_identifier = false;
            continue;
        }

        if (out_next_token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && out_next_token->length == 1U &&
            (out_next_token->text[0] == '.' || out_next_token->text[0] == ',')) {
            expecting_identifier = true;
            continue;
        }

        return MYLITE_SQL_PARSE_OK;
    }
}

static bool token_can_be_select_lock_target_identifier(const struct mylite_sql_token *token) {
    if (token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
        token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind != MYLITE_SQL_TOKEN_KEYWORD) {
        return false;
    }
    if ((token->keyword_flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
        return false;
    }
    return !token_text_equals(token, "FOR") && !token_text_equals(token, "LOCK") &&
           !token_text_equals(token, "LOCKED") && !token_text_equals(token, "NOWAIT") &&
           !token_text_equals(token, "SHARE") && !token_text_equals(token, "SKIP") &&
           !token_text_equals(token, "UPDATE");
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
    return !token_text_equals(token, "SELECT") && !token_text_equals(token, "FROM") &&
           !token_text_equals(token, "WHERE") && !token_text_equals(token, "HAVING") &&
           !token_text_equals(token, "ON") && !token_text_equals(token, "GROUP") &&
           !token_text_equals(token, "ORDER") && !token_text_equals(token, "BY") &&
           !token_text_equals(token, "LIMIT") && !token_text_equals(token, "UNION") &&
           !token_text_equals(token, "IN") && !token_text_equals(token, "NOT") &&
           !token_text_equals(token, "VALUES") && !token_text_equals(token, "SET") &&
           !token_text_equals(token, "UPDATE") && !token_text_equals(token, "DELETE") &&
           !token_text_equals(token, "INSERT") && !token_text_equals(token, "REPLACE") &&
           !token_text_equals(token, "JOIN");
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
        if (is_comment_token(token.kind)) {
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

static bool map_direct_lexer_token(enum mylite_sql_token_kind kind, int *out_parser_token) {
    static const struct mylite_sql_token_kind_mapping mappings[] = {
        {MYLITE_SQL_TOKEN_IDENTIFIER, MYLITE_SQL_PARSE_IDENTIFIER},
        {MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER, MYLITE_SQL_PARSE_QUOTED_IDENTIFIER},
        {MYLITE_SQL_TOKEN_STRING, MYLITE_SQL_PARSE_STRING},
        {MYLITE_SQL_TOKEN_NATIONAL_STRING, MYLITE_SQL_PARSE_NATIONAL_STRING},
        {MYLITE_SQL_TOKEN_HEX_LITERAL, MYLITE_SQL_PARSE_HEX_LITERAL},
        {MYLITE_SQL_TOKEN_BIT_LITERAL, MYLITE_SQL_PARSE_BIT_LITERAL},
        {MYLITE_SQL_TOKEN_INTEGER, MYLITE_SQL_PARSE_INTEGER},
        {MYLITE_SQL_TOKEN_DECIMAL, MYLITE_SQL_PARSE_DECIMAL},
        {MYLITE_SQL_TOKEN_FLOAT, MYLITE_SQL_PARSE_FLOAT},
        {MYLITE_SQL_TOKEN_USER_VARIABLE, MYLITE_SQL_PARSE_USER_VARIABLE},
        {MYLITE_SQL_TOKEN_SYSTEM_VARIABLE, MYLITE_SQL_PARSE_SYSTEM_VARIABLE},
        {MYLITE_SQL_TOKEN_CHARSET_INTRODUCER, MYLITE_SQL_PARSE_CHARSET_INTRODUCER},
        {MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER, MYLITE_SQL_PARSE_TEMPORAL_LITERAL_INTRODUCER
        },
    };

    for (size_t i = 0U; i < sizeof(mappings) / sizeof(mappings[0]); ++i) {
        if (mappings[i].kind == kind) {
            *out_parser_token = mappings[i].parser_token;
            return true;
        }
    }

    return false;
}

static bool lexer_token_has_immediate_left_paren(
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token
) {
    if (lexer == NULL || lexer->input == NULL || token == NULL || token->offset > lexer->offset ||
        token->length != lexer->offset - token->offset || lexer->offset >= lexer->length) {
        return false;
    }

    return lexer->input[lexer->offset] == '(';
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

static bool is_comment_token(enum mylite_sql_token_kind kind) {
    if (kind == MYLITE_SQL_TOKEN_COMMENT || kind == MYLITE_SQL_TOKEN_VERSION_COMMENT ||
        kind == MYLITE_SQL_TOKEN_HINT_COMMENT) {
        return true;
    }
    return false;
}

static bool map_keyword_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
    bool has_immediate_left_paren,
    int *out_parser_token
) {
    static const struct {
        const char *keyword;
        int parser_token;
    } keyword_mappings[] = {
        {"SELECT", MYLITE_SQL_PARSE_SELECT},
        {"ALL", MYLITE_SQL_PARSE_ALL},
        {"ALGORITHM", MYLITE_SQL_PARSE_ALGORITHM},
        {"ALTER", MYLITE_SQL_PARSE_ALTER},
        {"AS", MYLITE_SQL_PARSE_AS},
        {"CASCADED", MYLITE_SQL_PARSE_CASCADED},
        {"CAST", MYLITE_SQL_PARSE_CAST},
        {"CONVERT", MYLITE_SQL_PARSE_CONVERT},
        {"CONVERT_TZ", MYLITE_SQL_PARSE_CONVERT_TZ},
        {"DEFINER", MYLITE_SQL_PARSE_DEFINER},
        {"ESCAPE", MYLITE_SQL_PARSE_ESCAPE},
        {"EXCEPT", MYLITE_SQL_PARSE_EXCEPT},
        {"AGAINST", MYLITE_SQL_PARSE_AGAINST},
        {"FROM", MYLITE_SQL_PARSE_FROM},
        {"INVOKER", MYLITE_SQL_PARSE_INVOKER},
        {"INTERSECT", MYLITE_SQL_PARSE_INTERSECT},
        {"LANGUAGE", MYLITE_SQL_PARSE_LANGUAGE},
        {"MATCH", MYLITE_SQL_PARSE_MATCH},
        {"MERGE", MYLITE_SQL_PARSE_MERGE},
        {"NATURAL", MYLITE_SQL_PARSE_NATURAL},
        {"OPTION", MYLITE_SQL_PARSE_OPTION},
        {"QUERY", MYLITE_SQL_PARSE_QUERY},
        {"ROLLUP", MYLITE_SQL_PARSE_ROLLUP},
        {"SECURITY", MYLITE_SQL_PARSE_SECURITY},
        {"SQL", MYLITE_SQL_PARSE_SQL},
        {"TEMPTABLE", MYLITE_SQL_PARSE_TEMPTABLE},
        {"UNDEFINED", MYLITE_SQL_PARSE_UNDEFINED},
        {"UNION", MYLITE_SQL_PARSE_UNION},
        {"WHERE", MYLITE_SQL_PARSE_WHERE},
        {"AND", MYLITE_SQL_PARSE_AND},
        {"BETWEEN", MYLITE_SQL_PARSE_BETWEEN},
        {"OR", MYLITE_SQL_PARSE_OR},
        {"XOR", MYLITE_SQL_PARSE_XOR},
        {"GROUP", MYLITE_SQL_PARSE_GROUP},
        {"GROUP_CONCAT", MYLITE_SQL_PARSE_GROUP_CONCAT},
        {"GROUPING", MYLITE_SQL_PARSE_GROUPING},
        {"ANY_VALUE", MYLITE_SQL_PARSE_ANY_VALUE},
        {"HAVING", MYLITE_SQL_PARSE_HAVING},
        {"ORDER", MYLITE_SQL_PARSE_ORDER},
        {"BY", MYLITE_SQL_PARSE_BY},
        {"BINARY", MYLITE_SQL_PARSE_BINARY},
        {"USING", MYLITE_SQL_PARSE_USING},
        {"BIT", MYLITE_SQL_PARSE_BIT},
        {"BIN", MYLITE_SQL_PARSE_BIN},
        {"BIT_LENGTH", MYLITE_SQL_PARSE_BIT_LENGTH},
        {"OCT", MYLITE_SQL_PARSE_OCT},
        {"OCTET_LENGTH", MYLITE_SQL_PARSE_OCTET_LENGTH},
        {"ORD", MYLITE_SQL_PARSE_ORD},
        {"ABS", MYLITE_SQL_PARSE_ABS},
        {"ACOS", MYLITE_SQL_PARSE_ACOS},
        {"ASCII", MYLITE_SQL_PARSE_ASCII},
        {"ASIN", MYLITE_SQL_PARSE_ASIN},
        {"ATAN", MYLITE_SQL_PARSE_ATAN},
        {"ATAN2", MYLITE_SQL_PARSE_ATAN2},
        {"COS", MYLITE_SQL_PARSE_COS},
        {"COT", MYLITE_SQL_PARSE_COT},
        {"EXP", MYLITE_SQL_PARSE_EXP},
        {"LN", MYLITE_SQL_PARSE_LN},
        {"LOG", MYLITE_SQL_PARSE_LOG},
        {"LOGS", MYLITE_SQL_PARSE_LOGS},
        {"LOG10", MYLITE_SQL_PARSE_LOG10},
        {"LOG2", MYLITE_SQL_PARSE_LOG2},
        {"POW", MYLITE_SQL_PARSE_POW},
        {"POWER", MYLITE_SQL_PARSE_POWER},
        {"SIGN", MYLITE_SQL_PARSE_SIGN},
        {"CEIL", MYLITE_SQL_PARSE_CEIL},
        {"CEILING", MYLITE_SQL_PARSE_CEILING},
        {"FLOOR", MYLITE_SQL_PARSE_FLOOR},
        {"ROUND", MYLITE_SQL_PARSE_ROUND},
        {"PI", MYLITE_SQL_PARSE_PI},
        {"RAND", MYLITE_SQL_PARSE_RAND},
        {"REPLICA", MYLITE_SQL_PARSE_REPLICA},
        {"REPLICAS", MYLITE_SQL_PARSE_REPLICAS},
        {"SIN", MYLITE_SQL_PARSE_SIN},
        {"SQRT", MYLITE_SQL_PARSE_SQRT},
        {"TAN", MYLITE_SQL_PARSE_TAN},
        {"DEGREES", MYLITE_SQL_PARSE_DEGREES},
        {"RADIANS", MYLITE_SQL_PARSE_RADIANS},
        {"CONNECTION_ID", MYLITE_SQL_PARSE_CONNECTION_ID},
        {"COUNT", MYLITE_SQL_PARSE_COUNT},
        {"CRC32", MYLITE_SQL_PARSE_CRC32},
        {"FROM_BASE64", MYLITE_SQL_PARSE_FROM_BASE64},
        {"HEX", MYLITE_SQL_PARSE_HEX},
        {"TO_BASE64", MYLITE_SQL_PARSE_TO_BASE64},
        {"UNHEX", MYLITE_SQL_PARSE_UNHEX},
        {"IS_UUID", MYLITE_SQL_PARSE_IS_UUID},
        {"UUID", MYLITE_SQL_PARSE_UUID},
        {"UUID_TO_BIN", MYLITE_SQL_PARSE_UUID_TO_BIN},
        {"BIN_TO_UUID", MYLITE_SQL_PARSE_BIN_TO_UUID},
        {"AVG", MYLITE_SQL_PARSE_AVG},
        {"BIT_AND", MYLITE_SQL_PARSE_BIT_AND},
        {"BIT_COUNT", MYLITE_SQL_PARSE_BIT_COUNT},
        {"BIT_OR", MYLITE_SQL_PARSE_BIT_OR},
        {"BIT_XOR", MYLITE_SQL_PARSE_BIT_XOR},
        {"BOTH", MYLITE_SQL_PARSE_BOTH},
        {"CROSS", MYLITE_SQL_PARSE_CROSS},
        {"DISTINCT", MYLITE_SQL_PARSE_DISTINCT},
        {"DISTINCTROW", MYLITE_SQL_PARSE_DISTINCTROW},
        {"CURDATE", MYLITE_SQL_PARSE_CURDATE},
        {"CURRENT_DATE", MYLITE_SQL_PARSE_CURRENT_DATE},
        {"CURRENT_ROLE", MYLITE_SQL_PARSE_CURRENT_ROLE},
        {"CURRENT_TIME", MYLITE_SQL_PARSE_CURRENT_TIME},
        {"CURRENT_TIMESTAMP", MYLITE_SQL_PARSE_CURRENT_TIMESTAMP},
        {"CURRENT_USER", MYLITE_SQL_PARSE_CURRENT_USER},
        {"CURTIME", MYLITE_SQL_PARSE_CURTIME},
        {"UTC_DATE", MYLITE_SQL_PARSE_UTC_DATE},
        {"UTC_TIME", MYLITE_SQL_PARSE_UTC_TIME},
        {"UTC_TIMESTAMP", MYLITE_SQL_PARSE_UTC_TIMESTAMP},
        {"SYSDATE", MYLITE_SQL_PARSE_SYSDATE},
        {"ASC", MYLITE_SQL_PARSE_ASC},
        {"DESC", MYLITE_SQL_PARSE_DESC},
        {"AUTO_INCREMENT", MYLITE_SQL_PARSE_AUTO_INCREMENT},
        {"LAST_INSERT_ID", MYLITE_SQL_PARSE_LAST_INSERT_ID},
        {"LCASE", MYLITE_SQL_PARSE_LCASE},
        {"LEADING", MYLITE_SQL_PARSE_LEADING},
        {"LENGTH", MYLITE_SQL_PARSE_LENGTH},
        {"LOCATE", MYLITE_SQL_PARSE_LOCATE},
        {"LPAD", MYLITE_SQL_PARSE_LPAD},
        {"MID", MYLITE_SQL_PARSE_MID},
        {"MINUTE", MYLITE_SQL_PARSE_MINUTE},
        {"MONTH", MYLITE_SQL_PARSE_MONTH},
        {"RIGHT", MYLITE_SQL_PARSE_RIGHT},
        {"REPEAT", MYLITE_SQL_PARSE_REPEAT},
        {"REVERSE", MYLITE_SQL_PARSE_REVERSE},
        {"QUOTE", MYLITE_SQL_PARSE_QUOTE},
        {"SOUNDEX", MYLITE_SQL_PARSE_SOUNDEX},
        {"RPAD", MYLITE_SQL_PARSE_RPAD},
        {"INSTR", MYLITE_SQL_PARSE_INSTR},
        {"LOWER", MYLITE_SQL_PARSE_LOWER},
        {"LTRIM", MYLITE_SQL_PARSE_LTRIM},
        {"MAX", MYLITE_SQL_PARSE_MAX},
        {"MIN", MYLITE_SQL_PARSE_MIN},
        {"SUM", MYLITE_SQL_PARSE_SUM},
        {"LIMIT", MYLITE_SQL_PARSE_LIMIT},
        {"OFFSET", MYLITE_SQL_PARSE_OFFSET},
        {"SPACE", MYLITE_SQL_PARSE_SPACE},
        {"USE", MYLITE_SQL_PARSE_USE},
        {"CALL", MYLITE_SQL_PARSE_CALL},
        {"CREATE", MYLITE_SQL_PARSE_CREATE},
        {"TABLE", MYLITE_SQL_PARSE_TABLE},
        {"VIEW", MYLITE_SQL_PARSE_VIEW},
        {"TEMPORARY", MYLITE_SQL_PARSE_TEMPORARY},
        {"GENERATED", MYLITE_SQL_PARSE_GENERATED},
        {"ALWAYS", MYLITE_SQL_PARSE_ALWAYS},
        {"ARRAY", MYLITE_SQL_PARSE_ARRAY},
        {"VIRTUAL", MYLITE_SQL_PARSE_VIRTUAL},
        {"STORED", MYLITE_SQL_PARSE_STORED},
        {"IF", MYLITE_SQL_PARSE_IF},
        {"IFNULL", MYLITE_SQL_PARSE_IFNULL},
        {"COALESCE", MYLITE_SQL_PARSE_COALESCE},
        {"COERCIBILITY", MYLITE_SQL_PARSE_COERCIBILITY},
        {"CONCAT", MYLITE_SQL_PARSE_CONCAT},
        {"CONCAT_WS", MYLITE_SQL_PARSE_CONCAT_WS},
        {"CONV", MYLITE_SQL_PARSE_CONV},
        {"POSITION", MYLITE_SQL_PARSE_POSITION},
        {"NULLIF", MYLITE_SQL_PARSE_NULLIF},
        {"ISNULL", MYLITE_SQL_PARSE_ISNULL},
        {"CASE", MYLITE_SQL_PARSE_CASE},
        {"WHEN", MYLITE_SQL_PARSE_WHEN},
        {"THEN", MYLITE_SQL_PARSE_THEN},
        {"ELSE", MYLITE_SQL_PARSE_ELSE},
        {"END", MYLITE_SQL_PARSE_END},
        {"MOD", MYLITE_SQL_PARSE_MOD},
        {"DIV", MYLITE_SQL_PARSE_DIV},
        {"IGNORE", MYLITE_SQL_PARSE_IGNORE},
        {"EXISTS", MYLITE_SQL_PARSE_EXISTS},
        {"DATABASE", MYLITE_SQL_PARSE_DATABASE},
        {"DATABASES", MYLITE_SQL_PARSE_DATABASES},
        {"DATA", MYLITE_SQL_PARSE_DATA},
        {"DAY", MYLITE_SQL_PARSE_DAY},
        {"DAYNAME", MYLITE_SQL_PARSE_DAYNAME},
        {"DAY_HOUR", MYLITE_SQL_PARSE_DAY_HOUR},
        {"DAY_MICROSECOND", MYLITE_SQL_PARSE_DAY_MICROSECOND},
        {"DAY_MINUTE", MYLITE_SQL_PARSE_DAY_MINUTE},
        {"DAYOFMONTH", MYLITE_SQL_PARSE_DAYOFMONTH},
        {"DAYOFWEEK", MYLITE_SQL_PARSE_DAYOFWEEK},
        {"DAYOFYEAR", MYLITE_SQL_PARSE_DAYOFYEAR},
        {"DAY_SECOND", MYLITE_SQL_PARSE_DAY_SECOND},
        {"ADDDATE", MYLITE_SQL_PARSE_ADDDATE},
        {"ADDTIME", MYLITE_SQL_PARSE_ADDTIME},
        {"DATEDIFF", MYLITE_SQL_PARSE_DATEDIFF},
        {"DATE_ADD", MYLITE_SQL_PARSE_DATE_ADD},
        {"DATE_SUB", MYLITE_SQL_PARSE_DATE_SUB},
        {"DATE_FORMAT", MYLITE_SQL_PARSE_DATE_FORMAT},
        {"GET_FORMAT", MYLITE_SQL_PARSE_GET_FORMAT},
        {"EXTRACT", MYLITE_SQL_PARSE_EXTRACT},
        {"FROM_DAYS", MYLITE_SQL_PARSE_FROM_DAYS},
        {"FROM_UNIXTIME", MYLITE_SQL_PARSE_FROM_UNIXTIME},
        {"HOUR_MICROSECOND", MYLITE_SQL_PARSE_HOUR_MICROSECOND},
        {"HOUR_MINUTE", MYLITE_SQL_PARSE_HOUR_MINUTE},
        {"HOUR_SECOND", MYLITE_SQL_PARSE_HOUR_SECOND},
        {"MICROSECOND", MYLITE_SQL_PARSE_MICROSECOND},
        {"MINUTE_MICROSECOND", MYLITE_SQL_PARSE_MINUTE_MICROSECOND},
        {"MINUTE_SECOND", MYLITE_SQL_PARSE_MINUTE_SECOND},
        {"MAKEDATE", MYLITE_SQL_PARSE_MAKEDATE},
        {"MAKETIME", MYLITE_SQL_PARSE_MAKETIME},
        {"MONTHNAME", MYLITE_SQL_PARSE_MONTHNAME},
        {"PERIOD_ADD", MYLITE_SQL_PARSE_PERIOD_ADD},
        {"PERIOD_DIFF", MYLITE_SQL_PARSE_PERIOD_DIFF},
        {"QUARTER", MYLITE_SQL_PARSE_QUARTER},
        {"SECOND_MICROSECOND", MYLITE_SQL_PARSE_SECOND_MICROSECOND},
        {"SQL_TSI_DAY", MYLITE_SQL_PARSE_SQL_TSI_DAY},
        {"SQL_TSI_HOUR", MYLITE_SQL_PARSE_SQL_TSI_HOUR},
        {"SQL_TSI_MINUTE", MYLITE_SQL_PARSE_SQL_TSI_MINUTE},
        {"SQL_TSI_MONTH", MYLITE_SQL_PARSE_SQL_TSI_MONTH},
        {"SQL_TSI_QUARTER", MYLITE_SQL_PARSE_SQL_TSI_QUARTER},
        {"SQL_TSI_SECOND", MYLITE_SQL_PARSE_SQL_TSI_SECOND},
        {"SQL_TSI_WEEK", MYLITE_SQL_PARSE_SQL_TSI_WEEK},
        {"SQL_TSI_YEAR", MYLITE_SQL_PARSE_SQL_TSI_YEAR},
        {"TIMEDIFF", MYLITE_SQL_PARSE_TIMEDIFF},
        {"TIMESTAMPADD", MYLITE_SQL_PARSE_TIMESTAMPADD},
        {"TIMESTAMPDIFF", MYLITE_SQL_PARSE_TIMESTAMPDIFF},
        {"UNIX_TIMESTAMP", MYLITE_SQL_PARSE_UNIX_TIMESTAMP},
        {"TIME_FORMAT", MYLITE_SQL_PARSE_TIME_FORMAT},
        {"TIME_TO_SEC", MYLITE_SQL_PARSE_TIME_TO_SEC},
        {"TO_DAYS", MYLITE_SQL_PARSE_TO_DAYS},
        {"TO_SECONDS", MYLITE_SQL_PARSE_TO_SECONDS},
        {"SEC_TO_TIME", MYLITE_SQL_PARSE_SEC_TO_TIME},
        {"STR_TO_DATE", MYLITE_SQL_PARSE_STR_TO_DATE},
        {"REGEXP_INSTR", MYLITE_SQL_PARSE_REGEXP_INSTR},
        {"REGEXP_LIKE", MYLITE_SQL_PARSE_REGEXP_LIKE},
        {"REGEXP_REPLACE", MYLITE_SQL_PARSE_REGEXP_REPLACE},
        {"REGEXP_SUBSTR", MYLITE_SQL_PARSE_REGEXP_SUBSTR},
        {"LAST_DAY", MYLITE_SQL_PARSE_LAST_DAY},
        {"WEEK", MYLITE_SQL_PARSE_WEEK},
        {"WEEKDAY", MYLITE_SQL_PARSE_WEEKDAY},
        {"WEEKOFYEAR", MYLITE_SQL_PARSE_WEEKOFYEAR},
        {"YEAR_MONTH", MYLITE_SQL_PARSE_YEAR_MONTH},
        {"YEARWEEK", MYLITE_SQL_PARSE_YEARWEEK},
        {"DROP", MYLITE_SQL_PARSE_DROP},
        {"TRUNCATE", MYLITE_SQL_PARSE_TRUNCATE},
        {"SUBSTR", MYLITE_SQL_PARSE_SUBSTR},
        {"SUBSTRING", MYLITE_SQL_PARSE_SUBSTRING},
        {"SUBSTRING_INDEX", MYLITE_SQL_PARSE_SUBSTRING_INDEX},
        {"STRCMP", MYLITE_SQL_PARSE_STRCMP},
        {"RTRIM", MYLITE_SQL_PARSE_RTRIM},
        {"TRAILING", MYLITE_SQL_PARSE_TRAILING},
        {"TRIM", MYLITE_SQL_PARSE_TRIM},
        {"SUBDATE", MYLITE_SQL_PARSE_SUBDATE},
        {"SUBTIME", MYLITE_SQL_PARSE_SUBTIME},
        {"UCASE", MYLITE_SQL_PARSE_UCASE},
        {"UPPER", MYLITE_SQL_PARSE_UPPER},
        {"SHOW", MYLITE_SQL_PARSE_SHOW},
        {"TABLES", MYLITE_SQL_PARSE_TABLES},
        {"COLUMNS", MYLITE_SQL_PARSE_COLUMNS},
        {"ELT", MYLITE_SQL_PARSE_ELT},
        {"EXPORT_SET", MYLITE_SQL_PARSE_EXPORT_SET},
        {"FIELD", MYLITE_SQL_PARSE_FIELD},
        {"FIELDS", MYLITE_SQL_PARSE_FIELDS},
        {"FIND_IN_SET", MYLITE_SQL_PARSE_FIND_IN_SET},
        {"FORMAT", MYLITE_SQL_PARSE_FORMAT},
        {"GREATEST", MYLITE_SQL_PARSE_GREATEST},
        {"INDEX", MYLITE_SQL_PARSE_INDEX},
        {"INDEXES", MYLITE_SQL_PARSE_INDEXES},
        {"LEAST", MYLITE_SQL_PARSE_LEAST},
        {"MAKE_SET", MYLITE_SQL_PARSE_MAKE_SET},
        {"CONSTRAINT", MYLITE_SQL_PARSE_CONSTRAINT},
        {"FOREIGN", MYLITE_SQL_PARSE_FOREIGN},
        {"KEY", MYLITE_SQL_PARSE_KEY},
        {"KEYS", MYLITE_SQL_PARSE_KEYS},
        {"REFERENCES", MYLITE_SQL_PARSE_REFERENCES},
        {"ACTION", MYLITE_SQL_PARSE_ACTION},
        {"CASCADE", MYLITE_SQL_PARSE_CASCADE},
        {"ENFORCED", MYLITE_SQL_PARSE_ENFORCED},
        {"PRIMARY", MYLITE_SQL_PARSE_PRIMARY},
        {"RESTRICT", MYLITE_SQL_PARSE_RESTRICT},
        {"UNIQUE", MYLITE_SQL_PARSE_UNIQUE},
        {"FULLTEXT", MYLITE_SQL_PARSE_FULLTEXT},
        {"SPATIAL", MYLITE_SQL_PARSE_SPATIAL},
        {"GEOMETRY", MYLITE_SQL_PARSE_GEOMETRY},
        {"GEOMCOLLECTION", MYLITE_SQL_PARSE_GEOMCOLLECTION},
        {"GEOMETRYCOLLECTION", MYLITE_SQL_PARSE_GEOMETRYCOLLECTION},
        {"LINESTRING", MYLITE_SQL_PARSE_LINESTRING},
        {"MULTILINESTRING", MYLITE_SQL_PARSE_MULTILINESTRING},
        {"MULTIPOINT", MYLITE_SQL_PARSE_MULTIPOINT},
        {"MULTIPOLYGON", MYLITE_SQL_PARSE_MULTIPOLYGON},
        {"POINT", MYLITE_SQL_PARSE_POINT},
        {"POLYGON", MYLITE_SQL_PARSE_POLYGON},
        {"FULL", MYLITE_SQL_PARSE_FULL},
        {"TRIGGERS", MYLITE_SQL_PARSE_TRIGGERS},
        {"EVENTS", MYLITE_SQL_PARSE_EVENTS},
        {"OPEN", MYLITE_SQL_PARSE_OPEN},
        {"PROCESSLIST", MYLITE_SQL_PARSE_PROCESSLIST},
        {"GRANTS", MYLITE_SQL_PARSE_GRANTS},
        {"WARNINGS", MYLITE_SQL_PARSE_WARNINGS},
        {"ERRORS", MYLITE_SQL_PARSE_ERRORS},
        {"PROCEDURE", MYLITE_SQL_PARSE_PROCEDURE},
        {"FUNCTION", MYLITE_SQL_PARSE_FUNCTION},
        {"ENGINE", MYLITE_SQL_PARSE_ENGINE},
        {"ENGINES", MYLITE_SQL_PARSE_ENGINES},
        {"PLUGINS", MYLITE_SQL_PARSE_PLUGINS},
        {"PRIVILEGES", MYLITE_SQL_PARSE_PRIVILEGES},
        {"ENUM", MYLITE_SQL_PARSE_ENUM},
        {"COMMENT", MYLITE_SQL_PARSE_COMMENT},
        {"STATUS", MYLITE_SQL_PARSE_STATUS},
        {"DISK", MYLITE_SQL_PARSE_DISK},
        {"STORAGE", MYLITE_SQL_PARSE_STORAGE},
        {"TABLESPACE", MYLITE_SQL_PARSE_TABLESPACE},
        {"INSERT_METHOD", MYLITE_SQL_PARSE_INSERT_METHOD},
        {"VARIABLES", MYLITE_SQL_PARSE_VARIABLES},
        {"DEFAULT", MYLITE_SQL_PARSE_DEFAULT},
        {"CHAR", MYLITE_SQL_PARSE_CHAR},
        {"CHARACTER", MYLITE_SQL_PARSE_CHARACTER},
        {"CHARACTER_LENGTH", MYLITE_SQL_PARSE_CHARACTER_LENGTH},
        {"CHAR_LENGTH", MYLITE_SQL_PARSE_CHAR_LENGTH},
        {"CHARSET", MYLITE_SQL_PARSE_CHARSET},
        {"COLLATE", MYLITE_SQL_PARSE_COLLATE},
        {"COLLATION", MYLITE_SQL_PARSE_COLLATION},
        {"LIKE", MYLITE_SQL_PARSE_LIKE},
        {"REGEXP", MYLITE_SQL_PARSE_REGEXP},
        {"RLIKE", MYLITE_SQL_PARSE_RLIKE},
        {"SCHEMA", MYLITE_SQL_PARSE_SCHEMA},
        {"SCHEMAS", MYLITE_SQL_PARSE_SCHEMAS},
        {"DESCRIBE", MYLITE_SQL_PARSE_DESCRIBE},
        {"EXPLAIN", MYLITE_SQL_PARSE_EXPLAIN},
        {"SESSION_USER", MYLITE_SQL_PARSE_SESSION_USER},
        {"RENAME", MYLITE_SQL_PARSE_RENAME},
        {"ADD", MYLITE_SQL_PARSE_ADD},
        {"AFTER", MYLITE_SQL_PARSE_AFTER},
        {"MODIFY", MYLITE_SQL_PARSE_MODIFY},
        {"CHANGE", MYLITE_SQL_PARSE_CHANGE},
        {"COLUMN", MYLITE_SQL_PARSE_COLUMN},
        {"FIRST", MYLITE_SQL_PARSE_FIRST},
        {"FOR", MYLITE_SQL_PARSE_FOR},
        {"FORCE", MYLITE_SQL_PARSE_FORCE},
        {"INSERT", MYLITE_SQL_PARSE_INSERT},
        {"INFILE", MYLITE_SQL_PARSE_INFILE},
        {"INNER", MYLITE_SQL_PARSE_INNER},
        {"JOIN", MYLITE_SQL_PARSE_JOIN},
        {"LEFT", MYLITE_SQL_PARSE_LEFT},
        {"OUTER", MYLITE_SQL_PARSE_OUTER},
        {"REPLACE", MYLITE_SQL_PARSE_REPLACE},
        {"LOW_PRIORITY", MYLITE_SQL_PARSE_LOW_PRIORITY},
        {"HIGH_PRIORITY", MYLITE_SQL_PARSE_HIGH_PRIORITY},
        {"DELAYED", MYLITE_SQL_PARSE_DELAYED},
        {"INTO", MYLITE_SQL_PARSE_INTO},
        {"LOCK", MYLITE_SQL_PARSE_LOCK},
        {"LOCKED", MYLITE_SQL_PARSE_LOCKED},
        {"LOAD", MYLITE_SQL_PARSE_LOAD},
        {"LAST", MYLITE_SQL_PARSE_LAST},
        {"MEMORY", MYLITE_SQL_PARSE_MEMORY},
        {"MODE", MYLITE_SQL_PARSE_MODE},
        {"NOWAIT", MYLITE_SQL_PARSE_NOWAIT},
        {"READ", MYLITE_SQL_PARSE_READ},
        {"COMMITTED", MYLITE_SQL_PARSE_COMMITTED},
        {"ISOLATION", MYLITE_SQL_PARSE_ISOLATION},
        {"LEVEL", MYLITE_SQL_PARSE_LEVEL},
        {"ONLY", MYLITE_SQL_PARSE_ONLY},
        {"REPEATABLE", MYLITE_SQL_PARSE_REPEATABLE},
        {"SERIALIZABLE", MYLITE_SQL_PARSE_SERIALIZABLE},
        {"UNCOMMITTED", MYLITE_SQL_PARSE_UNCOMMITTED},
        {"ROW", MYLITE_SQL_PARSE_ROW},
        {"VALUE", MYLITE_SQL_PARSE_VALUE},
        {"VALUES", MYLITE_SQL_PARSE_VALUES},
        {"DUPLICATE", MYLITE_SQL_PARSE_DUPLICATE},
        {"TO", MYLITE_SQL_PARSE_TO},
        {"DELETE", MYLITE_SQL_PARSE_DELETE},
        {"DEALLOCATE", MYLITE_SQL_PARSE_DEALLOCATE},
        {"DO", MYLITE_SQL_PARSE_DO},
        {"EXECUTE", MYLITE_SQL_PARSE_EXECUTE},
        {"PREPARE", MYLITE_SQL_PARSE_PREPARE},
        {"UPDATE", MYLITE_SQL_PARSE_UPDATE},
        {"START", MYLITE_SQL_PARSE_START},
        {"TRANSACTION", MYLITE_SQL_PARSE_TRANSACTION},
        {"WITH", MYLITE_SQL_PARSE_WITH},
        {"CONSISTENT", MYLITE_SQL_PARSE_CONSISTENT},
        {"SNAPSHOT", MYLITE_SQL_PARSE_SNAPSHOT},
        {"BEGIN", MYLITE_SQL_PARSE_BEGIN},
        {"WORK", MYLITE_SQL_PARSE_WORK},
        {"COMMIT", MYLITE_SQL_PARSE_COMMIT},
        {"ROLLBACK", MYLITE_SQL_PARSE_ROLLBACK},
        {"SAVEPOINT", MYLITE_SQL_PARSE_SAVEPOINT},
        {"RELEASE", MYLITE_SQL_PARSE_RELEASE},
        {"UNLOCK", MYLITE_SQL_PARSE_UNLOCK},
        {"WRITE", MYLITE_SQL_PARSE_WRITE},
        {"ANALYZE", MYLITE_SQL_PARSE_ANALYZE},
        {"CHECK", MYLITE_SQL_PARSE_CHECK},
        {"OPTIMIZE", MYLITE_SQL_PARSE_OPTIMIZE},
        {"REPAIR", MYLITE_SQL_PARSE_REPAIR},
        {"NO_WRITE_TO_BINLOG", MYLITE_SQL_PARSE_NO_WRITE_TO_BINLOG},
        {"QUICK", MYLITE_SQL_PARSE_QUICK},
        {"FAST", MYLITE_SQL_PARSE_FAST},
        {"MEDIUM", MYLITE_SQL_PARSE_MEDIUM},
        {"EXTENDED", MYLITE_SQL_PARSE_EXTENDED},
        {"CHANGED", MYLITE_SQL_PARSE_CHANGED},
        {"UPGRADE", MYLITE_SQL_PARSE_UPGRADE},
        {"USE_FRM", MYLITE_SQL_PARSE_USE_FRM},
        {"SET", MYLITE_SQL_PARSE_SET},
        {"SESSION", MYLITE_SQL_PARSE_SESSION},
        {"LOCAL", MYLITE_SQL_PARSE_LOCAL},
        {"LINES", MYLITE_SQL_PARSE_LINES},
        {"LOCALTIME", MYLITE_SQL_PARSE_LOCALTIME},
        {"LOCALTIMESTAMP", MYLITE_SQL_PARSE_LOCALTIMESTAMP},
        {"GLOBAL", MYLITE_SQL_PARSE_GLOBAL},
        {"SYSTEM", MYLITE_SQL_PARSE_SYSTEM},
        {"ON", MYLITE_SQL_PARSE_ON},
        {"OVER", MYLITE_SQL_PARSE_OVER},
        {"WINDOW", MYLITE_SQL_PARSE_WINDOW},
        {"NULLS", MYLITE_SQL_PARSE_NULLS},
        {"RESPECT", MYLITE_SQL_PARSE_RESPECT},
        {"ROWS", MYLITE_SQL_PARSE_ROWS},
        {"RANGE", MYLITE_SQL_PARSE_RANGE},
        {"UNBOUNDED", MYLITE_SQL_PARSE_UNBOUNDED},
        {"PRECEDING", MYLITE_SQL_PARSE_PRECEDING},
        {"FOLLOWING", MYLITE_SQL_PARSE_FOLLOWING},
        {"CURRENT", MYLITE_SQL_PARSE_CURRENT},
        {"NO", MYLITE_SQL_PARSE_NO},
        {"OFF", MYLITE_SQL_PARSE_OFF},
        {"NAMES", MYLITE_SQL_PARSE_NAMES},
        {"NATIONAL", MYLITE_SQL_PARSE_NATIONAL},
        {"NCHAR", MYLITE_SQL_PARSE_NCHAR},
        {"INT", MYLITE_SQL_PARSE_INT},
        {"TINYINT", MYLITE_SQL_PARSE_TINYINT},
        {"SMALLINT", MYLITE_SQL_PARSE_SMALLINT},
        {"MEDIUMINT", MYLITE_SQL_PARSE_MEDIUMINT},
        {"INTEGER", MYLITE_SQL_PARSE_INTEGER_TYPE},
        {"BIGINT", MYLITE_SQL_PARSE_BIGINT},
        {"DECIMAL", MYLITE_SQL_PARSE_DECIMAL_TYPE},
        {"DEC", MYLITE_SQL_PARSE_DEC},
        {"NUMERIC", MYLITE_SQL_PARSE_NUMERIC},
        {"FIXED", MYLITE_SQL_PARSE_FIXED},
        {"ROW_FORMAT", MYLITE_SQL_PARSE_ROW_FORMAT},
        {"PARTITION", MYLITE_SQL_PARSE_PARTITION},
        {"KEY_BLOCK_SIZE", MYLITE_SQL_PARSE_KEY_BLOCK_SIZE},
        {"PACK_KEYS", MYLITE_SQL_PARSE_PACK_KEYS},
        {"DISABLE", MYLITE_SQL_PARSE_DISABLE},
        {"ENABLE", MYLITE_SQL_PARSE_ENABLE},
        {"CHECKSUM", MYLITE_SQL_PARSE_CHECKSUM},
        {"STATS_PERSISTENT", MYLITE_SQL_PARSE_STATS_PERSISTENT},
        {"STATS_AUTO_RECALC", MYLITE_SQL_PARSE_STATS_AUTO_RECALC},
        {"STATS_SAMPLE_PAGES", MYLITE_SQL_PARSE_STATS_SAMPLE_PAGES},
        {"MIN_ROWS", MYLITE_SQL_PARSE_MIN_ROWS},
        {"MAX_ROWS", MYLITE_SQL_PARSE_MAX_ROWS},
        {"AVG_ROW_LENGTH", MYLITE_SQL_PARSE_AVG_ROW_LENGTH},
        {"DELAY_KEY_WRITE", MYLITE_SQL_PARSE_DELAY_KEY_WRITE},
        {"DYNAMIC", MYLITE_SQL_PARSE_DYNAMIC},
        {"COMPACT", MYLITE_SQL_PARSE_COMPACT},
        {"REDUNDANT", MYLITE_SQL_PARSE_REDUNDANT},
        {"COMPRESSED", MYLITE_SQL_PARSE_COMPRESSED},
        {"FLOAT", MYLITE_SQL_PARSE_FLOAT_TYPE},
        {"FLOAT4", MYLITE_SQL_PARSE_FLOAT4},
        {"FLOAT8", MYLITE_SQL_PARSE_FLOAT8},
        {"DOUBLE", MYLITE_SQL_PARSE_DOUBLE},
        {"EXPANSION", MYLITE_SQL_PARSE_EXPANSION},
        {"PRECISION", MYLITE_SQL_PARSE_PRECISION},
        {"REAL", MYLITE_SQL_PARSE_REAL},
        {"DATE", MYLITE_SQL_PARSE_DATE},
        {"DATETIME", MYLITE_SQL_PARSE_DATETIME},
        {"HOUR", MYLITE_SQL_PARSE_HOUR},
        {"INTERVAL", MYLITE_SQL_PARSE_INTERVAL},
        {"SECOND", MYLITE_SQL_PARSE_SECOND},
        {"TIME", MYLITE_SQL_PARSE_TIME},
        {"TIMESTAMP", MYLITE_SQL_PARSE_TIMESTAMP},
        {"YEAR", MYLITE_SQL_PARSE_YEAR},
        {"VARCHAR", MYLITE_SQL_PARSE_VARCHAR},
        {"NVARCHAR", MYLITE_SQL_PARSE_NVARCHAR},
        {"VARBINARY", MYLITE_SQL_PARSE_VARBINARY},
        {"BYTE", MYLITE_SQL_PARSE_BYTE},
        {"TINYBLOB", MYLITE_SQL_PARSE_TINYBLOB},
        {"BLOB", MYLITE_SQL_PARSE_BLOB},
        {"MEDIUMBLOB", MYLITE_SQL_PARSE_MEDIUMBLOB},
        {"LONGBLOB", MYLITE_SQL_PARSE_LONGBLOB},
        {"LONG", MYLITE_SQL_PARSE_LONG},
        {"VARYING", MYLITE_SQL_PARSE_VARYING},
        {"TINYTEXT", MYLITE_SQL_PARSE_TINYTEXT},
        {"TEXT", MYLITE_SQL_PARSE_TEXT},
        {"MEDIUMTEXT", MYLITE_SQL_PARSE_MEDIUMTEXT},
        {"LONGTEXT", MYLITE_SQL_PARSE_LONGTEXT},
        {"JSON", MYLITE_SQL_PARSE_JSON},
        {"JSON_ARRAY", MYLITE_SQL_PARSE_JSON_ARRAY},
        {"JSON_CONTAINS", MYLITE_SQL_PARSE_JSON_CONTAINS},
        {"JSON_CONTAINS_PATH", MYLITE_SQL_PARSE_JSON_CONTAINS_PATH},
        {"JSON_EXTRACT", MYLITE_SQL_PARSE_JSON_EXTRACT},
        {"JSON_INSERT", MYLITE_SQL_PARSE_JSON_INSERT},
        {"JSON_KEYS", MYLITE_SQL_PARSE_JSON_KEYS},
        {"JSON_LENGTH", MYLITE_SQL_PARSE_JSON_LENGTH},
        {"JSON_OBJECT", MYLITE_SQL_PARSE_JSON_OBJECT},
        {"JSON_QUOTE", MYLITE_SQL_PARSE_JSON_QUOTE},
        {"JSON_REMOVE", MYLITE_SQL_PARSE_JSON_REMOVE},
        {"JSON_REPLACE", MYLITE_SQL_PARSE_JSON_REPLACE},
        {"JSON_SET", MYLITE_SQL_PARSE_JSON_SET},
        {"JSON_TYPE", MYLITE_SQL_PARSE_JSON_TYPE},
        {"JSON_UNQUOTE", MYLITE_SQL_PARSE_JSON_UNQUOTE},
        {"JSON_VALUE", MYLITE_SQL_PARSE_JSON_VALUE},
        {"JSON_VALID", MYLITE_SQL_PARSE_JSON_VALID},
        {"BOOL", MYLITE_SQL_PARSE_BOOL},
        {"BOOLEAN", MYLITE_SQL_PARSE_BOOLEAN},
        {"INVISIBLE", MYLITE_SQL_PARSE_INVISIBLE},
        {"VISIBLE", MYLITE_SQL_PARSE_VISIBLE},
        {"SRID", MYLITE_SQL_PARSE_SRID},
        {"INT1", MYLITE_SQL_PARSE_INT1},
        {"INT2", MYLITE_SQL_PARSE_INT2},
        {"INT3", MYLITE_SQL_PARSE_INT3},
        {"INT4", MYLITE_SQL_PARSE_INT4},
        {"INT8", MYLITE_SQL_PARSE_INT8},
        {"SIGNED", MYLITE_SQL_PARSE_SIGNED},
        {"UNSIGNED", MYLITE_SQL_PARSE_UNSIGNED},
        {"NOT", MYLITE_SQL_PARSE_NOT},
        {"NOW", MYLITE_SQL_PARSE_NOW},
        {"IS", MYLITE_SQL_PARSE_IS},
        {"IN", MYLITE_SQL_PARSE_IN},
        {"TRUE", MYLITE_SQL_PARSE_TRUE},
        {"FALSE", MYLITE_SQL_PARSE_FALSE},
        {"FOUND_ROWS", MYLITE_SQL_PARSE_FOUND_ROWS},
        {"UNICODE", MYLITE_SQL_PARSE_UNICODE},
        {"UNKNOWN", MYLITE_SQL_PARSE_UNKNOWN},
        {"NULL", MYLITE_SQL_PARSE_NULL},
        {"DUAL", MYLITE_SQL_PARSE_DUAL},
        {"USER", MYLITE_SQL_PARSE_USER},
        {"UTC", MYLITE_SQL_PARSE_UTC},
        {"VERSION", MYLITE_SQL_PARSE_VERSION},
        {"WEIGHT_STRING", MYLITE_SQL_PARSE_WEIGHT_STRING},
        {"ROW_COUNT", MYLITE_SQL_PARSE_ROW_COUNT},
        {"CUME_DIST", MYLITE_SQL_PARSE_CUME_DIST},
        {"DENSE_RANK", MYLITE_SQL_PARSE_DENSE_RANK},
        {"FIRST_VALUE", MYLITE_SQL_PARSE_FIRST_VALUE},
        {"LAG", MYLITE_SQL_PARSE_LAG},
        {"LAST_VALUE", MYLITE_SQL_PARSE_LAST_VALUE},
        {"LEAD", MYLITE_SQL_PARSE_LEAD},
        {"NTH_VALUE", MYLITE_SQL_PARSE_NTH_VALUE},
        {"NTILE", MYLITE_SQL_PARSE_NTILE},
        {"PERCENT_RANK", MYLITE_SQL_PARSE_PERCENT_RANK},
        {"RANK", MYLITE_SQL_PARSE_RANK},
        {"ROW_NUMBER", MYLITE_SQL_PARSE_ROW_NUMBER},
        {"SEPARATOR", MYLITE_SQL_PARSE_SEPARATOR},
        {"SERIAL", MYLITE_SQL_PARSE_SERIAL},
        {"SHARE", MYLITE_SQL_PARSE_SHARE},
        {"SKIP", MYLITE_SQL_PARSE_SKIP},
        {"SQL_CALC_FOUND_ROWS", MYLITE_SQL_PARSE_SQL_CALC_FOUND_ROWS},
        {"SQL_BIG_RESULT", MYLITE_SQL_PARSE_SQL_BIG_RESULT},
        {"SQL_SMALL_RESULT", MYLITE_SQL_PARSE_SQL_SMALL_RESULT},
        {"STRAIGHT_JOIN", MYLITE_SQL_PARSE_STRAIGHT_JOIN},
        {"SYSTEM_USER", MYLITE_SQL_PARSE_SYSTEM_USER},
    };

    if (previous_token_was_dot) {
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    if (!has_immediate_left_paren && !parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        token_text_is_count_function_name(token)) {
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    if (previous_token_allows_select_noop_modifier(previous_parser_token)) {
        if (token_text_equals(token, "SQL_BUFFER_RESULT")) {
            *out_parser_token = MYLITE_SQL_PARSE_SQL_BUFFER_RESULT;
            return true;
        }
        if (token_text_equals(token, "SQL_NO_CACHE")) {
            *out_parser_token = MYLITE_SQL_PARSE_SQL_NO_CACHE;
            return true;
        }
    }

    for (size_t index = 0U; index < sizeof(keyword_mappings) / sizeof(keyword_mappings[0]);
         ++index) {
        if (token_text_equals(token, keyword_mappings[index].keyword)) {
            *out_parser_token = keyword_mappings[index].parser_token;
            return true;
        }
    }

    if ((token->keyword_flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
        return false;
    }

    *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
    return true;
}

static bool token_text_is_count_function_name(const struct mylite_sql_token *token) {
    return token_text_equals(token, "COUNT");
}

static bool token_text_is_generic_aggregate_window_function_name(
    const struct mylite_sql_token *token
) {
    static const char *const names[] = {
        "JSON_ARRAYAGG",
        "JSON_OBJECTAGG",
        "STDDEV",
        "STDDEV_POP",
        "STDDEV_SAMP",
        "VAR_POP",
        "VAR_SAMP",
        "VARIANCE",
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (token_text_equals(token, names[index])) {
            return true;
        }
    }
    return false;
}

static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token) {
    static const struct mylite_sql_punctuation_mapping mappings[] = {
        {';', MYLITE_SQL_PARSE_SEMICOLON},
        {',', MYLITE_SQL_PARSE_COMMA},
        {'.', MYLITE_SQL_PARSE_DOT},
        {'(', MYLITE_SQL_PARSE_LPAREN},
        {')', MYLITE_SQL_PARSE_RPAREN},
    };

    if (token->length != 1U) {
        return false;
    }

    for (size_t i = 0U; i < sizeof(mappings) / sizeof(mappings[0]); ++i) {
        if (mappings[i].punctuation == token->text[0]) {
            *out_parser_token = mappings[i].parser_token;
            return true;
        }
    }

    return false;
}

static bool map_operator_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    int *out_parser_token
) {
    static const struct mylite_sql_operator_mapping mappings[] = {
        {MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL, MYLITE_SQL_PARSE_NULL_SAFE_EQUAL},
        {MYLITE_SQL_OPERATOR_LESS_EQUAL, MYLITE_SQL_PARSE_LESS_EQUAL},
        {MYLITE_SQL_OPERATOR_GREATER_EQUAL, MYLITE_SQL_PARSE_GREATER_EQUAL},
        {MYLITE_SQL_OPERATOR_NOT_EQUAL, MYLITE_SQL_PARSE_NOT_EQUAL},
        {MYLITE_SQL_OPERATOR_EQUAL, MYLITE_SQL_PARSE_EQUAL},
        {MYLITE_SQL_OPERATOR_LESS, MYLITE_SQL_PARSE_LESS},
        {MYLITE_SQL_OPERATOR_GREATER, MYLITE_SQL_PARSE_GREATER},
        {MYLITE_SQL_OPERATOR_PLUS, MYLITE_SQL_PARSE_PLUS},
        {MYLITE_SQL_OPERATOR_MINUS, MYLITE_SQL_PARSE_MINUS},
        {MYLITE_SQL_OPERATOR_STAR, MYLITE_SQL_PARSE_STAR},
        {MYLITE_SQL_OPERATOR_SLASH, MYLITE_SQL_PARSE_SLASH},
        {MYLITE_SQL_OPERATOR_PERCENT, MYLITE_SQL_PARSE_PERCENT},
        {MYLITE_SQL_OPERATOR_LOGICAL_AND, MYLITE_SQL_PARSE_LOGICAL_AND},
        {MYLITE_SQL_OPERATOR_LEFT_SHIFT, MYLITE_SQL_PARSE_LEFT_SHIFT},
        {MYLITE_SQL_OPERATOR_RIGHT_SHIFT, MYLITE_SQL_PARSE_RIGHT_SHIFT},
        {MYLITE_SQL_OPERATOR_BITWISE_NOT, MYLITE_SQL_PARSE_BITWISE_NOT},
        {MYLITE_SQL_OPERATOR_BITWISE_XOR, MYLITE_SQL_PARSE_BITWISE_XOR},
        {MYLITE_SQL_OPERATOR_BITWISE_AND, MYLITE_SQL_PARSE_BITWISE_AND},
        {MYLITE_SQL_OPERATOR_BITWISE_OR, MYLITE_SQL_PARSE_BITWISE_OR},
        {MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT, MYLITE_SQL_PARSE_JSON_UNQUOTE_EXTRACT_OPERATOR},
        {MYLITE_SQL_OPERATOR_JSON_EXTRACT, MYLITE_SQL_PARSE_JSON_EXTRACT_OPERATOR},
        {MYLITE_SQL_OPERATOR_ASSIGN, MYLITE_SQL_PARSE_ASSIGN},
    };

    for (size_t i = 0U; i < sizeof(mappings) / sizeof(mappings[0]); ++i) {
        if (mappings[i].operator_kind == token->operator_kind) {
            *out_parser_token = mappings[i].parser_token;
            return true;
        }
    }

    switch (token->operator_kind) {
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
        if (parser_sql_mode_has(state, MYLITE_SQL_MODE_PIPES_AS_CONCAT)) {
            *out_parser_token = MYLITE_SQL_PARSE_CONCAT_OPERATOR;
        } else {
            *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_OR;
        }
        return true;
    default:
        return false;
    }

    return false;
}

static struct mylite_sql_ast_node *parser_child_at(struct mylite_sql_ast_node *node, size_t index) {
    struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }
    child = node->first_child;
    for (size_t child_index = 0U; child != NULL && child_index < index; ++child_index) {
        child = child->next_sibling;
    }
    return child;
}

static bool previous_token_allows_select_noop_modifier(int previous_parser_token) {
    switch (previous_parser_token) {
    case MYLITE_SQL_PARSE_SELECT:
    case MYLITE_SQL_PARSE_ALL:
    case MYLITE_SQL_PARSE_DISTINCT:
    case MYLITE_SQL_PARSE_DISTINCTROW:
    case MYLITE_SQL_PARSE_HIGH_PRIORITY:
    case MYLITE_SQL_PARSE_STRAIGHT_JOIN:
    case MYLITE_SQL_PARSE_SQL_SMALL_RESULT:
    case MYLITE_SQL_PARSE_SQL_BIG_RESULT:
    case MYLITE_SQL_PARSE_SQL_BUFFER_RESULT:
    case MYLITE_SQL_PARSE_SQL_NO_CACHE:
    case MYLITE_SQL_PARSE_SQL_CALC_FOUND_ROWS:
        return true;
    default:
        return false;
    }
}

static bool token_text_equals(const struct mylite_sql_token *token, const char *text) {
    size_t length = strlen(text);

    if (token->length != length) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        if (ascii_upper((unsigned char)token->text[index]) != text[index]) {
            return false;
        }
    }

    return true;
}

static char ascii_upper(unsigned char byte) {
    if (byte >= 'a' && byte <= 'z') {
        return (char)(byte - ('a' - 'A'));
    }
    return (char)byte;
}

static bool is_parse_ok(const struct mylite_sql_parser_state *state) {
    if (state != NULL && state->result != NULL && state->result->status == MYLITE_SQL_PARSE_OK) {
        return true;
    }
    return false;
}

static bool parser_sql_mode_has(
    const struct mylite_sql_parser_state *state,
    enum mylite_sql_mode mode
) {
    if (state == NULL) {
        return false;
    }
    return (state->modes & (unsigned int)mode) != 0U;
}

static bool create_table_name_is_no_space_function_identifier(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *table_name,
    const struct mylite_sql_token *left_paren
) {
    const struct mylite_sql_ast_node *last_identifier = NULL;

    if (parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) || table_name == NULL ||
        left_paren == NULL) {
        return false;
    }
    last_identifier = last_identifier_component(table_name);
    if (last_identifier == NULL) {
        return false;
    }
    if (left_paren->offset != last_identifier->span.offset + last_identifier->span.length) {
        return false;
    }
    return span_text_matches_ignore_space_function_name(&last_identifier->span);
}

static void set_state_status(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_parse_status status
) {
    if (!is_parse_ok(state)) {
        return;
    }

    state->result->status = status;
}

static struct mylite_sql_ast_node *make_node(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
) {
    struct mylite_sql_ast_node *node = NULL;

    if (!is_parse_ok(state)) {
        return NULL;
    }

    node = mylite_sql_ast_new_node(&state->result->ast, kind, span);
    if (node == NULL) {
        set_state_status(state, MYLITE_SQL_PARSE_NOMEM);
    }
    return node;
}

static struct mylite_sql_source_span span_from_token(const struct mylite_sql_token *token) {
    if (token == NULL) {
        return (struct mylite_sql_source_span){0};
    }

    return (struct mylite_sql_source_span){
        .text = token->text,
        .length = token->length,
        .offset = token->offset,
        .line = token->line,
        .column = token->column,
    };
}

static struct mylite_sql_source_span span_join(
    struct mylite_sql_source_span left,
    struct mylite_sql_source_span right
) {
    struct mylite_sql_source_span start = left;
    size_t left_end = left.offset + left.length;
    size_t right_end = right.offset + right.length;
    size_t end = left_end > right_end ? left_end : right_end;

    if (left.text == NULL || left.length == 0U) {
        return right;
    }
    if (right.text == NULL || right.length == 0U) {
        return left;
    }

    if (right.offset < left.offset) {
        start = right;
    }

    start.length = end - start.offset;
    return start;
}

static const struct mylite_sql_ast_node *last_identifier_component(
    const struct mylite_sql_ast_node *identifier
) {
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = current->last_child;
    }
    if (current != NULL && current->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return current;
    }
    return NULL;
}

static bool span_text_equals(const struct mylite_sql_source_span *span, const char *text) {
    size_t length = strlen(text);

    if (span == NULL || span->text == NULL || span->length != length) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        if (ascii_upper((unsigned char)span->text[index]) != text[index]) {
            return false;
        }
    }

    return true;
}

static bool span_text_matches_ignore_space_function_name(const struct mylite_sql_source_span *span
) {
    static const char *const function_names[] = {
        "BIT_AND",
        "BIT_OR",
        "BIT_XOR",
        "CAST",
        "CONVERT",
        "COUNT",
        "CURDATE",
        "CURTIME",
        "DATE_ADD",
        "DATE_SUB",
        "GROUP_CONCAT",
        "MAX",
        "MIN",
        "SESSION_USER",
        "SUM",
        "SYSDATE",
        "SYSTEM_USER",
    };

    for (size_t index = 0U; index < sizeof(function_names) / sizeof(function_names[0]); ++index) {
        if (span_text_equals(span, function_names[index])) {
            return true;
        }
    }
    return false;
}

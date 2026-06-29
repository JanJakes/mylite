#include "mylite_parser_placeholders.h"

#include "mylite_parse.h"
#include "mylite_parser_driver.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_token_map.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

struct mylite_sql_parse_error {
    enum mylite_sql_parse_status status;
    int parser_token;
    struct mylite_sql_token token;
};

struct placeholder_row_arithmetic_subject_retry {
    size_t start_index;
    size_t end_index;
    struct mylite_sql_token placeholder_token;
};

struct placeholder_row_arithmetic_subject_retries {
    struct placeholder_row_arithmetic_subject_retry *items;
    size_t count;
    size_t capacity;
};

struct placeholder_ast_node_stack {
    struct mylite_sql_ast_node **items;
    size_t count;
    size_t capacity;
};

struct row_arithmetic_retry_clone_work_item {
    const struct mylite_sql_ast_node *source;
    struct mylite_sql_ast_node *clone;
};

struct row_arithmetic_retry_clone_stack {
    struct row_arithmetic_retry_clone_work_item *items;
    size_t count;
    size_t capacity;
};

enum {
    placeholder_initial_token_capacity = 16,
    placeholder_row_arithmetic_retry_initial_capacity = 4,
    placeholder_ast_node_stack_initial_capacity = 16,
    row_arithmetic_retry_clone_stack_initial_capacity = 8,
    placeholder_create_scan_token_limit = 12,
    create_table_partition_min_token_count = 6,
    create_table_select_min_token_count = 5,
    cte_placeholder_min_token_count = 5,
    alter_table_partition_min_token_count = 5,
    row_bitwise_order_by_min_token_count = 5,
};

enum placeholder_row_bitwise_order_key_end_mode {
    PLACEHOLDER_ROW_BITWISE_ORDER_KEY_END_NORMAL = 0,
    PLACEHOLDER_ROW_BITWISE_ORDER_KEY_END_SKIP_COMMA = 1,
};

struct placeholder_row_bitwise_order_key_end {
    size_t end_index;
    size_t next_index;
    enum placeholder_row_bitwise_order_key_end_mode mode;
    size_t *out_end_index;
    size_t *out_next_index;
};

struct row_constructor_predicate_scan {
    size_t left_right_paren_index;
    size_t right_row_index;
    size_t right_right_paren_index;
};

struct row_constructor_in_predicate_scan {
    size_t left_right_paren_index;
    size_t list_left_paren_index;
    size_t list_right_paren_index;
};

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
static bool scan_can_retry_parenthesized_row_constructors(
    const struct placeholder_statement_scan *scan
);
static enum mylite_sql_parse_status scan_parenthesized_row_arithmetic_predicate_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *out_retries,
    bool *out_can_retry,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_constructor_predicate_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *out_retries,
    bool *out_can_retry
);
static bool placeholder_scan_row_constructor_predicate_is_in_where_context(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_row_constructor_comparison_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    struct row_constructor_predicate_scan *out_predicate_scan
);
static enum mylite_sql_parse_status scan_row_constructor_in_predicate_retries_at(
    const struct placeholder_statement_scan *scan,
    size_t row_index,
    const char *placeholder_text,
    size_t placeholder_text_length,
    struct placeholder_row_arithmetic_subject_retries *out_retries,
    bool *out_can_retry,
    size_t *out_end_index
);
static bool placeholder_scan_row_constructor_in_predicate_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    struct row_constructor_in_predicate_scan *out_predicate_scan
);
static bool placeholder_scan_predicate_row_value_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_right_paren_index
);
static enum mylite_sql_parse_status append_row_constructor_predicate_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    const char *placeholder_text,
    size_t placeholder_text_length,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
);
static bool placeholder_scan_token_is_row_constructor_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum mylite_sql_parse_status scan_row_bitwise_order_key_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_bitwise_order_items(
    const struct placeholder_statement_scan *scan,
    size_t item_start,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_next_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status append_row_bitwise_order_key_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
);
static bool placeholder_scan_row_bitwise_order_key_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_end_index,
    size_t *out_next_index
);
static bool placeholder_scan_token_updates_row_bitwise_order_depth(
    const struct placeholder_statement_scan *scan,
    size_t index,
    int *inout_paren_depth,
    size_t *inout_end_index,
    bool *out_stop
);
static bool placeholder_scan_finish_row_bitwise_order_key(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_bitwise_order_key_end key_end
);
static bool placeholder_scan_token_starts_top_level_order_by(
    const struct placeholder_statement_scan *scan,
    size_t index,
    int paren_depth
);
static bool placeholder_scan_token_stops_row_bitwise_order_key(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_row_bitwise_order_key_can_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_row_bitwise_range_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_row_concat_range_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_range_has_top_level_bitwise_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_range_has_top_level_concat_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_parenthesized_row_arithmetic_predicate_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index,
    bool allow_concat_operator
);
static bool placeholder_scan_can_retry_row_arithmetic_predicate_values(
    const struct placeholder_statement_scan *scan
);
static enum mylite_sql_parse_status scan_row_arithmetic_predicate_retry_at(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_scalar_non_predicate_context_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_scalar_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_direct_row_scalar_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator,
    bool *out_handled
);
static bool placeholder_scan_find_row_scalar_predicate_subject_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    bool allow_concat_operator,
    size_t *out_end_index
);
static enum mylite_sql_parse_status scan_literal_left_string_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status append_row_scalar_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
);
static bool placeholder_scan_token_is_retryable_row_scalar_context_function(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_sys_helper_function(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_top_level_where_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_row_scalar_predicate_subject_has_valid_suffix(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_row_scalar_predicate_subject_uses_truth_placeholder(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_scalar_update_assignment_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_scalar_duplicate_update_assignment_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_scalar_assignment_list_retries(
    const struct placeholder_statement_scan *scan,
    size_t index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool use_literal_placeholder,
    bool allow_concat_operator
);
static bool placeholder_scan_top_level_update_set_index(
    const struct placeholder_statement_scan *scan,
    size_t *out_set_index
);
static bool placeholder_scan_top_level_duplicate_update_index(
    const struct placeholder_statement_scan *scan,
    size_t *out_update_index
);
static bool placeholder_scan_update_target_is_single_table(
    const struct placeholder_statement_scan *scan,
    size_t set_index
);
static bool placeholder_scan_update_assignment_equal_index(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_equal_index
);
static bool placeholder_scan_update_assignment_value_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_end_index,
    size_t *out_next_index
);
static bool placeholder_scan_update_assignment_value_can_retry_as_followup_row_scalar(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_token_is_retryable_update_assignment_row_scalar_function(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_update_assignment_equal(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_stops_update_assignment_list(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static enum mylite_sql_parse_status scan_row_arithmetic_predicate_value_retries(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_arithmetic_comparison_value_retry(
    const struct placeholder_statement_scan *scan,
    size_t comparison_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_arithmetic_between_value_retries(
    const struct placeholder_statement_scan *scan,
    size_t between_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
);
static enum mylite_sql_parse_status scan_row_arithmetic_in_value_retries(
    const struct placeholder_statement_scan *scan,
    size_t in_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_literal_left_string_between_has_descriptor_context(
    const struct placeholder_statement_scan *scan,
    size_t between_index,
    size_t *out_end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_literal_left_string_in_has_descriptor_context(
    const struct placeholder_statement_scan *scan,
    size_t in_index,
    size_t *out_end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_predicate_value_range_has_descriptor_context(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_token_is_in_where_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_predicate_value_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    bool stop_at_comma,
    size_t *out_end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_between_lower_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_and_index
);
static bool placeholder_scan_row_arithmetic_value_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_predicate_value_can_retry_as_row_scalar(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_predicate_value_is_direct_row_scalar_expression(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_predicate_value_is_bare_row_operand(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_row_arithmetic_value_has_row_operand(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_token_is_row_arithmetic_value_identifier_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_row_arithmetic_value_has_operator_in_range(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_row_arithmetic_value_has_operator_at(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    size_t index,
    bool allow_concat_operator
);
static bool placeholder_scan_row_arithmetic_value_has_unsupported_function_call(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static bool placeholder_scan_token_is_predicate_value_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_supported_row_arithmetic_value_function(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_supported_row_scalar_value_function(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_stops_predicate_value(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool stop_at_comma,
    bool allow_concat_operator
);
static bool placeholder_scan_in_list_is_subquery(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
);
static enum mylite_sql_parse_status append_row_arithmetic_predicate_value_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
);
static bool placeholder_scan_token_starts_row_arithmetic_predicate_subject(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool allow_concat_operator
);
static bool placeholder_scan_token_continues_row_arithmetic_predicate_subject(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool allow_concat_operator
);
static bool placeholder_scan_boolean_connective_is_in_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool allow_concat_operator
);
static bool placeholder_scan_parenthesized_row_arithmetic_has_suffix(
    const struct placeholder_statement_scan *scan,
    size_t right_paren_index
);
static bool placeholder_scan_parenthesized_row_arithmetic_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
);
static bool placeholder_scan_parenthesized_row_arithmetic_has_operator_at(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    size_t index,
    bool allow_concat_operator
);
static bool placeholder_scan_token_is_row_concat_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_row_bitwise_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_is_row_arithmetic_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_scan_token_starts_mod_function(
    const struct placeholder_statement_scan *scan,
    size_t index
);
static bool placeholder_row_arithmetic_subject_retries_push(
    struct placeholder_row_arithmetic_subject_retries *retries,
    struct placeholder_row_arithmetic_subject_retry retry
);
static void placeholder_row_arithmetic_subject_retries_deinit(
    struct placeholder_row_arithmetic_subject_retries *retries
);
static enum mylite_sql_parse_status parse_parenthesized_row_arithmetic_predicate_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries,
    struct mylite_sql_parse_result *out_result
);
static enum mylite_sql_parse_status validate_parenthesized_row_arithmetic_retries(
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries
);
static enum mylite_sql_parse_status feed_parenthesized_row_arithmetic_retry_statement_tokens(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries
);
static enum mylite_sql_parse_status feed_parenthesized_row_arithmetic_retry_token(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token
);
static enum mylite_sql_parse_status replace_parenthesized_row_arithmetic_retry_subjects(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries,
    struct mylite_sql_parse_result *out_result
);
static enum mylite_sql_parse_status replace_parenthesized_row_arithmetic_retry_subject(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retry *retry,
    struct mylite_sql_parse_result *out_result
);
static enum mylite_sql_parse_status parse_row_arithmetic_subject_expression_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct mylite_sql_parse_result *out_result,
    struct mylite_sql_ast_node **out_expression
);
static struct mylite_sql_ast_node *clone_row_arithmetic_retry_expression_tree(
    struct mylite_sql_ast *ast,
    const struct mylite_sql_ast_node *node
);
static struct mylite_sql_ast_node *clone_row_arithmetic_retry_expression_node(
    struct mylite_sql_ast *ast,
    const struct mylite_sql_ast_node *node
);
static bool clone_row_arithmetic_retry_expression_children(
    struct mylite_sql_ast *ast,
    struct row_arithmetic_retry_clone_stack *stack,
    const struct row_arithmetic_retry_clone_work_item *item
);
static bool row_arithmetic_retry_clone_stack_push(
    struct row_arithmetic_retry_clone_stack *stack,
    const struct mylite_sql_ast_node *source,
    struct mylite_sql_ast_node *clone
);
static bool row_arithmetic_retry_clone_stack_reserve(struct row_arithmetic_retry_clone_stack *stack
);
static void row_arithmetic_retry_clone_stack_deinit(struct row_arithmetic_retry_clone_stack *stack);
static enum mylite_sql_parse_status feed_row_arithmetic_subject_expression_token(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token
);
static enum mylite_sql_parse_status find_row_arithmetic_placeholder_node(
    struct mylite_sql_ast_node *node,
    const struct mylite_sql_token *placeholder_token,
    struct mylite_sql_ast_node **out_node
);
static bool row_arithmetic_placeholder_node_matches(
    const struct mylite_sql_ast_node *node,
    const struct mylite_sql_token *placeholder_token
);
static bool placeholder_ast_node_stack_push(
    struct placeholder_ast_node_stack *stack,
    struct mylite_sql_ast_node *node
);
static void placeholder_ast_node_stack_deinit(struct placeholder_ast_node_stack *stack);
static void replace_row_arithmetic_placeholder_node(
    struct mylite_sql_ast_node *placeholder,
    const struct mylite_sql_ast_node *expression
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
static bool spatial_reference_system_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
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
static bool logfile_group_placeholder_statement_is_supported(
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
static bool change_replication_filter_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
);
static bool replication_control_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
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
static bool show_engine_mutex_placeholder_statement_is_supported(
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
static void record_parse_error(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parse_error error
);

#include "mylite_parser_placeholders_admin.inc"
#include "mylite_parser_placeholders_ddl.inc"
#include "mylite_parser_placeholders_query.inc"
#include "mylite_parser_placeholders_retry.inc"

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

#include "mylite_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_lexer.h"
#include "mylite_parser_internal.h"
#include "generated/mylite_lemon.h"

typedef enum DmlAssignmentMode {
  DML_ASSIGNMENT_NONE = 0,
  DML_ASSIGNMENT_UPDATE,
  DML_ASSIGNMENT_INSERT_SET,
  DML_ASSIGNMENT_REPLACE_SET,
  DML_ASSIGNMENT_DUPLICATE
} DmlAssignmentMode;

typedef enum ColumnDefinitionTailState {
  COLUMN_DEFINITION_TAIL_READY = 0,
  COLUMN_DEFINITION_TAIL_AFTER_NOT,
  COLUMN_DEFINITION_TAIL_AFTER_PRIMARY,
  COLUMN_DEFINITION_TAIL_AFTER_DEFAULT,
  COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE,
  COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_INTRODUCER,
  COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN,
  COLUMN_DEFINITION_TAIL_AFTER_VALUE_DOT,
  COLUMN_DEFINITION_TAIL_AFTER_COMMENT,
  COLUMN_DEFINITION_TAIL_AFTER_COLLATE,
  COLUMN_DEFINITION_TAIL_AFTER_CHARACTER,
  COLUMN_DEFINITION_TAIL_AFTER_CHARSET,
  COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT,
  COLUMN_DEFINITION_TAIL_AFTER_STORAGE,
  COLUMN_DEFINITION_TAIL_AFTER_SRID,
  COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE,
  COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE_EQUALS,
  COLUMN_DEFINITION_TAIL_AFTER_AFTER,
  COLUMN_DEFINITION_TAIL_AFTER_ON,
  COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE,
  COLUMN_DEFINITION_TAIL_AFTER_REFERENCES,
  COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE,
  COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE_DOT,
  COLUMN_DEFINITION_TAIL_REFERENCES_LIST_NEED_NAME,
  COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_NAME,
  COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_DOT,
  COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST,
  COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_MATCH,
  COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_ON,
  COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_ON_ACTION,
  COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_SET,
  COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_NO,
  COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT,
  COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME,
  COLUMN_DEFINITION_TAIL_AFTER_SERIAL_ATTRIBUTE,
  COLUMN_DEFINITION_TAIL_AFTER_SERIAL_DEFAULT,
  COLUMN_DEFINITION_TAIL_AFTER_GENERATED,
  COLUMN_DEFINITION_TAIL_AFTER_ALWAYS,
  COLUMN_DEFINITION_TAIL_AFTER_AS
} ColumnDefinitionTailState;

enum {
  COLUMN_DEFINITION_FLAG_GENERATED_EXPRESSION = 1 << 0,
  COLUMN_DEFINITION_FLAG_GENERATED_STORAGE = 1 << 1,
  COLUMN_DEFINITION_FLAG_COLLATE = 1 << 2
};

enum {
  REFERENCE_TAIL_FLAG_MATCH = 1 << 8,
  REFERENCE_TAIL_FLAG_ON_UPDATE = 1 << 9,
  REFERENCE_TAIL_FLAG_ON_DELETE = 1 << 10
};

enum {
  QUERY_EXPRESSION_ALLOW_BARE_DEFAULT = 1 << 0
};

typedef enum ColumnTypeParameterKind {
  COLUMN_TYPE_PARAMETER_NONE = 0,
  COLUMN_TYPE_PARAMETER_NUMERIC_ONE,
  COLUMN_TYPE_PARAMETER_NUMERIC_ONE_OR_TWO,
  COLUMN_TYPE_PARAMETER_NUMERIC_TWO,
  COLUMN_TYPE_PARAMETER_YEAR,
  COLUMN_TYPE_PARAMETER_STRING_LIST
} ColumnTypeParameterKind;

typedef enum ColumnTypeState {
  COLUMN_TYPE_STATE_COMPLETE = 0,
  COLUMN_TYPE_STATE_CHAR,
  COLUMN_TYPE_STATE_NCHAR,
  COLUMN_TYPE_STATE_NATIONAL,
  COLUMN_TYPE_STATE_NATIONAL_CHAR,
  COLUMN_TYPE_STATE_LONG,
  COLUMN_TYPE_STATE_LONG_CHAR
} ColumnTypeState;

typedef enum ColumnTypeCharsetModifierState {
  COLUMN_TYPE_CHARSET_AVAILABLE = 0,
  COLUMN_TYPE_CHARSET_BINARY_PREFIX,
  COLUMN_TYPE_CHARSET_ASCII_OR_UNICODE,
  COLUMN_TYPE_CHARSET_CHARACTER_SET,
  COLUMN_TYPE_CHARSET_DONE
} ColumnTypeCharsetModifierState;

enum {
  COLUMN_TYPE_MODIFIER_NUMERIC = 1 << 0,
  COLUMN_TYPE_MODIFIER_CHARSET = 1 << 1,
  COLUMN_TYPE_MODIFIER_NATIONAL_BINARY = 1 << 2
};

typedef enum CreateTableOptionValueKind {
  CREATE_TABLE_OPTION_VALUE_NONE = 0,
  CREATE_TABLE_OPTION_VALUE_DECIMAL_NUMBER,
  CREATE_TABLE_OPTION_VALUE_SIZE_NUMBER,
  CREATE_TABLE_OPTION_VALUE_DEFAULT_BOOLEAN,
  CREATE_TABLE_OPTION_VALUE_STATS_SAMPLE_PAGES,
  CREATE_TABLE_OPTION_VALUE_STRING,
  CREATE_TABLE_OPTION_VALUE_ENCRYPTION,
  CREATE_TABLE_OPTION_VALUE_NAME,
  CREATE_TABLE_OPTION_VALUE_CHARSET,
  CREATE_TABLE_OPTION_VALUE_INSERT_METHOD,
  CREATE_TABLE_OPTION_VALUE_ROW_FORMAT,
  CREATE_TABLE_OPTION_VALUE_STORAGE
} CreateTableOptionValueKind;

/*
 * Expression validators keep frames on the C stack in several parser actions.
 * 512 levels keeps long query expressions below common thread stack limits
 * while still allowing far deeper nesting than ordinary MySQL input uses.
 */
#define MYLITE_EXPRESSION_STACK_LIMIT 512

typedef struct MyliteExpressionFrame {
  int active;
  int allow_empty;
  int allow_comma;
  int validate_adjacent;
  int started;
  int previous_top_token_id;
  int previous_was_operator;
  int flags;
  int default_identifier_parts;
  int default_identifier_after_dot;
  MyliteToken previous_top_token;
} MyliteExpressionFrame;

typedef struct MyliteExpressionStack {
  MyliteExpressionFrame frames[MYLITE_EXPRESSION_STACK_LIMIT];
} MyliteExpressionStack;

static void result_init(MyliteParseResult *result);
static MyliteParseStatus parse_sql(const char *sql, size_t length,
                                   int permissive,
                                   MyliteParseResult *result);
static int finish_unclosed_set_fragment(MyliteParseContext *ctx);
static int is_unclosed_set_assignment_fragment(const char *sql, size_t length);
static size_t skip_leading_space(const char *sql, size_t length);
static int ascii_alpha_equal(char actual, char expected);
static int is_word_boundary(char ch);
static int token_contains_identifier_letter(MyliteToken token);
static int token_is_invalid_identifier_atom(MyliteToken token,
                                            int allow_string_literal);
static int token_starts_numeric_literal(MyliteToken token);
static int token_is_hex_literal(MyliteToken token, size_t offset);
static int token_is_binary_literal(MyliteToken token, size_t offset);
static int token_is_decimal_literal(MyliteToken token, size_t offset);
static int ascii_is_digit(char ch);
static int ascii_is_hex_digit(char ch);
static void set_parser_error(MyliteParseContext *ctx, const MyliteToken *token,
                             const char *message);
static void format_near_token(MyliteParseContext *ctx, int token_id,
                              const MyliteToken *token);
static void validate_select_statement_from(MyliteParseContext *ctx,
                                           int use_start, MyliteToken start);
static void validate_select_list_tails_from(MyliteParseContext *ctx,
                                            int use_start, MyliteToken start,
                                            int stop_at_insert_duplicate);
static void validate_table_statement_from(MyliteParseContext *ctx,
                                          MyliteToken start,
                                          int parenthesized_boundary);
static int select_clause_requires_by(int token_id);
static int select_clause_requires_operand(int token_id);
static int select_from_starts_nth_modifier(MyliteParseContext *ctx,
                                           MyliteToken from_token);
static int select_from_follows_nth_value_call(MyliteParseContext *ctx,
                                              MyliteToken from_token);
static int select_token_followed_by_nulls(MyliteParseContext *ctx,
                                          MyliteToken token);
static int select_expression_clause_boundary(int token_id, MyliteToken token);
static int select_alias_token(int token_id, MyliteToken token);
static int select_null_treatment_function_token(int token_id,
                                                MyliteToken token);
static int select_operand_boundary(int token_id);
static int select_modifier_flag(int token_id);
static int select_order_direction_boundary(int token_id);
static int select_rollup_boundary(int token_id, MyliteToken token);
static int select_set_operator(int token_id);
static int select_set_option(int token_id);
static int select_set_operand_start(int token_id);
static void validate_parenthesized_query_body_from(MyliteParseContext *ctx,
                                                   MyliteToken start);
static void validate_query_set_operand_after_operator_from(
    MyliteParseContext *ctx, MyliteToken start, const char *message);
static int parenthesized_query_order_boundary(int token_id);
static int query_expression_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *depth,
    int *previous_top_token_id, MyliteToken *previous_top_token,
    int *previous_was_operator, MyliteExpressionStack *stack,
    const char *message);
static int query_expression_token_with_flags(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *depth,
    int *previous_top_token_id, MyliteToken *previous_top_token,
    int *previous_was_operator, MyliteExpressionStack *stack, int flags,
    const char *message);
static int query_expression_depth_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *depth,
    MyliteExpressionStack *stack, const char *message);
static int query_expression_stack_active(MyliteExpressionStack *stack,
                                         int depth);
static int query_expression_stack_rejects_comma(MyliteExpressionStack *stack,
                                                int depth);
static int query_expression_stack_open_from_previous(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int depth,
    int previous_top_token_id, MyliteToken previous_top_token,
    int previous_was_operator, int token_id, MyliteToken token, int flags,
    const char *message);
static int query_expression_stack_open(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int current_depth,
    int new_depth, int token_id, MyliteToken token, const char *message);
static void query_expression_stack_open_list(MyliteExpressionStack *stack,
                                             int depth, MyliteToken token,
                                             int allow_empty, int flags);
static int query_expression_stack_token(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int depth,
    int token_id, MyliteToken token, const char *message);
static int query_expression_stack_close(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int depth,
    MyliteToken token, const char *message);
static void query_expression_stack_note_terminal(
    MyliteExpressionStack *stack, int depth, int token_id, MyliteToken token);
static int query_expression_group_allows_empty(int previous_top_token_id,
                                               MyliteToken previous_top_token,
                                               int previous_was_operator,
                                               int token_id);
static int query_expression_group_validates_adjacent(int allow_empty,
                                                     int token_id);
static int query_expression_group_disables_adjacent(int token_id);
static int query_expression_malformed_operator_sequence(
    int previous_top_token_id, MyliteToken previous_top_token, int token_id);
static int query_expression_stack_default_token(
    MyliteParseContext *ctx, MyliteExpressionFrame *frame, int token_id,
    MyliteToken token, const char *message);
static int query_expression_default_identifier_token(int token_id,
                                                     MyliteToken token);
static void validate_expression_tail_from(MyliteParseContext *ctx,
                                          MyliteToken start,
                                          int boundary_token_id,
                                          int allow_double_at_assignment,
                                          int flags,
                                          const char *message);
static int set_user_variable_value_is_invalid(MyliteParseContext *ctx,
                                              MyliteToken start);
static int expression_start_follows_user_variable_assignment(
    MyliteParseContext *ctx, MyliteToken start);
static int set_assignment_target_is_user_variable(int token_id,
                                                  MyliteToken token);
static int expression_start_follows_double_at_assignment(
    MyliteParseContext *ctx, MyliteToken start);
static void validate_with_statement_from(MyliteParseContext *ctx,
                                         MyliteToken start,
                                         const char *message);
static void validate_with_cte_body_from(MyliteParseContext *ctx,
                                        MyliteToken start);
static void validate_query_body_after_optional_as_from(MyliteParseContext *ctx,
                                                       MyliteToken start);
static void validate_query_body_from(MyliteParseContext *ctx, int token_id,
                                     MyliteToken token);
static int with_cte_name_token(int token_id);
static int with_query_body_start(int token_id);
static int select_window_name_token(int token_id, MyliteToken token);
static int select_lock_table_ref_start(int token_id, MyliteToken token);
static int select_lock_table_ref_part(int token_id);
static int select_into_output_option_start(int token_id);
static int select_into_output_follow_token(int token_id, MyliteToken token);
static int select_into_variable_target_token(int token_id, MyliteToken token);
static int select_into_variable_at_target_token(int token_id,
                                                MyliteToken token);
static int select_outfile_field_option_start(int token_id);
static int select_outfile_line_option_start(int token_id);
static int select_index_hint_name_token(int token_id);
static int select_index_hint_type(int token_id);
static int select_partition_name_token(int token_id);
static int select_tablesample_boundary(int token_id, MyliteToken token);
static int select_tablesample_percentage_token(int token_id);
static int select_charset_name_token(int token_id, MyliteToken token);
static int select_limit_option_token(int token_id);
static int select_string_literal_token(int token_id);
static int do_clause_boundary(int token_id);
static int do_expression_operator(int token_id, MyliteToken token);
static int do_expression_value_start(int token_id, MyliteToken token);
static int do_expression_value_terminal(int token_id, MyliteToken token);
static int do_expression_interval_unit_token(int token_id, MyliteToken token);
static int do_expression_hex_or_bit_literal_token(MyliteToken token);
static int do_expression_allows_adjacent(int previous_id,
                                         MyliteToken previous, int current_id,
                                         MyliteToken current);
static int do_expression_conditional_comment_operator_between(
    MyliteToken previous, MyliteToken current);
static int kill_at_sign_target_token(int token_id);
static int kill_target_allows_call(int token_id);
static int kill_target_token(int token_id);
static int reset_persist_name_part_token(int token_id, MyliteToken token);
static int create_table_query_body_start(int token_id);
static int create_table_query_expression_start(int token_id);
static int create_table_tail_option_start_token(int token_id);
static void validate_create_table_tail_options(MyliteParseContext *ctx,
                                               MyliteToken start);
static void validate_alter_table_option_values(MyliteParseContext *ctx,
                                               MyliteToken start);
static int validate_alter_table_option_value(MyliteParseContext *ctx,
                                             MyliteLexer *lexer,
                                             MyliteToken option,
                                             int value_kind);
static int create_table_tail_option_value_token(
    int kind, int token_id, MyliteToken token);
static int create_table_tail_option_value_allows_equals(int kind);
static int create_table_tail_option_number_token(int token_id);
static int create_table_tail_option_decimal_number_token(int token_id,
                                                         MyliteToken token);
static int create_table_tail_option_size_number_token(int token_id,
                                                      MyliteToken token);
static int create_table_tail_option_default_boolean_token(int token_id);
static int create_table_tail_option_stats_sample_pages_token(
    int token_id, MyliteToken token);
static int create_table_tail_option_string_token(int token_id,
                                                 MyliteToken token);
static int create_table_tail_option_insert_method_token(int token_id);
static int create_table_tail_option_row_format_token(MyliteToken token);
static int create_table_tail_option_storage_token(int token_id,
                                                  MyliteToken token);
static int token_is_unsigned_decimal_literal(MyliteToken token);
static int token_is_unsigned_size_literal(MyliteToken token);
static int token_is_plain_unsigned_integer(MyliteToken token,
                                           unsigned long *value);
static int token_is_quoted_hex_literal(MyliteToken token);
static int token_is_quoted_hex_or_bit_literal(MyliteToken token);
static int token_has_leading_sign(MyliteToken token);
static int create_table_column_name_needs_type_check(int token_id,
                                                     MyliteToken token);
static int create_table_column_type_start(int token_id, MyliteToken token);
static ColumnTypeParameterKind column_type_parameter_kind(int token_id,
                                                          MyliteToken token);
static ColumnTypeState column_type_state_after_start(int token_id,
                                                     MyliteToken token);
static int column_type_modifier_mask_after_start(int token_id,
                                                 MyliteToken token);
static int column_type_incomplete(ColumnTypeState state);
static int consume_column_type_tail_token_if_pending(
    MyliteParseContext *ctx, int token_id, MyliteToken token,
    ColumnTypeState *state, int *modifier_mask, int *parameter_pending,
    int *parameter_required, int *parameter_forbidden,
    ColumnTypeParameterKind *parameter_kind,
    ColumnTypeCharsetModifierState *charset_state, const char *message,
    int *consumed);
static int column_type_numeric_modifier_token(int token_id, MyliteToken token);
static int column_type_simple_charset_modifier_token(int token_id,
                                                     MyliteToken token);
static int column_type_ascii_or_unicode_modifier_token(int token_id,
                                                       MyliteToken token);
static int column_type_charset_introducer_token(int token_id,
                                                MyliteToken token);
static int column_type_varying_token(int token_id, MyliteToken token);
static int column_type_character_token(int token_id, MyliteToken token);
static int column_type_varchar_token(int token_id, MyliteToken token);
static int column_type_varbinary_token(int token_id, MyliteToken token);
static int column_definition_serial_attribute_token(int token_id,
                                                    MyliteToken token);
static int column_definition_engine_attribute_token(int token_id);
static int column_definition_json_attribute_token(int token_id);
static int column_type_integer_family_token(int token_id, MyliteToken token);
static int column_type_real_family_token(int token_id, MyliteToken token);
static int column_type_numeric_family_token(int token_id, MyliteToken token);
static int column_type_character_family_token(int token_id,
                                              MyliteToken token);
static int column_type_national_family_token(int token_id,
                                             MyliteToken token);
static int column_type_text_family_token(int token_id, MyliteToken token);
static int column_type_blob_family_parameter_forbidden(int token_id,
                                                       MyliteToken token);
static int column_type_numeric_parameter_token(int token_id,
                                               MyliteToken token);
static int column_type_integer_parameter_token(int token_id,
                                               MyliteToken token);
static int column_type_year_length_token(int token_id, MyliteToken token);
static int column_type_string_parameter_token(int token_id,
                                              MyliteToken token);
static int validate_column_type_parameter_list(
    MyliteParseContext *ctx, MyliteLexer *lexer, MyliteToken start,
    ColumnTypeParameterKind kind, const char *message);
static int consume_column_type_parameter_list_if_pending(
    MyliteParseContext *ctx, MyliteLexer *lexer, int token_id,
    MyliteToken token, int *pending, int *required,
    ColumnTypeParameterKind kind, const char *message, int *consumed);
static int consume_column_precision_modifier_if_pending(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *pending,
    int *parameter_pending, int *parameter_required,
    ColumnTypeParameterKind *parameter_kind, const char *message,
    int *consumed);
static int column_definition_tail_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token,
    ColumnDefinitionTailState *state, int *depth, int *check_pending,
    MyliteToken *pending_token, int *flags, int allow_position,
    const char *message);
static int column_definition_tail_complete(ColumnDefinitionTailState state);
static int column_definition_tail_wants_boundary_token(
    ColumnDefinitionTailState state, int token_id);
static int column_definition_tail_parenthesized_expression(
    ColumnDefinitionTailState state);
static void column_definition_tail_finish_parenthesized_expression(
    ColumnDefinitionTailState *state, int *flags);
static int column_definition_value_token(int token_id, MyliteToken token);
static int column_definition_default_introducer_token(int token_id,
                                                     MyliteToken token);
static int column_definition_on_update_value_token(MyliteToken token);
static int column_definition_temporal_function_token(MyliteToken token);
static int validate_column_temporal_precision_list(MyliteParseContext *ctx,
                                                   MyliteLexer *lexer,
                                                   MyliteToken start,
                                                   const char *message);
static int column_definition_charset_name_token(int token_id,
                                                MyliteToken token);
static int column_definition_attribute_start(int token_id, MyliteToken token,
                                             int allow_position);
static int column_definition_type_modifier(int token_id, MyliteToken token);
static int column_type_allows_precision_modifier(int token_id,
                                                 MyliteToken token);
static int column_type_precision_modifier_token(int token_id,
                                                MyliteToken token);
static int column_type_requires_parameter(int token_id, MyliteToken token,
                                          int long_prefix);
static int column_type_forbids_parameter(int token_id, MyliteToken token,
                                         int long_prefix);
static int foreign_key_match_option(int token_id, MyliteToken token);
static int foreign_key_reference_action_token(int token_id);
static int validate_parenthesized_identifier_list(MyliteParseContext *ctx,
                                                  MyliteLexer *lexer,
                                                  MyliteToken start,
                                                  const char *message);
static int validate_parenthesized_expression_body(MyliteParseContext *ctx,
                                                  MyliteLexer *lexer,
                                                  MyliteToken start,
                                                  const char *message);
static int validate_create_table_index_key_list(MyliteParseContext *ctx,
                                                MyliteLexer *lexer,
                                                MyliteToken start);
static int create_index_prefix_length_token(int token_id);
static int create_index_option_number_token(int token_id, MyliteToken token);
static int index_using_type_token(MyliteToken token);
static int alter_table_add_index_marker(int token_id);
static int alter_table_add_non_index_marker(int token_id);
static void validate_alter_table_expression_tails(MyliteParseContext *ctx,
                                                  MyliteToken start);
static void validate_alter_table_order_by_from(MyliteParseContext *ctx,
                                               MyliteToken start);
static int alter_table_partition_method_token(int token_id, MyliteToken token);
static int alter_table_parenthesized_group_empty(MyliteParseContext *ctx,
                                                 MyliteToken start);
static int event_interval_unit_token(MyliteToken token);
static int event_schedule_boundary(int token_id);
static int event_schedule_option_start(MyliteToken token);
static void validate_event_body_statement(MyliteParseContext *ctx,
                                          MyliteToken start);
static void validate_trigger_body_statement(MyliteParseContext *ctx,
                                            MyliteToken start);
static void validate_embedded_statement_body_from(MyliteParseContext *ctx,
                                                  int token_id,
                                                  MyliteToken token);
static void validate_select_list_tail_from(MyliteParseContext *ctx,
                                           MyliteToken start,
                                           int parenthesized_boundary,
                                           int validate_nested,
                                           int stop_at_insert_duplicate);
static void validate_routine_statement_body_from(MyliteParseContext *ctx,
                                                 int token_id,
                                                 MyliteToken token);
static void validate_compound_statement_body_from(MyliteParseContext *ctx,
                                                  MyliteToken start);
static void validate_return_statement_from(MyliteParseContext *ctx,
                                           MyliteToken start);
static void validate_call_statement_from(MyliteParseContext *ctx,
                                         MyliteToken start);
static void validate_signal_statement_from(MyliteParseContext *ctx,
                                           MyliteToken start);
static void validate_get_diagnostics_statement_from(MyliteParseContext *ctx,
                                                    MyliteToken start);
static void validate_cursor_statement_from(MyliteParseContext *ctx,
                                           int token_id, MyliteToken start);
static void validate_label_control_statement_from(MyliteParseContext *ctx,
                                                  MyliteToken start);
static void validate_flow_control_statement_from(MyliteParseContext *ctx,
                                                 int token_id,
                                                 MyliteToken token);
static void validate_control_condition_from(MyliteParseContext *ctx,
                                            MyliteToken start,
                                            int boundary_token_id,
                                            const char *message);
static void validate_embedded_set_statement_from(MyliteParseContext *ctx,
                                                 MyliteToken start);
static int validate_embedded_set_value(MyliteParseContext *ctx,
                                       MyliteLexer *lexer, int token_id,
                                       MyliteToken token);
static int routine_body_statement_start_token(int token_id);
static int routine_direct_query_body_start_token(int token_id);
static int routine_compound_statement_start_token(int token_id);
static int routine_end_suffix_token(int token_id);
static int diagnostics_item_name_token(int token_id);
static int routine_characteristic_token(MyliteParseContext *ctx,
                                        MyliteLexer *lexer, int *token_id,
                                        MyliteToken *token);
static int token_is_statement_terminator(int token_id, MyliteToken token);
static int token_is_plus(MyliteToken token);
static int dml_assignment_boundary(int mode, int token_id);
static int dml_assignment_operator(int token_id);
static int dml_assignment_target_token(int token_id);
static int dml_assignment_value_allows_function(int token_id,
                                                MyliteToken token);
static int parenthesized_query_start_follows(MyliteParseContext *ctx,
                                             MyliteToken token);
static int insert_duplicate_clause_follows(MyliteParseContext *ctx,
                                           MyliteToken token);
static int view_query_order_boundary(int token_id);
static int dml_query_order_boundary(int token_id);
static int dml_clause_operand_boundary(int token_id);
static int dml_limit_option_token(int token_id);
static int dml_literal_token(int token_id, MyliteToken token);
static int set_statement_previous_value_terminal(int token_id,
                                                 MyliteToken token);
static int dml_row_alias_token(int token_id);
static int dml_values_unclosed_string_fragment(int token_id,
                                               MyliteToken token);
static int grant_object_start_token(int token_id, MyliteToken token,
                                    int proxy_grant);
static int token_opens_nested_expression(int token_id);
static int token_closes_nested_expression(int token_id);
static int token_ascii_equal(MyliteToken token, const char *expected);

MyliteParseStatus mylite_parse_sql(const char *sql, size_t length,
                                   MyliteParseResult *result) {
  return parse_sql(sql, length, 0, result);
}

MyliteParseStatus mylite_parse_sql_permissive(const char *sql, size_t length,
                                              MyliteParseResult *result) {
  return parse_sql(sql, length, 1, result);
}

const char *mylite_statement_kind_name(MyliteStatementKind kind) {
  static const char *const names[] = {
      "empty",
      "select",
      "insert",
      "replace",
      "update",
      "delete",
      "ddl",
      "transaction",
      "prepared",
      "show",
      "utility",
      "admin",
      "stored_program",
      "replication",
      "permissive",
  };

  if (kind >= MYLITE_STATEMENT_KIND_COUNT) {
    return "unknown";
  }

  return names[kind];
}

static MyliteParseStatus parse_sql(const char *sql, size_t length,
                                   int permissive,
                                   MyliteParseResult *result) {
  MyliteParseResult local_result;
  MyliteParseContext ctx;
  MyliteLexer lexer;
  MyliteToken token;
  void *parser;
  int token_id;
  int last_token_id = 0;

  if (result == NULL) {
    result = &local_result;
  }
  result_init(result);

  ctx.sql = sql;
  ctx.length = length;
  ctx.permissive = permissive;
  ctx.permissive_fallbacks = 0;
  ctx.accepted = 0;
  ctx.failed = 0;
  ctx.result = result;

  parser = MyLiteLemonAlloc(malloc);
  if (parser == NULL) {
    snprintf(result->error_message, sizeof(result->error_message),
             "failed to allocate parser");
    return MYLITE_PARSE_ERROR;
  }

  mylite_lexer_init(&lexer, sql, length, result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    result->token_count++;
    MyLiteLemon(parser, token_id, token, &ctx);
    last_token_id = token_id;
    if (ctx.failed) {
      int recovered = finish_unclosed_set_fragment(&ctx);
      MyLiteLemonFree(parser, free);
      if (recovered) {
        result->permissive_fallbacks = ctx.permissive_fallbacks;
        return MYLITE_PARSE_OK;
      }
      return MYLITE_PARSE_ERROR;
    }
  }

  if (token_id < 0) {
    MyLiteLemonFree(parser, free);
    return MYLITE_PARSE_ERROR;
  }

  token.start = sql + length;
  token.length = 0;
  token.offset = length;
  token.line = lexer.line;
  token.column = lexer.column;
  if (result->token_count > 0 && last_token_id != ML_SEMI) {
    MyLiteLemon(parser, ML_SEMI, token, &ctx);
  }
  MyLiteLemon(parser, 0, token, &ctx);
  MyLiteLemonFree(parser, free);

  if (ctx.failed || !ctx.accepted) {
    if (finish_unclosed_set_fragment(&ctx)) {
      result->permissive_fallbacks = ctx.permissive_fallbacks;
      return MYLITE_PARSE_OK;
    }

    if (result->error_message[0] == '\0') {
      set_parser_error(&ctx, &token, "unexpected end of input");
    }
    return MYLITE_PARSE_ERROR;
  }

  result->permissive_fallbacks = ctx.permissive_fallbacks;

  return MYLITE_PARSE_OK;
}

static int finish_unclosed_set_fragment(MyliteParseContext *ctx) {
  MyliteParseResult *result = ctx->result;

  if (!is_unclosed_set_assignment_fragment(ctx->sql, ctx->length)) {
    return 0;
  }

  ctx->failed = 0;
  ctx->accepted = 1;
  if (ctx->permissive) {
    ctx->permissive_fallbacks++;
  }

  result->statement_count++;
  result->statement_kind_counts[MYLITE_STATEMENT_UTILITY]++;
  result->error_offset = 0;
  result->error_line = 1;
  result->error_column = 1;
  result->error_message[0] = '\0';

  return 1;
}

static int is_unclosed_set_assignment_fragment(const char *sql, size_t length) {
  size_t i = skip_leading_space(sql, length);
  int has_assignment = 0;
  int paren_depth = 0;

  if (i + 3 > length || !ascii_alpha_equal(sql[i], 's') ||
      !ascii_alpha_equal(sql[i + 1], 'e') ||
      !ascii_alpha_equal(sql[i + 2], 't')) {
    return 0;
  }

  i += 3;
  if (i < length && !is_word_boundary(sql[i])) {
    return 0;
  }

  while (i < length) {
    char ch = sql[i];

    if (ch == '\'' || ch == '"' || ch == '`') {
      char quote = ch;
      i++;
      while (i < length) {
        if (sql[i] == '\\' && quote != '`' && i + 1 < length) {
          i += 2;
          continue;
        }
        if (sql[i++] == quote) {
          break;
        }
      }
      continue;
    }

    if (ch == '-' && i + 2 < length && sql[i + 1] == '-' &&
        is_word_boundary(sql[i + 2])) {
      i += 2;
      while (i < length && sql[i] != '\n') {
        i++;
      }
      continue;
    }

    if (ch == '/' && i + 1 < length && sql[i + 1] == '*') {
      i += 2;
      while (i + 1 < length && !(sql[i] == '*' && sql[i + 1] == '/')) {
        i++;
      }
      if (i + 1 < length) {
        i += 2;
      }
      continue;
    }

    if (ch == '=') {
      has_assignment = 1;
    } else if (ch == ':' && i + 1 < length && sql[i + 1] == '=') {
      has_assignment = 1;
      i++;
    } else if (ch == '(') {
      paren_depth++;
    } else if (ch == ')' && paren_depth > 0) {
      paren_depth--;
    }

    i++;
  }

  return has_assignment && paren_depth > 0;
}

static size_t skip_leading_space(const char *sql, size_t length) {
  size_t i = 0;

  while (i < length) {
    char ch = sql[i];
    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\f') {
      break;
    }
    i++;
  }

  return i;
}

static int ascii_alpha_equal(char actual, char expected) {
  if (actual >= 'A' && actual <= 'Z') {
    actual = (char)(actual - 'A' + 'a');
  }
  return actual == expected;
}

static int is_word_boundary(char ch) {
  return ch == '\0' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
         ch == '\f' || ch == '(' || ch == '=' || ch == ':' || ch == ';';
}

static int token_contains_identifier_letter(MyliteToken token) {
  size_t i;

  for (i = 0; i < token.length; i++) {
    unsigned char ch = (unsigned char) token.start[i];
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        ch == '_' || ch == '$') {
      return 1;
    }
  }

  return 0;
}

void mylite_parser_accept(MyliteParseContext *ctx) {
  ctx->accepted = 1;
}

void mylite_parser_failure(MyliteParseContext *ctx) {
  ctx->failed = 1;
}

void mylite_parser_syntax_error(MyliteParseContext *ctx, int token_id,
                                MyliteToken token) {
  if (ctx->failed) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, token_id, &token);
}

void mylite_parser_record_statement(MyliteParseContext *ctx,
                                    MyliteStatementKind kind) {
  if (kind >= MYLITE_STATEMENT_KIND_COUNT) {
    kind = MYLITE_STATEMENT_UTILITY;
  }

  ctx->result->statement_count++;
  ctx->result->statement_kind_counts[kind]++;
}

void mylite_parser_record_empty_statement(MyliteParseContext *ctx) {
  ctx->result->empty_statement_count++;
  ctx->result->statement_kind_counts[MYLITE_STATEMENT_EMPTY]++;
}

void mylite_parser_validate_select_statement(MyliteParseContext *ctx) {
  MyliteToken start = {0};
  validate_select_statement_from(ctx, 0, start);
  if (!ctx->failed) {
    validate_select_list_tails_from(ctx, 0, start, 0);
  }
}

void mylite_parser_validate_select_statement_from(MyliteParseContext *ctx,
                                                  MyliteToken start) {
  validate_select_statement_from(ctx, 1, start);
  if (!ctx->failed) {
    validate_select_list_tails_from(ctx, 1, start, 0);
  }
}

void mylite_parser_validate_table_statement_from(MyliteParseContext *ctx,
                                                 MyliteToken start) {
  validate_table_statement_from(ctx, start, 0);
}

static void validate_select_statement_from(MyliteParseContext *ctx,
                                           int use_start, MyliteToken start) {
  enum {
    SELECT_MODIFIER_ALL = 1 << 0,
    SELECT_MODIFIER_DISTINCT = 1 << 1
  };
  enum {
    SELECT_LOCK_NONE,
    SELECT_LOCK_AFTER_LOCK,
    SELECT_LOCK_AFTER_LOCK_IN,
    SELECT_LOCK_AFTER_LOCK_IN_SHARE,
    SELECT_LOCK_AFTER_FOR,
    SELECT_LOCK_AFTER_STRENGTH,
    SELECT_LOCK_AFTER_OF,
    SELECT_LOCK_AFTER_TABLE,
    SELECT_LOCK_AFTER_DOT,
    SELECT_LOCK_AFTER_COMMA,
    SELECT_LOCK_AFTER_SKIP,
    SELECT_LOCK_COMPLETE
  };
  enum {
    SELECT_INTO_NONE,
    SELECT_INTO_AFTER_INTO,
    SELECT_INTO_VAR_READY,
    SELECT_INTO_AFTER_VAR_COMMA,
    SELECT_INTO_AFTER_VAR_AT,
    SELECT_INTO_AFTER_OUTFILE,
    SELECT_INTO_AFTER_DUMPFILE,
    SELECT_INTO_OUTFILE_READY,
    SELECT_INTO_DUMPFILE_READY,
    SELECT_INTO_AFTER_CHARACTER,
    SELECT_INTO_AFTER_CHARSET,
    SELECT_INTO_AFTER_FIELDS,
    SELECT_INTO_AFTER_FIELD_OPTION,
    SELECT_INTO_AFTER_FIELD_BY,
    SELECT_INTO_AFTER_OPTIONALLY,
    SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED,
    SELECT_INTO_AFTER_LINES,
    SELECT_INTO_AFTER_LINE_OPTION,
    SELECT_INTO_AFTER_LINE_BY
  };
  enum {
    SELECT_LIMIT_NONE,
    SELECT_LIMIT_AFTER_LIMIT,
    SELECT_LIMIT_AFTER_FIRST_VALUE,
    SELECT_LIMIT_AFTER_COMMA,
    SELECT_LIMIT_AFTER_OFFSET,
    SELECT_LIMIT_AFTER_FINAL_VALUE
  };
  enum {
    SELECT_WINDOW_NONE,
    SELECT_WINDOW_AFTER_WINDOW,
    SELECT_WINDOW_AFTER_NAME,
    SELECT_WINDOW_AFTER_AS,
    SELECT_WINDOW_AFTER_SPEC
  };
  enum {
    SELECT_ROLLUP_NONE,
    SELECT_ROLLUP_AFTER_WITH,
    SELECT_ROLLUP_COMPLETE
  };
  enum {
    SELECT_ORDER_DIRECTION_NONE,
    SELECT_ORDER_DIRECTION_COMPLETE
  };
  enum {
    SELECT_INDEX_HINT_NONE,
    SELECT_INDEX_HINT_AFTER_TYPE,
    SELECT_INDEX_HINT_AFTER_KEY,
    SELECT_INDEX_HINT_AFTER_FOR,
    SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP,
    SELECT_INDEX_HINT_AFTER_SCOPE,
    SELECT_INDEX_HINT_AFTER_LP,
    SELECT_INDEX_HINT_AFTER_NAME,
    SELECT_INDEX_HINT_AFTER_COMMA
  };
  enum {
    SELECT_PARTITION_NONE,
    SELECT_PARTITION_AFTER_PARTITION,
    SELECT_PARTITION_AFTER_LP,
    SELECT_PARTITION_AFTER_NAME,
    SELECT_PARTITION_AFTER_COMMA
  };
  enum {
    SELECT_TABLESAMPLE_NONE,
    SELECT_TABLESAMPLE_AFTER_TABLESAMPLE,
    SELECT_TABLESAMPLE_AFTER_METHOD,
    SELECT_TABLESAMPLE_AFTER_LP,
    SELECT_TABLESAMPLE_AFTER_PERCENTAGE,
    SELECT_TABLESAMPLE_COMPLETE
  };
  enum {
    SELECT_NTH_FROM_NONE,
    SELECT_NTH_FROM_AFTER_FROM,
    SELECT_NTH_FROM_AFTER_DIRECTION,
    SELECT_NTH_FROM_AFTER_NULL_TREATMENT,
    SELECT_NTH_FROM_AFTER_NULLS
  };
  enum {
    SELECT_PHASE_BODY,
    SELECT_PHASE_WHERE,
    SELECT_PHASE_GROUP,
    SELECT_PHASE_HAVING,
    SELECT_PHASE_WINDOW,
    SELECT_PHASE_QUALIFY,
    SELECT_PHASE_ORDER,
    SELECT_PHASE_LIMIT,
    SELECT_PHASE_LOCK
  };
  enum {
    SELECT_EXPRESSION_NONE,
    SELECT_EXPRESSION_WHERE,
    SELECT_EXPRESSION_HAVING,
    SELECT_EXPRESSION_QUALIFY
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token;
  int token_id;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int pre_select_depth = 0;
  int select_has_outer_parenthesis = 0;
  int saw_select = 0;
  int select_prefix = 1;
  int select_modifiers = 0;
  int need_by = 0;
  int need_operand = 0;
  int operand_clause = 0;
  int need_set_operand = 0;
  int set_option_seen = 0;
  int lock_state = SELECT_LOCK_NONE;
  int saw_lock_tail = 0;
  int into_state = SELECT_INTO_NONE;
  int outfile_fields = 0;
  int outfile_lines = 0;
  int limit_state = SELECT_LIMIT_NONE;
  int window_state = SELECT_WINDOW_NONE;
  int group_clause = 0;
  int rollup_state = SELECT_ROLLUP_NONE;
  int group_direction_state = SELECT_ORDER_DIRECTION_NONE;
  int group_previous_top_token_id = 0;
  int group_previous_was_operator = 1;
  MyliteToken group_previous_top_token = {0};
  int order_clause = 0;
  int order_direction_state = SELECT_ORDER_DIRECTION_NONE;
  int order_previous_top_token_id = 0;
  int order_previous_was_operator = 1;
  MyliteToken order_previous_top_token = {0};
  int expression_clause = SELECT_EXPRESSION_NONE;
  int expression_previous_top_token_id = 0;
  int expression_previous_was_operator = 1;
  int expression_match_list = 0;
  MyliteToken expression_previous_top_token = {0};
  int index_hint_state = SELECT_INDEX_HINT_NONE;
  int index_hint_allow_empty = 0;
  int from_clause = 0;
  int join_condition_slots = 0;
  int seen_from_clause = 0;
  int seen_where_clause = 0;
  int seen_group_clause = 0;
  int seen_having_clause = 0;
  int seen_order_clause = 0;
  int seen_limit_clause = 0;
  int seen_into_clause = 0;
  int seen_window_clause = 0;
  int seen_qualify_clause = 0;
  int clause_phase = SELECT_PHASE_BODY;
  int partition_state = SELECT_PARTITION_NONE;
  int tablesample_state = SELECT_TABLESAMPLE_NONE;
  int nth_from_state = SELECT_NTH_FROM_NONE;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_select) {
      if (token_opens_nested_expression(token_id)) {
        pre_select_depth++;
      } else if (token_closes_nested_expression(token_id) &&
                 pre_select_depth > 0) {
        pre_select_depth--;
      }
      if ((!use_start || token.offset >= start.offset) &&
          token_id == ML_SELECT) {
        saw_select = 1;
        select_has_outer_parenthesis = pre_select_depth > 0;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      break;
    }

    if (depth > 0) {
      const char *nested_message = "malformed SELECT expression clause";
      if (group_clause) {
        nested_message = "malformed SELECT GROUP BY clause";
      } else if (order_clause) {
        nested_message = "malformed SELECT ORDER BY clause";
      }
      if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                        &expression_stack, nested_message)) {
        return;
      }
      if (token_closes_nested_expression(token_id)) {
        if (depth == 0 && order_clause) {
          order_previous_top_token_id = token_id;
          order_previous_top_token = token;
          order_previous_was_operator = 0;
        }
        if (depth == 0 && group_clause) {
          group_previous_top_token_id = token_id;
          group_previous_top_token = token;
          group_previous_was_operator = 0;
        }
        if (depth == 0 && expression_clause != SELECT_EXPRESSION_NONE) {
          expression_previous_top_token_id = token_id;
          expression_previous_top_token = token;
          expression_previous_was_operator = 0;
        }
      }
      continue;
    }

    if (select_prefix) {
      int modifier_flag = select_modifier_flag(token_id);
      if (modifier_flag) {
        if ((select_modifiers & SELECT_MODIFIER_ALL) &&
            (modifier_flag & SELECT_MODIFIER_DISTINCT)) {
          mylite_parser_reject(ctx, token, "invalid SELECT modifiers");
          return;
        }
        if ((select_modifiers & SELECT_MODIFIER_DISTINCT) &&
            (modifier_flag & SELECT_MODIFIER_ALL)) {
          mylite_parser_reject(ctx, token, "invalid SELECT modifiers");
          return;
        }
        select_modifiers |= modifier_flag;
        continue;
      }
      select_prefix = 0;
    }

    if (nth_from_state == SELECT_NTH_FROM_AFTER_FROM) {
      if (token_id == ML_FIRST || token_id == ML_LAST) {
        nth_from_state = SELECT_NTH_FROM_AFTER_DIRECTION;
        continue;
      }
      nth_from_state = SELECT_NTH_FROM_NONE;
    }
    if (nth_from_state == SELECT_NTH_FROM_AFTER_DIRECTION) {
      if (token_ascii_equal(token, "over")) {
        nth_from_state = SELECT_NTH_FROM_NONE;
        continue;
      }
      if (token_id == ML_IGNORE || token_ascii_equal(token, "respect")) {
        nth_from_state = SELECT_NTH_FROM_AFTER_NULL_TREATMENT;
        continue;
      }
      nth_from_state = SELECT_NTH_FROM_NONE;
    }
    if (nth_from_state == SELECT_NTH_FROM_AFTER_NULL_TREATMENT) {
      if (token_ascii_equal(token, "nulls")) {
        nth_from_state = SELECT_NTH_FROM_AFTER_NULLS;
        continue;
      }
      nth_from_state = SELECT_NTH_FROM_NONE;
    }
    if (nth_from_state == SELECT_NTH_FROM_AFTER_NULLS) {
      if (token_ascii_equal(token, "over")) {
        nth_from_state = SELECT_NTH_FROM_NONE;
        continue;
      }
      nth_from_state = SELECT_NTH_FROM_NONE;
    }

    if (rollup_state == SELECT_ROLLUP_AFTER_WITH) {
      if (!token_ascii_equal(token, "rollup")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT GROUP BY rollup clause");
        return;
      }
      rollup_state = SELECT_ROLLUP_COMPLETE;
      continue;
    }
    if (rollup_state == SELECT_ROLLUP_COMPLETE) {
      if (!select_rollup_boundary(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT GROUP BY rollup clause");
        return;
      }
      group_clause = 0;
      rollup_state = SELECT_ROLLUP_NONE;
    }

    if (order_direction_state == SELECT_ORDER_DIRECTION_COMPLETE) {
      if (!select_order_direction_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT ORDER BY direction");
        return;
      }
      order_direction_state = SELECT_ORDER_DIRECTION_NONE;
      order_clause = 0;
    }
    if (group_direction_state == SELECT_ORDER_DIRECTION_COMPLETE) {
      if (token_id != ML_COMMA && token_id != ML_WITH &&
          !select_rollup_boundary(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT GROUP BY direction");
        return;
      }
      group_direction_state = SELECT_ORDER_DIRECTION_NONE;
      if (token_id != ML_COMMA && token_id != ML_WITH) {
        group_clause = 0;
      }
    }

    if (index_hint_state == SELECT_INDEX_HINT_AFTER_TYPE) {
      if (token_id != ML_INDEX && token_id != ML_KEY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_KEY;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_KEY) {
      if (token_id == ML_FOR) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LP) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_LP;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete SELECT index hint");
      return;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_FOR) {
      if (token_id == ML_JOIN) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_SCOPE;
        continue;
      }
      if (token_id == ML_GROUP || token_id == ML_ORDER) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete SELECT index hint");
      return;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_SCOPE;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_SCOPE) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_LP;
      pending_token = token;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_LP) {
      if (token_id == ML_RP && index_hint_allow_empty) {
        index_hint_state = SELECT_INDEX_HINT_NONE;
        continue;
      }
      if (!select_index_hint_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_NAME;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_NAME) {
      if (token_id == ML_COMMA) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_RP) {
        index_hint_state = SELECT_INDEX_HINT_NONE;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT index hint");
      return;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_COMMA) {
      if (!select_index_hint_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_NAME;
      continue;
    }

    if (partition_state == SELECT_PARTITION_AFTER_PARTITION) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT partition clause");
        return;
      }
      partition_state = SELECT_PARTITION_AFTER_LP;
      pending_token = token;
      continue;
    }
    if (partition_state == SELECT_PARTITION_AFTER_LP) {
      if (!select_partition_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT partition clause");
        return;
      }
      partition_state = SELECT_PARTITION_AFTER_NAME;
      continue;
    }
    if (partition_state == SELECT_PARTITION_AFTER_NAME) {
      if (token_id == ML_COMMA) {
        partition_state = SELECT_PARTITION_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_RP) {
        partition_state = SELECT_PARTITION_NONE;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT partition clause");
      return;
    }
    if (partition_state == SELECT_PARTITION_AFTER_COMMA) {
      if (!select_partition_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT partition clause");
        return;
      }
      partition_state = SELECT_PARTITION_AFTER_NAME;
      continue;
    }

    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_TABLESAMPLE) {
      if (!token_ascii_equal(token, "bernoulli") &&
          token_id != ML_SYSTEM) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_AFTER_METHOD;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_METHOD) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_AFTER_LP;
      pending_token = token;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_LP) {
      if (!select_tablesample_percentage_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_AFTER_PERCENTAGE;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_PERCENTAGE) {
      if (token_id != ML_RP) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_COMPLETE;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_COMPLETE) {
      if (!select_tablesample_boundary(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_NONE;
    }

    if (lock_state == SELECT_LOCK_AFTER_LOCK) {
      if (token_id != ML_IN) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_LOCK_IN;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_LOCK_IN) {
      if (!token_ascii_equal(token, "share")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_LOCK_IN_SHARE;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_LOCK_IN_SHARE) {
      if (!token_ascii_equal(token, "mode")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_COMPLETE;
      saw_lock_tail = 1;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_FOR) {
      if (!saw_lock_tail &&
          (token_id == ML_GROUP || token_id == ML_JOIN ||
           token_id == ML_ORDER)) {
        lock_state = SELECT_LOCK_NONE;
        continue;
      }
      if (token_id == ML_UPDATE || token_ascii_equal(token, "share")) {
        lock_state = SELECT_LOCK_AFTER_STRENGTH;
        saw_lock_tail = 1;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete SELECT lock clause");
      return;
    }
    if (lock_state == SELECT_LOCK_AFTER_STRENGTH) {
      if (select_has_outer_parenthesis &&
          token_closes_nested_expression(token_id)) {
        break;
      }
      if (token_ascii_equal(token, "of")) {
        lock_state = SELECT_LOCK_AFTER_OF;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "skip")) {
        lock_state = SELECT_LOCK_AFTER_SKIP;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "nowait")) {
        lock_state = SELECT_LOCK_COMPLETE;
        continue;
      }
      if (token_id == ML_FOR) {
        lock_state = SELECT_LOCK_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LOCK) {
        lock_state = SELECT_LOCK_AFTER_LOCK;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        lock_state = SELECT_LOCK_NONE;
        into_state = SELECT_INTO_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT lock clause");
      return;
    }
    if (lock_state == SELECT_LOCK_AFTER_OF ||
        lock_state == SELECT_LOCK_AFTER_COMMA) {
      if (!select_lock_table_ref_start(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_TABLE;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_TABLE) {
      if (token_id == ML_DOT) {
        lock_state = SELECT_LOCK_AFTER_DOT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_COMMA) {
        lock_state = SELECT_LOCK_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "skip")) {
        lock_state = SELECT_LOCK_AFTER_SKIP;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "nowait")) {
        lock_state = SELECT_LOCK_COMPLETE;
        continue;
      }
      if (token_id == ML_FOR) {
        lock_state = SELECT_LOCK_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LOCK) {
        lock_state = SELECT_LOCK_AFTER_LOCK;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        lock_state = SELECT_LOCK_NONE;
        into_state = SELECT_INTO_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT lock clause");
      return;
    }
    if (lock_state == SELECT_LOCK_AFTER_DOT) {
      if (!select_lock_table_ref_part(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_TABLE;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_SKIP) {
      if (!token_ascii_equal(token, "locked")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_COMPLETE;
      continue;
    }
    if (lock_state == SELECT_LOCK_COMPLETE) {
      if (select_has_outer_parenthesis &&
          token_closes_nested_expression(token_id)) {
        break;
      }
      if (token_id == ML_FOR) {
        lock_state = SELECT_LOCK_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LOCK) {
        lock_state = SELECT_LOCK_AFTER_LOCK;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        lock_state = SELECT_LOCK_NONE;
        into_state = SELECT_INTO_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT lock clause");
      return;
    }

    if (into_state == SELECT_INTO_AFTER_INTO) {
      if (token_id == ML_OUTFILE) {
        into_state = SELECT_INTO_AFTER_OUTFILE;
        outfile_fields = 0;
        outfile_lines = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_DUMPFILE) {
        into_state = SELECT_INTO_AFTER_DUMPFILE;
        pending_token = token;
        continue;
      }
      if (token_id == ML_AT_SIGN) {
        into_state = SELECT_INTO_AFTER_VAR_AT;
        pending_token = token;
        continue;
      }
      if (select_into_variable_target_token(token_id, token)) {
        into_state = SELECT_INTO_VAR_READY;
        continue;
      }
      if (select_operand_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO clause");
        return;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete SELECT INTO clause");
      return;
    }
    if (into_state == SELECT_INTO_AFTER_VAR_COMMA) {
      if (token_id == ML_AT_SIGN) {
        into_state = SELECT_INTO_AFTER_VAR_AT;
        pending_token = token;
        continue;
      }
      if (!select_into_variable_target_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO clause");
        return;
      }
      into_state = SELECT_INTO_VAR_READY;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_VAR_AT) {
      if (!select_into_variable_at_target_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO clause");
        return;
      }
      into_state = SELECT_INTO_VAR_READY;
      continue;
    }
    if (into_state == SELECT_INTO_VAR_READY) {
      if (token_id == ML_COMMA) {
        into_state = SELECT_INTO_AFTER_VAR_COMMA;
        pending_token = token;
        continue;
      }
      if (select_has_outer_parenthesis &&
          token_closes_nested_expression(token_id)) {
        break;
      }
      if (select_into_output_follow_token(token_id, token)) {
        into_state = SELECT_INTO_NONE;
      } else {
        mylite_parser_reject(ctx, token, "malformed SELECT INTO clause");
        return;
      }
    }
    if (into_state == SELECT_INTO_AFTER_OUTFILE) {
      if (!create_table_tail_option_string_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO file target");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_DUMPFILE) {
      if (!create_table_tail_option_string_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO file target");
        return;
      }
      into_state = SELECT_INTO_DUMPFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_OUTFILE_READY) {
      if (select_has_outer_parenthesis &&
          token_closes_nested_expression(token_id)) {
        break;
      }
      if (token_id == ML_CHARACTER) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        into_state = SELECT_INTO_AFTER_CHARACTER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHARSET) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        into_state = SELECT_INTO_AFTER_CHARSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_FIELDS || token_id == ML_COLUMNS) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        outfile_fields = 1;
        into_state = SELECT_INTO_AFTER_FIELDS;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LINES) {
        if (outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        outfile_lines = 1;
        into_state = SELECT_INTO_AFTER_LINES;
        pending_token = token;
        continue;
      }
      if (select_outfile_line_option_start(token_id) && outfile_lines) {
        into_state = SELECT_INTO_AFTER_LINE_OPTION;
        pending_token = token;
        continue;
      }
      if (select_outfile_field_option_start(token_id)) {
        if (!outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        if (token_id == ML_OPTIONALLY) {
          into_state = SELECT_INTO_AFTER_OPTIONALLY;
        } else {
          into_state = SELECT_INTO_AFTER_FIELD_OPTION;
        }
        pending_token = token;
        continue;
      }
      if (select_outfile_line_option_start(token_id)) {
        mylite_parser_reject(ctx, token,
                             "malformed SELECT INTO OUTFILE option");
        return;
      }
      if (!select_into_output_follow_token(token_id, token)) {
        mylite_parser_reject(ctx, token,
                             "malformed SELECT INTO OUTFILE option");
        return;
      }
      into_state = SELECT_INTO_NONE;
    }
    if (into_state == SELECT_INTO_DUMPFILE_READY) {
      if (select_has_outer_parenthesis &&
          token_closes_nested_expression(token_id)) {
        break;
      }
      if (select_into_output_option_start(token_id)) {
        mylite_parser_reject(ctx, token,
                             "malformed SELECT INTO DUMPFILE option");
        return;
      }
      if (!select_into_output_follow_token(token_id, token)) {
        mylite_parser_reject(ctx, token,
                             "malformed SELECT INTO DUMPFILE option");
        return;
      }
      into_state = SELECT_INTO_NONE;
    }
    if (into_state == SELECT_INTO_AFTER_CHARACTER) {
      if (token_id != ML_SET) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE charset");
        return;
      }
      into_state = SELECT_INTO_AFTER_CHARSET;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_CHARSET) {
      if (!select_charset_name_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE charset");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_FIELDS) {
      if (!select_outfile_field_option_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      if (token_id == ML_OPTIONALLY) {
        into_state = SELECT_INTO_AFTER_OPTIONALLY;
      } else {
        into_state = SELECT_INTO_AFTER_FIELD_OPTION;
      }
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_FIELD_OPTION) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_AFTER_FIELD_BY;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_FIELD_BY) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_OPTIONALLY) {
      if (token_id != ML_ENCLOSED) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_AFTER_FIELD_BY;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_LINES) {
      if (!select_outfile_line_option_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE lines option");
        return;
      }
      into_state = SELECT_INTO_AFTER_LINE_OPTION;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_LINE_OPTION) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE lines option");
        return;
      }
      into_state = SELECT_INTO_AFTER_LINE_BY;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_LINE_BY) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE lines option");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }

    if (limit_state == SELECT_LIMIT_AFTER_LIMIT ||
        limit_state == SELECT_LIMIT_AFTER_COMMA ||
        limit_state == SELECT_LIMIT_AFTER_OFFSET) {
      if (!select_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT LIMIT clause");
        return;
      }
      if (limit_state == SELECT_LIMIT_AFTER_LIMIT) {
        limit_state = SELECT_LIMIT_AFTER_FIRST_VALUE;
      } else {
        limit_state = SELECT_LIMIT_AFTER_FINAL_VALUE;
      }
      continue;
    }
    if (limit_state == SELECT_LIMIT_AFTER_FIRST_VALUE) {
      if (token_id == ML_COMMA) {
        limit_state = SELECT_LIMIT_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        limit_state = SELECT_LIMIT_AFTER_OFFSET;
        pending_token = token;
        continue;
      }
      if (select_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, token, "malformed SELECT LIMIT clause");
        return;
      }
      limit_state = SELECT_LIMIT_NONE;
    }
    if (limit_state == SELECT_LIMIT_AFTER_FINAL_VALUE) {
      if (token_id == ML_COMMA || token_id == ML_OFFSET ||
          select_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, token, "malformed SELECT LIMIT clause");
        return;
      }
      limit_state = SELECT_LIMIT_NONE;
    }

    if (window_state == SELECT_WINDOW_AFTER_WINDOW) {
      if (!select_window_name_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT WINDOW clause");
        return;
      }
      window_state = SELECT_WINDOW_AFTER_NAME;
      continue;
    }
    if (window_state == SELECT_WINDOW_AFTER_NAME) {
      if (token_id != ML_AS) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT WINDOW clause");
        return;
      }
      window_state = SELECT_WINDOW_AFTER_AS;
      pending_token = token;
      continue;
    }
    if (window_state == SELECT_WINDOW_AFTER_AS) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT WINDOW clause");
        return;
      }
      depth++;
      window_state = SELECT_WINDOW_AFTER_SPEC;
      continue;
    }
    if (window_state == SELECT_WINDOW_AFTER_SPEC) {
      if (token_id == ML_COMMA) {
        window_state = SELECT_WINDOW_AFTER_WINDOW;
        pending_token = token;
        continue;
      }
      window_state = SELECT_WINDOW_NONE;
    }

    if (need_by) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT clause");
        return;
      }
      need_by = 0;
      need_operand = 1;
      pending_token = token;
      continue;
    }

    if (need_set_operand) {
      if (select_set_option(token_id)) {
        if (set_option_seen) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT set operation");
          return;
        }
        set_option_seen = 1;
        continue;
      }
      if (!select_set_operand_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT set operation");
        return;
      }
      need_set_operand = 0;
      set_option_seen = 0;
    }

    if (need_operand) {
      if (select_operand_boundary(token_id) ||
          token_ascii_equal(token, "qualify")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT clause");
        return;
      }
      if (operand_clause == ML_JOIN || operand_clause == ML_STRAIGHT_JOIN) {
        join_condition_slots++;
      }
      operand_clause = 0;
      need_operand = 0;
    }

    if (expression_clause == SELECT_EXPRESSION_NONE && !group_clause &&
        !order_clause &&
        (token_id == ML_LP || token_id == ML_LB || token_id == ML_LC)) {
      depth++;
      continue;
    }
    if (expression_clause == SELECT_EXPRESSION_NONE && !group_clause &&
        !order_clause &&
        (token_id == ML_RP || token_id == ML_RB || token_id == ML_RC)) {
      if (select_has_outer_parenthesis) {
        break;
      }
      continue;
    }

    if (token_id == ML_COMMA) {
      if (expression_clause != SELECT_EXPRESSION_NONE) {
        if (!expression_match_list) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT expression clause");
          return;
        }
        expression_previous_top_token_id = 0;
        expression_previous_was_operator = 1;
        expression_previous_top_token = token;
        continue;
      }
      if (clause_phase == SELECT_PHASE_GROUP) {
        if (group_previous_was_operator) {
          mylite_parser_reject(ctx, group_previous_top_token,
                               "incomplete SELECT GROUP BY clause");
          return;
        }
        group_previous_top_token_id = 0;
        group_previous_was_operator = 1;
        group_previous_top_token = token;
        continue;
      }
      if (clause_phase == SELECT_PHASE_ORDER) {
        order_clause = 1;
        order_previous_top_token_id = 0;
        order_previous_was_operator = 1;
        order_previous_top_token = token;
      }
      join_condition_slots = 0;
      need_operand = 1;
      operand_clause = 0;
      pending_token = token;
      continue;
    }

    if (order_clause && (token_id == ML_ASC || token_id == ML_DESC)) {
      order_direction_state = SELECT_ORDER_DIRECTION_COMPLETE;
      pending_token = token;
      continue;
    }

    if (select_index_hint_type(token_id) &&
        !(token_id == ML_IGNORE &&
          select_token_followed_by_nulls(ctx, token))) {
      index_hint_state = SELECT_INDEX_HINT_AFTER_TYPE;
      index_hint_allow_empty = token_id == ML_USE;
      pending_token = token;
      continue;
    }

    if (from_clause && token_id == ML_PARTITION) {
      partition_state = SELECT_PARTITION_AFTER_PARTITION;
      pending_token = token;
      continue;
    }

    if (from_clause && token_ascii_equal(token, "tablesample")) {
      tablesample_state = SELECT_TABLESAMPLE_AFTER_TABLESAMPLE;
      pending_token = token;
      continue;
    }

    if (group_clause && token_id == ML_WITH) {
      if (group_previous_was_operator) {
        mylite_parser_reject(ctx, group_previous_top_token,
                             "incomplete SELECT GROUP BY clause");
        return;
      }
      rollup_state = SELECT_ROLLUP_AFTER_WITH;
      pending_token = token;
      continue;
    }

    if (expression_clause != SELECT_EXPRESSION_NONE &&
        select_expression_clause_boundary(token_id, token)) {
      if (expression_previous_was_operator) {
        mylite_parser_reject(ctx, expression_previous_top_token,
                             "incomplete SELECT expression clause");
        return;
      }
      expression_clause = SELECT_EXPRESSION_NONE;
      expression_match_list = 0;
    }

    if (group_clause && select_rollup_boundary(token_id, token)) {
      if (group_previous_was_operator) {
        mylite_parser_reject(ctx, group_previous_top_token,
                             "incomplete SELECT GROUP BY clause");
        return;
      }
      group_clause = 0;
    }

    if (token_id == ML_LOCK) {
      clause_phase = SELECT_PHASE_LOCK;
      join_condition_slots = 0;
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      lock_state = SELECT_LOCK_AFTER_LOCK;
      pending_token = token;
      continue;
    }
    if (token_id == ML_FOR) {
      clause_phase = SELECT_PHASE_LOCK;
      join_condition_slots = 0;
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      lock_state = SELECT_LOCK_AFTER_FOR;
      pending_token = token;
      continue;
    }
    if (token_id == ML_INTO) {
      if (seen_into_clause) {
        mylite_parser_reject(ctx, token, "duplicate SELECT clause");
        return;
      }
      seen_into_clause = 1;
      join_condition_slots = 0;
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      into_state = SELECT_INTO_AFTER_INTO;
      pending_token = token;
      continue;
    }
    if (token_id == ML_LIMIT) {
      if (seen_limit_clause) {
        mylite_parser_reject(ctx, token, "duplicate SELECT clause");
        return;
      }
      if (clause_phase > SELECT_PHASE_ORDER) {
        mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
        return;
      }
      seen_limit_clause = 1;
      clause_phase = SELECT_PHASE_LIMIT;
      join_condition_slots = 0;
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      limit_state = SELECT_LIMIT_AFTER_LIMIT;
      pending_token = token;
      continue;
    }
    if (token_ascii_equal(token, "window")) {
      if (seen_window_clause) {
        mylite_parser_reject(ctx, token, "duplicate SELECT clause");
        return;
      }
      if (clause_phase > SELECT_PHASE_HAVING) {
        mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
        return;
      }
      seen_window_clause = 1;
      clause_phase = SELECT_PHASE_WINDOW;
      join_condition_slots = 0;
      group_clause = 0;
      from_clause = 0;
      window_state = SELECT_WINDOW_AFTER_WINDOW;
      pending_token = token;
      continue;
    }
    if (token_ascii_equal(token, "qualify")) {
      if (seen_qualify_clause) {
        mylite_parser_reject(ctx, token, "duplicate SELECT clause");
        return;
      }
      if (clause_phase > SELECT_PHASE_WINDOW) {
        mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
        return;
      }
      seen_qualify_clause = 1;
      clause_phase = SELECT_PHASE_QUALIFY;
      join_condition_slots = 0;
      group_clause = 0;
      from_clause = 0;
      expression_clause = SELECT_EXPRESSION_QUALIFY;
      expression_previous_top_token_id = 0;
      expression_previous_was_operator = 1;
      expression_match_list = 0;
      expression_previous_top_token = token;
      need_operand = 1;
      operand_clause = 0;
      pending_token = token;
      continue;
    }
    if (token_id == ML_PROCEDURE) {
      mylite_parser_reject(ctx, token, "removed SELECT PROCEDURE clause");
      return;
    }

    if (select_clause_requires_by(token_id)) {
      if (token_id == ML_GROUP) {
        if (seen_group_clause) {
          mylite_parser_reject(ctx, token, "duplicate SELECT clause");
          return;
        }
        if (clause_phase > SELECT_PHASE_WHERE) {
          mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
          return;
        }
        seen_group_clause = 1;
      } else if (token_id == ML_ORDER) {
        if (seen_order_clause) {
          mylite_parser_reject(ctx, token, "duplicate SELECT clause");
          return;
        }
        if (clause_phase > SELECT_PHASE_QUALIFY) {
          mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
          return;
        }
        seen_order_clause = 1;
      }
      clause_phase = token_id == ML_GROUP ? SELECT_PHASE_GROUP :
                                            SELECT_PHASE_ORDER;
      join_condition_slots = 0;
      from_clause = 0;
      group_clause = token_id == ML_GROUP;
      order_clause = token_id == ML_ORDER;
      if (group_clause) {
        group_previous_top_token_id = 0;
        group_previous_was_operator = 1;
        group_previous_top_token = token;
      }
      if (order_clause) {
        order_previous_top_token_id = 0;
        order_previous_was_operator = 1;
        order_previous_top_token = token;
      }
      need_by = 1;
      pending_token = token;
      continue;
    }
    if (select_clause_requires_operand(token_id)) {
      if (token_id == ML_FROM) {
        if (select_from_starts_nth_modifier(ctx, token)) {
          nth_from_state = SELECT_NTH_FROM_AFTER_FROM;
          continue;
        }
        if (seen_from_clause) {
          mylite_parser_reject(ctx, token, "duplicate SELECT clause");
          return;
        }
        if (clause_phase > SELECT_PHASE_BODY) {
          mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
          return;
        }
        seen_from_clause = 1;
        join_condition_slots = 0;
      } else if (token_id == ML_ON || token_id == ML_USING) {
        if (join_condition_slots <= 0) {
          mylite_parser_reject(ctx, token, "misplaced SELECT join clause");
          return;
        }
        join_condition_slots--;
      } else if (token_id == ML_WHERE) {
        if (seen_where_clause) {
          mylite_parser_reject(ctx, token, "duplicate SELECT clause");
          return;
        }
        if (clause_phase > SELECT_PHASE_BODY) {
          mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
          return;
        }
        seen_where_clause = 1;
        clause_phase = SELECT_PHASE_WHERE;
        join_condition_slots = 0;
        expression_clause = SELECT_EXPRESSION_WHERE;
        expression_previous_top_token_id = 0;
        expression_previous_was_operator = 1;
        expression_match_list = 0;
        expression_previous_top_token = token;
      } else if (token_id == ML_HAVING) {
        if (seen_having_clause) {
          mylite_parser_reject(ctx, token, "duplicate SELECT clause");
          return;
        }
        if (clause_phase > SELECT_PHASE_GROUP) {
          mylite_parser_reject(ctx, token, "out-of-order SELECT clause");
          return;
        }
        seen_having_clause = 1;
        clause_phase = SELECT_PHASE_HAVING;
        join_condition_slots = 0;
        expression_clause = SELECT_EXPRESSION_HAVING;
        expression_previous_top_token_id = 0;
        expression_previous_was_operator = 1;
        expression_match_list = 0;
        expression_previous_top_token = token;
      } else if (token_id == ML_PROCEDURE) {
        join_condition_slots = 0;
      }
      if (token_id == ML_HAVING || token_id == ML_JOIN || token_id == ML_ON ||
          token_id == ML_PROCEDURE || token_id == ML_STRAIGHT_JOIN ||
          token_id == ML_USING ||
          token_id == ML_WHERE) {
        group_clause = 0;
        order_clause = 0;
      }
      from_clause = token_id == ML_FROM;
      need_operand = 1;
      operand_clause = token_id;
      pending_token = token;
      continue;
    }
    if (group_clause) {
      if (token_closes_nested_expression(token_id) &&
          select_has_outer_parenthesis) {
        if (group_previous_was_operator) {
          mylite_parser_reject(ctx, group_previous_top_token,
                               "incomplete SELECT GROUP BY clause");
          return;
        }
        break;
      }
      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (group_previous_was_operator) {
          mylite_parser_reject(ctx, group_previous_top_token,
                               "incomplete SELECT GROUP BY clause");
          return;
        }
        group_direction_state = SELECT_ORDER_DIRECTION_COMPLETE;
        pending_token = token;
        continue;
      }
      if (!query_expression_token(
              ctx, token_id, token, &depth, &group_previous_top_token_id,
              &group_previous_top_token, &group_previous_was_operator,
              &expression_stack,
              "malformed SELECT GROUP BY clause")) {
        return;
      }
      continue;
    }
    if (select_set_operator(token_id)) {
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      join_condition_slots = 0;
      seen_from_clause = 0;
      seen_where_clause = 0;
      seen_group_clause = 0;
      seen_having_clause = 0;
      seen_order_clause = 0;
      seen_limit_clause = 0;
      seen_into_clause = 0;
      seen_window_clause = 0;
      seen_qualify_clause = 0;
      clause_phase = SELECT_PHASE_BODY;
      expression_clause = SELECT_EXPRESSION_NONE;
      expression_match_list = 0;
      need_set_operand = 1;
      set_option_seen = 0;
      operand_clause = 0;
      pending_token = token;
      continue;
    }

    if (expression_clause != SELECT_EXPRESSION_NONE) {
      if (token_closes_nested_expression(token_id)) {
        if (select_has_outer_parenthesis) {
          if (expression_previous_was_operator) {
            mylite_parser_reject(ctx, expression_previous_top_token,
                                 "incomplete SELECT expression clause");
            return;
          }
          break;
        }
        mylite_parser_reject(ctx, token,
                             "malformed SELECT expression clause");
        return;
      }
      if (!query_expression_token(
              ctx, token_id, token, &depth, &expression_previous_top_token_id,
              &expression_previous_top_token, &expression_previous_was_operator,
              &expression_stack,
              "malformed SELECT expression clause")) {
        return;
      }
      if (token_ascii_equal(token, "match")) {
        expression_match_list = 1;
      } else if (expression_match_list &&
                 token_ascii_equal(token, "against")) {
        expression_match_list = 0;
      }
      continue;
    }

    if (order_clause) {
      if (token_closes_nested_expression(token_id)) {
        if (select_has_outer_parenthesis) {
          if (order_previous_was_operator) {
            mylite_parser_reject(ctx, order_previous_top_token,
                                 "incomplete SELECT ORDER BY clause");
            return;
          }
          break;
        }
        mylite_parser_reject(ctx, token, "malformed SELECT ORDER BY clause");
        return;
      }
      if (!query_expression_token(
              ctx, token_id, token, &depth, &order_previous_top_token_id,
              &order_previous_top_token, &order_previous_was_operator,
              &expression_stack,
              "malformed SELECT ORDER BY clause")) {
        return;
      }
      continue;
    }
  }

  if (lock_state == SELECT_LOCK_AFTER_LOCK ||
      lock_state == SELECT_LOCK_AFTER_LOCK_IN ||
      lock_state == SELECT_LOCK_AFTER_LOCK_IN_SHARE ||
      lock_state == SELECT_LOCK_AFTER_FOR ||
      lock_state == SELECT_LOCK_AFTER_OF ||
      lock_state == SELECT_LOCK_AFTER_DOT ||
      lock_state == SELECT_LOCK_AFTER_COMMA ||
      lock_state == SELECT_LOCK_AFTER_SKIP) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT lock clause");
  } else if (into_state == SELECT_INTO_AFTER_INTO) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO clause");
  } else if (into_state == SELECT_INTO_AFTER_VAR_COMMA ||
             into_state == SELECT_INTO_AFTER_VAR_AT) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO clause");
  } else if (into_state == SELECT_INTO_AFTER_OUTFILE ||
             into_state == SELECT_INTO_AFTER_DUMPFILE) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO file target");
  } else if (into_state == SELECT_INTO_AFTER_CHARACTER ||
             into_state == SELECT_INTO_AFTER_CHARSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO OUTFILE charset");
  } else if (into_state == SELECT_INTO_AFTER_FIELDS ||
             into_state == SELECT_INTO_AFTER_FIELD_OPTION ||
             into_state == SELECT_INTO_AFTER_FIELD_BY ||
             into_state == SELECT_INTO_AFTER_OPTIONALLY ||
             into_state == SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO OUTFILE fields option");
  } else if (into_state == SELECT_INTO_AFTER_LINES ||
             into_state == SELECT_INTO_AFTER_LINE_OPTION ||
             into_state == SELECT_INTO_AFTER_LINE_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO OUTFILE lines option");
  } else if (limit_state == SELECT_LIMIT_AFTER_LIMIT ||
             limit_state == SELECT_LIMIT_AFTER_COMMA ||
             limit_state == SELECT_LIMIT_AFTER_OFFSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT LIMIT clause");
  } else if (window_state == SELECT_WINDOW_AFTER_WINDOW ||
             window_state == SELECT_WINDOW_AFTER_NAME ||
             window_state == SELECT_WINDOW_AFTER_AS) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT WINDOW clause");
  } else if (order_clause && order_previous_was_operator) {
    mylite_parser_reject(ctx, order_previous_top_token,
                         "incomplete SELECT ORDER BY clause");
  } else if (group_clause && group_previous_was_operator) {
    mylite_parser_reject(ctx, group_previous_top_token,
                         "incomplete SELECT GROUP BY clause");
  } else if (depth == 0 && expression_clause != SELECT_EXPRESSION_NONE &&
             expression_previous_was_operator) {
    mylite_parser_reject(ctx, expression_previous_top_token,
                         "incomplete SELECT expression clause");
  } else if (rollup_state == SELECT_ROLLUP_AFTER_WITH) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT GROUP BY rollup clause");
  } else if (index_hint_state == SELECT_INDEX_HINT_AFTER_TYPE ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_KEY ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_FOR ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_SCOPE ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_LP ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT index hint");
  } else if (partition_state == SELECT_PARTITION_AFTER_PARTITION ||
             partition_state == SELECT_PARTITION_AFTER_LP ||
             partition_state == SELECT_PARTITION_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT partition clause");
  } else if (tablesample_state == SELECT_TABLESAMPLE_AFTER_TABLESAMPLE ||
             tablesample_state == SELECT_TABLESAMPLE_AFTER_METHOD ||
             tablesample_state == SELECT_TABLESAMPLE_AFTER_LP) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT TABLESAMPLE clause");
  } else if (need_set_operand) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT set operation");
  } else if (nth_from_state != SELECT_NTH_FROM_NONE ||
             need_by || need_operand) {
    mylite_parser_reject(ctx, pending_token, "incomplete SELECT clause");
  }
}

static void validate_select_list_tails_from(MyliteParseContext *ctx,
                                            int use_start, MyliteToken start,
                                            int stop_at_insert_duplicate) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int scanning = !use_start;
  int depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!scanning) {
      if (token.offset >= start.offset) {
        scanning = 1;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      return;
    }

    if (stop_at_insert_duplicate && token_id == ML_ON &&
        insert_duplicate_clause_follows(ctx, token)) {
      return;
    }

    if (token_id == ML_SELECT) {
      validate_select_list_tail_from(ctx, token, 0, 0,
                                     stop_at_insert_duplicate);
      if (ctx->failed) {
        return;
      }
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }
}

static void validate_table_statement_from(MyliteParseContext *ctx,
                                          MyliteToken start,
                                          int parenthesized_boundary) {
  enum {
    TABLE_AFTER_TABLE,
    TABLE_AFTER_NAME,
    TABLE_AFTER_DOT,
    TABLE_AFTER_TARGET,
    TABLE_AFTER_ORDER,
    TABLE_AFTER_ORDER_BY,
    TABLE_IN_ORDER_EXPR,
    TABLE_AFTER_ORDER_DIRECTION,
    TABLE_AFTER_LIMIT,
    TABLE_AFTER_LIMIT_VALUE,
    TABLE_AFTER_LIMIT_COMMA,
    TABLE_AFTER_LIMIT_OFFSET,
    TABLE_AFTER_LIMIT_FINAL_VALUE,
    TABLE_AFTER_INTO,
    TABLE_AFTER_INTO_AT,
    TABLE_AFTER_INTO_FILE,
    TABLE_AFTER_INTO_TARGET
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  MyliteToken order_previous_top_token = start;
  int token_id;
  int saw_statement = 0;
  int state = TABLE_AFTER_TABLE;
  int order_depth = 0;
  int order_previous_top_token_id = 0;
  int order_previous_was_operator = 1;
  MyliteExpressionStack order_expression_stack = {0};

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (order_depth > 0) {
      if (!query_expression_depth_token(
              ctx, token_id, token, &order_depth, &order_expression_stack,
              "malformed TABLE ORDER BY clause")) {
        return;
      }
      if (token_closes_nested_expression(token_id) && order_depth == 0) {
        order_previous_top_token_id = token_id;
        order_previous_top_token = token;
        order_previous_was_operator = 0;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token) ||
        (parenthesized_boundary && token_id == ML_RP)) {
      if (state == TABLE_AFTER_TABLE || state == TABLE_AFTER_DOT) {
        mylite_parser_reject(ctx, pending_token, "incomplete TABLE target");
      } else if (state == TABLE_AFTER_ORDER || state == TABLE_AFTER_ORDER_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE ORDER BY clause");
      } else if (state == TABLE_IN_ORDER_EXPR && order_previous_was_operator) {
        mylite_parser_reject(ctx, order_previous_top_token,
                             "incomplete TABLE ORDER BY clause");
      } else if (state == TABLE_AFTER_LIMIT ||
                 state == TABLE_AFTER_LIMIT_COMMA ||
                 state == TABLE_AFTER_LIMIT_OFFSET) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE LIMIT clause");
      } else if (state == TABLE_AFTER_INTO ||
                 state == TABLE_AFTER_INTO_AT ||
                 state == TABLE_AFTER_INTO_FILE) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE INTO clause");
      }
      return;
    }

    if (state == TABLE_AFTER_TABLE || state == TABLE_AFTER_DOT) {
      if (!dml_row_alias_token(token_id)) {
        mylite_parser_reject(ctx, pending_token, "incomplete TABLE target");
        return;
      }
      state = state == TABLE_AFTER_TABLE ? TABLE_AFTER_NAME : TABLE_AFTER_TARGET;
      pending_token = token;
      continue;
    }

    if (state == TABLE_AFTER_NAME) {
      if (token_id == ML_DOT) {
        state = TABLE_AFTER_DOT;
        pending_token = token;
        continue;
      }
      state = TABLE_AFTER_TARGET;
    }

    if (state == TABLE_AFTER_TARGET) {
      if (select_set_operator(token_id)) {
        mylite_parser_validate_select_statement_from(ctx, token);
        return;
      }
      if (token_id == ML_ORDER) {
        state = TABLE_AFTER_ORDER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        state = TABLE_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        state = TABLE_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token, "malformed TABLE statement");
      return;
    }

    if (state == TABLE_AFTER_ORDER) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE ORDER BY clause");
        return;
      }
      state = TABLE_AFTER_ORDER_BY;
      pending_token = token;
      order_previous_top_token_id = 0;
      order_previous_was_operator = 1;
      order_previous_top_token = token;
      continue;
    }

    if (state == TABLE_AFTER_ORDER_BY) {
      if (token_id == ML_ASC || token_id == ML_DESC || token_id == ML_COMMA ||
          token_id == ML_LIMIT || token_id == ML_INTO ||
          select_set_operator(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE ORDER BY clause");
        return;
      }
      state = TABLE_IN_ORDER_EXPR;
    }

    if (state == TABLE_IN_ORDER_EXPR) {
      if (token_id == ML_COMMA || token_id == ML_LIMIT || token_id == ML_INTO ||
          select_set_operator(token_id)) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete TABLE ORDER BY clause");
          return;
        }
        if (token_id == ML_COMMA) {
          state = TABLE_AFTER_ORDER_BY;
          pending_token = token;
          order_previous_top_token_id = 0;
          order_previous_was_operator = 1;
          order_previous_top_token = token;
          continue;
        }
        if (token_id == ML_LIMIT) {
          state = TABLE_AFTER_LIMIT;
          pending_token = token;
          continue;
        }
        if (token_id == ML_INTO) {
          state = TABLE_AFTER_INTO;
          pending_token = token;
          continue;
        }
        mylite_parser_validate_select_statement_from(ctx, token);
        return;
      }
      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete TABLE ORDER BY clause");
          return;
        }
        state = TABLE_AFTER_ORDER_DIRECTION;
        pending_token = token;
        continue;
      }
      if (!query_expression_token(
              ctx, token_id, token, &order_depth, &order_previous_top_token_id,
              &order_previous_top_token, &order_previous_was_operator,
              &order_expression_stack, "malformed TABLE ORDER BY clause")) {
        return;
      }
      continue;
    }

    if (state == TABLE_AFTER_ORDER_DIRECTION) {
      if (token_id == ML_COMMA) {
        state = TABLE_AFTER_ORDER_BY;
        pending_token = token;
        order_previous_top_token_id = 0;
        order_previous_was_operator = 1;
        order_previous_top_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        state = TABLE_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        state = TABLE_AFTER_INTO;
        pending_token = token;
        continue;
      }
      if (select_set_operator(token_id)) {
        mylite_parser_validate_select_statement_from(ctx, token);
        return;
      }
      mylite_parser_reject(ctx, token, "malformed TABLE ORDER BY clause");
      return;
    }

    if (state == TABLE_AFTER_LIMIT || state == TABLE_AFTER_LIMIT_COMMA ||
        state == TABLE_AFTER_LIMIT_OFFSET) {
      if (!select_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token, "incomplete TABLE LIMIT clause");
        return;
      }
      state = state == TABLE_AFTER_LIMIT ? TABLE_AFTER_LIMIT_VALUE
                                         : TABLE_AFTER_LIMIT_FINAL_VALUE;
      continue;
    }

    if (state == TABLE_AFTER_LIMIT_VALUE) {
      if (token_id == ML_COMMA) {
        state = TABLE_AFTER_LIMIT_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        state = TABLE_AFTER_LIMIT_OFFSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        state = TABLE_AFTER_INTO;
        pending_token = token;
        continue;
      }
      if (select_set_operator(token_id)) {
        mylite_parser_validate_select_statement_from(ctx, token);
        return;
      }
      mylite_parser_reject(ctx, token, "malformed TABLE LIMIT clause");
      return;
    }

    if (state == TABLE_AFTER_LIMIT_FINAL_VALUE) {
      if (token_id == ML_INTO) {
        state = TABLE_AFTER_INTO;
        pending_token = token;
        continue;
      }
      if (select_set_operator(token_id)) {
        mylite_parser_validate_select_statement_from(ctx, token);
        return;
      }
      mylite_parser_reject(ctx, token, "malformed TABLE LIMIT clause");
      return;
    }

    if (state == TABLE_AFTER_INTO) {
      if (token_id == ML_OUTFILE || token_id == ML_DUMPFILE) {
        state = TABLE_AFTER_INTO_FILE;
        pending_token = token;
        continue;
      }
      if (token_id == ML_AT_SIGN) {
        state = TABLE_AFTER_INTO_AT;
        pending_token = token;
        continue;
      }
      if (!dml_row_alias_token(token_id) && token_id != ML_AT_HOST) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE INTO clause");
        return;
      }
      state = TABLE_AFTER_INTO_TARGET;
      continue;
    }

    if (state == TABLE_AFTER_INTO_AT) {
      if (!dml_row_alias_token(token_id) &&
          token_id != ML_DOUBLE_QUOTED_STRING &&
          token_id != ML_STRING_LITERAL) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE INTO clause");
        return;
      }
      state = TABLE_AFTER_INTO_TARGET;
      continue;
    }

    if (state == TABLE_AFTER_INTO_FILE) {
      if (!create_table_tail_option_string_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete TABLE INTO clause");
        return;
      }
      state = TABLE_AFTER_INTO_TARGET;
      continue;
    }

    if (state == TABLE_AFTER_INTO_TARGET) {
      if (token_id == ML_COMMA) {
        state = TABLE_AFTER_INTO;
        pending_token = token;
        continue;
      }
      return;
    }
  }

  if (state == TABLE_AFTER_TABLE || state == TABLE_AFTER_DOT) {
    mylite_parser_reject(ctx, pending_token, "incomplete TABLE target");
  } else if (state == TABLE_AFTER_ORDER || state == TABLE_AFTER_ORDER_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete TABLE ORDER BY clause");
  } else if (state == TABLE_IN_ORDER_EXPR && order_previous_was_operator) {
    mylite_parser_reject(ctx, order_previous_top_token,
                         "incomplete TABLE ORDER BY clause");
  } else if (state == TABLE_AFTER_LIMIT ||
             state == TABLE_AFTER_LIMIT_COMMA ||
             state == TABLE_AFTER_LIMIT_OFFSET) {
    mylite_parser_reject(ctx, pending_token, "incomplete TABLE LIMIT clause");
  } else if (state == TABLE_AFTER_INTO || state == TABLE_AFTER_INTO_AT ||
             state == TABLE_AFTER_INTO_FILE) {
    mylite_parser_reject(ctx, pending_token, "incomplete TABLE INTO clause");
  }
}

void mylite_parser_validate_parenthesized_statement(MyliteParseContext *ctx,
                                                    MyliteToken start) {
  enum {
    PAREN_QUERY_IN_BODY,
    PAREN_QUERY_AFTER_RP,
    PAREN_QUERY_AFTER_ORDER,
    PAREN_QUERY_AFTER_ORDER_BY,
    PAREN_QUERY_AFTER_ORDER_EXPR,
    PAREN_QUERY_AFTER_ORDER_DIRECTION,
    PAREN_QUERY_AFTER_LIMIT,
    PAREN_QUERY_AFTER_LIMIT_VALUE,
    PAREN_QUERY_AFTER_LIMIT_COMMA,
    PAREN_QUERY_AFTER_LIMIT_OFFSET,
    PAREN_QUERY_AFTER_LIMIT_FINAL_VALUE,
    PAREN_QUERY_AFTER_INTO,
    PAREN_QUERY_AFTER_INTO_AT,
    PAREN_QUERY_AFTER_INTO_TARGET,
    PAREN_QUERY_AFTER_OUTFILE,
    PAREN_QUERY_AFTER_DUMPFILE,
    PAREN_QUERY_OUTFILE_READY,
    PAREN_QUERY_DUMPFILE_READY,
    PAREN_QUERY_AFTER_CHARACTER,
    PAREN_QUERY_AFTER_CHARSET,
    PAREN_QUERY_AFTER_FIELDS,
    PAREN_QUERY_AFTER_FIELD_OPTION,
    PAREN_QUERY_AFTER_FIELD_BY,
    PAREN_QUERY_AFTER_OPTIONALLY,
    PAREN_QUERY_AFTER_OPTIONALLY_ENCLOSED,
    PAREN_QUERY_AFTER_LINES,
    PAREN_QUERY_AFTER_LINE_OPTION,
    PAREN_QUERY_AFTER_LINE_BY
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_start = 0;
  int depth = 0;
  int state = PAREN_QUERY_IN_BODY;
  int outfile_fields = 0;
  int outfile_lines = 0;
  int order_depth = 0;
  MyliteExpressionStack order_expression_stack = {0};
  int order_previous_top_token_id = 0;
  int order_previous_was_operator = 1;
  MyliteToken order_previous_top_token = start;

  validate_parenthesized_query_body_from(ctx, start);
  if (ctx->failed) {
    return;
  }

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_start) {
      if (token.offset == start.offset) {
        saw_start = 1;
        depth = 1;
      }
      continue;
    }

    if (state == PAREN_QUERY_IN_BODY) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0) {
          state = PAREN_QUERY_AFTER_RP;
          pending_token = token;
        }
      }
      continue;
    }

    if (order_depth > 0) {
      if (!query_expression_depth_token(
              ctx, token_id, token, &order_depth, &order_expression_stack,
              "malformed parenthesized query ORDER BY")) {
        return;
      }
      if (token_closes_nested_expression(token_id)) {
        if (order_depth == 0) {
          order_previous_top_token_id = token_id;
          order_previous_top_token = token;
          order_previous_was_operator = 0;
        }
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (state == PAREN_QUERY_AFTER_RP) {
      if (select_set_operator(token_id)) {
        validate_query_set_operand_after_operator_from(
            ctx, token, "incomplete parenthesized query expression");
        return;
      }
      if (token_id == ML_ORDER) {
        state = PAREN_QUERY_AFTER_ORDER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        state = PAREN_QUERY_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        state = PAREN_QUERY_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed parenthesized query expression");
      return;
    }

    if (state == PAREN_QUERY_AFTER_ORDER) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query ORDER BY");
        return;
      }
      state = PAREN_QUERY_AFTER_ORDER_BY;
      pending_token = token;
      order_previous_top_token_id = 0;
      order_previous_was_operator = 1;
      order_previous_top_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_ORDER_BY) {
      if (select_operand_boundary(token_id) || token_id == ML_ASC ||
          token_id == ML_DESC) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query ORDER BY");
        return;
      }
      state = PAREN_QUERY_AFTER_ORDER_EXPR;
    }

    if (state == PAREN_QUERY_AFTER_ORDER_EXPR) {
      if (parenthesized_query_order_boundary(token_id)) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete parenthesized query ORDER BY");
          return;
        }
        if (token_id == ML_COMMA) {
          state = PAREN_QUERY_AFTER_ORDER_BY;
          pending_token = token;
          order_previous_top_token_id = 0;
          order_previous_was_operator = 1;
          order_previous_top_token = token;
          continue;
        }
        if (token_id == ML_LIMIT) {
          state = PAREN_QUERY_AFTER_LIMIT;
          pending_token = token;
          continue;
        }
        if (token_id == ML_INTO) {
          state = PAREN_QUERY_AFTER_INTO;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "malformed parenthesized query ORDER BY");
        return;
      }
      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete parenthesized query ORDER BY");
          return;
        }
        state = PAREN_QUERY_AFTER_ORDER_DIRECTION;
        pending_token = token;
        continue;
      }
      if (!query_expression_token(
              ctx, token_id, token, &order_depth,
              &order_previous_top_token_id, &order_previous_top_token,
              &order_previous_was_operator, &order_expression_stack,
              "malformed parenthesized query ORDER BY")) {
        return;
      }
      continue;
    }

    if (state == PAREN_QUERY_AFTER_ORDER_DIRECTION) {
      if (token_id == ML_COMMA) {
        state = PAREN_QUERY_AFTER_ORDER_BY;
        pending_token = token;
        order_previous_top_token_id = 0;
        order_previous_was_operator = 1;
        order_previous_top_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        state = PAREN_QUERY_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        state = PAREN_QUERY_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed parenthesized query ORDER BY");
      return;
    }

    if (state == PAREN_QUERY_AFTER_LIMIT ||
        state == PAREN_QUERY_AFTER_LIMIT_COMMA ||
        state == PAREN_QUERY_AFTER_LIMIT_OFFSET) {
      if (!select_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query LIMIT");
        return;
      }
      state = state == PAREN_QUERY_AFTER_LIMIT
                  ? PAREN_QUERY_AFTER_LIMIT_VALUE
                  : PAREN_QUERY_AFTER_LIMIT_FINAL_VALUE;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_LIMIT_VALUE) {
      if (token_id == ML_COMMA) {
        state = PAREN_QUERY_AFTER_LIMIT_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        state = PAREN_QUERY_AFTER_LIMIT_OFFSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        state = PAREN_QUERY_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed parenthesized query LIMIT");
      return;
    }

    if (state == PAREN_QUERY_AFTER_LIMIT_FINAL_VALUE) {
      if (token_id == ML_INTO) {
        state = PAREN_QUERY_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed parenthesized query LIMIT");
      return;
    }

    if (state == PAREN_QUERY_AFTER_INTO) {
      if (token_id == ML_OUTFILE) {
        state = PAREN_QUERY_AFTER_OUTFILE;
        outfile_fields = 0;
        outfile_lines = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_DUMPFILE) {
        state = PAREN_QUERY_AFTER_DUMPFILE;
        pending_token = token;
        continue;
      }
      if (token_id == ML_AT_SIGN) {
        state = PAREN_QUERY_AFTER_INTO_AT;
        pending_token = token;
        continue;
      }
      if (!select_window_name_token(token_id, token) &&
          token_id != ML_AT_HOST) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO");
        return;
      }
      state = PAREN_QUERY_AFTER_INTO_TARGET;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_INTO_AT) {
      if (!select_window_name_token(token_id, token) &&
          token_id != ML_DOUBLE_QUOTED_STRING &&
          token_id != ML_STRING_LITERAL) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO");
        return;
      }
      state = PAREN_QUERY_AFTER_INTO_TARGET;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_INTO_TARGET) {
      if (token_id == ML_COMMA) {
        state = PAREN_QUERY_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed parenthesized query INTO");
      return;
    }

    if (state == PAREN_QUERY_AFTER_OUTFILE) {
      if (!create_table_tail_option_string_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO file target");
        return;
      }
      state = PAREN_QUERY_OUTFILE_READY;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_DUMPFILE) {
      if (!create_table_tail_option_string_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO file target");
        return;
      }
      state = PAREN_QUERY_DUMPFILE_READY;
      continue;
    }

    if (state == PAREN_QUERY_OUTFILE_READY) {
      if (token_id == ML_CHARACTER) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed parenthesized query INTO OUTFILE option");
          return;
        }
        state = PAREN_QUERY_AFTER_CHARACTER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHARSET) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed parenthesized query INTO OUTFILE option");
          return;
        }
        state = PAREN_QUERY_AFTER_CHARSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_FIELDS || token_id == ML_COLUMNS) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed parenthesized query INTO OUTFILE option");
          return;
        }
        outfile_fields = 1;
        state = PAREN_QUERY_AFTER_FIELDS;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LINES) {
        if (outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed parenthesized query INTO OUTFILE option");
          return;
        }
        outfile_lines = 1;
        state = PAREN_QUERY_AFTER_LINES;
        pending_token = token;
        continue;
      }
      if (select_outfile_line_option_start(token_id) && outfile_lines) {
        state = PAREN_QUERY_AFTER_LINE_OPTION;
        pending_token = token;
        continue;
      }
      if (select_outfile_field_option_start(token_id)) {
        if (!outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed parenthesized query INTO OUTFILE option");
          return;
        }
        state = token_id == ML_OPTIONALLY ? PAREN_QUERY_AFTER_OPTIONALLY
                                          : PAREN_QUERY_AFTER_FIELD_OPTION;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed parenthesized query INTO OUTFILE option");
      return;
    }

    if (state == PAREN_QUERY_DUMPFILE_READY) {
      if (select_into_output_option_start(token_id)) {
        mylite_parser_reject(ctx, token,
                             "malformed parenthesized query INTO DUMPFILE option");
        return;
      }
      mylite_parser_reject(ctx, token,
                           "malformed parenthesized query INTO DUMPFILE option");
      return;
    }

    if (state == PAREN_QUERY_AFTER_CHARACTER) {
      if (token_id != ML_SET) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE charset");
        return;
      }
      state = PAREN_QUERY_AFTER_CHARSET;
      pending_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_CHARSET) {
      if (!select_charset_name_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE charset");
        return;
      }
      state = PAREN_QUERY_OUTFILE_READY;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_FIELDS) {
      if (!select_outfile_field_option_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE fields option");
        return;
      }
      state = token_id == ML_OPTIONALLY ? PAREN_QUERY_AFTER_OPTIONALLY
                                        : PAREN_QUERY_AFTER_FIELD_OPTION;
      pending_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_FIELD_OPTION) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE fields option");
        return;
      }
      state = PAREN_QUERY_AFTER_FIELD_BY;
      pending_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_FIELD_BY) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE fields option");
        return;
      }
      state = PAREN_QUERY_OUTFILE_READY;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_OPTIONALLY) {
      if (token_id != ML_ENCLOSED) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE fields option");
        return;
      }
      state = PAREN_QUERY_AFTER_OPTIONALLY_ENCLOSED;
      pending_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_OPTIONALLY_ENCLOSED) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE fields option");
        return;
      }
      state = PAREN_QUERY_AFTER_FIELD_BY;
      pending_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_LINES) {
      if (!select_outfile_line_option_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE lines option");
        return;
      }
      state = PAREN_QUERY_AFTER_LINE_OPTION;
      pending_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_LINE_OPTION) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE lines option");
        return;
      }
      state = PAREN_QUERY_AFTER_LINE_BY;
      pending_token = token;
      continue;
    }

    if (state == PAREN_QUERY_AFTER_LINE_BY) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete parenthesized query INTO OUTFILE lines option");
        return;
      }
      state = PAREN_QUERY_OUTFILE_READY;
      continue;
    }
  }

  if (state == PAREN_QUERY_AFTER_ORDER) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query ORDER BY");
  } else if (state == PAREN_QUERY_AFTER_ORDER_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query ORDER BY");
  } else if (state == PAREN_QUERY_AFTER_ORDER_EXPR &&
             order_previous_was_operator) {
    mylite_parser_reject(ctx, order_previous_top_token,
                         "incomplete parenthesized query ORDER BY");
  } else if (state == PAREN_QUERY_AFTER_LIMIT ||
             state == PAREN_QUERY_AFTER_LIMIT_COMMA ||
             state == PAREN_QUERY_AFTER_LIMIT_OFFSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query LIMIT");
  } else if (state == PAREN_QUERY_AFTER_INTO ||
             state == PAREN_QUERY_AFTER_INTO_AT) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query INTO");
  } else if (state == PAREN_QUERY_AFTER_OUTFILE ||
             state == PAREN_QUERY_AFTER_DUMPFILE) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query INTO file target");
  } else if (state == PAREN_QUERY_AFTER_CHARACTER ||
             state == PAREN_QUERY_AFTER_CHARSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query INTO OUTFILE charset");
  } else if (state == PAREN_QUERY_AFTER_FIELDS ||
             state == PAREN_QUERY_AFTER_FIELD_OPTION ||
             state == PAREN_QUERY_AFTER_FIELD_BY ||
             state == PAREN_QUERY_AFTER_OPTIONALLY ||
             state == PAREN_QUERY_AFTER_OPTIONALLY_ENCLOSED) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query INTO OUTFILE fields option");
  } else if (state == PAREN_QUERY_AFTER_LINES ||
             state == PAREN_QUERY_AFTER_LINE_OPTION ||
             state == PAREN_QUERY_AFTER_LINE_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete parenthesized query INTO OUTFILE lines option");
  }
}

static void validate_query_set_operand_after_operator_from(
    MyliteParseContext *ctx, MyliteToken start, const char *message) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_operator = 0;
  int set_option_seen = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_operator) {
      if (token.offset == start.offset) {
        saw_operator = 1;
      }
      continue;
    }

    if (select_set_option(token_id)) {
      if (set_option_seen) {
        mylite_parser_reject(ctx, token, "malformed SELECT set operation");
        return;
      }
      set_option_seen = 1;
      continue;
    }

    if (!select_set_operand_start(token_id)) {
      mylite_parser_reject(ctx, start, message);
      return;
    }

    validate_query_body_from(ctx, token_id, token);
    return;
  }

  mylite_parser_reject(ctx, start, message);
}

static void validate_parenthesized_query_body_from(MyliteParseContext *ctx,
                                                   MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_start = 0;
  int depth = 0;
  int query_validated = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_start) {
      if (token.offset == start.offset) {
        saw_start = 1;
        depth = 1;
      }
      continue;
    }

    if (depth <= 0) {
      return;
    }

    if (depth == 1) {
      if (token_id == ML_SELECT) {
        if (!query_validated) {
          mylite_parser_validate_select_statement_from(ctx, token);
          if (ctx->failed) {
            return;
          }
          query_validated = 1;
        }
        validate_select_list_tail_from(ctx, token, 1, 1, 0);
        if (ctx->failed) {
          return;
        }
      } else if (!query_validated && token_id == ML_TABLE) {
        validate_table_statement_from(ctx, token, 1);
        if (ctx->failed) {
          return;
        }
        query_validated = 1;
      } else if (!query_validated && token_id == ML_VALUES) {
        mylite_parser_validate_values_statement_from(ctx, token);
        if (ctx->failed) {
          return;
        }
        query_validated = 1;
      } else if (!query_validated && token_id == ML_WITH) {
        mylite_parser_validate_with_statement_from(ctx, token);
        if (ctx->failed) {
          return;
        }
        query_validated = 1;
      }
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
      continue;
    }
    if (token_closes_nested_expression(token_id)) {
      depth--;
      if (depth == 0) {
        return;
      }
    }
  }
}

void mylite_parser_validate_dml_statement(MyliteParseContext *ctx,
                                          MyliteToken start,
                                          MyliteStatementKind kind) {
  enum {
    DML_ASSIGN_NONE,
    DML_ASSIGN_TARGET,
    DML_ASSIGN_AFTER_TARGET,
    DML_ASSIGN_AFTER_DOT,
    DML_ASSIGN_VALUE
  };
  enum {
    DML_DUP_NONE,
    DML_DUP_AFTER_ON,
    DML_DUP_AFTER_DUPLICATE,
    DML_DUP_AFTER_KEY
  };
  enum {
    DML_WHERE_NONE,
    DML_WHERE_AFTER_WHERE,
    DML_WHERE_STARTED
  };
  enum {
    DML_ORDER_NONE,
    DML_ORDER_AFTER_ORDER,
    DML_ORDER_AFTER_BY,
    DML_ORDER_STARTED,
    DML_ORDER_AFTER_DIRECTION
  };
  enum {
    DML_LIMIT_NONE,
    DML_LIMIT_AFTER_LIMIT,
    DML_LIMIT_AFTER_VALUE
  };
  enum {
    DML_PAYLOAD_NONE,
    DML_PAYLOAD_SET,
    DML_PAYLOAD_VALUES,
    DML_PAYLOAD_QUERY
  };
  enum {
    DML_QUERY_TAIL_NONE,
    DML_QUERY_TAIL_AFTER_RP,
    DML_QUERY_TAIL_AFTER_ORDER,
    DML_QUERY_TAIL_AFTER_ORDER_BY,
    DML_QUERY_TAIL_ORDER_EXPR,
    DML_QUERY_TAIL_ORDER_DIRECTION,
    DML_QUERY_TAIL_AFTER_LIMIT,
    DML_QUERY_TAIL_AFTER_LIMIT_VALUE,
    DML_QUERY_TAIL_AFTER_LIMIT_COMMA,
    DML_QUERY_TAIL_AFTER_LIMIT_OFFSET,
    DML_QUERY_TAIL_AFTER_LIMIT_FINAL_VALUE
  };
  enum {
    DML_VALUES_NONE,
    DML_VALUES_AFTER_VALUES,
    DML_VALUES_AFTER_ROW_KEYWORD,
    DML_VALUES_IN_ROW,
    DML_VALUES_AFTER_ROW,
    DML_VALUES_AFTER_COMMA,
    DML_VALUES_AFTER_AS,
    DML_VALUES_AFTER_ALIAS,
    DML_VALUES_AFTER_ALIAS_LP,
    DML_VALUES_AFTER_ALIAS_COLUMN,
    DML_VALUES_AFTER_ALIAS_COMMA,
    DML_VALUES_AFTER_ALIAS_RP
  };
  enum {
    DML_SET_ALIAS_NONE,
    DML_SET_ALIAS_AFTER_AS,
    DML_SET_ALIAS_AFTER_ALIAS,
    DML_SET_ALIAS_AFTER_LP,
    DML_SET_ALIAS_AFTER_COLUMN,
    DML_SET_ALIAS_AFTER_COMMA,
    DML_SET_ALIAS_AFTER_RP
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  MyliteToken values_row_last_token = start;
  MyliteToken where_previous_top_token = start;
  MyliteToken order_previous_top_token = start;
  MyliteToken query_order_previous_top_token = start;
  int token_id;
  int values_row_last_token_id = 0;
  int saw_statement = 0;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int assignment_state = DML_ASSIGN_NONE;
  int assignment_mode = DML_ASSIGNMENT_NONE;
  int assignment_value_started = 0;
  int assignment_value_last_token_id = 0;
  int assignment_value_previous_top_token_id = 0;
  int assignment_value_previous_was_operator = 1;
  MyliteToken assignment_value_last_token = start;
  MyliteToken assignment_value_previous_top_token = start;
  int duplicate_state = DML_DUP_NONE;
  int duplicate_strict = 0;
  int where_state = DML_WHERE_NONE;
  int where_previous_top_token_id = 0;
  int where_previous_was_operator = 1;
  int where_match_list = 0;
  int order_state = DML_ORDER_NONE;
  int order_previous_top_token_id = 0;
  int order_previous_was_operator = 1;
  int limit_state = DML_LIMIT_NONE;
  int seen_where = 0;
  int seen_order = 0;
  int seen_limit = 0;
  int payload_kind = DML_PAYLOAD_NONE;
  int query_parenthesized_payload = 0;
  int query_tail_state = DML_QUERY_TAIL_NONE;
  int query_order_previous_top_token_id = 0;
  int query_order_previous_was_operator = 1;
  int values_state = DML_VALUES_NONE;
  int values_row_keyword_allowed = 0;
  int set_alias_state = DML_SET_ALIAS_NONE;
  int insert_modifier_scan = kind == MYLITE_STATEMENT_INSERT;
  int insert_priority_modifier_seen = 0;
  int insert_ignore_modifier_seen = 0;
  int replace_modifier_scan = kind == MYLITE_STATEMENT_REPLACE;
  int replace_modifier_seen = 0;
  int update_table_condition_started = 0;
  int update_table_alias_pending = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      } else {
        continue;
      }
    }

    if (insert_modifier_scan) {
      if (token.offset == start.offset) {
        continue;
      }
      if (token_id == ML_DELAYED || token_id == ML_HIGH_PRIORITY ||
          token_id == ML_LOW_PRIORITY) {
        if (insert_priority_modifier_seen || insert_ignore_modifier_seen) {
          mylite_parser_reject(ctx, token, "malformed INSERT modifier");
          return;
        }
        insert_priority_modifier_seen = 1;
        continue;
      }
      if (token_id == ML_IGNORE) {
        if (insert_ignore_modifier_seen) {
          mylite_parser_reject(ctx, token, "malformed INSERT modifier");
          return;
        }
        insert_ignore_modifier_seen = 1;
        continue;
      }
      insert_modifier_scan = 0;
    }

    if (replace_modifier_scan) {
      if (token.offset == start.offset) {
        continue;
      }
      if (token_id == ML_DELAYED || token_id == ML_LOW_PRIORITY) {
        if (replace_modifier_seen) {
          mylite_parser_reject(ctx, token, "malformed REPLACE modifier");
          return;
        }
        replace_modifier_seen = 1;
        continue;
      }
      replace_modifier_scan = 0;
    }

    if (depth > 0) {
      const char *nested_message = "malformed DML expression";
      if (values_state == DML_VALUES_IN_ROW) {
        values_row_last_token = token;
        values_row_last_token_id = token_id;
      }
      if (values_state == DML_VALUES_IN_ROW) {
        nested_message = "malformed DML VALUES row list";
      } else if (assignment_state == DML_ASSIGN_VALUE) {
        nested_message = "malformed DML assignment";
      } else if (where_state == DML_WHERE_STARTED) {
        nested_message = "malformed DML WHERE clause";
      } else if (order_state == DML_ORDER_STARTED) {
        nested_message = "malformed DML ORDER BY clause";
      } else if (query_tail_state == DML_QUERY_TAIL_ORDER_EXPR) {
        nested_message = "malformed DML parenthesized query ORDER BY";
      }
      if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                        &expression_stack, nested_message)) {
        return;
      }
      if (token_closes_nested_expression(token_id)) {
        if (depth == 0 && order_state == DML_ORDER_STARTED) {
          order_previous_top_token_id = token_id;
          order_previous_top_token = token;
          order_previous_was_operator = 0;
        }
        if (depth == 0 && query_tail_state == DML_QUERY_TAIL_ORDER_EXPR) {
          query_order_previous_top_token_id = token_id;
          query_order_previous_top_token = token;
          query_order_previous_was_operator = 0;
        }
        if (depth == 0 && where_state == DML_WHERE_STARTED) {
          where_previous_top_token_id = token_id;
          where_previous_top_token = token;
          where_previous_was_operator = 0;
        }
        if (depth == 0 && assignment_state == DML_ASSIGN_VALUE) {
          assignment_value_previous_top_token_id = token_id;
          assignment_value_previous_top_token = token;
          assignment_value_previous_was_operator = 0;
        }
        if (depth == 0 && query_parenthesized_payload) {
          query_parenthesized_payload = 0;
          query_tail_state = DML_QUERY_TAIL_AFTER_RP;
        }
        if (depth == 0 && values_state == DML_VALUES_IN_ROW) {
          values_state = DML_VALUES_AFTER_ROW;
        }
      }
      continue;
    }

    if (assignment_state != DML_ASSIGN_NONE) {
      int boundary = dml_assignment_boundary(assignment_mode, token_id);
      if (token_id == ML_SEMI || boundary) {
        if (assignment_state != DML_ASSIGN_VALUE ||
            !assignment_value_started ||
            assignment_value_previous_was_operator) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_NONE;
        assignment_mode = DML_ASSIGNMENT_NONE;
        assignment_value_started = 0;
        assignment_value_last_token_id = 0;
        assignment_value_previous_top_token_id = 0;
        assignment_value_previous_was_operator = 1;
        if (token_id == ML_SEMI) {
          break;
        }
      } else if (token_id == ML_COMMA) {
        if (assignment_state != DML_ASSIGN_VALUE ||
            !assignment_value_started ||
            assignment_value_previous_was_operator) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_TARGET;
        assignment_value_started = 0;
        assignment_value_last_token_id = 0;
        assignment_value_previous_top_token_id = 0;
        assignment_value_previous_was_operator = 1;
        assignment_value_previous_top_token = token;
        pending_token = token;
        continue;
      } else if (assignment_state == DML_ASSIGN_TARGET) {
        if (!dml_assignment_target_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_AFTER_TARGET;
        continue;
      } else if (assignment_state == DML_ASSIGN_AFTER_TARGET) {
        if (token_id == ML_DOT) {
          assignment_state = DML_ASSIGN_AFTER_DOT;
          pending_token = token;
          continue;
        }
        if (!dml_assignment_operator(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_VALUE;
        assignment_value_started = 0;
        assignment_value_last_token_id = 0;
        assignment_value_previous_top_token_id = 0;
        assignment_value_previous_was_operator = 1;
        assignment_value_previous_top_token = token;
        pending_token = token;
        continue;
      } else if (assignment_state == DML_ASSIGN_AFTER_DOT) {
        if (!dml_assignment_target_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_AFTER_TARGET;
        continue;
      } else if (assignment_state == DML_ASSIGN_VALUE) {
        if (assignment_value_started &&
            (token_id == ML_FROM || token_id == ML_SELECT ||
             token_id == ML_VALUE || token_id == ML_VALUES ||
             ((assignment_mode == DML_ASSIGNMENT_DUPLICATE ||
               assignment_mode == DML_ASSIGNMENT_UPDATE) &&
              token_id == ML_ON))) {
          if ((token_id != ML_VALUE && token_id != ML_VALUES) ||
              !dml_assignment_value_allows_function(
                  assignment_value_last_token_id,
                  assignment_value_last_token)) {
            mylite_parser_reject(ctx, token, "malformed DML assignment");
            return;
          }
        }
        if (dml_clause_operand_boundary(token_id) ||
            token_id == ML_COMMA || token_id == ML_SEMI ||
            token_id == ML_SET) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_value_started = 1;
        assignment_value_last_token_id = token_id;
        assignment_value_last_token = token;
        if (!query_expression_token_with_flags(
                ctx, token_id, token, &depth,
                &assignment_value_previous_top_token_id,
                &assignment_value_previous_top_token,
                &assignment_value_previous_was_operator, &expression_stack,
                QUERY_EXPRESSION_ALLOW_BARE_DEFAULT,
                "malformed DML assignment")) {
          return;
        }
        continue;
      }
    }

    if (values_state != DML_VALUES_NONE) {
      if (values_state == DML_VALUES_AFTER_VALUES ||
          values_state == DML_VALUES_AFTER_COMMA) {
        if (token_id == ML_ROW) {
          if (!values_row_keyword_allowed) {
            mylite_parser_reject(ctx, token,
                                 "malformed DML VALUES row list");
            return;
          }
          values_state = DML_VALUES_AFTER_ROW_KEYWORD;
          pending_token = token;
          continue;
        }
        if (token_id == ML_LP) {
          values_state = DML_VALUES_IN_ROW;
          values_row_last_token_id = 0;
          depth = 1;
          query_expression_stack_open_list(
              &expression_stack, depth, token, 1,
              QUERY_EXPRESSION_ALLOW_BARE_DEFAULT);
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML VALUES row list");
        return;
      }
      if (values_state == DML_VALUES_AFTER_ROW_KEYWORD) {
        if (token_id != ML_LP) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row list");
          return;
        }
        values_state = DML_VALUES_IN_ROW;
        values_row_last_token_id = 0;
        depth = 1;
        query_expression_stack_open_list(
            &expression_stack, depth, token, 1,
            QUERY_EXPRESSION_ALLOW_BARE_DEFAULT);
        pending_token = token;
        continue;
      }
      if (values_state == DML_VALUES_AFTER_ROW) {
        if (token_id == ML_COMMA) {
          values_state = DML_VALUES_AFTER_COMMA;
          pending_token = token;
          continue;
        }
        if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_AS) {
          values_state = DML_VALUES_AFTER_AS;
          pending_token = token;
          continue;
        }
        if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
          values_state = DML_VALUES_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed DML VALUES row list");
          return;
        }
      } else if (values_state == DML_VALUES_AFTER_AS) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row alias");
          return;
        }
        values_state = DML_VALUES_AFTER_ALIAS;
        continue;
      } else if (values_state == DML_VALUES_AFTER_ALIAS) {
        if (token_id == ML_LP) {
          values_state = DML_VALUES_AFTER_ALIAS_LP;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          values_state = DML_VALUES_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed DML VALUES row alias");
          return;
        }
      } else if (values_state == DML_VALUES_AFTER_ALIAS_LP) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row alias");
          return;
        }
        values_state = DML_VALUES_AFTER_ALIAS_COLUMN;
        continue;
      } else if (values_state == DML_VALUES_AFTER_ALIAS_COLUMN) {
        if (token_id == ML_COMMA) {
          values_state = DML_VALUES_AFTER_ALIAS_COMMA;
          pending_token = token;
          continue;
        }
        if (token_id == ML_RP) {
          values_state = DML_VALUES_AFTER_ALIAS_RP;
          continue;
        }
        mylite_parser_reject(ctx, token, "malformed DML VALUES row alias");
        return;
      } else if (values_state == DML_VALUES_AFTER_ALIAS_COMMA) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row alias");
          return;
        }
        values_state = DML_VALUES_AFTER_ALIAS_COLUMN;
        continue;
      } else if (values_state == DML_VALUES_AFTER_ALIAS_RP) {
        if (token_id == ML_ON) {
          values_state = DML_VALUES_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed DML VALUES row alias");
          return;
        }
      }
    }

    if (set_alias_state != DML_SET_ALIAS_NONE) {
      if (set_alias_state == DML_SET_ALIAS_AFTER_AS) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT SET row alias");
          return;
        }
        set_alias_state = DML_SET_ALIAS_AFTER_ALIAS;
        continue;
      }
      if (set_alias_state == DML_SET_ALIAS_AFTER_ALIAS) {
        if (token_id == ML_LP) {
          set_alias_state = DML_SET_ALIAS_AFTER_LP;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          set_alias_state = DML_SET_ALIAS_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed INSERT SET row alias");
          return;
        }
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_LP) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT SET row alias");
          return;
        }
        set_alias_state = DML_SET_ALIAS_AFTER_COLUMN;
        continue;
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_COLUMN) {
        if (token_id == ML_COMMA) {
          set_alias_state = DML_SET_ALIAS_AFTER_COMMA;
          pending_token = token;
          continue;
        }
        if (token_id == ML_RP) {
          set_alias_state = DML_SET_ALIAS_AFTER_RP;
          continue;
        }
        mylite_parser_reject(ctx, token, "malformed INSERT SET row alias");
        return;
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_COMMA) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT SET row alias");
          return;
        }
        set_alias_state = DML_SET_ALIAS_AFTER_COLUMN;
        continue;
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_RP) {
        if (token_id == ML_ON) {
          set_alias_state = DML_SET_ALIAS_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed INSERT SET row alias");
          return;
        }
      }
    }

    if (duplicate_state == DML_DUP_AFTER_ON) {
      if (token_id != ML_DUPLICATE) {
        if (duplicate_strict) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT duplicate key clause");
          return;
        }
        duplicate_state = DML_DUP_NONE;
        continue;
      }
      duplicate_state = DML_DUP_AFTER_DUPLICATE;
      continue;
    }
    if (duplicate_state == DML_DUP_AFTER_DUPLICATE) {
      if (token_id != ML_KEY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete INSERT duplicate key clause");
        return;
      }
      duplicate_state = DML_DUP_AFTER_KEY;
      continue;
    }
    if (duplicate_state == DML_DUP_AFTER_KEY) {
      if (token_id != ML_UPDATE) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete INSERT duplicate key clause");
        return;
      }
      duplicate_state = DML_DUP_NONE;
      assignment_state = DML_ASSIGN_TARGET;
      assignment_mode = DML_ASSIGNMENT_DUPLICATE;
      assignment_value_started = 0;
      pending_token = token;
      continue;
    }

    if (query_tail_state == DML_QUERY_TAIL_AFTER_RP) {
      if (token_id == ML_SEMI) {
        break;
      }
      if (select_set_operator(token_id)) {
        validate_query_set_operand_after_operator_from(
            ctx, token, "incomplete DML parenthesized query tail");
        if (ctx->failed) {
          return;
        }
        query_tail_state = DML_QUERY_TAIL_NONE;
        continue;
      }
      if (token_id == ML_ORDER) {
        query_tail_state = DML_QUERY_TAIL_AFTER_ORDER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        query_tail_state = DML_QUERY_TAIL_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
        query_tail_state = DML_QUERY_TAIL_NONE;
        duplicate_state = DML_DUP_AFTER_ON;
        duplicate_strict = 1;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed DML parenthesized query tail");
      return;
    }
    if (query_tail_state == DML_QUERY_TAIL_AFTER_ORDER) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML parenthesized query ORDER BY");
        return;
      }
      query_tail_state = DML_QUERY_TAIL_AFTER_ORDER_BY;
      pending_token = token;
      query_order_previous_top_token_id = 0;
      query_order_previous_was_operator = 1;
      query_order_previous_top_token = token;
      continue;
    }
    if (query_tail_state == DML_QUERY_TAIL_AFTER_ORDER_BY) {
      if (dml_query_order_boundary(token_id) || token_id == ML_ASC ||
          token_id == ML_DESC) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML parenthesized query ORDER BY");
        return;
      }
      query_tail_state = DML_QUERY_TAIL_ORDER_EXPR;
    }
    if (query_tail_state == DML_QUERY_TAIL_ORDER_EXPR) {
      if (dml_query_order_boundary(token_id)) {
        if (query_order_previous_was_operator) {
          mylite_parser_reject(
              ctx, query_order_previous_top_token,
              "incomplete DML parenthesized query ORDER BY");
          return;
        }
        if (token_id == ML_COMMA) {
          query_tail_state = DML_QUERY_TAIL_AFTER_ORDER_BY;
          pending_token = token;
          query_order_previous_top_token_id = 0;
          query_order_previous_was_operator = 1;
          query_order_previous_top_token = token;
          continue;
        }
        if (token_id == ML_LIMIT) {
          query_tail_state = DML_QUERY_TAIL_AFTER_LIMIT;
          pending_token = token;
          continue;
        }
        if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
          query_tail_state = DML_QUERY_TAIL_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id == ML_SEMI) {
          break;
        }
        mylite_parser_reject(ctx, token,
                             "malformed DML parenthesized query ORDER BY");
        return;
      }
      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (query_order_previous_was_operator) {
          mylite_parser_reject(
              ctx, query_order_previous_top_token,
              "incomplete DML parenthesized query ORDER BY");
          return;
        }
        query_tail_state = DML_QUERY_TAIL_ORDER_DIRECTION;
        pending_token = token;
        continue;
      }
      if (token_closes_nested_expression(token_id)) {
        mylite_parser_reject(ctx, token,
                             "malformed DML parenthesized query ORDER BY");
        return;
      }
      if (!query_expression_token(
              ctx, token_id, token, &depth, &query_order_previous_top_token_id,
              &query_order_previous_top_token,
              &query_order_previous_was_operator, &expression_stack,
              "malformed DML parenthesized query ORDER BY")) {
        return;
      }
      continue;
    }
    if (query_tail_state == DML_QUERY_TAIL_ORDER_DIRECTION) {
      if (token_id == ML_COMMA) {
        query_tail_state = DML_QUERY_TAIL_AFTER_ORDER_BY;
        pending_token = token;
        query_order_previous_top_token_id = 0;
        query_order_previous_was_operator = 1;
        query_order_previous_top_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        query_tail_state = DML_QUERY_TAIL_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
        query_tail_state = DML_QUERY_TAIL_NONE;
        duplicate_state = DML_DUP_AFTER_ON;
        duplicate_strict = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, token,
                           "malformed DML parenthesized query ORDER BY");
      return;
    }
    if (query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT ||
        query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT_COMMA ||
        query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT_OFFSET) {
      if (!dml_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML parenthesized query LIMIT");
        return;
      }
      query_tail_state = query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT
                             ? DML_QUERY_TAIL_AFTER_LIMIT_VALUE
                             : DML_QUERY_TAIL_AFTER_LIMIT_FINAL_VALUE;
      continue;
    }
    if (query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT_VALUE) {
      if (token_id == ML_COMMA) {
        query_tail_state = DML_QUERY_TAIL_AFTER_LIMIT_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        query_tail_state = DML_QUERY_TAIL_AFTER_LIMIT_OFFSET;
        pending_token = token;
        continue;
      }
      if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
        query_tail_state = DML_QUERY_TAIL_NONE;
        duplicate_state = DML_DUP_AFTER_ON;
        duplicate_strict = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, token,
                           "malformed DML parenthesized query LIMIT");
      return;
    }
    if (query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT_FINAL_VALUE) {
      if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
        query_tail_state = DML_QUERY_TAIL_NONE;
        duplicate_state = DML_DUP_AFTER_ON;
        duplicate_strict = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, token,
                           "malformed DML parenthesized query LIMIT");
      return;
    }

    if (where_state == DML_WHERE_AFTER_WHERE) {
      if (dml_clause_operand_boundary(token_id) || token_id == ML_COMMA) {
        mylite_parser_reject(ctx, pending_token, "incomplete DML WHERE clause");
        return;
      }
      where_state = DML_WHERE_STARTED;
      where_previous_top_token_id = 0;
      where_previous_was_operator = 1;
      where_match_list = 0;
      where_previous_top_token = token;
    }
    if (where_state == DML_WHERE_STARTED) {
      if (token_id == ML_COMMA) {
        if (!where_match_list) {
          mylite_parser_reject(ctx, token, "malformed DML WHERE clause");
          return;
        }
        where_previous_top_token_id = 0;
        where_previous_was_operator = 1;
        where_previous_top_token = token;
        continue;
      }
      if (dml_clause_operand_boundary(token_id)) {
        if (where_previous_was_operator) {
          mylite_parser_reject(ctx, where_previous_top_token,
                               "incomplete DML WHERE clause");
          return;
        }
        where_state = DML_WHERE_NONE;
        where_match_list = 0;
        if (token_id == ML_SEMI) {
          break;
        }
      } else if (token_closes_nested_expression(token_id)) {
        mylite_parser_reject(ctx, token, "malformed DML WHERE clause");
        return;
      } else {
        if (!query_expression_token(
                ctx, token_id, token, &depth, &where_previous_top_token_id,
                &where_previous_top_token, &where_previous_was_operator,
                &expression_stack,
                "malformed DML WHERE clause")) {
          return;
        }
        if (token_ascii_equal(token, "match")) {
          where_match_list = 1;
        } else if (where_match_list && token_ascii_equal(token, "against")) {
          where_match_list = 0;
        }
        continue;
      }
    }

    if (order_state == DML_ORDER_AFTER_ORDER) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML ORDER BY clause");
        return;
      }
      order_state = DML_ORDER_AFTER_BY;
      pending_token = token;
      continue;
    }
    if (order_state == DML_ORDER_AFTER_BY) {
      if (token_id == ML_COMMA || dml_clause_operand_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML ORDER BY clause");
        return;
      }
      order_state = DML_ORDER_STARTED;
      order_previous_top_token_id = 0;
      order_previous_was_operator = 1;
      order_previous_top_token = token;
      if (!query_expression_token(
              ctx, token_id, token, &depth, &order_previous_top_token_id,
              &order_previous_top_token, &order_previous_was_operator,
              &expression_stack,
              "malformed DML ORDER BY clause")) {
        return;
      }
      continue;
    }
    if (order_state == DML_ORDER_STARTED) {
      if (token_id == ML_COMMA) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete DML ORDER BY clause");
          return;
        }
        order_state = DML_ORDER_AFTER_BY;
        pending_token = token;
        continue;
      }
      if (token_id == ML_ORDER || token_id == ML_WHERE) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete DML ORDER BY clause");
          return;
        }
        order_state = DML_ORDER_AFTER_DIRECTION;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete DML ORDER BY clause");
          return;
        }
        order_state = DML_ORDER_NONE;
      } else if (token_id == ML_SEMI) {
        if (order_previous_was_operator) {
          mylite_parser_reject(ctx, order_previous_top_token,
                               "incomplete DML ORDER BY clause");
          return;
        }
        break;
      } else if (token_closes_nested_expression(token_id)) {
        mylite_parser_reject(ctx, token, "malformed DML ORDER BY clause");
        return;
      } else {
        if (!query_expression_token(
                ctx, token_id, token, &depth, &order_previous_top_token_id,
                &order_previous_top_token, &order_previous_was_operator,
                &expression_stack,
                "malformed DML ORDER BY clause")) {
          return;
        }
        continue;
      }
    }
    if (order_state == DML_ORDER_AFTER_DIRECTION) {
      if (token_id == ML_COMMA) {
        order_state = DML_ORDER_AFTER_BY;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        order_state = DML_ORDER_NONE;
      } else if (token_id == ML_SEMI) {
        break;
      } else {
        mylite_parser_reject(ctx, pending_token,
                             "malformed DML ORDER BY direction");
        return;
      }
    }

    if (limit_state == DML_LIMIT_AFTER_LIMIT) {
      if (!dml_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML LIMIT clause");
        return;
      }
      limit_state = DML_LIMIT_AFTER_VALUE;
      continue;
    }
    if (limit_state == DML_LIMIT_AFTER_VALUE) {
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed DML LIMIT clause");
      return;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (kind == MYLITE_STATEMENT_UPDATE && token_id == ML_SET) {
      if (update_table_alias_pending) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete UPDATE table alias");
        return;
      }
      assignment_state = DML_ASSIGN_TARGET;
      assignment_mode = DML_ASSIGNMENT_UPDATE;
      assignment_value_started = 0;
      pending_token = token;
      continue;
    }

    if (kind == MYLITE_STATEMENT_UPDATE &&
        assignment_state == DML_ASSIGN_NONE) {
      if (update_table_alias_pending) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete UPDATE table alias");
          return;
        }
        update_table_alias_pending = 0;
        continue;
      }
      if (token_id == ML_AS) {
        update_table_alias_pending = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_ON || token_id == ML_USING) {
        update_table_condition_started = 1;
      } else if (!update_table_condition_started &&
                 dml_literal_token(token_id, token)) {
        mylite_parser_reject(ctx, token, "invalid UPDATE table reference");
        return;
      }
    }

    if ((kind == MYLITE_STATEMENT_INSERT ||
         kind == MYLITE_STATEMENT_REPLACE) &&
        payload_kind == DML_PAYLOAD_NONE && token_id == ML_SET) {
      payload_kind = DML_PAYLOAD_SET;
      assignment_state = DML_ASSIGN_TARGET;
      assignment_mode = kind == MYLITE_STATEMENT_INSERT
                            ? DML_ASSIGNMENT_INSERT_SET
                            : DML_ASSIGNMENT_REPLACE_SET;
      assignment_value_started = 0;
      pending_token = token;
      continue;
    }

    if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
      duplicate_state = DML_DUP_AFTER_ON;
      duplicate_strict = payload_kind != DML_PAYLOAD_QUERY;
      pending_token = token;
      continue;
    }

    if (kind == MYLITE_STATEMENT_INSERT &&
        payload_kind == DML_PAYLOAD_SET && token_id == ML_AS) {
      set_alias_state = DML_SET_ALIAS_AFTER_AS;
      pending_token = token;
      continue;
    }

    if (kind == MYLITE_STATEMENT_REPLACE &&
        payload_kind == DML_PAYLOAD_SET && token_id == ML_AS) {
      mylite_parser_reject(ctx, token, "malformed REPLACE SET clause");
      return;
    }

    if ((kind == MYLITE_STATEMENT_INSERT ||
         kind == MYLITE_STATEMENT_REPLACE) &&
        payload_kind == DML_PAYLOAD_NONE &&
        (token_id == ML_VALUE || token_id == ML_VALUES)) {
      payload_kind = DML_PAYLOAD_VALUES;
      values_state = DML_VALUES_AFTER_VALUES;
      values_row_keyword_allowed = token_id == ML_VALUES;
      pending_token = token;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_INSERT ||
         kind == MYLITE_STATEMENT_REPLACE) &&
        payload_kind == DML_PAYLOAD_NONE &&
        token_id == ML_LP &&
        parenthesized_query_start_follows(ctx, token)) {
      validate_parenthesized_query_body_from(ctx, token);
      if (ctx->failed) {
        return;
      }
      payload_kind = DML_PAYLOAD_QUERY;
      query_parenthesized_payload = 1;
      depth++;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_INSERT ||
         kind == MYLITE_STATEMENT_REPLACE) &&
        payload_kind == DML_PAYLOAD_NONE &&
        (token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_WITH)) {
      if (token_id == ML_SELECT) {
        validate_select_list_tails_from(ctx, 1, token, 1);
      } else if (token_id == ML_TABLE) {
        mylite_parser_validate_table_statement_from(ctx, token);
      } else {
        mylite_parser_validate_with_statement_from(ctx, token);
      }
      if (ctx->failed) {
        return;
      }
      payload_kind = DML_PAYLOAD_QUERY;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_UPDATE ||
         kind == MYLITE_STATEMENT_DELETE) &&
        token_id == ML_WHERE) {
      if (seen_where || seen_order || seen_limit) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      seen_where = 1;
      where_state = DML_WHERE_AFTER_WHERE;
      pending_token = token;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_UPDATE ||
         kind == MYLITE_STATEMENT_DELETE) &&
        token_id == ML_ORDER) {
      if (seen_order || seen_limit) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      seen_order = 1;
      order_state = DML_ORDER_AFTER_ORDER;
      pending_token = token;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_UPDATE ||
         kind == MYLITE_STATEMENT_DELETE) &&
        token_id == ML_LIMIT) {
      if (seen_limit) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      seen_limit = 1;
      limit_state = DML_LIMIT_AFTER_LIMIT;
      pending_token = token;
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }

  if (assignment_state != DML_ASSIGN_NONE) {
    if (assignment_state == DML_ASSIGN_VALUE && assignment_value_started &&
        !assignment_value_previous_was_operator) {
      return;
    }
    mylite_parser_reject(ctx, pending_token, "incomplete DML assignment");
  } else if (duplicate_state != DML_DUP_NONE) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete INSERT duplicate key clause");
  } else if (values_state == DML_VALUES_AFTER_VALUES ||
             values_state == DML_VALUES_AFTER_ROW_KEYWORD ||
             values_state == DML_VALUES_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML VALUES row list");
  } else if (values_state == DML_VALUES_IN_ROW) {
    if (!dml_values_unclosed_string_fragment(values_row_last_token_id,
                                             values_row_last_token)) {
      mylite_parser_reject(ctx, pending_token,
                           "incomplete DML VALUES row list");
    }
  } else if (values_state == DML_VALUES_AFTER_AS ||
             values_state == DML_VALUES_AFTER_ALIAS_LP ||
             values_state == DML_VALUES_AFTER_ALIAS_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML VALUES row alias");
  } else if (set_alias_state == DML_SET_ALIAS_AFTER_AS ||
             set_alias_state == DML_SET_ALIAS_AFTER_LP ||
             set_alias_state == DML_SET_ALIAS_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete INSERT SET row alias");
  } else if (where_state == DML_WHERE_AFTER_WHERE) {
    mylite_parser_reject(ctx, pending_token, "incomplete DML WHERE clause");
  } else if (where_state == DML_WHERE_STARTED &&
             where_previous_was_operator) {
    mylite_parser_reject(ctx, where_previous_top_token,
                         "incomplete DML WHERE clause");
  } else if (order_state == DML_ORDER_AFTER_ORDER ||
             order_state == DML_ORDER_AFTER_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML ORDER BY clause");
  } else if (order_state == DML_ORDER_STARTED &&
             order_previous_was_operator) {
    mylite_parser_reject(ctx, order_previous_top_token,
                         "incomplete DML ORDER BY clause");
  } else if (query_tail_state == DML_QUERY_TAIL_AFTER_ORDER ||
             query_tail_state == DML_QUERY_TAIL_AFTER_ORDER_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML parenthesized query ORDER BY");
  } else if (query_tail_state == DML_QUERY_TAIL_ORDER_EXPR &&
             query_order_previous_was_operator) {
    mylite_parser_reject(ctx, query_order_previous_top_token,
                         "incomplete DML parenthesized query ORDER BY");
  } else if (query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT ||
             query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT_COMMA ||
             query_tail_state == DML_QUERY_TAIL_AFTER_LIMIT_OFFSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML parenthesized query LIMIT");
  } else if (limit_state == DML_LIMIT_AFTER_LIMIT) {
    mylite_parser_reject(ctx, pending_token, "incomplete DML LIMIT clause");
  } else if (update_table_alias_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete UPDATE table alias");
  }
}

void mylite_parser_validate_handler_statement(MyliteParseContext *ctx,
                                              MyliteToken start) {
  enum {
    HANDLER_WHERE_NONE,
    HANDLER_WHERE_AFTER_WHERE,
    HANDLER_WHERE_STARTED
  };
  enum {
    HANDLER_LIMIT_NONE,
    HANDLER_LIMIT_AFTER_LIMIT,
    HANDLER_LIMIT_AFTER_VALUE,
    HANDLER_LIMIT_AFTER_COMMA,
    HANDLER_LIMIT_AFTER_OFFSET,
    HANDLER_LIMIT_AFTER_FINAL_VALUE
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  MyliteToken where_previous_top_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_read = 0;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int seen_where = 0;
  int seen_limit = 0;
  int where_state = HANDLER_WHERE_NONE;
  int where_previous_top_token_id = 0;
  int where_previous_was_operator = 1;
  int where_match_list = 0;
  int limit_state = HANDLER_LIMIT_NONE;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (!query_expression_depth_token(
              ctx, token_id, token, &depth, &expression_stack,
              "malformed HANDLER WHERE clause")) {
        return;
      }
      if (token_closes_nested_expression(token_id)) {
        if (depth == 0 && where_state == HANDLER_WHERE_STARTED) {
          where_previous_top_token_id = token_id;
          where_previous_top_token = token;
          where_previous_was_operator = 0;
        }
      }
      continue;
    }

    if (!saw_read) {
      if (token_id == ML_READ) {
        saw_read = 1;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (where_state == HANDLER_WHERE_AFTER_WHERE) {
      if (token_id == ML_LIMIT || token_id == ML_ORDER ||
          token_id == ML_SEMI || token_id == ML_WHERE ||
          token_id == ML_COMMA) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete HANDLER WHERE clause");
        return;
      }
      where_state = HANDLER_WHERE_STARTED;
      where_previous_top_token_id = 0;
      where_previous_was_operator = 1;
      where_match_list = 0;
      where_previous_top_token = token;
    }

    if (where_state == HANDLER_WHERE_STARTED) {
      if (token_id == ML_COMMA) {
        if (!where_match_list) {
          mylite_parser_reject(ctx, token,
                               "malformed HANDLER WHERE clause");
          return;
        }
        where_previous_top_token_id = 0;
        where_previous_was_operator = 1;
        where_previous_top_token = token;
        continue;
      }
      if (token_id == ML_ORDER || token_id == ML_WHERE) {
        mylite_parser_reject(ctx, token,
                             "malformed HANDLER READ clause");
        return;
      }
      if (token_id == ML_LIMIT) {
        if (seen_limit) {
          mylite_parser_reject(ctx, token,
                               "malformed HANDLER READ clause");
          return;
        }
        if (where_previous_was_operator) {
          mylite_parser_reject(ctx, where_previous_top_token,
                               "incomplete HANDLER WHERE clause");
          return;
        }
        seen_limit = 1;
        where_state = HANDLER_WHERE_NONE;
        where_match_list = 0;
        limit_state = HANDLER_LIMIT_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        if (where_previous_was_operator) {
          mylite_parser_reject(ctx, where_previous_top_token,
                               "incomplete HANDLER WHERE clause");
          return;
        }
        break;
      }
      if (token_closes_nested_expression(token_id)) {
        mylite_parser_reject(ctx, token, "malformed HANDLER WHERE clause");
        return;
      }
      if (!query_expression_token(
              ctx, token_id, token, &depth, &where_previous_top_token_id,
              &where_previous_top_token, &where_previous_was_operator,
              &expression_stack,
              "malformed HANDLER WHERE clause")) {
        return;
      }
      if (token_ascii_equal(token, "match")) {
        where_match_list = 1;
      } else if (where_match_list && token_ascii_equal(token, "against")) {
        where_match_list = 0;
      }
      continue;
    }

    if (limit_state == HANDLER_LIMIT_AFTER_LIMIT ||
        limit_state == HANDLER_LIMIT_AFTER_COMMA ||
        limit_state == HANDLER_LIMIT_AFTER_OFFSET) {
      if (!dml_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete HANDLER LIMIT clause");
        return;
      }
      if (limit_state == HANDLER_LIMIT_AFTER_LIMIT) {
        limit_state = HANDLER_LIMIT_AFTER_VALUE;
      } else {
        limit_state = HANDLER_LIMIT_AFTER_FINAL_VALUE;
      }
      continue;
    }
    if (limit_state == HANDLER_LIMIT_AFTER_VALUE) {
      if (token_id == ML_COMMA) {
        limit_state = HANDLER_LIMIT_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        limit_state = HANDLER_LIMIT_AFTER_OFFSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed HANDLER LIMIT clause");
      return;
    }
    if (limit_state == HANDLER_LIMIT_AFTER_FINAL_VALUE) {
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed HANDLER LIMIT clause");
      return;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (token_id == ML_WHERE) {
      if (seen_where || seen_limit) {
        mylite_parser_reject(ctx, token,
                             "malformed HANDLER READ clause");
        return;
      }
      seen_where = 1;
      where_state = HANDLER_WHERE_AFTER_WHERE;
      pending_token = token;
      continue;
    }

    if (token_id == ML_LIMIT) {
      if (seen_limit) {
        mylite_parser_reject(ctx, token,
                             "malformed HANDLER READ clause");
        return;
      }
      seen_limit = 1;
      limit_state = HANDLER_LIMIT_AFTER_LIMIT;
      pending_token = token;
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }

  if (where_state == HANDLER_WHERE_AFTER_WHERE) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete HANDLER WHERE clause");
  } else if (where_state == HANDLER_WHERE_STARTED &&
             where_previous_was_operator) {
    mylite_parser_reject(ctx, where_previous_top_token,
                         "incomplete HANDLER WHERE clause");
  } else if (limit_state == HANDLER_LIMIT_AFTER_LIMIT ||
             limit_state == HANDLER_LIMIT_AFTER_COMMA ||
             limit_state == HANDLER_LIMIT_AFTER_OFFSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete HANDLER LIMIT clause");
  }
}

void mylite_parser_validate_do_statement(MyliteParseContext *ctx,
                                          MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int need_expression = 1;
  int previous_top_token_id = 0;
  int previous_was_operator = 1;
  MyliteToken previous_top_token = start;
  int seen_item = 0;
  int item_start;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (!query_expression_depth_token(
              ctx, token_id, token, &depth, &expression_stack,
              "malformed DO expression list")) {
        return;
      }
      if (token_closes_nested_expression(token_id) && depth == 0) {
        previous_top_token_id = token_id;
        previous_top_token = token;
        previous_was_operator = 0;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      if (need_expression) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DO expression list");
      } else if (previous_was_operator) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete DO expression list");
      }
      break;
    }

    if (token_id == ML_COMMA) {
      if (need_expression) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DO expression list");
        return;
      }
      seen_item = 1;
      need_expression = 1;
      previous_top_token_id = 0;
      previous_was_operator = 1;
      pending_token = token;
      memset(&expression_stack, 0, sizeof(expression_stack));
      continue;
    }

    item_start = need_expression;
    if (need_expression) {
      if (do_clause_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DO expression list");
        return;
      }
      need_expression = 0;
    } else if (do_clause_boundary(token_id)) {
      mylite_parser_reject(ctx, token, "malformed DO expression list");
      return;
    }

    if (previous_top_token_id == ML_STAR && !previous_was_operator) {
      mylite_parser_reject(ctx, token, "malformed DO expression list");
      return;
    }

    if (token_id == ML_STAR &&
        ((item_start && !seen_item) || previous_top_token_id == ML_DOT)) {
      need_expression = 0;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 0;
      continue;
    }

    if (!query_expression_token(
            ctx, token_id, token, &depth, &previous_top_token_id,
            &previous_top_token, &previous_was_operator, &expression_stack,
            "malformed DO expression list")) {
      return;
    }
  }

  if (need_expression) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DO expression list");
  } else if (previous_was_operator) {
    mylite_parser_reject(ctx, previous_top_token,
                         "incomplete DO expression list");
  }
}

void mylite_parser_validate_kill_statement(MyliteParseContext *ctx,
                                           MyliteToken start) {
  enum {
    KILL_AFTER_KILL,
    KILL_AFTER_MODE,
    KILL_AFTER_AT_SIGN,
    KILL_AFTER_TARGET,
    KILL_IN_CALL
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int state = KILL_AFTER_KILL;
  int target_allows_call = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0 && state == KILL_IN_CALL) {
          state = KILL_AFTER_TARGET;
        }
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if (state == KILL_AFTER_KILL || state == KILL_AFTER_MODE ||
          state == KILL_AFTER_AT_SIGN) {
        mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
      }
      break;
    }

    if (state == KILL_AFTER_KILL &&
        (token_id == ML_CONNECTION || token_id == ML_QUERY)) {
      state = KILL_AFTER_MODE;
      pending_token = token;
      continue;
    }

    if (state == KILL_AFTER_KILL || state == KILL_AFTER_MODE) {
      if (token_id == ML_AT_SIGN) {
        state = KILL_AFTER_AT_SIGN;
        target_allows_call = 0;
        pending_token = token;
        continue;
      }
      if (!kill_target_token(token_id)) {
        mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
        return;
      }
      state = KILL_AFTER_TARGET;
      target_allows_call = kill_target_allows_call(token_id);
      continue;
    }

    if (state == KILL_AFTER_AT_SIGN) {
      if (!kill_at_sign_target_token(token_id)) {
        mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
        return;
      }
      state = KILL_AFTER_TARGET;
      continue;
    }

    if (state == KILL_AFTER_TARGET && token_id == ML_LP &&
        target_allows_call) {
      state = KILL_IN_CALL;
      depth = 1;
      target_allows_call = 0;
      pending_token = token;
      continue;
    }

    mylite_parser_reject(ctx, token, "malformed KILL target");
    return;
  }

  if (state == KILL_AFTER_KILL || state == KILL_AFTER_MODE ||
      state == KILL_AFTER_AT_SIGN) {
    mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
  } else if (state == KILL_IN_CALL) {
    mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
  }
}

void mylite_parser_validate_reset_statement(MyliteParseContext *ctx,
                                            MyliteToken start) {
  enum {
    RESET_AFTER_RESET,
    RESET_AFTER_PERSIST,
    RESET_AFTER_IF,
    RESET_AFTER_EXISTS,
    RESET_IN_PERSIST_TARGET,
    RESET_AFTER_PERSIST_DOT
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int state = RESET_AFTER_RESET;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if (state == RESET_AFTER_IF || state == RESET_AFTER_EXISTS ||
          state == RESET_AFTER_PERSIST_DOT) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete RESET PERSIST target");
      }
      break;
    }

    if (state == RESET_AFTER_RESET) {
      if (token_id != ML_PERSIST) {
        return;
      }
      state = RESET_AFTER_PERSIST;
      pending_token = token;
      continue;
    }

    if (state == RESET_AFTER_PERSIST) {
      if (token_id == ML_IF) {
        state = RESET_AFTER_IF;
        pending_token = token;
        continue;
      }
      if (!reset_persist_name_part_token(token_id, token)) {
        mylite_parser_reject(ctx, token, "malformed RESET PERSIST target");
        return;
      }
      state = RESET_IN_PERSIST_TARGET;
      continue;
    }

    if (state == RESET_AFTER_IF) {
      if (token_id != ML_EXISTS) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete RESET PERSIST target");
        return;
      }
      state = RESET_AFTER_EXISTS;
      pending_token = token;
      continue;
    }

    if (state == RESET_AFTER_EXISTS) {
      if (!reset_persist_name_part_token(token_id, token)) {
        mylite_parser_reject(ctx, token, "malformed RESET PERSIST target");
        return;
      }
      state = RESET_IN_PERSIST_TARGET;
      continue;
    }

    if (state == RESET_IN_PERSIST_TARGET) {
      if (token_id == ML_DOT) {
        state = RESET_AFTER_PERSIST_DOT;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token, "malformed RESET PERSIST target");
      return;
    }

    if (state == RESET_AFTER_PERSIST_DOT) {
      if (!reset_persist_name_part_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete RESET PERSIST target");
        return;
      }
      state = RESET_IN_PERSIST_TARGET;
      continue;
    }
  }

  if (state == RESET_AFTER_IF || state == RESET_AFTER_EXISTS ||
      state == RESET_AFTER_PERSIST_DOT) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete RESET PERSIST target");
  }
}

void mylite_parser_validate_set_statement(MyliteParseContext *ctx,
                                          MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int previous_top_token_id = 0;
  MyliteToken previous_top_token = start;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        previous_top_token_id = token_id;
        previous_top_token = token;
      }
      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0) {
          previous_top_token_id = token_id;
          previous_top_token = token;
        }
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      break;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
      previous_top_token_id = token_id;
      previous_top_token = token;
      continue;
    }

    if (token_id == ML_SET && previous_top_token_id != ML_CHARACTER &&
        set_statement_previous_value_terminal(previous_top_token_id,
                                              previous_top_token)) {
      mylite_parser_reject(ctx, token, "malformed SET statement");
      return;
    }

    previous_top_token_id = token_id;
    previous_top_token = token;
  }
}

void mylite_parser_validate_show_statement(MyliteParseContext *ctx,
                                           MyliteToken start) {
  enum {
    SHOW_WHERE_NONE,
    SHOW_WHERE_AFTER_WHERE,
    SHOW_WHERE_STARTED
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  MyliteToken previous_top_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int where_state = SHOW_WHERE_NONE;
  int previous_top_token_id = 0;
  int previous_was_operator = 1;
  int match_list = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                        &expression_stack,
                                        "malformed SHOW WHERE clause")) {
        return;
      }
      if (token_closes_nested_expression(token_id)) {
        if (depth == 0 && where_state == SHOW_WHERE_STARTED) {
          previous_top_token_id = token_id;
          previous_top_token = token;
          previous_was_operator = 0;
        }
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (where_state == SHOW_WHERE_NONE) {
      if (token_id == ML_PARSE_TREE) {
        return;
      }
      if (token_id == ML_WHERE) {
        where_state = SHOW_WHERE_AFTER_WHERE;
        previous_top_token_id = 0;
        previous_was_operator = 1;
        match_list = 0;
        pending_token = token;
      }
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (where_state == SHOW_WHERE_AFTER_WHERE) {
      if (select_expression_clause_boundary(token_id, token) ||
          token_id == ML_COMMA) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SHOW WHERE clause");
        return;
      }
      where_state = SHOW_WHERE_STARTED;
    }

    if (where_state == SHOW_WHERE_STARTED) {
      if (token_id == ML_COMMA) {
        if (!match_list) {
          mylite_parser_reject(ctx, token, "malformed SHOW WHERE clause");
          return;
        }
        previous_top_token_id = 0;
        previous_was_operator = 1;
        previous_top_token = token;
        continue;
      }
      if (select_expression_clause_boundary(token_id, token)) {
        mylite_parser_reject(ctx, token, previous_was_operator
                                             ? "incomplete SHOW WHERE clause"
                                             : "malformed SHOW WHERE clause");
        return;
      }
      if (token_closes_nested_expression(token_id)) {
        mylite_parser_reject(ctx, token, "malformed SHOW WHERE clause");
        return;
      }
      if (!query_expression_token(
              ctx, token_id, token, &depth, &previous_top_token_id,
              &previous_top_token, &previous_was_operator, &expression_stack,
              "malformed SHOW WHERE clause")) {
        return;
      }
      if (token_ascii_equal(token, "match")) {
        match_list = 1;
      } else if (match_list && token_ascii_equal(token, "against")) {
        match_list = 0;
      }
      continue;
    }
  }

  if (depth == 0 && where_state == SHOW_WHERE_AFTER_WHERE) {
    mylite_parser_reject(ctx, pending_token, "incomplete SHOW WHERE clause");
  } else if (depth == 0 && where_state == SHOW_WHERE_STARTED &&
             previous_was_operator) {
    mylite_parser_reject(ctx, previous_top_token,
                         "incomplete SHOW WHERE clause");
  }
}

void mylite_parser_validate_grant_statement(MyliteParseContext *ctx,
                                            MyliteToken start, int revoke) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int in_object = 0;
  int object_seen = 0;
  int proxy_grant = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      return;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
      continue;
    }

    if (!in_object) {
      if (token_id == ML_PROXY) {
        proxy_grant = 1;
      }
      if (token_id == ML_ON) {
        in_object = 1;
        object_seen = 0;
      }
      continue;
    }

    if ((!revoke && token_id == ML_TO) || (revoke && token_id == ML_FROM)) {
      return;
    }

    if (!object_seen) {
      if (!grant_object_start_token(token_id, token, proxy_grant)) {
        mylite_parser_reject(ctx, token, "invalid grant object");
        return;
      }
      object_seen = 1;
    }
  }
}

void mylite_parser_validate_values_statement_from(MyliteParseContext *ctx,
                                                  MyliteToken start) {
  enum {
    VALUES_VALIDATE_READY,
    VALUES_VALIDATE_AFTER_ROW,
    VALUES_VALIDATE_IN_ROW
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int state = VALUES_VALIDATE_READY;
  MyliteExpressionStack expression_stack = {0};

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                        &expression_stack,
                                        "malformed VALUES row")) {
        return;
      }
      if (token_closes_nested_expression(token_id) && depth == 0) {
        state = VALUES_VALIDATE_READY;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (state == VALUES_VALIDATE_AFTER_ROW) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token, "incomplete VALUES row");
        return;
      }
      state = VALUES_VALIDATE_IN_ROW;
      depth = 1;
      query_expression_stack_open_list(&expression_stack, depth, token, 0,
                                       QUERY_EXPRESSION_ALLOW_BARE_DEFAULT);
      pending_token = token;
      continue;
    }

    if (token_id == ML_ROW) {
      state = VALUES_VALIDATE_AFTER_ROW;
      pending_token = token;
      continue;
    }
  }

  if (state == VALUES_VALIDATE_AFTER_ROW) {
    mylite_parser_reject(ctx, pending_token, "incomplete VALUES row");
  }
}

void mylite_parser_validate_parenthesized_expression_list_from(
    MyliteParseContext *ctx, MyliteToken start, const char *message) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_list = 0;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_list) {
      if (token.offset == start.offset) {
        saw_list = 1;
        if (token_id != ML_LP) {
          return;
        }
        depth = 1;
        pending_token = token;
        query_expression_stack_open_list(&expression_stack, depth, token, 0, 0);
      }
      continue;
    }

    if (depth <= 0) {
      break;
    }

    if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                      &expression_stack, message)) {
      return;
    }
  }

  if (saw_list && depth > 0) {
    mylite_parser_reject(ctx, pending_token, message);
  }
}

void mylite_parser_validate_expression_from(MyliteParseContext *ctx,
                                            MyliteToken start,
                                            const char *message) {
  validate_expression_tail_from(ctx, start, 0, 1, 0, message);
}

void mylite_parser_validate_expression_until_from(MyliteParseContext *ctx,
                                                  MyliteToken start,
                                                  int boundary_token_id,
                                                  const char *message) {
  validate_expression_tail_from(ctx, start, boundary_token_id, 0, 0, message);
}

void mylite_parser_validate_default_value_expression_from(
    MyliteParseContext *ctx, MyliteToken start, const char *message) {
  validate_expression_tail_from(ctx, start, 0, 1,
                                QUERY_EXPRESSION_ALLOW_BARE_DEFAULT, message);
}

void mylite_parser_validate_set_assignment_expression_from(
    MyliteParseContext *ctx, MyliteToken start, const char *message) {
  if (set_user_variable_value_is_invalid(ctx, start)) {
    mylite_parser_reject(ctx, start, message);
    return;
  }

  if (expression_start_follows_user_variable_assignment(ctx, start)) {
    validate_expression_tail_from(ctx, start, 0, 1, 0, message);
    return;
  }

  validate_expression_tail_from(ctx, start, 0, 1,
                                QUERY_EXPRESSION_ALLOW_BARE_DEFAULT, message);
}

static void validate_expression_tail_from(MyliteParseContext *ctx,
                                          MyliteToken start,
                                          int boundary_token_id,
                                          int allow_double_at_assignment,
                                          int flags,
                                          const char *message) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken previous_top_token = start;
  int token_id;
  int saw_expression = 0;
  int depth = 0;
  int previous_top_token_id = 0;
  int previous_was_operator = 1;
  MyliteExpressionStack expression_stack = {0};

  if (allow_double_at_assignment &&
      expression_start_follows_double_at_assignment(ctx, start)) {
    return;
  }

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_expression) {
      if (token.offset == start.offset) {
        saw_expression = 1;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                        &expression_stack, message)) {
        return;
      }
      if (token_closes_nested_expression(token_id) && depth == 0) {
        previous_top_token_id = token_id;
        previous_top_token = token;
        previous_was_operator = 0;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token) || token_id == ML_COMMA ||
        (boundary_token_id && token_id == boundary_token_id)) {
      break;
    }

    if (!query_expression_token_with_flags(
            ctx, token_id, token, &depth, &previous_top_token_id,
            &previous_top_token, &previous_was_operator, &expression_stack,
            flags, message)) {
      return;
    }
  }

  if (saw_expression &&
      (previous_top_token_id == 0 || previous_was_operator)) {
    mylite_parser_reject(ctx, previous_top_token, message);
  }
}

static int set_user_variable_value_is_invalid(MyliteParseContext *ctx,
                                              MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken next;
  int token_id;
  int next_id;
  size_t offset;

  if (!expression_start_follows_user_variable_assignment(ctx, start)) {
    return 0;
  }

  offset = start.offset;
  if (offset >= ctx->length) {
    return 0;
  }

  mylite_lexer_init(&lexer, ctx->sql + offset, ctx->length - offset, NULL);
  token_id = mylite_lexer_next(&lexer, &token);
  next_id = mylite_lexer_next(&lexer, &next);

  if (token_id == ML_DEFAULT) {
    return next_id != ML_DOT && next_id != ML_LP;
  }

  if (token_id == ML_ON || token_id == ML_ALL || token_id == ML_SYSTEM) {
    return 1;
  }

  if (token_id == ML_ROW) {
    return next_id != ML_LP;
  }

  if (token_id == ML_BINARY) {
    return next_id <= 0 || next_id == ML_COMMA ||
           token_is_statement_terminator(next_id, next);
  }

  return 0;
}

static int expression_start_follows_user_variable_assignment(
    MyliteParseContext *ctx, MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken first_target = {0};
  int token_id;
  int first_target_id = 0;
  int depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (token.offset >= start.offset) {
      return 0;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_id == ML_COMMA || token_id == ML_SET) {
      first_target_id = 0;
      first_target = token;
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
      continue;
    }

    if (token_id == ML_ASSIGN || token_id == ML_EQUALS) {
      return set_assignment_target_is_user_variable(first_target_id,
                                                    first_target);
    }

    if (first_target_id == 0) {
      first_target_id = token_id;
      first_target = token;
    }
  }

  return 0;
}

static int set_assignment_target_is_user_variable(int token_id,
                                                  MyliteToken token) {
  return token_id == ML_AT_SIGN ||
         (token_id == ML_AT_HOST && token.length > 0 &&
          token.start[0] == '@');
}

static int expression_start_follows_double_at_assignment(
    MyliteParseContext *ctx, MyliteToken start) {
  size_t i = start.offset;
  int saw_assignment_operator = 0;

  while (i > 0) {
    unsigned char c = (unsigned char) ctx->sql[i - 1];
    i--;

    if (!saw_assignment_operator) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
        continue;
      }
      if (c == '=') {
        saw_assignment_operator = 1;
      }
      continue;
    }

    if (c == ',' || c == ';') {
      return 0;
    }
    if (c == '@' && i > 0 && ctx->sql[i - 1] == '@') {
      return 1;
    }
  }

  return 0;
}

void mylite_parser_validate_declare_statement(MyliteParseContext *ctx,
                                              MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int after_default = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if (after_default) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DECLARE DEFAULT expression");
      }
      return;
    }

    if (after_default) {
      mylite_parser_validate_expression_from(
          ctx, token, "malformed DECLARE DEFAULT expression");
      return;
    }

    if (token_id == ML_DEFAULT) {
      after_default = 1;
      pending_token = token;
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }

  if (after_default) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DECLARE DEFAULT expression");
  }
}

void mylite_parser_validate_explain_statement(MyliteParseContext *ctx,
                                              MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      return;
    }

    if (token_id == ML_WITH) {
      validate_with_statement_from(ctx, token,
                                   "incomplete EXPLAIN WITH clause");
      return;
    }
    if (token_id == ML_SELECT || token_id == ML_TABLE || token_id == ML_LP ||
        token_id == ML_DELETE || token_id == ML_INSERT ||
        token_id == ML_REPLACE || token_id == ML_UPDATE) {
      validate_query_body_from(ctx, token_id, token);
      return;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }
}

void mylite_parser_validate_with_statement_from(MyliteParseContext *ctx,
                                                MyliteToken start) {
  validate_with_statement_from(ctx, start, "incomplete WITH clause");
}

static void validate_with_statement_from(MyliteParseContext *ctx,
                                         MyliteToken start,
                                         const char *message) {
  enum {
    WITH_AFTER_WITH,
    WITH_AFTER_RECURSIVE,
    WITH_AFTER_CTE_NAME,
    WITH_IN_CTE_COLUMNS,
    WITH_AFTER_CTE_COLUMNS,
    WITH_AFTER_AS,
    WITH_IN_CTE_BODY,
    WITH_AFTER_CTE_BODY
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int state = WITH_AFTER_WITH;
  int depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (state == WITH_AFTER_WITH) {
      if (token_id == ML_RECURSIVE) {
        state = WITH_AFTER_RECURSIVE;
        pending_token = token;
        continue;
      }
      state = WITH_AFTER_RECURSIVE;
    }

    if (state == WITH_AFTER_RECURSIVE) {
      if (!with_cte_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token, message);
        return;
      }
      state = WITH_AFTER_CTE_NAME;
      continue;
    }

    if (state == WITH_AFTER_CTE_NAME) {
      if (token_id == ML_LP) {
        state = WITH_IN_CTE_COLUMNS;
        pending_token = token;
        depth = 1;
        continue;
      }
      if (token_id == ML_AS) {
        state = WITH_AFTER_AS;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token, message);
      return;
    }

    if (state == WITH_IN_CTE_COLUMNS) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0) {
          state = WITH_AFTER_CTE_COLUMNS;
        }
      }
      continue;
    }

    if (state == WITH_AFTER_CTE_COLUMNS) {
      if (token_id != ML_AS) {
        mylite_parser_reject(ctx, pending_token, message);
        return;
      }
      state = WITH_AFTER_AS;
      pending_token = token;
      continue;
    }

    if (state == WITH_AFTER_AS) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token, message);
        return;
      }
      state = WITH_IN_CTE_BODY;
      pending_token = token;
      depth = 1;
      continue;
    }

    if (state == WITH_IN_CTE_BODY) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0) {
          validate_with_cte_body_from(ctx, pending_token);
          if (ctx->failed) {
            return;
          }
          state = WITH_AFTER_CTE_BODY;
        }
      }
      continue;
    }

    if (state == WITH_AFTER_CTE_BODY) {
      if (token_id == ML_COMMA) {
        state = WITH_AFTER_RECURSIVE;
        pending_token = token;
        continue;
      }
      if (with_query_body_start(token_id)) {
        validate_query_body_from(ctx, token_id, token);
        return;
      }
      mylite_parser_reject(ctx, pending_token, message);
      return;
    }
  }

  mylite_parser_reject(ctx, pending_token, message);
}

static void validate_with_cte_body_from(MyliteParseContext *ctx,
                                        MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_body = 0;
  int depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_body) {
      if (token.offset == start.offset) {
        saw_body = 1;
        depth = 1;
      }
      continue;
    }

    if (depth == 1 && token_id == ML_SELECT) {
      validate_select_list_tail_from(ctx, token, 1, 1, 0);
      if (ctx->failed) {
        return;
      }
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
      continue;
    }
    if (token_closes_nested_expression(token_id)) {
      depth--;
      if (depth == 0) {
        return;
      }
    }
  }
}

static void validate_query_body_from(MyliteParseContext *ctx, int token_id,
                                     MyliteToken token) {
  if (token_id == ML_SELECT) {
    mylite_parser_validate_select_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_TABLE) {
    mylite_parser_validate_table_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_VALUES) {
    mylite_parser_validate_values_statement_from(ctx, token);
    if (!ctx->failed) {
      mylite_parser_validate_select_statement_from(ctx, token);
    }
    return;
  }
  if (token_id == ML_WITH) {
    mylite_parser_validate_with_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_LP) {
    mylite_parser_validate_parenthesized_statement(ctx, token);
    return;
  }
  if (token_id == ML_DELETE) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_DELETE);
    return;
  }
  if (token_id == ML_INSERT) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_INSERT);
    return;
  }
  if (token_id == ML_REPLACE) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_REPLACE);
    return;
  }
  if (token_id == ML_UPDATE) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_UPDATE);
  }
}

static void validate_query_body_after_optional_as_from(MyliteParseContext *ctx,
                                                       MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_start = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_start) {
      if (token.offset == start.offset) {
        saw_start = 1;
      } else {
        continue;
      }
    }

    if (token_id == ML_AS) {
      continue;
    }
    if (token_id == ML_LIKE || token_is_statement_terminator(token_id, token)) {
      return;
    }
    if (token_id == ML_LP && parenthesized_query_start_follows(ctx, token)) {
      mylite_parser_validate_parenthesized_statement(ctx, token);
      return;
    }
    if (with_query_body_start(token_id)) {
      validate_query_body_from(ctx, token_id, token);
      return;
    }
    return;
  }
}

static int with_cte_name_token(int token_id) {
  return token_id != ML_COMMA && token_id != ML_LP && token_id != ML_RP &&
         token_id != ML_SEMI;
}

static int with_query_body_start(int token_id) {
  return token_id == ML_DELETE || token_id == ML_INSERT ||
         token_id == ML_LP || token_id == ML_REPLACE ||
         token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_UPDATE || token_id == ML_VALUES ||
         token_id == ML_WITH;
}

void mylite_parser_validate_create_table_statement(MyliteParseContext *ctx,
                                                   MyliteToken start) {
  enum {
    CREATE_TABLE_FIND_BODY,
    CREATE_TABLE_BODY_START,
    CREATE_TABLE_IN_DEFINITION
  };
  enum {
    CREATE_TABLE_FK_NONE,
    CREATE_TABLE_FK_AFTER_FOREIGN,
    CREATE_TABLE_FK_BEFORE_CHILD_LIST,
    CREATE_TABLE_FK_FIND_REFERENCES,
    CREATE_TABLE_FK_AFTER_REFERENCES,
    CREATE_TABLE_FK_AFTER_PARENT_TABLE,
    CREATE_TABLE_FK_AFTER_PARENT_TABLE_DOT,
    CREATE_TABLE_FK_AFTER_PARENT_LIST,
    CREATE_TABLE_FK_AFTER_MATCH,
    CREATE_TABLE_FK_AFTER_ON,
    CREATE_TABLE_FK_AFTER_ON_ACTION,
    CREATE_TABLE_FK_AFTER_SET,
    CREATE_TABLE_FK_AFTER_NO
  };
  enum {
    CREATE_TABLE_CHECK_NONE,
    CREATE_TABLE_CHECK_READY,
    CREATE_TABLE_CHECK_AFTER_NOT,
    CREATE_TABLE_CHECK_DONE
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int state = CREATE_TABLE_FIND_BODY;
  int depth = 0;
  int index_candidate = 0;
  int foreign_state = CREATE_TABLE_FK_NONE;
  int foreign_tail_flags = 0;
  int check_pending = 0;
  int check_table_level = 0;
  int check_tail_state = CREATE_TABLE_CHECK_NONE;
  int column_needs_type = 0;
  int column_in_definition = 0;
  int column_type_parameter_pending = 0;
  int column_type_parameter_required = 0;
  int column_type_parameter_forbidden = 0;
  ColumnTypeParameterKind column_type_parameter_list_kind =
      COLUMN_TYPE_PARAMETER_NONE;
  ColumnTypeState column_type_state = COLUMN_TYPE_STATE_COMPLETE;
  ColumnTypeCharsetModifierState column_type_charset_modifier_state =
      COLUMN_TYPE_CHARSET_AVAILABLE;
  int column_type_modifier_mask = 0;
  int column_temporal_precision_pending = 0;
  int column_precision_modifier_pending = 0;
  ColumnDefinitionTailState column_tail_state =
      COLUMN_DEFINITION_TAIL_READY;
  int column_tail_flags = 0;
  int element_start = 0;
  int constraint_prefix = 0;
  int index_using_pending = 0;
  int index_using_requires_key_list = 0;
  int index_name_pending = 0;
  int find_if_not_exists_state = 0;
  int find_table_name_parts = 0;
  int find_table_dot_pending = 0;
  int find_table_ref_done = 0;
  int pre_query_option_tail = 0;
  int pre_query_depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (state == CREATE_TABLE_FIND_BODY) {
      if (!find_table_ref_done) {
        if (find_if_not_exists_state == 1) {
          find_if_not_exists_state = 2;
          continue;
        }
        if (find_if_not_exists_state == 2) {
          find_if_not_exists_state = 0;
          continue;
        }
        if (find_table_name_parts == 0 && token_id == ML_IF) {
          find_if_not_exists_state = 1;
          continue;
        }
        if (find_table_dot_pending) {
          find_table_name_parts++;
          find_table_dot_pending = 0;
          continue;
        }
        if (find_table_name_parts == 0) {
          find_table_name_parts = 1;
          continue;
        }
        if (token_id == ML_DOT && find_table_name_parts == 1) {
          find_table_dot_pending = 1;
          continue;
        }
        find_table_ref_done = 1;
      }
      if (pre_query_depth > 0) {
        if (token_opens_nested_expression(token_id)) {
          pre_query_depth++;
        } else if (token_closes_nested_expression(token_id)) {
          pre_query_depth--;
        }
        continue;
      }
      if (token_id == ML_LP) {
        if (pre_query_option_tail) {
          pre_query_depth = 1;
          continue;
        }
        if (parenthesized_query_start_follows(ctx, token)) {
          mylite_parser_validate_parenthesized_statement(ctx, token);
          return;
        }
        state = CREATE_TABLE_BODY_START;
        depth = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      if (create_table_query_body_start(token_id)) {
        validate_query_body_after_optional_as_from(ctx, token);
        if (!ctx->failed) {
          validate_create_table_tail_options(ctx, start);
        }
        return;
      }
      if (token_id == ML_PARTITION ||
          create_table_tail_option_start_token(token_id)) {
        pre_query_option_tail = 1;
      }
      continue;
    }

    if (depth > 1) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (state == CREATE_TABLE_BODY_START) {
      if (create_table_query_body_start(token_id) || token_id == ML_LP) {
        validate_query_body_after_optional_as_from(ctx, token);
        if (!ctx->failed) {
          validate_create_table_tail_options(ctx, start);
        }
        return;
      }
      state = CREATE_TABLE_IN_DEFINITION;
      element_start = 1;
    }

    if (column_needs_type) {
      if (!create_table_column_type_start(token_id, token)) {
        mylite_parser_reject(ctx, token, "invalid CREATE TABLE column type");
        return;
      }
      column_needs_type = 0;
      column_in_definition = 1;
      column_type_parameter_pending = 1;
      column_type_parameter_required =
          column_type_requires_parameter(token_id, token, 0);
      column_type_parameter_forbidden =
          column_type_forbids_parameter(token_id, token, 0);
      column_type_parameter_list_kind =
          column_type_parameter_kind(token_id, token);
      column_type_state = column_type_state_after_start(token_id, token);
      column_type_charset_modifier_state = COLUMN_TYPE_CHARSET_AVAILABLE;
      column_type_modifier_mask =
          column_type_modifier_mask_after_start(token_id, token);
      column_temporal_precision_pending = 0;
      column_precision_modifier_pending =
          column_type_allows_precision_modifier(token_id, token);
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      column_tail_flags = 0;
      continue;
    }

    if (check_pending) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE CHECK constraint");
        return;
      }
      if (!validate_parenthesized_expression_body(
              ctx, &lexer, token,
              "incomplete CREATE TABLE CHECK constraint")) {
        return;
      }
      check_pending = 0;
      check_tail_state = CREATE_TABLE_CHECK_READY;
      column_in_definition = 0;
      column_type_parameter_pending = 0;
      column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
      column_type_parameter_required = 0;
      column_type_parameter_forbidden = 0;
      column_precision_modifier_pending = 0;
      element_start = 0;
      constraint_prefix = 0;
      continue;
    }

    if (check_tail_state != CREATE_TABLE_CHECK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_RP) {
        if (check_tail_state == CREATE_TABLE_CHECK_AFTER_NOT) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE CHECK constraint");
          return;
        }
        check_tail_state = CREATE_TABLE_CHECK_NONE;
        check_table_level = 0;
        column_in_definition = 0;
        column_type_parameter_pending = 0;
        column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
        column_type_parameter_required = 0;
        column_type_parameter_forbidden = 0;
        column_precision_modifier_pending = 0;
        column_tail_state = COLUMN_DEFINITION_TAIL_READY;
        column_tail_flags = 0;
        if (token_id == ML_RP) {
          validate_create_table_tail_options(ctx, start);
          return;
        }
        element_start = 1;
        constraint_prefix = 0;
        continue;
      }

      if (check_tail_state == CREATE_TABLE_CHECK_READY) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = CREATE_TABLE_CHECK_DONE;
          continue;
        }
        if (token_id == ML_NOT) {
          check_tail_state = CREATE_TABLE_CHECK_AFTER_NOT;
          pending_token = token;
          continue;
        }
        if (!check_table_level &&
            column_definition_attribute_start(token_id, token, 0)) {
          check_tail_state = CREATE_TABLE_CHECK_NONE;
          column_in_definition = 1;
          column_type_parameter_pending = 0;
          column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
          column_type_parameter_required = 0;
          column_type_parameter_forbidden = 0;
          column_precision_modifier_pending = 0;
          column_tail_state = COLUMN_DEFINITION_TAIL_READY;
          if (!column_definition_tail_token(
                  ctx, token_id, token, &column_tail_state, &depth,
                  &check_pending, &pending_token, &column_tail_flags, 0,
                  "malformed CREATE TABLE column definition")) {
            return;
          }
          if (check_pending) {
            check_table_level = 0;
          }
          continue;
        }
      } else if (check_tail_state == CREATE_TABLE_CHECK_AFTER_NOT) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = CREATE_TABLE_CHECK_DONE;
          continue;
        }
        if (!check_table_level && token_id == ML_NULL) {
          check_tail_state = CREATE_TABLE_CHECK_NONE;
          column_in_definition = 1;
          column_type_parameter_pending = 0;
          column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
          column_type_parameter_required = 0;
          column_type_parameter_forbidden = 0;
          column_precision_modifier_pending = 0;
          column_tail_state = COLUMN_DEFINITION_TAIL_READY;
          continue;
        }
      } else if (check_tail_state == CREATE_TABLE_CHECK_DONE) {
        if (!check_table_level &&
            column_definition_attribute_start(token_id, token, 0)) {
          check_tail_state = CREATE_TABLE_CHECK_NONE;
          column_in_definition = 1;
          column_type_parameter_pending = 0;
          column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
          column_type_parameter_required = 0;
          column_type_parameter_forbidden = 0;
          column_precision_modifier_pending = 0;
          column_tail_state = COLUMN_DEFINITION_TAIL_READY;
          if (!column_definition_tail_token(
                  ctx, token_id, token, &column_tail_state, &depth,
                  &check_pending, &pending_token, &column_tail_flags, 0,
                  "malformed CREATE TABLE column definition")) {
            return;
          }
          if (check_pending) {
            check_table_level = 0;
          }
          continue;
        }
      }

      mylite_parser_reject(ctx, token,
                           "malformed CREATE TABLE CHECK constraint");
      return;
    }

    if (foreign_state != CREATE_TABLE_FK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_RP) {
        if (foreign_state != CREATE_TABLE_FK_AFTER_PARENT_TABLE &&
            foreign_state != CREATE_TABLE_FK_AFTER_PARENT_LIST) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key");
          return;
        }
        if (token_id == ML_RP) {
          validate_create_table_tail_options(ctx, start);
          return;
        }
        foreign_state = CREATE_TABLE_FK_NONE;
        foreign_tail_flags = 0;
        element_start = 1;
        constraint_prefix = 0;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_FOREIGN) {
        if (token_id != ML_KEY) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key");
          return;
        }
        foreign_state = CREATE_TABLE_FK_BEFORE_CHILD_LIST;
        pending_token = token;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_BEFORE_CHILD_LIST) {
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete CREATE TABLE foreign key column list")) {
            return;
          }
          foreign_state = CREATE_TABLE_FK_FIND_REFERENCES;
          pending_token = token;
          continue;
        }
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_FIND_REFERENCES) {
        if (token_ascii_equal(token, "references")) {
          foreign_state = CREATE_TABLE_FK_AFTER_REFERENCES;
          pending_token = token;
        }
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_REFERENCES) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_TABLE;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_PARENT_TABLE_DOT) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_TABLE;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_PARENT_TABLE) {
        if (token_id == ML_DOT) {
          foreign_state = CREATE_TABLE_FK_AFTER_PARENT_TABLE_DOT;
          pending_token = token;
          continue;
        }
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete CREATE TABLE foreign key reference list")) {
            return;
          }
          foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
          continue;
        }
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_PARENT_TABLE ||
          foreign_state == CREATE_TABLE_FK_AFTER_PARENT_LIST) {
        if (token_ascii_equal(token, "match")) {
          if ((foreign_tail_flags &
               (REFERENCE_TAIL_FLAG_MATCH | REFERENCE_TAIL_FLAG_ON_UPDATE |
                REFERENCE_TAIL_FLAG_ON_DELETE)) != 0) {
            mylite_parser_reject(ctx, token,
                                 "malformed CREATE TABLE foreign key option");
            return;
          }
          foreign_state = CREATE_TABLE_FK_AFTER_MATCH;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          foreign_state = CREATE_TABLE_FK_AFTER_ON;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "malformed CREATE TABLE foreign key option");
        return;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_MATCH) {
        if (!foreign_key_match_option(token_id, token)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key MATCH");
          return;
        }
        foreign_tail_flags |= REFERENCE_TAIL_FLAG_MATCH;
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_ON) {
        if (token_id != ML_DELETE && token_id != ML_UPDATE) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key option");
          return;
        }
        if (token_id == ML_DELETE) {
          if ((foreign_tail_flags & REFERENCE_TAIL_FLAG_ON_DELETE) != 0) {
            mylite_parser_reject(ctx, token,
                                 "malformed CREATE TABLE foreign key option");
            return;
          }
          foreign_tail_flags |= REFERENCE_TAIL_FLAG_ON_DELETE;
        } else {
          if ((foreign_tail_flags & REFERENCE_TAIL_FLAG_ON_UPDATE) != 0) {
            mylite_parser_reject(ctx, token,
                                 "malformed CREATE TABLE foreign key option");
            return;
          }
          foreign_tail_flags |= REFERENCE_TAIL_FLAG_ON_UPDATE;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_ON_ACTION;
        pending_token = token;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_ON_ACTION) {
        if (foreign_key_reference_action_token(token_id)) {
          foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
          continue;
        }
        if (token_id == ML_SET) {
          foreign_state = CREATE_TABLE_FK_AFTER_SET;
          pending_token = token;
          continue;
        }
        if (token_id == ML_NO) {
          foreign_state = CREATE_TABLE_FK_AFTER_NO;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE foreign key option");
        return;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_SET) {
        if (token_id != ML_DEFAULT && token_id != ML_NULL) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key option");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_NO) {
        if (!token_ascii_equal(token, "action")) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key option");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
        continue;
      }

      continue;
    }

    if (column_in_definition &&
        column_definition_tail_wants_boundary_token(column_tail_state,
                                                    token_id)) {
      if (!column_definition_tail_token(
              ctx, token_id, token, &column_tail_state, &depth,
              &check_pending, &pending_token, &column_tail_flags, 0,
              "malformed CREATE TABLE column definition")) {
        return;
      }
      if (check_pending) {
        check_table_level = 0;
      }
      continue;
    }

    if (token_id == ML_RP) {
      if (index_candidate) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
      } else if (column_in_definition &&
                 (column_type_parameter_required ||
                  column_type_incomplete(column_type_state))) {
        mylite_parser_reject(ctx, token, "malformed CREATE TABLE column type");
      } else if (column_in_definition &&
                 !column_definition_tail_complete(column_tail_state)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE column definition");
      }
      if (!ctx->failed) {
        validate_create_table_tail_options(ctx, start);
      }
      return;
    }

    if (token_id == ML_COMMA) {
      if (index_candidate) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return;
      }
      if (index_using_pending) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index USING");
        return;
      }
      if (column_in_definition &&
          (column_type_parameter_required ||
           column_type_incomplete(column_type_state))) {
        mylite_parser_reject(ctx, token, "malformed CREATE TABLE column type");
        return;
      }
      if (column_in_definition &&
          !column_definition_tail_complete(column_tail_state)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE column definition");
        return;
      }
      element_start = 1;
      constraint_prefix = 0;
      check_pending = 0;
      check_table_level = 0;
      check_tail_state = CREATE_TABLE_CHECK_NONE;
      column_needs_type = 0;
      column_in_definition = 0;
      column_type_parameter_pending = 0;
      column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
      column_type_parameter_required = 0;
      column_type_parameter_forbidden = 0;
      column_precision_modifier_pending = 0;
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      column_tail_flags = 0;
      index_using_pending = 0;
      index_using_requires_key_list = 0;
      index_name_pending = 0;
      continue;
    }

    if (index_candidate && index_name_pending && token_id != ML_LP &&
        token_id != ML_USING && token_id != ML_TYPE) {
      mylite_parser_require_strict_identifier_atom(ctx, token);
      if (ctx->failed) {
        return;
      }
      index_name_pending = 0;
      continue;
    }

    if (index_candidate && index_using_requires_key_list) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, token, "invalid CREATE TABLE index USING");
        return;
      }
      index_using_requires_key_list = 0;
    }

    if (token_id == ML_LP) {
      if (index_candidate) {
        if (!validate_create_table_index_key_list(ctx, &lexer, token)) {
          return;
        }
        index_candidate = 0;
        index_name_pending = 0;
        constraint_prefix = 0;
        element_start = 0;
      } else if (column_in_definition) {
        int consumed_type_parameters = 0;
        if (column_type_parameter_forbidden && token_id == ML_LP) {
          mylite_parser_reject(ctx, token, "malformed CREATE TABLE column type");
          return;
        }
        if (!consume_column_type_parameter_list_if_pending(
                ctx, &lexer, token_id, token, &column_type_parameter_pending,
                &column_type_parameter_required, column_type_parameter_list_kind,
                "malformed CREATE TABLE column type",
                &consumed_type_parameters)) {
          return;
        }
        if (consumed_type_parameters) {
          column_type_state = COLUMN_TYPE_STATE_COMPLETE;
          column_precision_modifier_pending = 0;
          column_type_parameter_forbidden = 0;
          continue;
        }
        if (column_temporal_precision_pending && token_id != ML_LP) {
          column_temporal_precision_pending = 0;
        }
        if (column_temporal_precision_pending) {
          column_temporal_precision_pending = 0;
          if (!validate_column_temporal_precision_list(
                  ctx, &lexer, token,
                  "malformed CREATE TABLE column definition")) {
            return;
          }
          continue;
        }
        if (column_definition_tail_parenthesized_expression(
                column_tail_state)) {
          if (!validate_parenthesized_expression_body(
                  ctx, &lexer, token,
                  "malformed CREATE TABLE column definition")) {
            return;
          }
          column_definition_tail_finish_parenthesized_expression(
              &column_tail_state, &column_tail_flags);
          continue;
        }
        if (!column_definition_tail_token(
                ctx, token_id, token, &column_tail_state, &depth,
                &check_pending, &pending_token, &column_tail_flags, 0,
                "malformed CREATE TABLE column definition")) {
          return;
        }
        if (check_pending) {
          check_table_level = 0;
        }
      } else {
        depth = 2;
      }
      continue;
    }

    if (token_id == ML_CHECK) {
      check_pending = 1;
      check_table_level = element_start || constraint_prefix > 0;
      element_start = 0;
      constraint_prefix = 0;
      pending_token = token;
      continue;
    }

    if (column_in_definition) {
      ColumnDefinitionTailState before_tail_state = column_tail_state;
      int consumed_type_parameters = 0;
      int consumed_precision_modifier = 0;
      int consumed_type_tail = 0;
      if (column_type_parameter_forbidden && token_id == ML_LP) {
        mylite_parser_reject(ctx, token, "malformed CREATE TABLE column type");
        return;
      }
      if (token_id != ML_LP) {
        column_type_parameter_forbidden = 0;
      }
      if (!consume_column_type_parameter_list_if_pending(
              ctx, &lexer, token_id, token, &column_type_parameter_pending,
              &column_type_parameter_required, column_type_parameter_list_kind,
              "malformed CREATE TABLE column type",
              &consumed_type_parameters)) {
        return;
      }
      if (consumed_type_parameters) {
        column_type_state = COLUMN_TYPE_STATE_COMPLETE;
        column_precision_modifier_pending = 0;
        column_type_parameter_forbidden = 0;
        continue;
      }
      if (!consume_column_precision_modifier_if_pending(
              ctx, token_id, token, &column_precision_modifier_pending,
              &column_type_parameter_pending, &column_type_parameter_required,
              &column_type_parameter_list_kind,
              "malformed CREATE TABLE column type",
              &consumed_precision_modifier)) {
        return;
      }
      if (consumed_precision_modifier) {
        continue;
      }
      column_precision_modifier_pending = 0;
      if (before_tail_state == COLUMN_DEFINITION_TAIL_READY) {
        if (!consume_column_type_tail_token_if_pending(
                ctx, token_id, token, &column_type_state,
                &column_type_modifier_mask, &column_type_parameter_pending,
                &column_type_parameter_required,
                &column_type_parameter_forbidden,
                &column_type_parameter_list_kind,
                &column_type_charset_modifier_state,
                "malformed CREATE TABLE column type", &consumed_type_tail)) {
          return;
        }
        if (consumed_type_tail) {
          continue;
        }
        if (!column_type_charset_introducer_token(token_id, token)) {
          column_type_modifier_mask = 0;
          column_type_charset_modifier_state = COLUMN_TYPE_CHARSET_DONE;
        }
      } else if (before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_CHARACTER &&
                 token_ascii_equal(token, "varying")) {
        mylite_parser_reject(ctx, token,
                             "malformed CREATE TABLE column definition");
        return;
      }
      if (!column_definition_tail_token(
              ctx, token_id, token, &column_tail_state, &depth,
              &check_pending, &pending_token, &column_tail_flags, 0,
              "malformed CREATE TABLE column definition")) {
        return;
      }
      column_temporal_precision_pending =
          (before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT ||
           before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE) &&
          column_definition_temporal_function_token(token);
      if (check_pending) {
        check_table_level = 0;
      }
      continue;
    }

    if (index_using_pending) {
      if (!index_using_type_token(token)) {
        mylite_parser_reject(ctx, pending_token,
                             "invalid CREATE TABLE index USING");
        return;
      }
      index_using_pending = 0;
      index_using_requires_key_list = 1;
      pending_token = token;
      continue;
    }

    if (!element_start && constraint_prefix == 0 && !index_candidate) {
      continue;
    }

    if (element_start && token_id == ML_CONSTRAINT) {
      constraint_prefix = 1;
      element_start = 0;
      pending_token = token;
      continue;
    }

    if (constraint_prefix > 0) {
      if (alter_table_add_index_marker(token_id)) {
        index_candidate = 1;
        index_name_pending = 1;
        constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_FOREIGN) {
        foreign_state = CREATE_TABLE_FK_AFTER_FOREIGN;
        foreign_tail_flags = 0;
        constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (constraint_prefix == 1) {
        mylite_parser_require_strict_identifier_atom(ctx, token);
        if (ctx->failed) {
          return;
        }
        constraint_prefix = 2;
        continue;
      }
      constraint_prefix = 0;
      continue;
    }

    if (element_start && alter_table_add_index_marker(token_id)) {
      index_candidate = 1;
      index_name_pending = 1;
      element_start = 0;
      pending_token = token;
      continue;
    }

    if (index_candidate && (token_id == ML_USING || token_id == ML_TYPE)) {
      index_using_pending = 1;
      index_name_pending = 0;
      pending_token = token;
      continue;
    }

    if (element_start && token_id == ML_FOREIGN) {
      foreign_state = CREATE_TABLE_FK_AFTER_FOREIGN;
      foreign_tail_flags = 0;
      element_start = 0;
      pending_token = token;
      continue;
    }

    if (element_start) {
      column_needs_type =
          create_table_column_name_needs_type_check(token_id, token);
      element_start = 0;
      pending_token = token;
      continue;
    }

    element_start = 0;
  }

  if (foreign_state != CREATE_TABLE_FK_NONE &&
      foreign_state != CREATE_TABLE_FK_AFTER_PARENT_TABLE &&
      foreign_state != CREATE_TABLE_FK_AFTER_PARENT_LIST) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE foreign key");
  } else if (check_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE CHECK constraint");
  } else if (check_tail_state == CREATE_TABLE_CHECK_AFTER_NOT) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE CHECK constraint");
  } else if (column_needs_type) {
    mylite_parser_reject(ctx, pending_token,
                         "invalid CREATE TABLE column type");
  } else if (column_in_definition &&
             (column_type_parameter_required ||
              column_type_incomplete(column_type_state))) {
    mylite_parser_reject(ctx, pending_token,
                         "malformed CREATE TABLE column type");
  } else if (column_in_definition &&
             !column_definition_tail_complete(column_tail_state)) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE column definition");
  } else if (index_candidate) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE index key part");
  } else if (index_using_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE index USING");
  } else if (index_using_requires_key_list) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE index USING");
  } else {
    validate_create_table_tail_options(ctx, start);
  }
}

void mylite_parser_validate_create_index_statement(MyliteParseContext *ctx,
                                                    MyliteToken start) {
  enum {
    INDEX_KEY_NEED_PART,
    INDEX_KEY_AFTER_NAME,
    INDEX_KEY_AFTER_DOT,
    INDEX_KEY_PREFIX_VALUE,
    INDEX_KEY_PREFIX_AFTER_VALUE,
    INDEX_KEY_AFTER_PART,
    INDEX_KEY_AFTER_DIRECTION,
    INDEX_KEY_IN_FUNCTION
  };
  enum {
    CREATE_INDEX_OPTION_READY,
    CREATE_INDEX_OPTION_NUMBER,
    CREATE_INDEX_OPTION_STRING
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_on = 0;
  int in_key_list = 0;
  int key_list_done = 0;
  int create_index_name_seen = 0;
  int index_using_pending = 0;
  int index_option_state = CREATE_INDEX_OPTION_READY;
  int index_option_equals = 0;
  int depth = 0;
  int key_state = INDEX_KEY_NEED_PART;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (!saw_on) {
      if (!create_index_name_seen) {
        create_index_name_seen = 1;
        continue;
      }
      if (index_using_pending) {
        if (!index_using_type_token(token)) {
          mylite_parser_reject(ctx, pending_token,
                               "invalid CREATE INDEX USING");
          return;
        }
        index_using_pending = 0;
        continue;
      }
      if (token_id == ML_USING || token_id == ML_TYPE) {
        index_using_pending = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_ON) {
        saw_on = 1;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (!in_key_list) {
      if (key_list_done) {
        if (index_option_state == CREATE_INDEX_OPTION_NUMBER) {
          if (token_id == ML_EQUALS && !index_option_equals) {
            index_option_equals = 1;
            continue;
          }
          if (!create_index_option_number_token(token_id, token)) {
            mylite_parser_reject(ctx, pending_token,
                                 "invalid CREATE INDEX option");
            return;
          }
          index_option_state = CREATE_INDEX_OPTION_READY;
          index_option_equals = 0;
          continue;
        }
        if (index_option_state == CREATE_INDEX_OPTION_STRING) {
          if (token_id == ML_EQUALS && !index_option_equals) {
            index_option_equals = 1;
            continue;
          }
          if (!create_table_tail_option_string_token(token_id, token)) {
            mylite_parser_reject(ctx, pending_token,
                                 "invalid CREATE INDEX option");
            return;
          }
          index_option_state = CREATE_INDEX_OPTION_READY;
          index_option_equals = 0;
          continue;
        }
        if (index_using_pending) {
          if (!index_using_type_token(token)) {
            mylite_parser_reject(ctx, pending_token,
                                 "invalid CREATE INDEX USING");
            return;
          }
          index_using_pending = 0;
          continue;
        }
        if (token_id == ML_USING || token_id == ML_TYPE) {
          index_using_pending = 1;
          pending_token = token;
          continue;
        }
        if (token_id == ML_KEY_BLOCK_SIZE) {
          index_option_state = CREATE_INDEX_OPTION_NUMBER;
          index_option_equals = 0;
          pending_token = token;
          continue;
        }
        if (token_id == ML_COMMENT || token_id == ML_ENGINE_ATTRIBUTE ||
            token_id == ML_SECONDARY_ENGINE_ATTRIBUTE) {
          index_option_state = CREATE_INDEX_OPTION_STRING;
          index_option_equals = 0;
          pending_token = token;
          continue;
        }
        if (token_id == ML_SEMI) {
          break;
        }
        continue;
      }
      if (token_id == ML_LP) {
        in_key_list = 1;
        depth = 1;
        pending_token = token;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (depth > 1) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 1 && key_state == INDEX_KEY_IN_FUNCTION) {
          key_state = INDEX_KEY_AFTER_PART;
        }
      }
      continue;
    }

    if (key_state == INDEX_KEY_PREFIX_VALUE) {
      if (!create_index_prefix_length_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_PREFIX_AFTER_VALUE;
      continue;
    }

    if (key_state == INDEX_KEY_PREFIX_AFTER_VALUE) {
      if (token_id != ML_RP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_PART;
      continue;
    }

    if (token_id == ML_LP) {
      if (key_state == INDEX_KEY_NEED_PART) {
        key_state = INDEX_KEY_IN_FUNCTION;
        depth = 2;
        pending_token = token;
        continue;
      }
      if (key_state == INDEX_KEY_AFTER_NAME) {
        key_state = INDEX_KEY_PREFIX_VALUE;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
      return;
    }

    if (token_id == ML_RP) {
      if (key_state == INDEX_KEY_NEED_PART ||
          key_state == INDEX_KEY_AFTER_DOT ||
          key_state == INDEX_KEY_IN_FUNCTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      in_key_list = 0;
      key_list_done = 1;
      continue;
    }

    if (token_id == ML_COMMA) {
      if (key_state != INDEX_KEY_AFTER_NAME &&
          key_state != INDEX_KEY_AFTER_PART &&
          key_state != INDEX_KEY_AFTER_DIRECTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_NEED_PART;
      pending_token = token;
      continue;
    }

    if (token_id == ML_ASC || token_id == ML_DESC) {
      if (key_state != INDEX_KEY_AFTER_NAME &&
          key_state != INDEX_KEY_AFTER_PART) {
        mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_DIRECTION;
      continue;
    }

    if (token_id == ML_DOT) {
      if (key_state != INDEX_KEY_AFTER_NAME) {
        mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_DOT;
      pending_token = token;
      continue;
    }

    if (key_state == INDEX_KEY_NEED_PART ||
        key_state == INDEX_KEY_AFTER_DOT) {
      if (!dml_row_alias_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_NAME;
      continue;
    }

    mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
    return;
  }

  if (in_key_list) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE INDEX key part");
  } else if (index_using_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE INDEX USING");
  } else if (index_option_state != CREATE_INDEX_OPTION_READY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE INDEX option");
  }
}

void mylite_parser_validate_alter_table_statement(MyliteParseContext *ctx,
                                                  MyliteToken start) {
  enum {
    ALTER_INDEX_KEY_NEED_PART,
    ALTER_INDEX_KEY_AFTER_NAME,
    ALTER_INDEX_KEY_AFTER_DOT,
    ALTER_INDEX_KEY_PREFIX_VALUE,
    ALTER_INDEX_KEY_PREFIX_AFTER_VALUE,
    ALTER_INDEX_KEY_AFTER_PART,
    ALTER_INDEX_KEY_AFTER_DIRECTION,
    ALTER_INDEX_KEY_IN_FUNCTION
  };
  enum {
    ALTER_FK_NONE,
    ALTER_FK_AFTER_FOREIGN,
    ALTER_FK_BEFORE_CHILD_LIST,
    ALTER_FK_FIND_REFERENCES,
    ALTER_FK_AFTER_REFERENCES,
    ALTER_FK_AFTER_PARENT_TABLE,
    ALTER_FK_AFTER_PARENT_TABLE_DOT,
    ALTER_FK_AFTER_PARENT_LIST,
    ALTER_FK_AFTER_MATCH,
    ALTER_FK_AFTER_ON,
    ALTER_FK_AFTER_ON_ACTION,
    ALTER_FK_AFTER_SET,
    ALTER_FK_AFTER_NO
  };
  enum {
    ALTER_CHECK_NONE,
    ALTER_CHECK_READY,
    ALTER_CHECK_AFTER_NOT,
    ALTER_CHECK_DONE
  };
  enum {
    ALTER_COLUMN_NONE,
    ALTER_COLUMN_MODIFY_EXPECT_NAME,
    ALTER_COLUMN_CHANGE_EXPECT_OLD_NAME,
    ALTER_COLUMN_CHANGE_EXPECT_NEW_NAME,
    ALTER_COLUMN_EXPECT_TYPE,
    ALTER_COLUMN_IN_DEFINITION
  };
  enum {
    ALTER_INDEX_OPTION_READY,
    ALTER_INDEX_OPTION_NUMBER,
    ALTER_INDEX_OPTION_STRING,
    ALTER_INDEX_OPTION_USING,
    ALTER_INDEX_OPTION_AFTER_WITH,
    ALTER_INDEX_OPTION_AFTER_WITH_PARSER
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_table = 0;
  int table_ref_done = 0;
  int table_name_parts = 0;
  int table_dot_pending = 0;
  int add_scan = 0;
  int add_index_candidate = 0;
  int add_foreign_state = ALTER_FK_NONE;
  int add_foreign_tail_flags = 0;
  int check_pending = 0;
  int check_tail_state = ALTER_CHECK_NONE;
  int add_column_expect_name = 0;
  int add_column_type_pending = 0;
  int add_column_in_definition = 0;
  int add_constraint_prefix = 0;
  int alter_column_state = ALTER_COLUMN_NONE;
  int alter_column_optional_keyword = 0;
  int column_type_parameter_pending = 0;
  int column_type_parameter_required = 0;
  int column_type_parameter_forbidden = 0;
  ColumnTypeParameterKind column_type_parameter_list_kind =
      COLUMN_TYPE_PARAMETER_NONE;
  ColumnTypeState column_type_state = COLUMN_TYPE_STATE_COMPLETE;
  ColumnTypeCharsetModifierState column_type_charset_modifier_state =
      COLUMN_TYPE_CHARSET_AVAILABLE;
  int column_type_modifier_mask = 0;
  int column_temporal_precision_pending = 0;
  int column_precision_modifier_pending = 0;
  ColumnDefinitionTailState column_tail_state =
      COLUMN_DEFINITION_TAIL_READY;
  int column_tail_flags = 0;
  int validate_key_list = 0;
  int add_index_options = 0;
  int add_index_option_state = ALTER_INDEX_OPTION_READY;
  int add_index_option_equals = 0;
  int index_using_pending = 0;
  int index_using_requires_key_list = 0;
  int add_index_name_pending = 0;
  int depth = 0;
  int key_state = ALTER_INDEX_KEY_NEED_PART;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (!saw_table) {
      if (token_id != ML_TABLE) {
        return;
      }
      saw_table = 1;
      continue;
    }

    if (!table_ref_done) {
      if (token_id == ML_SEMI) {
        return;
      }
      if (table_dot_pending) {
        table_name_parts++;
        table_dot_pending = 0;
        continue;
      }
      if (table_name_parts == 0) {
        table_name_parts = 1;
        continue;
      }
      if (token_id == ML_DOT && table_name_parts == 1) {
        table_dot_pending = 1;
        continue;
      }
      table_ref_done = 1;
    }

    if (validate_key_list) {
      if (depth > 1) {
        if (token_opens_nested_expression(token_id)) {
          depth++;
        } else if (token_closes_nested_expression(token_id)) {
          depth--;
          if (depth == 1 &&
              key_state == ALTER_INDEX_KEY_IN_FUNCTION) {
            key_state = ALTER_INDEX_KEY_AFTER_PART;
          }
        }
        continue;
      }

      if (key_state == ALTER_INDEX_KEY_PREFIX_VALUE) {
        if (!create_index_prefix_length_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_PREFIX_AFTER_VALUE;
        continue;
      }

      if (key_state == ALTER_INDEX_KEY_PREFIX_AFTER_VALUE) {
        if (token_id != ML_RP) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_PART;
        continue;
      }

      if (token_id == ML_LP) {
        if (key_state == ALTER_INDEX_KEY_NEED_PART) {
          key_state = ALTER_INDEX_KEY_IN_FUNCTION;
          depth = 2;
          pending_token = token;
          continue;
        }
        if (key_state == ALTER_INDEX_KEY_AFTER_NAME) {
          key_state = ALTER_INDEX_KEY_PREFIX_VALUE;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "malformed ALTER TABLE index key part");
        return;
      }

      if (token_id == ML_RP) {
        if (key_state == ALTER_INDEX_KEY_NEED_PART ||
            key_state == ALTER_INDEX_KEY_AFTER_DOT ||
            key_state == ALTER_INDEX_KEY_IN_FUNCTION) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        validate_key_list = 0;
        depth = 0;
        add_scan = 0;
        add_index_candidate = 0;
        add_index_options = 1;
        add_index_option_state = ALTER_INDEX_OPTION_READY;
      add_index_option_equals = 0;
      index_using_requires_key_list = 0;
      continue;
      }

      if (token_id == ML_COMMA) {
        if (key_state != ALTER_INDEX_KEY_AFTER_NAME &&
            key_state != ALTER_INDEX_KEY_AFTER_PART &&
            key_state != ALTER_INDEX_KEY_AFTER_DIRECTION) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_NEED_PART;
        pending_token = token;
        continue;
      }

      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (key_state != ALTER_INDEX_KEY_AFTER_NAME &&
            key_state != ALTER_INDEX_KEY_AFTER_PART) {
          mylite_parser_reject(ctx, token,
                               "malformed ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_DIRECTION;
        continue;
      }

      if (token_id == ML_DOT) {
        if (key_state != ALTER_INDEX_KEY_AFTER_NAME) {
          mylite_parser_reject(ctx, token,
                               "malformed ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_DOT;
        pending_token = token;
        continue;
      }

      if (key_state == ALTER_INDEX_KEY_NEED_PART ||
          key_state == ALTER_INDEX_KEY_AFTER_DOT) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_NAME;
        continue;
      }

      mylite_parser_reject(ctx, token,
                           "malformed ALTER TABLE index key part");
      return;
    }

    if (check_pending) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE CHECK constraint");
        return;
      }
      if (!validate_parenthesized_expression_body(
              ctx, &lexer, token,
              "incomplete ALTER TABLE CHECK constraint")) {
        return;
      }
      check_pending = 0;
      check_tail_state = ALTER_CHECK_READY;
      add_scan = 0;
      add_index_candidate = 0;
      continue;
    }

    if (check_tail_state != ALTER_CHECK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_SEMI) {
        if (check_tail_state == ALTER_CHECK_AFTER_NOT) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE CHECK constraint");
          return;
        }
        check_tail_state = ALTER_CHECK_NONE;
        add_column_in_definition = 0;
        alter_column_state = ALTER_COLUMN_NONE;
        column_type_parameter_pending = 0;
        column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
        column_type_parameter_required = 0;
        column_type_parameter_forbidden = 0;
        column_precision_modifier_pending = 0;
        column_tail_state = COLUMN_DEFINITION_TAIL_READY;
        column_tail_flags = 0;
        if (token_id == ML_SEMI) {
          break;
        }
        add_scan = 0;
        add_index_candidate = 0;
        continue;
      }

      if (check_tail_state == ALTER_CHECK_READY) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = ALTER_CHECK_DONE;
          continue;
        }
        if (token_id == ML_NOT) {
          check_tail_state = ALTER_CHECK_AFTER_NOT;
          pending_token = token;
          continue;
        }
      } else if (check_tail_state == ALTER_CHECK_AFTER_NOT) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = ALTER_CHECK_DONE;
          continue;
        }
      }

      mylite_parser_reject(ctx, token,
                           "malformed ALTER TABLE CHECK constraint");
      return;
    }

    if (add_foreign_state != ALTER_FK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_SEMI) {
        if (add_foreign_state != ALTER_FK_AFTER_PARENT_TABLE &&
            add_foreign_state != ALTER_FK_AFTER_PARENT_LIST) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key");
          return;
        }
        add_foreign_state = ALTER_FK_NONE;
        add_foreign_tail_flags = 0;
        add_scan = 0;
        add_index_candidate = 0;
        if (token_id == ML_SEMI) {
          break;
        }
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_FOREIGN) {
        if (token_id != ML_KEY) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key");
          return;
        }
        add_foreign_state = ALTER_FK_BEFORE_CHILD_LIST;
        pending_token = token;
        continue;
      }

      if (add_foreign_state == ALTER_FK_BEFORE_CHILD_LIST) {
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete ALTER TABLE foreign key column list")) {
            return;
          }
          add_foreign_state = ALTER_FK_FIND_REFERENCES;
          pending_token = token;
          continue;
        }
        continue;
      }

      if (add_foreign_state == ALTER_FK_FIND_REFERENCES) {
        if (token_ascii_equal(token, "references")) {
          add_foreign_state = ALTER_FK_AFTER_REFERENCES;
          pending_token = token;
        }
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_REFERENCES) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_PARENT_TABLE;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_PARENT_TABLE_DOT) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_PARENT_TABLE;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_PARENT_TABLE) {
        if (token_id == ML_DOT) {
          add_foreign_state = ALTER_FK_AFTER_PARENT_TABLE_DOT;
          pending_token = token;
          continue;
        }
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete ALTER TABLE foreign key reference list")) {
            return;
          }
          add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
          continue;
        }
      }

      if (add_foreign_state == ALTER_FK_AFTER_PARENT_TABLE ||
          add_foreign_state == ALTER_FK_AFTER_PARENT_LIST) {
        if (token_ascii_equal(token, "match")) {
          if ((add_foreign_tail_flags &
               (REFERENCE_TAIL_FLAG_MATCH | REFERENCE_TAIL_FLAG_ON_UPDATE |
                REFERENCE_TAIL_FLAG_ON_DELETE)) != 0) {
            mylite_parser_reject(ctx, token,
                                 "malformed ALTER TABLE foreign key option");
            return;
          }
          add_foreign_state = ALTER_FK_AFTER_MATCH;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          add_foreign_state = ALTER_FK_AFTER_ON;
          pending_token = token;
          continue;
        }
        if (token_id == ML_PARTITION || token_id == ML_REMOVE) {
          add_foreign_state = ALTER_FK_NONE;
          add_foreign_tail_flags = 0;
          add_scan = 0;
          add_index_candidate = 0;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "malformed ALTER TABLE foreign key option");
        return;
      }

      if (add_foreign_state == ALTER_FK_AFTER_MATCH) {
        if (!foreign_key_match_option(token_id, token)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key MATCH");
          return;
        }
        add_foreign_tail_flags |= REFERENCE_TAIL_FLAG_MATCH;
        add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_ON) {
        if (token_id != ML_DELETE && token_id != ML_UPDATE) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key option");
          return;
        }
        if (token_id == ML_DELETE) {
          if ((add_foreign_tail_flags & REFERENCE_TAIL_FLAG_ON_DELETE) != 0) {
            mylite_parser_reject(ctx, token,
                                 "malformed ALTER TABLE foreign key option");
            return;
          }
          add_foreign_tail_flags |= REFERENCE_TAIL_FLAG_ON_DELETE;
        } else {
          if ((add_foreign_tail_flags & REFERENCE_TAIL_FLAG_ON_UPDATE) != 0) {
            mylite_parser_reject(ctx, token,
                                 "malformed ALTER TABLE foreign key option");
            return;
          }
          add_foreign_tail_flags |= REFERENCE_TAIL_FLAG_ON_UPDATE;
        }
        add_foreign_state = ALTER_FK_AFTER_ON_ACTION;
        pending_token = token;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_ON_ACTION) {
        if (foreign_key_reference_action_token(token_id)) {
          add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
          continue;
        }
        if (token_id == ML_SET) {
          add_foreign_state = ALTER_FK_AFTER_SET;
          pending_token = token;
          continue;
        }
        if (token_id == ML_NO) {
          add_foreign_state = ALTER_FK_AFTER_NO;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE foreign key option");
        return;
      }

      if (add_foreign_state == ALTER_FK_AFTER_SET) {
        if (token_id != ML_DEFAULT && token_id != ML_NULL) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key option");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_NO) {
        if (!token_ascii_equal(token, "action")) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key option");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
        continue;
      }

      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if ((add_column_in_definition ||
         alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
        column_definition_tail_wants_boundary_token(column_tail_state,
                                                    token_id)) {
      if (!column_definition_tail_token(
              ctx, token_id, token, &column_tail_state, &depth,
              &check_pending, &pending_token, &column_tail_flags, 1,
              "malformed ALTER TABLE column definition")) {
        return;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if ((add_column_in_definition ||
           alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
          (column_type_parameter_required ||
           column_type_incomplete(column_type_state))) {
        mylite_parser_reject(ctx, token, "malformed ALTER TABLE column type");
        return;
      }
      if (add_index_options &&
          add_index_option_state != ALTER_INDEX_OPTION_READY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE index option");
        return;
      }
      break;
    }

    if (token_id == ML_COMMA) {
      if ((add_column_in_definition ||
           alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
          (column_type_parameter_required ||
           column_type_incomplete(column_type_state))) {
        mylite_parser_reject(ctx, token, "malformed ALTER TABLE column type");
        return;
      }
      if ((add_column_in_definition ||
           alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
          !column_definition_tail_complete(column_tail_state)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE column definition");
        return;
      }
      if (index_using_pending) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE index USING");
        return;
      }
      if (add_index_options &&
          add_index_option_state != ALTER_INDEX_OPTION_READY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE index option");
        return;
      }
      add_scan = 0;
      add_index_candidate = 0;
      add_index_options = 0;
      add_index_option_state = ALTER_INDEX_OPTION_READY;
      add_index_option_equals = 0;
      add_foreign_state = ALTER_FK_NONE;
      check_pending = 0;
      check_tail_state = ALTER_CHECK_NONE;
      add_column_expect_name = 0;
      add_column_type_pending = 0;
      add_column_in_definition = 0;
      add_constraint_prefix = 0;
      alter_column_state = ALTER_COLUMN_NONE;
      alter_column_optional_keyword = 0;
      column_type_parameter_pending = 0;
      column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
      column_type_parameter_required = 0;
      column_type_parameter_forbidden = 0;
      column_precision_modifier_pending = 0;
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      column_tail_flags = 0;
      index_using_pending = 0;
      index_using_requires_key_list = 0;
      add_index_name_pending = 0;
      continue;
    }

    if (add_index_options) {
      if (add_index_option_state == ALTER_INDEX_OPTION_NUMBER) {
        if (token_id == ML_EQUALS && !add_index_option_equals) {
          add_index_option_equals = 1;
          continue;
        }
        if (!create_index_option_number_token(token_id, token)) {
          mylite_parser_reject(ctx, pending_token,
                               "invalid ALTER TABLE index option");
          return;
        }
        add_index_option_state = ALTER_INDEX_OPTION_READY;
        add_index_option_equals = 0;
        continue;
      }

      if (add_index_option_state == ALTER_INDEX_OPTION_STRING) {
        if (token_id == ML_EQUALS && !add_index_option_equals) {
          add_index_option_equals = 1;
          continue;
        }
        if (!create_table_tail_option_string_token(token_id, token)) {
          mylite_parser_reject(ctx, pending_token,
                               "invalid ALTER TABLE index option");
          return;
        }
        add_index_option_state = ALTER_INDEX_OPTION_READY;
        add_index_option_equals = 0;
        continue;
      }

      if (add_index_option_state == ALTER_INDEX_OPTION_USING) {
        if (!index_using_type_token(token)) {
          mylite_parser_reject(ctx, pending_token,
                               "invalid ALTER TABLE index option");
          return;
        }
        add_index_option_state = ALTER_INDEX_OPTION_READY;
        continue;
      }

      if (add_index_option_state == ALTER_INDEX_OPTION_AFTER_WITH) {
        if (token_id != ML_PARSER) {
          mylite_parser_reject(ctx, pending_token,
                               "invalid ALTER TABLE index option");
          return;
        }
        add_index_option_state = ALTER_INDEX_OPTION_AFTER_WITH_PARSER;
        pending_token = token;
        continue;
      }

      if (add_index_option_state == ALTER_INDEX_OPTION_AFTER_WITH_PARSER) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "invalid ALTER TABLE index option");
          return;
        }
        add_index_option_state = ALTER_INDEX_OPTION_READY;
        continue;
      }

      if (token_id == ML_KEY_BLOCK_SIZE) {
        add_index_option_state = ALTER_INDEX_OPTION_NUMBER;
        add_index_option_equals = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_COMMENT || token_id == ML_ENGINE_ATTRIBUTE ||
          token_id == ML_SECONDARY_ENGINE_ATTRIBUTE) {
        add_index_option_state = ALTER_INDEX_OPTION_STRING;
        add_index_option_equals = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_USING || token_id == ML_TYPE) {
        add_index_option_state = ALTER_INDEX_OPTION_USING;
        pending_token = token;
        continue;
      }
      if (token_id == ML_WITH) {
        add_index_option_state = ALTER_INDEX_OPTION_AFTER_WITH;
        pending_token = token;
        continue;
      }
      if (token_id == ML_VISIBLE || token_id == ML_INVISIBLE) {
        continue;
      }

      mylite_parser_reject(ctx, token, "invalid ALTER TABLE index option");
      return;
    }

    if (alter_column_state != ALTER_COLUMN_NONE) {
      if (alter_column_state == ALTER_COLUMN_MODIFY_EXPECT_NAME ||
          alter_column_state == ALTER_COLUMN_CHANGE_EXPECT_OLD_NAME) {
        if (alter_column_optional_keyword && token_id == ML_COLUMN) {
          alter_column_optional_keyword = 0;
          continue;
        }
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE column definition");
          return;
        }
        alter_column_optional_keyword = 0;
        if (alter_column_state == ALTER_COLUMN_MODIFY_EXPECT_NAME) {
          alter_column_state =
              create_table_column_name_needs_type_check(token_id, token)
                  ? ALTER_COLUMN_EXPECT_TYPE
                  : ALTER_COLUMN_IN_DEFINITION;
        } else {
          alter_column_state = ALTER_COLUMN_CHANGE_EXPECT_NEW_NAME;
        }
        pending_token = token;
        continue;
      }

      if (alter_column_state == ALTER_COLUMN_CHANGE_EXPECT_NEW_NAME) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE column definition");
          return;
        }
        alter_column_state =
            create_table_column_name_needs_type_check(token_id, token)
                ? ALTER_COLUMN_EXPECT_TYPE
                : ALTER_COLUMN_IN_DEFINITION;
        pending_token = token;
        continue;
      }

      if (alter_column_state == ALTER_COLUMN_EXPECT_TYPE) {
        if (!create_table_column_type_start(token_id, token)) {
          mylite_parser_reject(ctx, token, "invalid ALTER TABLE column type");
          return;
        }
        alter_column_state = ALTER_COLUMN_IN_DEFINITION;
        column_type_parameter_pending = 1;
        column_type_parameter_required =
            column_type_requires_parameter(token_id, token, 0);
        column_type_parameter_forbidden =
            column_type_forbids_parameter(token_id, token, 0);
        column_type_parameter_list_kind =
            column_type_parameter_kind(token_id, token);
        column_type_state = column_type_state_after_start(token_id, token);
        column_type_charset_modifier_state = COLUMN_TYPE_CHARSET_AVAILABLE;
        column_type_modifier_mask =
            column_type_modifier_mask_after_start(token_id, token);
        column_temporal_precision_pending = 0;
        column_precision_modifier_pending =
            column_type_allows_precision_modifier(token_id, token);
        column_tail_state = COLUMN_DEFINITION_TAIL_READY;
        column_tail_flags = 0;
        continue;
      }

      if (alter_column_state == ALTER_COLUMN_IN_DEFINITION) {
        ColumnDefinitionTailState before_tail_state = column_tail_state;
        int consumed_type_parameters = 0;
        int consumed_precision_modifier = 0;
        int consumed_type_tail = 0;
        if (column_definition_tail_complete(column_tail_state) &&
            (token_id == ML_PARTITION || token_id == ML_REMOVE)) {
          alter_column_state = ALTER_COLUMN_NONE;
          add_scan = 0;
          add_index_candidate = 0;
          column_type_parameter_pending = 0;
          column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
          column_type_parameter_required = 0;
          column_type_parameter_forbidden = 0;
          column_precision_modifier_pending = 0;
          column_tail_state = COLUMN_DEFINITION_TAIL_READY;
          column_tail_flags = 0;
          continue;
        }
        if (column_type_parameter_forbidden && token_id == ML_LP) {
          mylite_parser_reject(ctx, token, "malformed ALTER TABLE column type");
          return;
        }
        if (token_id != ML_LP) {
          column_type_parameter_forbidden = 0;
        }
        if (!consume_column_type_parameter_list_if_pending(
                ctx, &lexer, token_id, token, &column_type_parameter_pending,
                &column_type_parameter_required, column_type_parameter_list_kind,
                "malformed ALTER TABLE column type",
                &consumed_type_parameters)) {
          return;
        }
        if (consumed_type_parameters) {
          column_type_state = COLUMN_TYPE_STATE_COMPLETE;
          column_precision_modifier_pending = 0;
          column_type_parameter_forbidden = 0;
          continue;
        }
        if (!consume_column_precision_modifier_if_pending(
                ctx, token_id, token, &column_precision_modifier_pending,
                &column_type_parameter_pending, &column_type_parameter_required,
                &column_type_parameter_list_kind,
                "malformed ALTER TABLE column type",
                &consumed_precision_modifier)) {
          return;
        }
        if (consumed_precision_modifier) {
          continue;
        }
        column_precision_modifier_pending = 0;
        if (before_tail_state == COLUMN_DEFINITION_TAIL_READY) {
          if (!consume_column_type_tail_token_if_pending(
                  ctx, token_id, token, &column_type_state,
                  &column_type_modifier_mask, &column_type_parameter_pending,
                  &column_type_parameter_required,
                  &column_type_parameter_forbidden,
                  &column_type_parameter_list_kind,
                  &column_type_charset_modifier_state,
                  "malformed ALTER TABLE column type",
                  &consumed_type_tail)) {
            return;
          }
          if (consumed_type_tail) {
            continue;
          }
          if (!column_type_charset_introducer_token(token_id, token)) {
            column_type_modifier_mask = 0;
            column_type_charset_modifier_state = COLUMN_TYPE_CHARSET_DONE;
          }
        } else if (before_tail_state ==
                       COLUMN_DEFINITION_TAIL_AFTER_CHARACTER &&
                   token_ascii_equal(token, "varying")) {
          mylite_parser_reject(ctx, token,
                               "malformed ALTER TABLE column definition");
          return;
        }
        if (column_temporal_precision_pending && token_id != ML_LP) {
          column_temporal_precision_pending = 0;
        }
        if (column_temporal_precision_pending) {
          column_temporal_precision_pending = 0;
          if (!validate_column_temporal_precision_list(
                  ctx, &lexer, token,
                  "malformed ALTER TABLE column definition")) {
            return;
          }
          continue;
        }
        if (token_id == ML_LP &&
            column_definition_tail_parenthesized_expression(
                column_tail_state)) {
          if (!validate_parenthesized_expression_body(
                  ctx, &lexer, token,
                  "malformed ALTER TABLE column definition")) {
            return;
          }
          column_definition_tail_finish_parenthesized_expression(
              &column_tail_state, &column_tail_flags);
          continue;
        }
        if (!column_definition_tail_token(
                ctx, token_id, token, &column_tail_state, &depth,
                &check_pending, &pending_token, &column_tail_flags, 1,
                "malformed ALTER TABLE column definition")) {
          return;
        }
        column_temporal_precision_pending =
            (before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT ||
             before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE) &&
            column_definition_temporal_function_token(token);
        continue;
      }
    }

    if (token_id == ML_ADD) {
      add_scan = 1;
      add_index_candidate = 0;
      add_foreign_state = ALTER_FK_NONE;
      add_foreign_tail_flags = 0;
      check_pending = 0;
      check_tail_state = ALTER_CHECK_NONE;
      add_column_expect_name = 0;
      add_column_type_pending = 0;
      add_column_in_definition = 0;
      add_constraint_prefix = 0;
      alter_column_state = ALTER_COLUMN_NONE;
      alter_column_optional_keyword = 0;
      column_type_parameter_pending = 0;
      column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
      column_type_parameter_required = 0;
      column_type_parameter_forbidden = 0;
      column_precision_modifier_pending = 0;
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      column_tail_flags = 0;
      add_index_name_pending = 0;
      pending_token = token;
      continue;
    }

    if (!add_scan && token_id == ML_MODIFY) {
      alter_column_state = ALTER_COLUMN_MODIFY_EXPECT_NAME;
      alter_column_optional_keyword = 1;
      pending_token = token;
      continue;
    }

    if (!add_scan && token_id == ML_CHANGE) {
      alter_column_state = ALTER_COLUMN_CHANGE_EXPECT_OLD_NAME;
      alter_column_optional_keyword = 1;
      pending_token = token;
      continue;
    }

    if (!add_scan) {
      continue;
    }

    if (add_column_type_pending) {
      if (!create_table_column_type_start(token_id, token)) {
        mylite_parser_reject(ctx, token, "invalid ALTER TABLE column type");
        return;
      }
      add_column_type_pending = 0;
      add_column_in_definition = 1;
      column_type_parameter_pending = 1;
      column_type_parameter_required =
          column_type_requires_parameter(token_id, token, 0);
      column_type_parameter_forbidden =
          column_type_forbids_parameter(token_id, token, 0);
      column_type_parameter_list_kind =
          column_type_parameter_kind(token_id, token);
      column_type_state = column_type_state_after_start(token_id, token);
      column_type_charset_modifier_state = COLUMN_TYPE_CHARSET_AVAILABLE;
      column_type_modifier_mask =
          column_type_modifier_mask_after_start(token_id, token);
      column_temporal_precision_pending = 0;
      column_precision_modifier_pending =
          column_type_allows_precision_modifier(token_id, token);
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      column_tail_flags = 0;
      continue;
    }

    if (add_column_in_definition) {
      ColumnDefinitionTailState before_tail_state = column_tail_state;
      int consumed_type_parameters = 0;
      int consumed_precision_modifier = 0;
      int consumed_type_tail = 0;
      if (column_definition_tail_complete(column_tail_state) &&
          (token_id == ML_PARTITION || token_id == ML_REMOVE)) {
        add_column_in_definition = 0;
        add_scan = 0;
        add_index_candidate = 0;
        column_type_parameter_pending = 0;
        column_type_parameter_list_kind = COLUMN_TYPE_PARAMETER_NONE;
        column_type_parameter_required = 0;
        column_type_parameter_forbidden = 0;
        column_precision_modifier_pending = 0;
        column_tail_state = COLUMN_DEFINITION_TAIL_READY;
        column_tail_flags = 0;
        continue;
      }
      if (column_type_parameter_forbidden && token_id == ML_LP) {
        mylite_parser_reject(ctx, token, "malformed ALTER TABLE column type");
        return;
      }
      if (token_id != ML_LP) {
        column_type_parameter_forbidden = 0;
      }
      if (!consume_column_type_parameter_list_if_pending(
              ctx, &lexer, token_id, token, &column_type_parameter_pending,
              &column_type_parameter_required, column_type_parameter_list_kind,
              "malformed ALTER TABLE column type",
              &consumed_type_parameters)) {
        return;
      }
      if (consumed_type_parameters) {
        column_type_state = COLUMN_TYPE_STATE_COMPLETE;
        column_precision_modifier_pending = 0;
        column_type_parameter_forbidden = 0;
        continue;
      }
      if (!consume_column_precision_modifier_if_pending(
              ctx, token_id, token, &column_precision_modifier_pending,
              &column_type_parameter_pending, &column_type_parameter_required,
              &column_type_parameter_list_kind,
              "malformed ALTER TABLE column type",
              &consumed_precision_modifier)) {
        return;
      }
      if (consumed_precision_modifier) {
        continue;
      }
      column_precision_modifier_pending = 0;
      if (before_tail_state == COLUMN_DEFINITION_TAIL_READY) {
        if (!consume_column_type_tail_token_if_pending(
                ctx, token_id, token, &column_type_state,
                &column_type_modifier_mask, &column_type_parameter_pending,
                &column_type_parameter_required,
                &column_type_parameter_forbidden,
                &column_type_parameter_list_kind,
                &column_type_charset_modifier_state,
                "malformed ALTER TABLE column type", &consumed_type_tail)) {
          return;
        }
        if (consumed_type_tail) {
          continue;
        }
        if (!column_type_charset_introducer_token(token_id, token)) {
          column_type_modifier_mask = 0;
          column_type_charset_modifier_state = COLUMN_TYPE_CHARSET_DONE;
        }
      } else if (before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_CHARACTER &&
                 token_ascii_equal(token, "varying")) {
        mylite_parser_reject(ctx, token,
                             "malformed ALTER TABLE column definition");
        return;
      }
      if (column_temporal_precision_pending && token_id != ML_LP) {
        column_temporal_precision_pending = 0;
      }
      if (column_temporal_precision_pending) {
        column_temporal_precision_pending = 0;
        if (!validate_column_temporal_precision_list(
                ctx, &lexer, token,
                "malformed ALTER TABLE column definition")) {
          return;
        }
        continue;
      }
      if (token_id == ML_LP &&
          column_definition_tail_parenthesized_expression(
              column_tail_state)) {
        if (!validate_parenthesized_expression_body(
                ctx, &lexer, token,
                "malformed ALTER TABLE column definition")) {
          return;
        }
        column_definition_tail_finish_parenthesized_expression(
            &column_tail_state, &column_tail_flags);
        continue;
      }
      if (!column_definition_tail_token(
              ctx, token_id, token, &column_tail_state, &depth,
              &check_pending, &pending_token, &column_tail_flags, 1,
              "malformed ALTER TABLE column definition")) {
        return;
      }
      column_temporal_precision_pending =
          (before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT ||
           before_tail_state == COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE) &&
          column_definition_temporal_function_token(token);
      continue;
    }

    if (add_index_candidate && index_using_pending) {
      if (!index_using_type_token(token)) {
        mylite_parser_reject(ctx, pending_token,
                             "invalid ALTER TABLE index USING");
        return;
      }
      index_using_pending = 0;
      index_using_requires_key_list = 1;
      pending_token = token;
      continue;
    }

    if (add_index_candidate && index_using_requires_key_list) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, token, "invalid ALTER TABLE index USING");
        return;
      }
      index_using_requires_key_list = 0;
    }

    if (token_id == ML_CONSTRAINT) {
      add_constraint_prefix = 1;
      pending_token = token;
      continue;
    }

    if (add_constraint_prefix > 0) {
      if (alter_table_add_index_marker(token_id)) {
        add_index_candidate = 1;
        add_index_name_pending = 1;
        add_constraint_prefix = 0;
        continue;
      }
      if (token_id == ML_FOREIGN) {
        add_foreign_state = ALTER_FK_AFTER_FOREIGN;
        add_foreign_tail_flags = 0;
        add_constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHECK) {
        check_pending = 1;
        add_constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (add_constraint_prefix == 1) {
        mylite_parser_require_strict_identifier_atom(ctx, token);
        if (ctx->failed) {
          return;
        }
        add_constraint_prefix = 2;
      }
      continue;
    }

    if (token_id == ML_COLUMN) {
      add_column_expect_name = 1;
      continue;
    }

    if (add_column_expect_name && token_id == ML_LP) {
      depth = 1;
      add_scan = 0;
      add_column_expect_name = 0;
      continue;
    }

    if (token_id == ML_FOREIGN) {
      add_foreign_state = ALTER_FK_AFTER_FOREIGN;
      add_foreign_tail_flags = 0;
      pending_token = token;
      continue;
    }

    if (token_id == ML_CHECK) {
      check_pending = 1;
      pending_token = token;
      continue;
    }

    if (alter_table_add_non_index_marker(token_id)) {
      add_scan = 0;
      add_index_candidate = 0;
      continue;
    }

    if (alter_table_add_index_marker(token_id)) {
      add_index_candidate = 1;
      add_index_name_pending = 1;
      continue;
    }

    if (add_index_candidate && add_index_name_pending && token_id != ML_LP &&
        token_id != ML_USING && token_id != ML_TYPE) {
      mylite_parser_require_strict_identifier_atom(ctx, token);
      if (ctx->failed) {
        return;
      }
      add_index_name_pending = 0;
      continue;
    }

    if (add_index_candidate &&
        (token_id == ML_USING || token_id == ML_TYPE)) {
      index_using_pending = 1;
      add_index_name_pending = 0;
      pending_token = token;
      continue;
    }

    if (!add_index_candidate && token_id != ML_LP) {
      add_column_type_pending =
          create_table_column_name_needs_type_check(token_id, token);
      add_column_expect_name = 0;
      pending_token = token;
      continue;
    }

    if (token_id == ML_LP) {
      if (add_index_candidate) {
        validate_key_list = 1;
        depth = 1;
        key_state = ALTER_INDEX_KEY_NEED_PART;
        add_index_name_pending = 0;
        pending_token = token;
      } else {
        depth = 1;
      }
    }
  }

  if (add_foreign_state != ALTER_FK_NONE &&
      add_foreign_state != ALTER_FK_AFTER_PARENT_TABLE &&
      add_foreign_state != ALTER_FK_AFTER_PARENT_LIST) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE foreign key");
  } else if (check_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE CHECK constraint");
  } else if (check_tail_state == ALTER_CHECK_AFTER_NOT) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE CHECK constraint");
  } else if (add_column_type_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "invalid ALTER TABLE column type");
  } else if (alter_column_state != ALTER_COLUMN_NONE &&
             alter_column_state != ALTER_COLUMN_IN_DEFINITION) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE column definition");
  } else if ((add_column_in_definition ||
              alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
             (column_type_parameter_required ||
              column_type_incomplete(column_type_state))) {
    mylite_parser_reject(ctx, pending_token,
                         "malformed ALTER TABLE column type");
  } else if ((add_column_in_definition ||
              alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
             !column_definition_tail_complete(column_tail_state)) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE column definition");
  } else if (validate_key_list) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE index key part");
  } else if (add_index_options &&
             add_index_option_state != ALTER_INDEX_OPTION_READY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE index option");
  } else if (index_using_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE index USING");
  } else if (index_using_requires_key_list) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE index USING");
  } else if (!ctx->failed) {
    validate_alter_table_expression_tails(ctx, start);
    if (!ctx->failed) {
      validate_alter_table_option_values(ctx, start);
    }
  }
}

static void validate_alter_table_option_values(MyliteParseContext *ctx,
                                               MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_table = 0;
  int table_ref_done = 0;
  int table_name_parts = 0;
  int table_dot_pending = 0;
  int depth = 0;
  int previous_comma = 0;
  int option_allowed = 1;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (!saw_table) {
      if (token_id == ML_TABLE) {
        saw_table = 1;
      }
      continue;
    }

    if (!table_ref_done) {
      if (token_id == ML_SEMI) {
        return;
      }
      if (table_dot_pending) {
        table_name_parts++;
        table_dot_pending = 0;
        continue;
      }
      if (table_name_parts == 0) {
        table_name_parts = 1;
        continue;
      }
      if (token_id == ML_DOT && table_name_parts == 1) {
        table_dot_pending = 1;
        continue;
      }
      table_ref_done = 1;
    }

    if (token_is_statement_terminator(token_id, token)) {
      return;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth = 1;
      previous_comma = 0;
      option_allowed = 0;
      continue;
    }

    if (token_id == ML_COMMA) {
      if (previous_comma) {
        mylite_parser_reject(ctx, token,
                             "incomplete ALTER TABLE table option");
        return;
      }
      previous_comma = 1;
      option_allowed = 1;
      pending_token = token;
      continue;
    }
    previous_comma = 0;

    if (!option_allowed) {
      continue;
    }

    if (token_id == ML_AUTO_INCREMENT ||
        token_id == ML_AVG_ROW_LENGTH || token_id == ML_CHECKSUM ||
        token_id == ML_DELAY_KEY_WRITE ||
        token_id == ML_KEY_BLOCK_SIZE || token_id == ML_MAX_ROWS ||
        token_id == ML_MIN_ROWS) {
      if (!validate_alter_table_option_value(
              ctx, &lexer, token,
              CREATE_TABLE_OPTION_VALUE_DECIMAL_NUMBER)) {
        return;
      }
      option_allowed = 1;
      continue;
    }

    if (token_id == ML_AUTOEXTEND_SIZE) {
      if (!validate_alter_table_option_value(
              ctx, &lexer, token,
              CREATE_TABLE_OPTION_VALUE_SIZE_NUMBER)) {
        return;
      }
      option_allowed = 1;
      continue;
    }

    if (token_id == ML_STATS_SAMPLE_PAGES) {
      if (!validate_alter_table_option_value(
              ctx, &lexer, token,
              CREATE_TABLE_OPTION_VALUE_STATS_SAMPLE_PAGES)) {
        return;
      }
      option_allowed = 1;
      continue;
    }

    if (token_id == ML_TABLESPACE) {
      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id <= 0) {
        return;
      }
      option_allowed = 1;
      continue;
    }

    if (token_id == ML_STORAGE) {
      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id <= 0 ||
          !create_table_tail_option_storage_token(token_id, token)) {
        mylite_parser_reject(ctx, token_id > 0 ? token : pending_token,
                             "invalid ALTER TABLE storage option");
        return;
      }
      option_allowed = 1;
      continue;
    }

    if (token_id == ML_COMMENT || token_id == ML_COMPRESSION ||
        token_id == ML_CONNECTION || token_id == ML_ENCRYPTION ||
        token_id == ML_ENGINE_ATTRIBUTE || token_id == ML_PASSWORD ||
        token_id == ML_SECONDARY_ENGINE_ATTRIBUTE) {
      if (!validate_alter_table_option_value(
              ctx, &lexer, token, CREATE_TABLE_OPTION_VALUE_STRING)) {
        return;
      }
      option_allowed = 1;
      continue;
    }

    if (token_id == ML_DATA || token_id == ML_INDEX) {
      MyliteToken option = token;
      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id != ML_DIRECTORY) {
        option_allowed = 0;
        continue;
      }
      if (!validate_alter_table_option_value(
              ctx, &lexer, option, CREATE_TABLE_OPTION_VALUE_STRING)) {
        return;
      }
      option_allowed = 1;
      continue;
    }

    option_allowed = 0;
  }
}

static int validate_alter_table_option_value(MyliteParseContext *ctx,
                                             MyliteLexer *lexer,
                                             MyliteToken option,
                                             int value_kind) {
  MyliteToken token;
  int token_id = mylite_lexer_next(lexer, &token);

  if (token_id == ML_EQUALS) {
    token_id = mylite_lexer_next(lexer, &token);
  }
  if (token_id <= 0 ||
      !create_table_tail_option_value_token(value_kind, token_id, token)) {
    mylite_parser_reject(ctx, option, "invalid ALTER TABLE table option");
    return 0;
  }
  return 1;
}

static void validate_alter_table_expression_tails(MyliteParseContext *ctx,
                                                  MyliteToken start) {
  enum {
    ALTER_EXPR_READY,
    ALTER_EXPR_AFTER_ALTER,
    ALTER_EXPR_AFTER_ALTER_COLUMN,
    ALTER_EXPR_AFTER_ALTER_NAME,
    ALTER_EXPR_AFTER_ALTER_SET,
    ALTER_EXPR_AFTER_ORDER,
    ALTER_EXPR_AFTER_ORDER_BY,
    ALTER_EXPR_AFTER_PARTITION,
    ALTER_EXPR_AFTER_PARTITION_BY,
    ALTER_EXPR_AFTER_PARTITION_METHOD
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_table = 0;
  int table_ref_done = 0;
  int table_name_parts = 0;
  int table_dot_pending = 0;
  int state = ALTER_EXPR_READY;
  int partition_key_method = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (!saw_table) {
      if (token_id == ML_TABLE) {
        saw_table = 1;
      }
      continue;
    }

    if (!table_ref_done) {
      if (token_id == ML_SEMI) {
        return;
      }
      if (table_dot_pending) {
        table_name_parts++;
        table_dot_pending = 0;
        continue;
      }
      if (table_name_parts == 0) {
        table_name_parts = 1;
        continue;
      }
      if (token_id == ML_DOT && table_name_parts == 1) {
        table_dot_pending = 1;
        continue;
      }
      table_ref_done = 1;
    }

    if (token_is_statement_terminator(token_id, token)) {
      return;
    }

    if (state == ALTER_EXPR_AFTER_ORDER) {
      if (token_id != ML_BY) {
        state = ALTER_EXPR_READY;
      } else {
        state = ALTER_EXPR_AFTER_ORDER_BY;
        pending_token = token;
        continue;
      }
    }

    if (state == ALTER_EXPR_AFTER_ORDER_BY) {
      validate_alter_table_order_by_from(ctx, token);
      return;
    }

    if (state == ALTER_EXPR_AFTER_PARTITION) {
      if (token_id != ML_BY) {
        state = ALTER_EXPR_READY;
      } else {
        state = ALTER_EXPR_AFTER_PARTITION_BY;
        pending_token = token;
        continue;
      }
    }

    if (state == ALTER_EXPR_AFTER_PARTITION_BY) {
      if (token_ascii_equal(token, "linear")) {
        continue;
      }
      if (alter_table_partition_method_token(token_id, token)) {
        state = ALTER_EXPR_AFTER_PARTITION_METHOD;
        partition_key_method = token_id == ML_KEY;
      } else {
        state = ALTER_EXPR_READY;
      }
      pending_token = token;
      continue;
    }

    if (state == ALTER_EXPR_AFTER_PARTITION_METHOD) {
      if (token_id == ML_COLUMNS) {
        pending_token = token;
        continue;
      }
      if (token_id == ML_LP) {
        if (partition_key_method &&
            alter_table_parenthesized_group_empty(ctx, token)) {
          state = ALTER_EXPR_READY;
          partition_key_method = 0;
          continue;
        }
        mylite_parser_validate_parenthesized_expression_list_from(
            ctx, token, "malformed ALTER TABLE partition expression");
        state = ALTER_EXPR_READY;
        partition_key_method = 0;
        continue;
      }
      if (token_id == ML_ASSIGN || token_id == ML_EQUALS ||
          token_id == ML_NUMBER_LITERAL || token_id == ML_BOOLEAN_NUMBER ||
          token_id == ML_FACTOR_NUMBER) {
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete ALTER TABLE partition expression");
      partition_key_method = 0;
      return;
    }

    if (state == ALTER_EXPR_AFTER_ALTER) {
      if (token_id == ML_COLUMN) {
        state = ALTER_EXPR_AFTER_ALTER_COLUMN;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHECK || token_id == ML_CONSTRAINT ||
          token_id == ML_INDEX) {
        state = ALTER_EXPR_READY;
        continue;
      }
      state = ALTER_EXPR_AFTER_ALTER_NAME;
      continue;
    }

    if (state == ALTER_EXPR_AFTER_ALTER_COLUMN) {
      state = ALTER_EXPR_AFTER_ALTER_NAME;
      continue;
    }

    if (state == ALTER_EXPR_AFTER_ALTER_NAME) {
      if (token_id == ML_SET) {
        state = ALTER_EXPR_AFTER_ALTER_SET;
        pending_token = token;
        continue;
      }
      state = ALTER_EXPR_READY;
    }

    if (state == ALTER_EXPR_AFTER_ALTER_SET) {
      if (token_id != ML_DEFAULT) {
        state = ALTER_EXPR_READY;
        continue;
      }
      pending_token = token;
      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id <= 0 || token_id == ML_COMMA || token_id == ML_SEMI) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE DEFAULT expression");
        return;
      }
      mylite_parser_validate_expression_until_from(
          ctx, token, ML_COMMA, "malformed ALTER TABLE DEFAULT expression");
      if (ctx->failed) {
        return;
      }
      state = ALTER_EXPR_READY;
      continue;
    }

    if (token_id == ML_ORDER) {
      state = ALTER_EXPR_AFTER_ORDER;
      pending_token = token;
      continue;
    }
    if (token_id == ML_PARTITION) {
      state = ALTER_EXPR_AFTER_PARTITION;
      pending_token = token;
      continue;
    }
    if (token_id == ML_ALTER) {
      state = ALTER_EXPR_AFTER_ALTER;
      pending_token = token;
      continue;
    }
  }
}

static void validate_alter_table_order_by_from(MyliteParseContext *ctx,
                                               MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  MyliteToken previous_top_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int need_expression = 1;
  int after_direction = 0;
  int previous_top_token_id = 0;
  int previous_was_operator = 1;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (!query_expression_depth_token(
              ctx, token_id, token, &depth, &expression_stack,
              "malformed ALTER TABLE ORDER BY")) {
        return;
      }
      if (token_closes_nested_expression(token_id) && depth == 0) {
        previous_top_token_id = token_id;
        previous_top_token = token;
        previous_was_operator = 0;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if (need_expression) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE ORDER BY");
      } else if (previous_was_operator) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete ALTER TABLE ORDER BY");
      }
      return;
    }

    if (token_id == ML_COMMA) {
      if (need_expression || previous_was_operator) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE ORDER BY");
        return;
      }
      need_expression = 1;
      after_direction = 0;
      previous_top_token_id = 0;
      previous_was_operator = 1;
      pending_token = token;
      memset(&expression_stack, 0, sizeof(expression_stack));
      continue;
    }

    if (after_direction) {
      mylite_parser_reject(ctx, token, "malformed ALTER TABLE ORDER BY");
      return;
    }

    if (token_id == ML_ASC || token_id == ML_DESC) {
      if (need_expression || previous_was_operator) {
        mylite_parser_reject(ctx, token, "malformed ALTER TABLE ORDER BY");
        return;
      }
      after_direction = 1;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 0;
      continue;
    }

    if (need_expression) {
      need_expression = 0;
    }
    if (!query_expression_token(
            ctx, token_id, token, &depth, &previous_top_token_id,
            &previous_top_token, &previous_was_operator, &expression_stack,
            "malformed ALTER TABLE ORDER BY")) {
      return;
    }
  }

  if (need_expression) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE ORDER BY");
  } else if (previous_was_operator) {
    mylite_parser_reject(ctx, previous_top_token,
                         "incomplete ALTER TABLE ORDER BY");
  }
}

static int alter_table_partition_method_token(int token_id, MyliteToken token) {
  return token_id == ML_KEY || token_ascii_equal(token, "hash") ||
         token_ascii_equal(token, "list") || token_ascii_equal(token, "range");
}

static int alter_table_parenthesized_group_empty(MyliteParseContext *ctx,
                                                 MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_start = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_start) {
      if (token.offset == start.offset) {
        saw_start = 1;
      }
      continue;
    }
    return token_id == ML_RP;
  }

  return 0;
}

void mylite_parser_validate_view_statement(MyliteParseContext *ctx,
                                            MyliteToken start) {
  enum {
    VIEW_FIND_AS,
    VIEW_AFTER_AS,
    VIEW_TAIL
  };
  enum {
    VIEW_QUERY_TAIL_NONE,
    VIEW_QUERY_TAIL_AFTER_RP,
    VIEW_QUERY_TAIL_AFTER_ORDER,
    VIEW_QUERY_TAIL_AFTER_ORDER_BY,
    VIEW_QUERY_TAIL_ORDER_EXPR,
    VIEW_QUERY_TAIL_ORDER_DIRECTION,
    VIEW_QUERY_TAIL_AFTER_LIMIT,
    VIEW_QUERY_TAIL_AFTER_LIMIT_VALUE,
    VIEW_QUERY_TAIL_AFTER_LIMIT_COMMA,
    VIEW_QUERY_TAIL_AFTER_LIMIT_OFFSET,
    VIEW_QUERY_TAIL_AFTER_LIMIT_FINAL_VALUE,
    VIEW_QUERY_TAIL_AFTER_WITH,
    VIEW_QUERY_TAIL_AFTER_SCOPE,
    VIEW_QUERY_TAIL_AFTER_CHECK,
    VIEW_QUERY_TAIL_AFTER_OPTION
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  MyliteToken order_previous_top_token = start;
  int token_id;
  int saw_statement = 0;
  int state = VIEW_FIND_AS;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int tail_state = VIEW_QUERY_TAIL_NONE;
  int order_previous_top_token_id = 0;
  int order_previous_was_operator = 1;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      const char *nested_message = "malformed VIEW parenthesized query tail";
      if (tail_state == VIEW_QUERY_TAIL_ORDER_EXPR) {
        nested_message = "malformed VIEW parenthesized query ORDER BY";
      }
      if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                        &expression_stack, nested_message)) {
        return;
      }
      if (token_closes_nested_expression(token_id)) {
        if (depth == 0 && state != VIEW_TAIL) {
          tail_state = VIEW_QUERY_TAIL_AFTER_RP;
          state = VIEW_TAIL;
        }
      }
      continue;
    }

    if (state == VIEW_FIND_AS) {
      if (token_id == ML_AS) {
        state = VIEW_AFTER_AS;
        pending_token = token;
      }
      continue;
    }

    if (state == VIEW_AFTER_AS) {
      if (token_id == ML_LP && parenthesized_query_start_follows(ctx, token)) {
        validate_parenthesized_query_body_from(ctx, token);
        if (ctx->failed) {
          return;
        }
        depth = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SELECT || token_id == ML_WITH) {
        validate_query_body_from(ctx, token_id, token);
      }
      return;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_RP) {
      if (token_id == ML_SEMI) {
        break;
      }
      if (select_set_operator(token_id)) {
        validate_query_set_operand_after_operator_from(
            ctx, token, "incomplete VIEW parenthesized query");
        return;
      }
      if (token_id == ML_ORDER) {
        tail_state = VIEW_QUERY_TAIL_AFTER_ORDER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        tail_state = VIEW_QUERY_TAIL_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_WITH) {
        tail_state = VIEW_QUERY_TAIL_AFTER_WITH;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed VIEW parenthesized query tail");
      return;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_ORDER) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete VIEW parenthesized query ORDER BY");
        return;
      }
      tail_state = VIEW_QUERY_TAIL_AFTER_ORDER_BY;
      pending_token = token;
      order_previous_top_token_id = 0;
      order_previous_was_operator = 1;
      order_previous_top_token = token;
      continue;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_ORDER_BY) {
      if (view_query_order_boundary(token_id) || token_id == ML_ASC ||
          token_id == ML_DESC) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete VIEW parenthesized query ORDER BY");
        return;
      }
      tail_state = VIEW_QUERY_TAIL_ORDER_EXPR;
    }

    if (tail_state == VIEW_QUERY_TAIL_ORDER_EXPR) {
      if (view_query_order_boundary(token_id)) {
        if (order_previous_was_operator) {
          mylite_parser_reject(
              ctx, order_previous_top_token,
              "incomplete VIEW parenthesized query ORDER BY");
          return;
        }
        if (token_id == ML_COMMA) {
          tail_state = VIEW_QUERY_TAIL_AFTER_ORDER_BY;
          pending_token = token;
          order_previous_top_token_id = 0;
          order_previous_was_operator = 1;
          order_previous_top_token = token;
          continue;
        }
        if (token_id == ML_LIMIT) {
          tail_state = VIEW_QUERY_TAIL_AFTER_LIMIT;
          pending_token = token;
          continue;
        }
        if (token_id == ML_WITH) {
          tail_state = VIEW_QUERY_TAIL_AFTER_WITH;
          pending_token = token;
          continue;
        }
        if (token_id == ML_SEMI) {
          break;
        }
        mylite_parser_reject(ctx, token,
                             "malformed VIEW parenthesized query ORDER BY");
        return;
      }
      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (order_previous_was_operator) {
          mylite_parser_reject(
              ctx, order_previous_top_token,
              "incomplete VIEW parenthesized query ORDER BY");
          return;
        }
        tail_state = VIEW_QUERY_TAIL_ORDER_DIRECTION;
        pending_token = token;
        continue;
      }
      if (token_closes_nested_expression(token_id)) {
        mylite_parser_reject(ctx, token,
                             "malformed VIEW parenthesized query ORDER BY");
        return;
      }
      if (!query_expression_token(
              ctx, token_id, token, &depth, &order_previous_top_token_id,
              &order_previous_top_token, &order_previous_was_operator,
              &expression_stack,
              "malformed VIEW parenthesized query ORDER BY")) {
        return;
      }
      continue;
    }

    if (tail_state == VIEW_QUERY_TAIL_ORDER_DIRECTION) {
      if (token_id == ML_COMMA) {
        tail_state = VIEW_QUERY_TAIL_AFTER_ORDER_BY;
        pending_token = token;
        order_previous_top_token_id = 0;
        order_previous_was_operator = 1;
        order_previous_top_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        tail_state = VIEW_QUERY_TAIL_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_WITH) {
        tail_state = VIEW_QUERY_TAIL_AFTER_WITH;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, token,
                           "malformed VIEW parenthesized query ORDER BY");
      return;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT ||
        tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT_COMMA ||
        tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT_OFFSET) {
      if (!dml_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete VIEW parenthesized query LIMIT");
        return;
      }
      tail_state = tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT
                       ? VIEW_QUERY_TAIL_AFTER_LIMIT_VALUE
                       : VIEW_QUERY_TAIL_AFTER_LIMIT_FINAL_VALUE;
      continue;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT_VALUE) {
      if (token_id == ML_COMMA) {
        tail_state = VIEW_QUERY_TAIL_AFTER_LIMIT_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        tail_state = VIEW_QUERY_TAIL_AFTER_LIMIT_OFFSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_WITH) {
        tail_state = VIEW_QUERY_TAIL_AFTER_WITH;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, token,
                           "malformed VIEW parenthesized query LIMIT");
      return;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT_FINAL_VALUE) {
      if (token_id == ML_WITH) {
        tail_state = VIEW_QUERY_TAIL_AFTER_WITH;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, token,
                           "malformed VIEW parenthesized query LIMIT");
      return;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_WITH) {
      if (token_id == ML_CASCADED || token_id == ML_LOCAL) {
        tail_state = VIEW_QUERY_TAIL_AFTER_SCOPE;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHECK) {
        tail_state = VIEW_QUERY_TAIL_AFTER_CHECK;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete VIEW CHECK OPTION");
      return;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_SCOPE) {
      if (token_id != ML_CHECK) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete VIEW CHECK OPTION");
        return;
      }
      tail_state = VIEW_QUERY_TAIL_AFTER_CHECK;
      pending_token = token;
      continue;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_CHECK) {
      if (token_id != ML_OPTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete VIEW CHECK OPTION");
        return;
      }
      tail_state = VIEW_QUERY_TAIL_AFTER_OPTION;
      continue;
    }

    if (tail_state == VIEW_QUERY_TAIL_AFTER_OPTION) {
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, token, "malformed VIEW CHECK OPTION");
      return;
    }
  }

  if (depth > 0) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete VIEW parenthesized query");
  } else if (tail_state == VIEW_QUERY_TAIL_AFTER_ORDER ||
             tail_state == VIEW_QUERY_TAIL_AFTER_ORDER_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete VIEW parenthesized query ORDER BY");
  } else if (tail_state == VIEW_QUERY_TAIL_ORDER_EXPR &&
             order_previous_was_operator) {
    mylite_parser_reject(ctx, order_previous_top_token,
                         "incomplete VIEW parenthesized query ORDER BY");
  } else if (tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT ||
             tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT_COMMA ||
             tail_state == VIEW_QUERY_TAIL_AFTER_LIMIT_OFFSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete VIEW parenthesized query LIMIT");
  } else if (tail_state == VIEW_QUERY_TAIL_AFTER_WITH ||
             tail_state == VIEW_QUERY_TAIL_AFTER_SCOPE ||
             tail_state == VIEW_QUERY_TAIL_AFTER_CHECK) {
    mylite_parser_reject(ctx, pending_token, "incomplete VIEW CHECK OPTION");
  }
}

void mylite_parser_validate_event_statement(MyliteParseContext *ctx,
                                             MyliteToken start) {
  enum {
    EVENT_FIND_ON,
    EVENT_AFTER_ON,
    EVENT_AFTER_SCHEDULE,
    EVENT_AT_TIMESTAMP,
    EVENT_AT_AFTER_PLUS,
    EVENT_AT_AFTER_INTERVAL,
    EVENT_AT_INTERVAL_VALUE,
    EVENT_AT_INTERVAL_DONE,
    EVENT_EVERY_VALUE,
    EVENT_EVERY_DONE,
    EVENT_OPTION_TIMESTAMP,
    EVENT_OPTION_AFTER_PLUS,
    EVENT_OPTION_AFTER_INTERVAL,
    EVENT_OPTION_INTERVAL_VALUE,
    EVENT_OPTION_INTERVAL_DONE
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int state = EVENT_FIND_ON;
  int value_started = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (state == EVENT_FIND_ON) {
      if (token_id == ML_ON) {
        state = EVENT_AFTER_ON;
        pending_token = token;
      } else if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (state == EVENT_AFTER_ON) {
      if (token_id == ML_SCHEDULE) {
        state = EVENT_AFTER_SCHEDULE;
        pending_token = token;
      } else {
        state = EVENT_FIND_ON;
      }
      continue;
    }

    if (state == EVENT_AFTER_SCHEDULE) {
      if (token_id == ML_AT) {
        state = EVENT_AT_TIMESTAMP;
        value_started = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_EVERY) {
        state = EVENT_EVERY_VALUE;
        value_started = 0;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete EVENT schedule");
      return;
    }

    if (event_schedule_boundary(token_id)) {
      if (state == EVENT_AT_TIMESTAMP ||
          state == EVENT_OPTION_TIMESTAMP) {
        if (value_started) {
          validate_event_body_statement(ctx, start);
          return;
        }
      } else if (state == EVENT_AT_INTERVAL_DONE ||
                 state == EVENT_EVERY_DONE ||
                 state == EVENT_OPTION_INTERVAL_DONE) {
        validate_event_body_statement(ctx, start);
        return;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete EVENT schedule");
      return;
    }

    if (state == EVENT_EVERY_DONE &&
        event_schedule_option_start(token)) {
      state = EVENT_OPTION_TIMESTAMP;
      value_started = 0;
      pending_token = token;
      continue;
    }

    if ((state == EVENT_OPTION_TIMESTAMP ||
         state == EVENT_OPTION_INTERVAL_DONE) &&
        event_schedule_option_start(token)) {
      if (state == EVENT_OPTION_TIMESTAMP && !value_started) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete EVENT schedule");
        return;
      }
      state = EVENT_OPTION_TIMESTAMP;
      value_started = 0;
      pending_token = token;
      continue;
    }

    if (state == EVENT_AT_TIMESTAMP ||
        state == EVENT_OPTION_TIMESTAMP) {
      if (token_is_plus(token) && value_started) {
        state = state == EVENT_AT_TIMESTAMP ? EVENT_AT_AFTER_PLUS
                                            : EVENT_OPTION_AFTER_PLUS;
        pending_token = token;
        continue;
      }
      value_started = 1;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_AT_AFTER_PLUS ||
        state == EVENT_OPTION_AFTER_PLUS) {
      if (token_id != ML_INTERVAL) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete EVENT schedule interval");
        return;
      }
      state = state == EVENT_AT_AFTER_PLUS ? EVENT_AT_AFTER_INTERVAL
                                           : EVENT_OPTION_AFTER_INTERVAL;
      pending_token = token;
      continue;
    }

    if (state == EVENT_AT_AFTER_INTERVAL ||
        state == EVENT_OPTION_AFTER_INTERVAL) {
      if (event_interval_unit_token(token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete EVENT schedule interval");
        return;
      }
      state = state == EVENT_AT_AFTER_INTERVAL
                  ? EVENT_AT_INTERVAL_VALUE
                  : EVENT_OPTION_INTERVAL_VALUE;
      value_started = 1;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_AT_INTERVAL_VALUE ||
        state == EVENT_OPTION_INTERVAL_VALUE) {
      if (event_interval_unit_token(token)) {
        state = state == EVENT_AT_INTERVAL_VALUE
                    ? EVENT_AT_INTERVAL_DONE
                    : EVENT_OPTION_INTERVAL_DONE;
        continue;
      }
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_AT_INTERVAL_DONE ||
        state == EVENT_OPTION_INTERVAL_DONE) {
      if (token_is_plus(token)) {
        state = state == EVENT_AT_INTERVAL_DONE ? EVENT_AT_AFTER_PLUS
                                                : EVENT_OPTION_AFTER_PLUS;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token, "malformed EVENT schedule");
      return;
    }

    if (state == EVENT_EVERY_VALUE) {
      if (event_interval_unit_token(token)) {
        if (!value_started) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete EVENT schedule interval");
          return;
        }
        state = EVENT_EVERY_DONE;
        continue;
      }
      value_started = 1;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_EVERY_DONE) {
      mylite_parser_reject(ctx, token, "malformed EVENT schedule");
      return;
    }
  }

  if (state != EVENT_FIND_ON) {
    if (state == EVENT_AT_TIMESTAMP ||
        state == EVENT_OPTION_TIMESTAMP) {
      if (value_started) {
        validate_event_body_statement(ctx, start);
        return;
      }
    } else if (state == EVENT_AT_INTERVAL_DONE ||
               state == EVENT_EVERY_DONE ||
               state == EVENT_OPTION_INTERVAL_DONE) {
      validate_event_body_statement(ctx, start);
      return;
    }
    mylite_parser_reject(ctx, pending_token,
                         "incomplete EVENT schedule");
  } else if (!ctx->failed) {
    validate_event_body_statement(ctx, start);
  }
}

void mylite_parser_validate_trigger_statement(MyliteParseContext *ctx,
                                              MyliteToken start) {
  validate_trigger_body_statement(ctx, start);
}

void mylite_parser_validate_create_function_statement(MyliteParseContext *ctx,
                                                       MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int saw_returns = 0;
  int depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_id == ML_RETURNS) {
      saw_returns = 1;
      continue;
    }
    if (!saw_returns) {
      continue;
    }
    if (token_opens_nested_expression(token_id)) {
      depth++;
      continue;
    }
    if (routine_characteristic_token(ctx, &lexer, &token_id, &token)) {
      if (ctx->failed) {
        return;
      }
      continue;
    }
    if (token_id == ML_BEGIN || token_id == ML_RETURN) {
      validate_routine_statement_body_from(ctx, token_id, token);
      return;
    }
  }
}

void mylite_parser_validate_create_procedure_statement(MyliteParseContext *ctx,
                                                        MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int in_signature = 0;
  int signature_closed = 0;
  int depth = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (!signature_closed) {
      if (!in_signature) {
        if (token_id == ML_LP) {
          in_signature = 1;
          depth = 1;
        }
        continue;
      }

      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0) {
          signature_closed = 1;
        }
      }
      continue;
    }

    if (routine_characteristic_token(ctx, &lexer, &token_id, &token)) {
      if (ctx->failed) {
        return;
      }
      continue;
    }
    if (routine_body_statement_start_token(token_id) ||
        routine_direct_query_body_start_token(token_id)) {
      validate_routine_statement_body_from(ctx, token_id, token);
    }
    return;
  }
}

static void validate_event_body_statement(MyliteParseContext *ctx,
                                          MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int after_event_do = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (after_event_do) {
      validate_embedded_statement_body_from(ctx, token_id, token);
      return;
    }

    if (token_id == ML_COMMENT) {
      MyliteToken comment_token = token;
      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id <= 0 ||
          !create_table_tail_option_string_token(token_id, token)) {
        mylite_parser_reject(ctx, comment_token,
                             "invalid EVENT comment value");
        return;
      }
      continue;
    }

    if (token_id == ML_DO) {
      after_event_do = 1;
    }
  }
}

static void validate_trigger_body_statement(MyliteParseContext *ctx,
                                            MyliteToken start) {
  enum {
    TRIGGER_FIND_ROW,
    TRIGGER_AFTER_ROW,
    TRIGGER_AFTER_ORDER,
    TRIGGER_BODY
  };
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int state = TRIGGER_FIND_ROW;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (state == TRIGGER_FIND_ROW) {
      if (token_id == ML_ROW) {
        state = TRIGGER_AFTER_ROW;
      }
      continue;
    }

    if (state == TRIGGER_AFTER_ROW) {
      if (token_id == ML_FOLLOWS || token_id == ML_PRECEDES) {
        state = TRIGGER_AFTER_ORDER;
        continue;
      }
      validate_embedded_statement_body_from(ctx, token_id, token);
      return;
    }

    if (state == TRIGGER_AFTER_ORDER) {
      state = TRIGGER_BODY;
      continue;
    }

    if (state == TRIGGER_BODY) {
      validate_embedded_statement_body_from(ctx, token_id, token);
      return;
    }
  }
}

static void validate_embedded_statement_body_from(MyliteParseContext *ctx,
                                                  int token_id,
                                                  MyliteToken token) {
  if (token_id == ML_BEGIN) {
    validate_compound_statement_body_from(ctx, token);
    return;
  }
  if (token_id == ML_RETURN) {
    validate_return_statement_from(ctx, token);
    return;
  }
  if (routine_compound_statement_start_token(token_id) ||
      token_id == ML_UNTIL || token_id == ML_WHEN) {
    validate_flow_control_statement_from(ctx, token_id, token);
    return;
  }
  if (token_id == ML_CALL) {
    validate_call_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_SIGNAL || token_id == ML_RESIGNAL) {
    validate_signal_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_GET) {
    validate_get_diagnostics_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_OPEN || token_id == ML_FETCH || token_id == ML_CLOSE) {
    validate_cursor_statement_from(ctx, token_id, token);
    return;
  }
  if (token_id == ML_LEAVE || token_id == ML_ITERATE) {
    validate_label_control_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_DO) {
    mylite_parser_validate_do_statement(ctx, token);
    return;
  }
  if (token_id == ML_SET) {
    validate_embedded_set_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_SELECT) {
    mylite_parser_validate_select_statement_from(ctx, token);
    if (!ctx->failed) {
      validate_select_list_tail_from(ctx, token, 0, 1, 0);
    }
    return;
  }
  if (token_id == ML_TABLE) {
    mylite_parser_validate_table_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_VALUES) {
    mylite_parser_validate_values_statement_from(ctx, token);
    if (!ctx->failed) {
      mylite_parser_validate_select_statement_from(ctx, token);
    }
    return;
  }
  if (token_id == ML_WITH) {
    mylite_parser_validate_with_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_LP) {
    mylite_parser_validate_parenthesized_statement(ctx, token);
    return;
  }
  if (token_id == ML_DELETE) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_DELETE);
    return;
  }
  if (token_id == ML_INSERT) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_INSERT);
    return;
  }
  if (token_id == ML_REPLACE) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_REPLACE);
    return;
  }
  if (token_id == ML_UPDATE) {
    mylite_parser_validate_dml_statement(ctx, token, MYLITE_STATEMENT_UPDATE);
  }
}

static void validate_select_list_tail_from(MyliteParseContext *ctx,
                                           MyliteToken start,
                                           int parenthesized_boundary,
                                           int validate_nested,
                                           int stop_at_insert_duplicate) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken previous_top_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int previous_top_token_id = 0;
  int previous_was_operator = 1;
  int expression_started = 0;
  int select_prefix = 1;
  int alias_pending = 0;
  int alias_complete = 0;
  int seen_select_item = 0;
  int allow_null_treatment = 0;
  int null_treatment_state = 0;
  int null_treatment_candidate_depth = 0;
  MyliteExpressionStack *expression_stack;

  expression_stack = calloc(1, sizeof(*expression_stack));
  if (expression_stack == NULL) {
    mylite_parser_reject(ctx, start, "out of memory");
    return;
  }

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (select_prefix) {
      if (select_modifier_flag(token_id)) {
        continue;
      }
      select_prefix = 0;
    }

    if (depth > 0) {
      if (validate_nested || query_expression_stack_active(expression_stack,
                                                           depth)) {
        if (!query_expression_depth_token(
                ctx, token_id, token, &depth, expression_stack,
                "malformed SELECT expression clause")) {
          goto done;
        }
      } else {
        if (token_opens_nested_expression(token_id)) {
          depth++;
        } else if (token_closes_nested_expression(token_id)) {
          depth--;
        }
      }
      if (token_closes_nested_expression(token_id) && depth == 0) {
        allow_null_treatment = null_treatment_candidate_depth == 1;
        null_treatment_candidate_depth = 0;
        previous_top_token_id = token_id;
        previous_top_token = token;
        previous_was_operator = 0;
      }
      continue;
    }

    if (stop_at_insert_duplicate && token_id == ML_ON &&
        insert_duplicate_clause_follows(ctx, token)) {
      goto done;
    }

    if (alias_pending) {
      if (!select_alias_token(token_id, token)) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete SELECT expression alias");
        goto done;
      }
      alias_pending = 0;
      alias_complete = 1;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 0;
      continue;
    }

    if (null_treatment_state == 1) {
      if (!token_ascii_equal(token, "nulls")) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete SELECT null treatment");
        goto done;
      }
      null_treatment_state = 2;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 0;
      continue;
    }
    if (null_treatment_state == 2) {
      if (!token_ascii_equal(token, "over")) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete SELECT null treatment");
        goto done;
      }
      null_treatment_state = 0;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 1;
      continue;
    }

    if ((parenthesized_boundary && token_id == ML_RP) ||
        (alias_complete && token_closes_nested_expression(token_id)) ||
        token_is_statement_terminator(token_id, token) ||
        token_id == ML_FROM ||
        select_expression_clause_boundary(token_id, token)) {
      if (!expression_started || previous_was_operator) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete SELECT expression clause");
      }
      goto done;
    }

    if (token_id == ML_COMMA) {
      if (!expression_started || previous_was_operator) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete SELECT expression clause");
        goto done;
      }
      seen_select_item = 1;
      expression_started = 0;
      previous_top_token_id = 0;
      previous_was_operator = 1;
      previous_top_token = token;
      alias_pending = 0;
      alias_complete = 0;
      allow_null_treatment = 0;
      null_treatment_state = 0;
      null_treatment_candidate_depth = 0;
      continue;
    }

    if (alias_complete) {
      mylite_parser_reject(ctx, token, "malformed SELECT expression alias");
      goto done;
    }

    if (allow_null_treatment &&
        (token_id == ML_IGNORE || token_ascii_equal(token, "respect"))) {
      allow_null_treatment = 0;
      null_treatment_state = 1;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 0;
      continue;
    }
    allow_null_treatment = 0;

    if (expression_started && !previous_was_operator &&
        do_expression_value_start(token_id, token) &&
        !do_expression_operator(token_id, token) &&
        do_expression_value_terminal(previous_top_token_id,
                                     previous_top_token) &&
        !do_expression_allows_adjacent(previous_top_token_id,
                                       previous_top_token, token_id, token)) {
      if (!select_alias_token(token_id, token)) {
        mylite_parser_reject(ctx, token, "malformed SELECT expression alias");
        goto done;
      }
      alias_complete = 1;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 0;
      continue;
    }

    if (previous_top_token_id == ML_STAR && !previous_was_operator) {
      mylite_parser_reject(ctx, token, "malformed SELECT expression clause");
      goto done;
    }

    if (previous_top_token_id == ML_DEFAULT && token_id != ML_DOT &&
        token_id != ML_LP) {
      mylite_parser_reject(ctx, token, "malformed SELECT expression clause");
      goto done;
    }

    if (token_id == ML_AS) {
      if (!expression_started || previous_was_operator) {
        mylite_parser_reject(ctx, previous_top_token,
                             "incomplete SELECT expression clause");
        goto done;
      }
      alias_pending = 1;
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 1;
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      int null_treatment_candidate =
          token_id == ML_LP &&
          select_null_treatment_function_token(previous_top_token_id,
                                               previous_top_token);
      if (validate_nested || previous_top_token_id == ML_DEFAULT) {
        if (!query_expression_token(
                ctx, token_id, token, &depth, &previous_top_token_id,
                &previous_top_token, &previous_was_operator, expression_stack,
                "malformed SELECT expression clause")) {
          goto done;
        }
      } else {
        depth = 1;
        previous_top_token_id = token_id;
        previous_top_token = token;
        previous_was_operator = 0;
      }
      if (null_treatment_candidate) {
        null_treatment_candidate_depth = depth;
      }
      expression_started = 1;
      continue;
    }

    if (token_id == ML_STAR &&
        ((!expression_started && !seen_select_item) ||
         previous_top_token_id == ML_DOT)) {
      previous_top_token_id = token_id;
      previous_top_token = token;
      previous_was_operator = 0;
      expression_started = 1;
      continue;
    }

    previous_top_token_id = token_id;
    previous_top_token = token;
    previous_was_operator =
        token_id == ML_DEFAULT ? 1 : do_expression_operator(token_id, token);
    expression_started = 1;
  }

  if (alias_pending) {
    mylite_parser_reject(ctx, previous_top_token,
                         "incomplete SELECT expression alias");
  } else if (null_treatment_state != 0) {
    mylite_parser_reject(ctx, previous_top_token,
                         "incomplete SELECT null treatment");
  } else if (expression_started && previous_was_operator) {
    mylite_parser_reject(ctx, previous_top_token,
                         "incomplete SELECT expression clause");
  }

done:
  free(expression_stack);
}

static void validate_routine_statement_body_from(MyliteParseContext *ctx,
                                                 int token_id,
                                                 MyliteToken token) {
  if (token_id == ML_BEGIN) {
    validate_compound_statement_body_from(ctx, token);
    return;
  }
  if (token_id == ML_RETURN) {
    validate_return_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_DECLARE) {
    mylite_parser_validate_declare_statement(ctx, token);
    return;
  }
  if (token_id == ML_CALL) {
    validate_call_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_SIGNAL || token_id == ML_RESIGNAL) {
    validate_signal_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_GET) {
    validate_get_diagnostics_statement_from(ctx, token);
    return;
  }
  if (token_id == ML_OPEN || token_id == ML_FETCH || token_id == ML_CLOSE) {
    validate_cursor_statement_from(ctx, token_id, token);
    return;
  }
  if (token_id == ML_LEAVE || token_id == ML_ITERATE) {
    validate_label_control_statement_from(ctx, token);
    return;
  }
  if (routine_compound_statement_start_token(token_id) ||
      token_id == ML_UNTIL || token_id == ML_WHEN) {
    validate_flow_control_statement_from(ctx, token_id, token);
    return;
  }
  validate_embedded_statement_body_from(ctx, token_id, token);
}

static void validate_compound_statement_body_from(MyliteParseContext *ctx,
                                                  MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;
  int compound_depth = 0;
  int skip_statement = 0;
  int expression_depth = 0;
  int after_end = 0;
  int control_boundary = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        compound_depth = 1;
        if (token_id == ML_IF) {
          control_boundary = ML_THEN;
        } else if (token_id == ML_WHILE) {
          control_boundary = ML_DO;
        }
      }
      continue;
    }

    if (skip_statement) {
      if (expression_depth > 0) {
        if (token_opens_nested_expression(token_id)) {
          expression_depth++;
        } else if (token_closes_nested_expression(token_id)) {
          expression_depth--;
        }
        continue;
      }
      if (token_is_statement_terminator(token_id, token)) {
        skip_statement = 0;
        continue;
      }
      if (token_opens_nested_expression(token_id)) {
        expression_depth++;
      }
      continue;
    }

    if (control_boundary) {
      if (expression_depth > 0) {
        if (token_opens_nested_expression(token_id)) {
          expression_depth++;
        } else if (token_closes_nested_expression(token_id)) {
          expression_depth--;
        }
        continue;
      }
      if (token_id == control_boundary) {
        control_boundary = 0;
        continue;
      }
      if (token_opens_nested_expression(token_id)) {
        expression_depth++;
      }
      continue;
    }

    if (after_end) {
      if (routine_end_suffix_token(token_id) ||
          token_is_statement_terminator(token_id, token)) {
        after_end = 0;
        continue;
      }
      after_end = 0;
    }

    if (token_id == ML_END) {
      compound_depth--;
      if (compound_depth <= 0) {
        return;
      }
      after_end = 1;
      continue;
    }

    if (token_id == ML_WHEN) {
      validate_control_condition_from(ctx, token, ML_THEN,
                                      "malformed WHEN condition");
      if (ctx->failed) {
        return;
      }
      control_boundary = ML_THEN;
      expression_depth = 0;
      continue;
    }

    if (token_id == ML_UNTIL) {
      validate_control_condition_from(ctx, token, ML_END,
                                      "malformed UNTIL condition");
      if (ctx->failed) {
        return;
      }
      continue;
    }

    if (token_id == ML_SHOW) {
      skip_statement = 1;
      expression_depth = 0;
      continue;
    }

    if (routine_compound_statement_start_token(token_id)) {
      if (token_id == ML_IF) {
        validate_control_condition_from(ctx, token, ML_THEN,
                                        "malformed IF condition");
      } else if (token_id == ML_WHILE) {
        validate_control_condition_from(ctx, token, ML_DO,
                                        "malformed WHILE condition");
      }
      if (ctx->failed) {
        return;
      }
      compound_depth++;
      if (token_id == ML_IF) {
        control_boundary = ML_THEN;
      } else if (token_id == ML_WHILE) {
        control_boundary = ML_DO;
      }
      continue;
    }

    if (routine_body_statement_start_token(token_id)) {
      validate_routine_statement_body_from(ctx, token_id, token);
      if (ctx->failed) {
        return;
      }
      skip_statement = 1;
      expression_depth = 0;
    }
  }
}

static void validate_return_statement_from(MyliteParseContext *ctx,
                                           MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      mylite_parser_reject(ctx, start, "incomplete RETURN expression");
      return;
    }

    mylite_parser_validate_expression_from(ctx, token,
                                           "malformed RETURN expression");
    return;
  }

  mylite_parser_reject(ctx, start, "incomplete RETURN expression");
}

static void validate_call_statement_from(MyliteParseContext *ctx,
                                         MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      return;
    }

    if (token_id == ML_LP) {
      MyliteToken lp_token = token;

      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id == ML_RP) {
        return;
      }
      mylite_parser_validate_parenthesized_expression_list_from(
          ctx, lp_token, "malformed CALL argument list");
      return;
    }
  }
}

static void validate_signal_statement_from(MyliteParseContext *ctx,
                                           MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      return;
    }

    if (token_id == ML_SET) {
      validate_embedded_set_statement_from(ctx, token);
      return;
    }
  }
}

static void validate_get_diagnostics_statement_from(MyliteParseContext *ctx,
                                                    MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int seen_equals = 0;
  int last_was_comma = 0;
  int target_started = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      if (last_was_comma || !seen_equals) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete GET DIAGNOSTICS assignment");
      }
      return;
    }

    if (token_id == ML_COMMA) {
      if (last_was_comma || !seen_equals) {
        mylite_parser_reject(ctx, token,
                             "incomplete GET DIAGNOSTICS assignment");
        return;
      }
      last_was_comma = 1;
      target_started = 0;
      pending_token = token;
      continue;
    }

    if (token_id == ML_EQUALS) {
      if (!target_started) {
        mylite_parser_reject(ctx, token,
                             "incomplete GET DIAGNOSTICS assignment");
        return;
      }
      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id <= 0 || token_id == ML_COMMA ||
          token_is_statement_terminator(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete GET DIAGNOSTICS assignment");
        return;
      }
      if (!diagnostics_item_name_token(token_id)) {
        mylite_parser_reject(ctx, token, "malformed GET DIAGNOSTICS item");
        return;
      }
      seen_equals = 1;
      last_was_comma = 0;
      target_started = 0;
      continue;
    }

    target_started = 1;
    pending_token = token;
  }

  if (last_was_comma || !seen_equals) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete GET DIAGNOSTICS assignment");
  }
}

static void validate_cursor_statement_from(MyliteParseContext *ctx,
                                           int start_token_id,
                                           MyliteToken start) {
  enum {
    CURSOR_NEED_NAME,
    CURSOR_FETCH_AFTER_NEXT,
    CURSOR_FETCH_NEED_NAME,
    CURSOR_FETCH_NEED_INTO,
    CURSOR_FETCH_NEED_TARGET,
    CURSOR_FETCH_AFTER_TARGET,
    CURSOR_SIMPLE_AFTER_NAME
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int state = start_token_id == ML_FETCH ? CURSOR_FETCH_NEED_NAME
                                         : CURSOR_NEED_NAME;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      if (state == CURSOR_SIMPLE_AFTER_NAME ||
          state == CURSOR_FETCH_AFTER_TARGET) {
        return;
      }
      mylite_parser_reject(ctx, pending_token, "incomplete cursor statement");
      return;
    }

    if (state == CURSOR_NEED_NAME) {
      state = CURSOR_SIMPLE_AFTER_NAME;
      pending_token = token;
      continue;
    }

    if (state == CURSOR_SIMPLE_AFTER_NAME) {
      mylite_parser_reject(ctx, token, "malformed cursor statement");
      return;
    }

    if (state == CURSOR_FETCH_NEED_NAME) {
      if (token_id == ML_NEXT) {
        state = CURSOR_FETCH_AFTER_NEXT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_FROM) {
        state = CURSOR_FETCH_NEED_NAME;
        pending_token = token;
        continue;
      }
      state = CURSOR_FETCH_NEED_INTO;
      pending_token = token;
      continue;
    }

    if (state == CURSOR_FETCH_AFTER_NEXT) {
      if (token_id != ML_FROM) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete cursor statement");
        return;
      }
      state = CURSOR_FETCH_NEED_NAME;
      pending_token = token;
      continue;
    }

    if (state == CURSOR_FETCH_NEED_INTO) {
      if (token_id != ML_INTO) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete cursor statement");
        return;
      }
      state = CURSOR_FETCH_NEED_TARGET;
      pending_token = token;
      continue;
    }

    if (state == CURSOR_FETCH_NEED_TARGET) {
      if (token_id == ML_COMMA) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete cursor statement");
        return;
      }
      state = CURSOR_FETCH_AFTER_TARGET;
      pending_token = token;
      continue;
    }

    if (state == CURSOR_FETCH_AFTER_TARGET) {
      if (token_id == ML_COMMA) {
        state = CURSOR_FETCH_NEED_TARGET;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token, "malformed cursor statement");
      return;
    }
  }

  mylite_parser_reject(ctx, pending_token, "incomplete cursor statement");
}

static void validate_label_control_statement_from(MyliteParseContext *ctx,
                                                  MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_label = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_is_statement_terminator(token_id, token)) {
      if (!saw_label) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete label-control statement");
      }
      return;
    }

    if (saw_label) {
      mylite_parser_reject(ctx, token, "malformed label-control statement");
      return;
    }

    saw_label = 1;
    pending_token = token;
  }

  mylite_parser_reject(ctx, pending_token,
                       "incomplete label-control statement");
}

static void validate_flow_control_statement_from(MyliteParseContext *ctx,
                                                 int token_id,
                                                 MyliteToken token) {
  if (token_id == ML_IF) {
    validate_control_condition_from(ctx, token, ML_THEN,
                                    "malformed IF condition");
  } else if (token_id == ML_WHILE) {
    validate_control_condition_from(ctx, token, ML_DO,
                                    "malformed WHILE condition");
  } else if (token_id == ML_WHEN) {
    validate_control_condition_from(ctx, token, ML_THEN,
                                    "malformed WHEN condition");
  } else if (token_id == ML_UNTIL) {
    validate_control_condition_from(ctx, token, ML_END,
                                    "malformed UNTIL condition");
  }
  if (ctx->failed) {
    return;
  }
  if (routine_compound_statement_start_token(token_id)) {
    validate_compound_statement_body_from(ctx, token);
  }
}

static void validate_control_condition_from(MyliteParseContext *ctx,
                                            MyliteToken start,
                                            int boundary_token_id,
                                            const char *message) {
  MyliteLexer lexer;
  MyliteToken token;
  int token_id;
  int saw_statement = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (token_id == boundary_token_id ||
        token_is_statement_terminator(token_id, token)) {
      mylite_parser_reject(ctx, start, message);
      return;
    }

    mylite_parser_validate_expression_until_from(ctx, token, boundary_token_id,
                                                 message);
    return;
  }

  mylite_parser_reject(ctx, start, message);
}

static void validate_embedded_set_statement_from(MyliteParseContext *ctx,
                                                 MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;

  mylite_parser_validate_set_statement(ctx, start);
  if (ctx->failed) {
    return;
  }

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      return;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
      continue;
    }

    if (token_id == ML_ASSIGN || token_id == ML_EQUALS) {
      int boundary;

      pending_token = token;
      token_id = mylite_lexer_next(&lexer, &token);
      if (token_id <= 0 || token_id == ML_COMMA ||
          token_is_statement_terminator(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SET assignment");
        return;
      }
      boundary = validate_embedded_set_value(ctx, &lexer, token_id, token);
      if (boundary < 0 || ctx->failed || boundary == ML_SEMI) {
        return;
      }
    }
  }
}

static int validate_embedded_set_value(MyliteParseContext *ctx,
                                       MyliteLexer *lexer, int token_id,
                                       MyliteToken token) {
  MyliteToken previous_top_token = token;
  int depth = 0;
  MyliteExpressionStack expression_stack = {0};
  int previous_top_token_id = 0;
  int previous_was_operator = 1;
  int flags = QUERY_EXPRESSION_ALLOW_BARE_DEFAULT;

  if (set_user_variable_value_is_invalid(ctx, token)) {
    mylite_parser_reject(ctx, token, "malformed SET assignment");
    return -1;
  }
  if (expression_start_follows_user_variable_assignment(ctx, token)) {
    flags = 0;
  }

  for (;;) {
    if (depth > 0) {
      if (!query_expression_depth_token(
              ctx, token_id, token, &depth, &expression_stack,
              "malformed SET assignment")) {
        return -1;
      }
      if (token_closes_nested_expression(token_id) && depth == 0) {
        previous_top_token_id = token_id;
        previous_top_token = token;
        previous_was_operator = 0;
      }
    } else if (token_id == ML_COMMA ||
               token_is_statement_terminator(token_id, token)) {
      if (previous_top_token_id == 0 || previous_was_operator) {
        mylite_parser_reject(ctx, previous_top_token,
                             "malformed SET assignment");
        return -1;
      }
      return token_id == ML_COMMA ? ML_COMMA : ML_SEMI;
    } else if (!query_expression_token_with_flags(
                   ctx, token_id, token, &depth, &previous_top_token_id,
                   &previous_top_token, &previous_was_operator,
                   &expression_stack, flags, "malformed SET assignment")) {
      return -1;
    }

    token_id = mylite_lexer_next(lexer, &token);
    if (token_id <= 0) {
      if (previous_top_token_id == 0 || previous_was_operator) {
        mylite_parser_reject(ctx, previous_top_token,
                             "malformed SET assignment");
        return -1;
      }
      return token_id;
    }
  }
}

void mylite_parser_require_permissive(MyliteParseContext *ctx,
                                      MyliteToken token) {
  if (ctx->permissive) {
    ctx->permissive_fallbacks++;
    return;
  }

  ctx->failed = 1;
  set_parser_error(ctx, &token, "unsupported statement start");
}

void mylite_parser_require_row_format(MyliteParseContext *ctx,
                                      MyliteToken token) {
  if (token_ascii_equal(token, "fixed") ||
      token_ascii_equal(token, "dynamic") ||
      token_ascii_equal(token, "compressed") ||
      token_ascii_equal(token, "redundant") ||
      token_ascii_equal(token, "compact")) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid row format option");
}

void mylite_parser_require_storage_type(MyliteParseContext *ctx,
                                        MyliteToken token) {
  if (token_ascii_equal(token, "disk")) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid storage option");
}

void mylite_parser_require_xid_number(MyliteParseContext *ctx,
                                      MyliteToken token) {
  if (token.length > 2 && token.start[0] == '0' &&
      (token.start[1] == 'x' || token.start[1] == 'X' ||
       token.start[1] == 'b' || token.start[1] == 'B')) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid XA XID literal");
}

void mylite_parser_require_text_string_literal(MyliteParseContext *ctx,
                                               MyliteToken token) {
  if (!token_is_quoted_hex_or_bit_literal(token)) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid string literal");
}

void mylite_parser_require_quoted_hex_literal(MyliteParseContext *ctx,
                                              MyliteToken token) {
  if (token_is_quoted_hex_literal(token)) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid index number literal");
}

void mylite_parser_require_name_atom(MyliteParseContext *ctx,
                                     MyliteToken token) {
  if (token_is_invalid_identifier_atom(token, 1)) {
    mylite_parser_reject(ctx, token, "invalid identifier");
  }
}

void mylite_parser_require_strict_identifier_atom(MyliteParseContext *ctx,
                                                  MyliteToken token) {
  if (token_is_invalid_identifier_atom(token, 0) ||
      token_ascii_equal(token, "cascade") ||
      token_ascii_equal(token, "restrict")) {
    mylite_parser_reject(ctx, token, "invalid identifier");
  }
}

void mylite_parser_require_identifier_atom(MyliteParseContext *ctx,
                                           MyliteToken token) {
  if (token_is_invalid_identifier_atom(token, 0)) {
    mylite_parser_reject(ctx, token, "invalid identifier");
  }
}

void mylite_parser_require_account_principal_atom(MyliteParseContext *ctx,
                                                  MyliteToken token) {
  if (token_ascii_equal(token, "cascade") ||
      token_ascii_equal(token, "restrict")) {
    mylite_parser_reject(ctx, token, "invalid account name");
  }
}

void mylite_parser_require_charset_name_atom(MyliteParseContext *ctx,
                                             MyliteToken token) {
  if (token_is_quoted_hex_or_bit_literal(token)) {
    mylite_parser_reject(ctx, token, "invalid character set name");
    return;
  }

  if (token_is_invalid_identifier_atom(token, 1)) {
    mylite_parser_reject(ctx, token, "invalid character set name");
  }
}

void mylite_parser_require_use_target_atom(MyliteParseContext *ctx,
                                           MyliteToken token) {
  if (token.length > 0 && token.start[0] == '"') {
    mylite_parser_reject(ctx, token, "invalid USE target");
    return;
  }

  if (token_is_invalid_identifier_atom(token, 0) ||
      token_ascii_equal(token, "cascade") ||
      token_ascii_equal(token, "restrict")) {
    mylite_parser_reject(ctx, token, "invalid USE target");
  }
}

void mylite_parser_reject(MyliteParseContext *ctx, MyliteToken token,
                          const char *message) {
  if (ctx->failed) {
    return;
  }

  ctx->failed = 1;
  set_parser_error(ctx, &token, message);
}

static int token_ascii_equal(MyliteToken token, const char *expected) {
  size_t i = 0;

  while (i < token.length && expected[i] != '\0') {
    if (!ascii_alpha_equal(token.start[i], expected[i])) {
      return 0;
    }
    i++;
  }

  return i == token.length && expected[i] == '\0';
}

static int token_is_invalid_identifier_atom(MyliteToken token,
                                            int allow_string_literal) {
  if (token.length == 0) {
    return 0;
  }

  if (token.length == 1 &&
      strchr("+-=.:@/%&|!~^?", token.start[0]) != NULL) {
    return 1;
  }

  if (token.start[0] == '@') {
    return 1;
  }

  if (!allow_string_literal && token.start[0] == '\'') {
    return 1;
  }

  return token_starts_numeric_literal(token);
}

static int token_starts_numeric_literal(MyliteToken token) {
  size_t offset = 0;

  if (token.length == 0) {
    return 0;
  }

  if ((token.start[0] == '+' || token.start[0] == '-') && token.length > 1 &&
      ascii_is_digit(token.start[1])) {
    offset = 1;
  }

  if (!ascii_is_digit(token.start[offset])) {
    return 0;
  }

  if (token_is_hex_literal(token, offset) ||
      token_is_binary_literal(token, offset)) {
    return 1;
  }

  return token_is_decimal_literal(token, offset);
}

static int token_is_hex_literal(MyliteToken token, size_t offset) {
  size_t i;

  if (token.length <= offset + 2 || token.start[offset] != '0' ||
      (token.start[offset + 1] != 'x' && token.start[offset + 1] != 'X')) {
    return 0;
  }

  for (i = offset + 2; i < token.length; i++) {
    if (!ascii_is_hex_digit(token.start[i])) {
      return 0;
    }
  }

  return 1;
}

static int token_is_binary_literal(MyliteToken token, size_t offset) {
  size_t i;

  if (token.length <= offset + 2 || token.start[offset] != '0' ||
      (token.start[offset + 1] != 'b' && token.start[offset + 1] != 'B')) {
    return 0;
  }

  for (i = offset + 2; i < token.length; i++) {
    if (token.start[i] != '0' && token.start[i] != '1') {
      return 0;
    }
  }

  return 1;
}

static int token_is_decimal_literal(MyliteToken token, size_t offset) {
  size_t i = offset;

  while (i < token.length && ascii_is_digit(token.start[i])) {
    i++;
  }

  if (i == token.length || token.start[i] == '.') {
    return 1;
  }

  if ((token.start[i] == 'e' || token.start[i] == 'E') &&
      i + 1 < token.length) {
    size_t exponent = i + 1;
    if ((token.start[exponent] == '+' || token.start[exponent] == '-') &&
        exponent + 1 < token.length) {
      exponent++;
    }
    if (ascii_is_digit(token.start[exponent])) {
      return 1;
    }
  }

  return 0;
}

static int ascii_is_digit(char ch) {
  return ch >= '0' && ch <= '9';
}

static int ascii_is_hex_digit(char ch) {
  return ascii_is_digit(ch) || (ch >= 'a' && ch <= 'f') ||
         (ch >= 'A' && ch <= 'F');
}

static int select_clause_requires_by(int token_id) {
  return token_id == ML_GROUP || token_id == ML_ORDER;
}

static int select_clause_requires_operand(int token_id) {
  return token_id == ML_FROM || token_id == ML_HAVING ||
         token_id == ML_JOIN || token_id == ML_LIMIT || token_id == ML_ON ||
         token_id == ML_PROCEDURE || token_id == ML_STRAIGHT_JOIN ||
         token_id == ML_USING || token_id == ML_WHERE;
}

static int select_from_starts_nth_modifier(MyliteParseContext *ctx,
                                           MyliteToken from_token) {
  MyliteLexer lexer;
  MyliteToken token;
  size_t offset = from_token.offset + from_token.length;
  int token_id;

  if (offset >= ctx->length) {
    return 0;
  }
  if (!select_from_follows_nth_value_call(ctx, from_token)) {
    return 0;
  }

  mylite_lexer_init(&lexer, ctx->sql + offset, ctx->length - offset, NULL);
  token_id = mylite_lexer_next(&lexer, &token);
  if (token_id != ML_FIRST && token_id != ML_LAST) {
    return 0;
  }

  token_id = mylite_lexer_next(&lexer, &token);
  if (token_ascii_equal(token, "over")) {
    return 1;
  }
  if (token_id != ML_IGNORE && !token_ascii_equal(token, "respect")) {
    return 0;
  }

  token_id = mylite_lexer_next(&lexer, &token);
  if (!token_ascii_equal(token, "nulls")) {
    return 0;
  }

  token_id = mylite_lexer_next(&lexer, &token);
  (void) token_id;
  return token_ascii_equal(token, "over");
}

static int select_from_follows_nth_value_call(MyliteParseContext *ctx,
                                              MyliteToken from_token) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken last_top_token = {0};
  int token_id;
  int depth = 0;
  int nth_value_depth = 0;
  int closed_nth_value = 0;

  mylite_lexer_init(&lexer, ctx->sql, from_token.offset, NULL);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (depth > 0) {
      if (token_id == ML_LP || token_id == ML_LB || token_id == ML_LC) {
        depth++;
      } else if (token_id == ML_RP || token_id == ML_RB ||
                 token_id == ML_RC) {
        depth--;
        if (depth == 0 && nth_value_depth) {
          closed_nth_value = 1;
          nth_value_depth = 0;
        }
      }
      continue;
    }

    if (token_id == ML_LP || token_id == ML_LB || token_id == ML_LC) {
      if (token_id == ML_LP && token_ascii_equal(last_top_token, "nth_value")) {
        nth_value_depth = 1;
      } else {
        nth_value_depth = 0;
      }
      closed_nth_value = 0;
      depth = 1;
      continue;
    }

    closed_nth_value = 0;
    last_top_token = token;
  }

  return closed_nth_value;
}

static int select_token_followed_by_nulls(MyliteParseContext *ctx,
                                          MyliteToken token) {
  MyliteLexer lexer;
  MyliteToken next;
  int token_id;
  size_t offset = token.offset + token.length;

  if (offset >= ctx->length) {
    return 0;
  }

  mylite_lexer_init(&lexer, ctx->sql + offset, ctx->length - offset, NULL);
  token_id = mylite_lexer_next(&lexer, &next);

  return token_id > 0 && token_ascii_equal(next, "nulls");
}

static int select_expression_clause_boundary(int token_id, MyliteToken token) {
  return token_id == ML_FOR || token_id == ML_GROUP || token_id == ML_HAVING ||
         token_id == ML_INTO || token_id == ML_LIMIT || token_id == ML_LOCK ||
         token_id == ML_ORDER || token_id == ML_WHERE ||
         select_set_operator(token_id) || token_ascii_equal(token, "qualify") ||
         token_ascii_equal(token, "window");
}

static int select_alias_token(int token_id, MyliteToken token) {
  if (token_id == ML_FROM || select_expression_clause_boundary(token_id, token)) {
    return 0;
  }
  if (do_expression_operator(token_id, token)) {
    return 0;
  }
  if (token_id == ML_ATOM) {
    return 1;
  }

  if (token_id == ML_QUOTED_ID || token_id == ML_DOUBLE_QUOTED_STRING) {
    return 1;
  }
  if (token_id == ML_STRING_LITERAL) {
    return token.length > 0 && token.start[0] == '\'';
  }
  if ((token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
       token_id == ML_NUMBER_LITERAL) &&
      token_contains_identifier_letter(token) &&
      !token_starts_numeric_literal(token)) {
    return 1;
  }
  if (dml_row_alias_token(token_id)) {
    return 1;
  }

  return 0;
}

static int select_null_treatment_function_token(int token_id,
                                                MyliteToken token) {
  if (token_id != ML_ATOM) {
    return 0;
  }

  return token_ascii_equal(token, "first_value") ||
         token_ascii_equal(token, "lag") ||
         token_ascii_equal(token, "last_value") ||
         token_ascii_equal(token, "lead") ||
         token_ascii_equal(token, "nth_value");
}

static int select_operand_boundary(int token_id) {
  return token_id == ML_SEMI || token_id == ML_COMMA || token_id == ML_RP ||
         select_set_operator(token_id) ||
         select_clause_requires_by(token_id) ||
         select_clause_requires_operand(token_id);
}

static int select_modifier_flag(int token_id) {
  if (token_id == ML_ALL) {
    return 1 << 0;
  }
  if (token_id == ML_DISTINCT || token_id == ML_DISTINCTROW) {
    return 1 << 1;
  }
  if (token_id == ML_HIGH_PRIORITY || token_id == ML_SQL_BIG_RESULT ||
      token_id == ML_SQL_BUFFER_RESULT ||
      token_id == ML_SQL_CALC_FOUND_ROWS || token_id == ML_SQL_NO_CACHE ||
      token_id == ML_SQL_SMALL_RESULT || token_id == ML_STRAIGHT_JOIN) {
    return 1 << 2;
  }

  return 0;
}

static int select_order_direction_boundary(int token_id) {
  return token_id == ML_COMMA || token_id == ML_FOR || token_id == ML_INTO ||
         token_id == ML_LIMIT || token_id == ML_LOCK || token_id == ML_RP ||
         select_set_operator(token_id);
}

static int select_rollup_boundary(int token_id, MyliteToken token) {
  return token_id == ML_FOR || token_id == ML_HAVING || token_id == ML_INTO ||
         token_id == ML_LIMIT || token_id == ML_LOCK || token_id == ML_ORDER ||
         select_set_operator(token_id) || token_ascii_equal(token, "qualify") ||
         token_ascii_equal(token, "window");
}

static int select_set_operator(int token_id) {
  return token_id == ML_EXCEPT || token_id == ML_INTERSECT ||
         token_id == ML_UNION;
}

static int select_set_option(int token_id) {
  return token_id == ML_ALL || token_id == ML_DISTINCT ||
         token_id == ML_DISTINCTROW;
}

static int select_set_operand_start(int token_id) {
  return token_id == ML_LP || token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_VALUES || token_id == ML_WITH;
}

static int parenthesized_query_order_boundary(int token_id) {
  return token_id == ML_COMMA || token_id == ML_FOR || token_id == ML_INTO ||
         token_id == ML_LIMIT || token_id == ML_LOCK || token_id == ML_RB ||
         token_id == ML_RC || token_id == ML_RP ||
         select_clause_requires_by(token_id) ||
         select_clause_requires_operand(token_id) ||
         select_set_operator(token_id);
}

static int query_expression_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *depth,
    int *previous_top_token_id, MyliteToken *previous_top_token,
    int *previous_was_operator, MyliteExpressionStack *stack,
    const char *message) {
  return query_expression_token_with_flags(
      ctx, token_id, token, depth, previous_top_token_id, previous_top_token,
      previous_was_operator, stack, 0, message);
}

static int query_expression_token_with_flags(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *depth,
    int *previous_top_token_id, MyliteToken *previous_top_token,
    int *previous_was_operator, MyliteExpressionStack *stack, int flags,
    const char *message) {
  int bare_default_allowed = 0;

  if (do_expression_value_start(token_id, token) &&
      !do_expression_operator(token_id, token) && !*previous_was_operator &&
      do_expression_value_terminal(*previous_top_token_id,
                                   *previous_top_token) &&
      !do_expression_allows_adjacent(*previous_top_token_id,
                                     *previous_top_token, token_id, token)) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }
  if (query_expression_malformed_operator_sequence(
          *previous_top_token_id, *previous_top_token, token_id)) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (*previous_top_token_id == ML_DEFAULT && token_id != ML_DOT &&
      token_id != ML_LP) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (token_id == ML_DEFAULT &&
      (flags & QUERY_EXPRESSION_ALLOW_BARE_DEFAULT) &&
      (*previous_top_token_id == 0 || *previous_top_token_id == ML_COMMA)) {
    bare_default_allowed = 1;
  }

  if (token_opens_nested_expression(token_id)) {
    (*depth)++;
    if (stack &&
        !query_expression_stack_open_from_previous(
            ctx, stack, *depth, *previous_top_token_id, *previous_top_token,
            *previous_was_operator, token_id, token, flags, message)) {
      return 0;
    }
  } else {
    *previous_top_token_id = token_id;
    *previous_top_token = token;
    *previous_was_operator =
        token_id == ML_DEFAULT ? !bare_default_allowed
                               : do_expression_operator(token_id, token);
  }

  return 1;
}

static int query_expression_depth_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *depth,
    MyliteExpressionStack *stack, const char *message) {
  int active = query_expression_stack_active(stack, *depth);

  if (token_opens_nested_expression(token_id)) {
    if (active && stack->frames[*depth].default_identifier_parts >= 0) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    if (active &&
        !query_expression_stack_open(ctx, stack, *depth, *depth + 1,
                                     token_id, token, message)) {
      return 0;
    }
    (*depth)++;
  } else if (token_closes_nested_expression(token_id)) {
    if (active &&
        !query_expression_stack_close(ctx, stack, *depth, token, message)) {
      return 0;
    }
    (*depth)--;
    if (*depth > 0 && query_expression_stack_active(stack, *depth)) {
      query_expression_stack_note_terminal(stack, *depth, token_id, token);
    }
  } else if (active &&
             !query_expression_stack_token(ctx, stack, *depth, token_id,
                                           token, message)) {
    return 0;
  }

  return 1;
}

static int query_expression_stack_active(MyliteExpressionStack *stack,
                                         int depth) {
  return stack && depth > 0 && depth < MYLITE_EXPRESSION_STACK_LIMIT &&
         stack->frames[depth].active;
}

static int query_expression_stack_rejects_comma(MyliteExpressionStack *stack,
                                                int depth) {
  return query_expression_stack_active(stack, depth) &&
         !stack->frames[depth].allow_comma;
}

static int query_expression_stack_open_from_previous(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int depth,
    int previous_top_token_id, MyliteToken previous_top_token,
    int previous_was_operator, int token_id, MyliteToken token, int flags,
    const char *message) {
  MyliteExpressionFrame *frame;

  if (depth <= 0 || depth >= MYLITE_EXPRESSION_STACK_LIMIT) {
    return 1;
  }

  (void) ctx;
  (void) message;

  frame = &stack->frames[depth];
  frame->active = 1;
  frame->allow_empty = query_expression_group_allows_empty(
      previous_top_token_id, previous_top_token, previous_was_operator,
      token_id);
  frame->allow_comma =
      frame->allow_empty || previous_top_token_id == ML_IN;
  frame->validate_adjacent =
      query_expression_group_validates_adjacent(frame->allow_empty, token_id);
  frame->started = 0;
  frame->previous_top_token_id = 0;
  frame->previous_top_token = token;
  frame->previous_was_operator = 1;
  frame->flags = flags;
  frame->default_identifier_parts = -1;
  frame->default_identifier_after_dot = 0;
  if (previous_top_token_id == ML_DEFAULT && token_id == ML_LP) {
    frame->allow_empty = 0;
    frame->allow_comma = 0;
    frame->validate_adjacent = 0;
    frame->flags = 0;
    frame->default_identifier_parts = 0;
  }

  return 1;
}

static int query_expression_stack_open(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int current_depth,
    int new_depth, int token_id, MyliteToken token, const char *message) {
  MyliteExpressionFrame *frame = &stack->frames[current_depth];

  return query_expression_stack_open_from_previous(
      ctx, stack, new_depth, frame->previous_top_token_id,
      frame->previous_top_token, frame->previous_was_operator, token_id, token,
      0, message);
}

static void query_expression_stack_open_list(MyliteExpressionStack *stack,
                                             int depth, MyliteToken token,
                                             int allow_empty, int flags) {
  MyliteExpressionFrame *frame;

  if (depth <= 0 || depth >= MYLITE_EXPRESSION_STACK_LIMIT) {
    return;
  }

  frame = &stack->frames[depth];
  frame->active = 1;
  frame->allow_empty = allow_empty;
  frame->allow_comma = allow_empty;
  frame->validate_adjacent = 1;
  frame->started = 0;
  frame->previous_top_token_id = 0;
  frame->previous_top_token = token;
  frame->previous_was_operator = 1;
  frame->flags = flags;
  frame->default_identifier_parts = -1;
  frame->default_identifier_after_dot = 0;
}

static int query_expression_stack_token(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int depth,
    int token_id, MyliteToken token, const char *message) {
  MyliteExpressionFrame *frame = &stack->frames[depth];
  int bare_default_allowed = 0;

  if (frame->default_identifier_parts >= 0) {
    return query_expression_stack_default_token(ctx, frame, token_id, token,
                                                message);
  }
  if (frame->previous_top_token_id == ML_DEFAULT) {
    if (token_id != ML_DOT &&
        (token_id != ML_COMMA ||
         !(frame->flags & QUERY_EXPRESSION_ALLOW_BARE_DEFAULT) ||
         frame->previous_was_operator)) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
  }
  if (query_expression_group_disables_adjacent(token_id)) {
    frame->validate_adjacent = 0;
  }
  if (frame->validate_adjacent &&
      do_expression_value_start(token_id, token) &&
      !do_expression_operator(token_id, token) &&
      !frame->previous_was_operator &&
      do_expression_value_terminal(frame->previous_top_token_id,
                                   frame->previous_top_token) &&
      !do_expression_allows_adjacent(frame->previous_top_token_id,
                                     frame->previous_top_token, token_id,
                                     token)) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }
  if (query_expression_malformed_operator_sequence(
          frame->previous_top_token_id, frame->previous_top_token, token_id)) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (token_id == ML_DEFAULT &&
      (frame->flags & QUERY_EXPRESSION_ALLOW_BARE_DEFAULT) &&
      (frame->previous_top_token_id == 0 ||
       frame->previous_top_token_id == ML_COMMA)) {
    bare_default_allowed = 1;
  }

  frame->started = 1;
  frame->previous_top_token_id = token_id;
  frame->previous_top_token = token;
  frame->previous_was_operator =
      token_id == ML_DEFAULT ? !bare_default_allowed
                             : do_expression_operator(token_id, token);

  return 1;
}

static int query_expression_stack_close(
    MyliteParseContext *ctx, MyliteExpressionStack *stack, int depth,
    MyliteToken token, const char *message) {
  MyliteExpressionFrame *frame = &stack->frames[depth];

  if (frame->default_identifier_parts >= 0) {
    if (frame->default_identifier_parts == 0 ||
        frame->default_identifier_after_dot) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    frame->active = 0;
    return 1;
  }

  if ((!frame->started && !frame->allow_empty) ||
      (frame->started && frame->previous_was_operator &&
       !(frame->allow_empty &&
         (frame->previous_top_token_id == ML_STAR ||
          (frame->previous_top_token_id == ML_ATOM &&
           frame->previous_top_token.length == 1 &&
           frame->previous_top_token.start[0] == '*'))))) {
    mylite_parser_reject(
        ctx, frame->started ? frame->previous_top_token : token, message);
    return 0;
  }

  frame->active = 0;
  return 1;
}

static int query_expression_stack_default_token(
    MyliteParseContext *ctx, MyliteExpressionFrame *frame, int token_id,
    MyliteToken token, const char *message) {
  if (token_id == ML_DOT) {
    if (frame->default_identifier_parts == 0 ||
        frame->default_identifier_after_dot ||
        frame->default_identifier_parts >= 3) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    frame->started = 1;
    frame->previous_top_token_id = token_id;
    frame->previous_top_token = token;
    frame->previous_was_operator = 1;
    frame->default_identifier_after_dot = 1;
    return 1;
  }

  if (frame->default_identifier_parts > 0 &&
      !frame->default_identifier_after_dot) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (!query_expression_default_identifier_token(token_id, token)) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  frame->started = 1;
  frame->previous_top_token_id = token_id;
  frame->previous_top_token = token;
  frame->previous_was_operator = 0;
  frame->default_identifier_parts++;
  frame->default_identifier_after_dot = 0;
  return 1;
}

static int query_expression_default_identifier_token(int token_id,
                                                     MyliteToken token) {
  if (token_id == ML_ATOM || token_id == ML_QUOTED_ID) {
    return !token_is_invalid_identifier_atom(token, 0);
  }

  if (token_id == ML_CURRENT_USER || token_id == ML_FROM ||
      token_id == ML_SELECT || token_id == ML_TABLE || token_id == ML_UPDATE ||
      token_id == ML_WHERE) {
    return 0;
  }

  return dml_row_alias_token(token_id) &&
         !do_expression_operator(token_id, token);
}

static void query_expression_stack_note_terminal(
    MyliteExpressionStack *stack, int depth, int token_id, MyliteToken token) {
  MyliteExpressionFrame *frame = &stack->frames[depth];

  frame->started = 1;
  frame->previous_top_token_id = token_id;
  frame->previous_top_token = token;
  frame->previous_was_operator = 0;
}

static int query_expression_group_allows_empty(int previous_top_token_id,
                                               MyliteToken previous_top_token,
                                               int previous_was_operator,
                                               int token_id) {
  if (token_id != ML_LP || previous_top_token_id == 0) {
    return 0;
  }

  if (previous_top_token_id == ML_EXISTS) {
    return 0;
  }

  if (previous_top_token_id == ML_ATOM &&
      token_ascii_equal(previous_top_token, "over")) {
    return 1;
  }

  if (previous_was_operator) {
    return 0;
  }

  if (previous_top_token_id == ML_ATOM ||
      previous_top_token_id == ML_QUOTED_ID ||
      previous_top_token_id == ML_VALUE ||
      previous_top_token_id == ML_VALUES) {
    return 1;
  }

  return !do_expression_value_terminal(previous_top_token_id,
                                       previous_top_token);
}

static int query_expression_group_validates_adjacent(int allow_empty,
                                                     int token_id) {
  return token_id == ML_LP && !allow_empty;
}

static int query_expression_group_disables_adjacent(int token_id) {
  return token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_VALUES || token_id == ML_WITH;
}

static int query_expression_malformed_operator_sequence(
    int previous_top_token_id, MyliteToken previous_top_token, int token_id) {
  return token_id == ML_AND && previous_top_token_id == ML_ATOM &&
         token_ascii_equal(previous_top_token, "between");
}

static int select_window_name_token(int token_id, MyliteToken token) {
  if (token_id == ML_AS || token_ascii_equal(token, "window")) {
    return 0;
  }

  return token_id == ML_ATOM || token_id == ML_QUOTED_ID;
}

static int select_lock_table_ref_start(int token_id, MyliteToken token) {
  if (token_ascii_equal(token, "locked") || token_ascii_equal(token, "nowait") ||
      token_ascii_equal(token, "of") || token_ascii_equal(token, "skip")) {
    return 0;
  }

  return token_id == ML_ATOM || token_id == ML_QUOTED_ID;
}

static int select_lock_table_ref_part(int token_id) {
  return token_id == ML_ATOM || token_id == ML_QUOTED_ID ||
         token_id == ML_STAR;
}

static int select_into_output_option_start(int token_id) {
  return token_id == ML_CHARACTER || token_id == ML_CHARSET ||
         token_id == ML_COLUMNS || token_id == ML_ENCLOSED ||
         token_id == ML_ESCAPED || token_id == ML_FIELDS ||
         token_id == ML_LINES || token_id == ML_OPTIONALLY ||
         token_id == ML_STARTING || token_id == ML_TERMINATED;
}

static int select_into_output_follow_token(int token_id, MyliteToken token) {
  return token_id == ML_FOR || token_id == ML_FROM || token_id == ML_GROUP ||
         token_id == ML_HAVING || token_id == ML_LIMIT ||
         token_id == ML_LOCK || token_id == ML_ORDER || token_id == ML_WHERE ||
         token_ascii_equal(token, "qualify") ||
         token_ascii_equal(token, "window");
}

static int select_into_variable_target_token(int token_id, MyliteToken token) {
  return select_window_name_token(token_id, token) || token_id == ML_AT_HOST;
}

static int select_into_variable_at_target_token(int token_id,
                                                MyliteToken token) {
  return select_window_name_token(token_id, token) ||
         token_id == ML_DOUBLE_QUOTED_STRING || token_id == ML_STRING_LITERAL;
}

static int select_outfile_field_option_start(int token_id) {
  return token_id == ML_ENCLOSED || token_id == ML_ESCAPED ||
         token_id == ML_OPTIONALLY || token_id == ML_TERMINATED;
}

static int select_outfile_line_option_start(int token_id) {
  return token_id == ML_STARTING || token_id == ML_TERMINATED;
}

static int select_index_hint_name_token(int token_id) {
  return token_id != ML_COMMA && token_id != ML_LP && token_id != ML_RP &&
         token_id != ML_SEMI;
}

static int select_index_hint_type(int token_id) {
  return token_id == ML_FORCE || token_id == ML_IGNORE || token_id == ML_USE;
}

static int select_partition_name_token(int token_id) {
  return token_id != ML_COMMA && token_id != ML_LP && token_id != ML_RP &&
         token_id != ML_SEMI;
}

static int select_tablesample_boundary(int token_id, MyliteToken token) {
  return token_id == ML_COMMA || token_id == ML_FOR || token_id == ML_HAVING ||
         token_id == ML_INTO || token_id == ML_JOIN || token_id == ML_LIMIT ||
         token_id == ML_LOCK || token_id == ML_ORDER || token_id == ML_WHERE ||
         token_id == ML_STRAIGHT_JOIN || select_set_operator(token_id) ||
         token_ascii_equal(token, "cross") || token_ascii_equal(token, "inner") ||
         token_ascii_equal(token, "left") || token_ascii_equal(token, "natural") ||
         token_ascii_equal(token, "qualify") || token_ascii_equal(token, "right") ||
         token_ascii_equal(token, "window");
}

static int select_tablesample_percentage_token(int token_id) {
  return token_id == ML_AT_HOST || token_id == ML_ATOM ||
         token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
         token_id == ML_NUMBER_LITERAL || token_id == ML_QUOTED_ID;
}

static int select_charset_name_token(int token_id, MyliteToken token) {
  if (token_is_quoted_hex_or_bit_literal(token)) {
    return 0;
  }

  return token_id == ML_ATOM || token_id == ML_BINARY ||
         token_id == ML_DOUBLE_QUOTED_STRING || token_id == ML_QUOTED_ID ||
         token_id == ML_STRING_LITERAL || token_ascii_equal(token, "binary");
}

static int select_limit_option_token(int token_id) {
  return token_id == ML_ATOM || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID;
}

static int select_string_literal_token(int token_id) {
  return token_id == ML_DOUBLE_QUOTED_STRING || token_id == ML_STRING_LITERAL;
}

static int do_clause_boundary(int token_id) {
  return token_id == ML_FROM || token_id == ML_GROUP || token_id == ML_HAVING ||
         token_id == ML_INTO || token_id == ML_LIMIT || token_id == ML_ORDER ||
         token_id == ML_WHERE;
}

static int do_expression_operator(int token_id, MyliteToken token) {
  if (token_id == ML_AND || token_id == ML_ASSIGN || token_id == ML_COLLATE ||
      token_id == ML_COMMA || token_id == ML_EQUALS || token_id == ML_GE ||
      token_id == ML_GT || token_id == ML_IN || token_id == ML_LE ||
      token_id == ML_LIKE || token_id == ML_LT || token_id == ML_MINUS ||
      token_id == ML_NOT || token_id == ML_OR || token_id == ML_STAR ||
      token_id == ML_USING) {
    return 1;
  }

  if (token_id != ML_ATOM) {
    return 0;
  }

  if (token.length == 1 && strchr("!%&*+/^|~", token.start[0]) != NULL) {
    return 1;
  }

  return token_ascii_equal(token, "against") ||
         token_ascii_equal(token, "between") || token_ascii_equal(token, "div") ||
         token_ascii_equal(token, "escape") || token_ascii_equal(token, "is") ||
         token_ascii_equal(token, "member") || token_ascii_equal(token, "mod") ||
         token_ascii_equal(token, "of") || token_ascii_equal(token, "over") ||
         token_ascii_equal(token, "regexp") || token_ascii_equal(token, "rlike") ||
         token_ascii_equal(token, "sounds") || token_ascii_equal(token, "xor");
}

static int do_expression_value_start(int token_id, MyliteToken token) {
  (void) token;
  return token_id == ML_AT_HOST || token_id == ML_AT_SIGN ||
         token_id == ML_ATOM || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_DEFAULT || token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_LB ||
         token_id == ML_LC || token_id == ML_LP ||
         token_id == ML_NULL || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID || token_id == ML_STRING_LITERAL;
}

static int do_expression_value_terminal(int token_id, MyliteToken token) {
  (void) token;
  return token_id == ML_AT_HOST || token_id == ML_ATOM ||
         token_id == ML_BOOLEAN_NUMBER || token_id == ML_DEFAULT ||
         token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_NULL ||
         token_id == ML_NUMBER_LITERAL || token_id == ML_QUOTED_ID ||
         token_id == ML_RB || token_id == ML_RC || token_id == ML_RP ||
         token_id == ML_STRING_LITERAL;
}

static int do_expression_interval_unit_token(int token_id, MyliteToken token) {
  return token_id == ML_DAY || token_ascii_equal(token, "microsecond") ||
         token_ascii_equal(token, "minute") ||
         token_ascii_equal(token, "month") ||
         token_ascii_equal(token, "quarter") ||
         token_ascii_equal(token, "second") ||
         token_ascii_equal(token, "week") ||
         token_ascii_equal(token, "year") ||
         token_ascii_equal(token, "day_hour") ||
         token_ascii_equal(token, "day_microsecond") ||
         token_ascii_equal(token, "day_minute") ||
         token_ascii_equal(token, "day_second") ||
         token_ascii_equal(token, "hour_microsecond") ||
         token_ascii_equal(token, "hour_minute") ||
         token_ascii_equal(token, "hour_second") ||
         token_ascii_equal(token, "minute_microsecond") ||
         token_ascii_equal(token, "minute_second") ||
         token_ascii_equal(token, "second_microsecond") ||
         token_ascii_equal(token, "year_month");
}

static int do_expression_hex_or_bit_literal_token(MyliteToken token) {
  return token.length > 2 && token.start[0] == '0' &&
         (token.start[1] == 'x' || token.start[1] == 'X' ||
          token.start[1] == 'b' || token.start[1] == 'B');
}

static int do_expression_allows_adjacent(int previous_id,
                                         MyliteToken previous, int current_id,
                                         MyliteToken current) {
  if (current_id == ML_LP &&
      (previous_id == ML_ATOM || previous_id == ML_QUOTED_ID)) {
    return 1;
  }

  if (previous_id == ML_DEFAULT && current_id == ML_LP) {
    return 1;
  }

  if (previous_id == ML_RP && current_id == ML_ATOM) {
    return 1;
  }

  if ((current_id == ML_FACTOR_NUMBER || current_id == ML_NUMBER_LITERAL) &&
      current.length > 0 &&
      (current.start[0] == '+' || current.start[0] == '-')) {
    return 1;
  }

  if ((previous_id == ML_ATOM || previous_id == ML_QUOTED_ID) &&
      token_ascii_equal(previous, "match")) {
    return current_id == ML_ATOM || current_id == ML_QUOTED_ID;
  }

  if (previous_id == ML_ATOM && current_id == ML_ATOM &&
      token_ascii_equal(previous, "boolean")) {
    return token_ascii_equal(current, "mode");
  }

  if (previous_id == ML_ATOM && previous.length > 0 &&
      previous.start[0] == '_' &&
      do_expression_hex_or_bit_literal_token(current)) {
    return 1;
  }

  if ((previous_id == ML_BOOLEAN_NUMBER ||
       previous_id == ML_FACTOR_NUMBER ||
       previous_id == ML_NUMBER_LITERAL) &&
      do_expression_interval_unit_token(current_id, current)) {
    return 1;
  }

  if (do_expression_conditional_comment_operator_between(previous, current)) {
    return 1;
  }

  if ((previous_id == ML_STRING_LITERAL ||
       previous_id == ML_DOUBLE_QUOTED_STRING) &&
      (current_id == ML_STRING_LITERAL ||
       current_id == ML_DOUBLE_QUOTED_STRING)) {
    return 1;
  }

  if ((current_id == ML_STRING_LITERAL ||
       current_id == ML_DOUBLE_QUOTED_STRING) &&
      previous_id == ML_ATOM) {
    return previous.length > 0 &&
           (previous.start[0] == '_' || token_ascii_equal(previous, "date") ||
            token_ascii_equal(previous, "n") ||
            token_ascii_equal(previous, "time") ||
            token_ascii_equal(previous, "timestamp"));
  }

  return 0;
}

static int do_expression_conditional_comment_operator_between(
    MyliteToken previous, MyliteToken current) {
  const char *cursor = previous.start + previous.length;
  const char *end = current.start;
  int in_conditional_comment = 0;

  while (cursor < end) {
    if (!in_conditional_comment && end - cursor >= 3 && cursor[0] == '/' &&
        cursor[1] == '*' && cursor[2] == '!') {
      in_conditional_comment = 1;
      cursor += 3;
      continue;
    }
    if (in_conditional_comment && strchr("!%&*+-/<=>^|~", *cursor) != NULL) {
      return 1;
    }
    cursor++;
  }

  return 0;
}

static int kill_at_sign_target_token(int token_id) {
  return token_id == ML_ATOM || token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_QUOTED_ID || token_id == ML_STRING_LITERAL;
}

static int kill_target_allows_call(int token_id) {
  return token_id != ML_AT_EMPTY && token_id != ML_AT_HOST &&
         token_id != ML_BOOLEAN_NUMBER && token_id != ML_FACTOR_NUMBER &&
         token_id != ML_NUMBER_LITERAL;
}

static int kill_target_token(int token_id) {
  return token_id != ML_STAR &&
         (token_id == ML_AT_HOST || token_id == ML_BOOLEAN_NUMBER ||
          token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL ||
          dml_row_alias_token(token_id));
}

static int reset_persist_name_part_token(int token_id, MyliteToken token) {
  return token_id != ML_COMMA && token_id != ML_DOT && token_id != ML_SEMI &&
         token_id != ML_STAR && token_id != ML_AT_SIGN &&
         token_id != ML_AT_HOST && token_id != ML_AT_EMPTY &&
         !token_ascii_equal(token, "*");
}

static int create_table_query_body_start(int token_id) {
  return token_id == ML_AS || token_id == ML_LIKE || token_id == ML_SELECT ||
         token_id == ML_TABLE || token_id == ML_VALUES || token_id == ML_WITH;
}

static int create_table_query_expression_start(int token_id) {
  return token_id == ML_LP || token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_VALUES || token_id == ML_WITH;
}

static int create_table_tail_option_start_token(int token_id) {
  return token_id == ML_AUTOEXTEND_SIZE || token_id == ML_AUTO_INCREMENT ||
         token_id == ML_AVG_ROW_LENGTH || token_id == ML_CHARACTER ||
         token_id == ML_CHARSET || token_id == ML_CHECKSUM ||
         token_id == ML_COLLATE || token_id == ML_COMMENT ||
         token_id == ML_COMPRESSION || token_id == ML_CONNECTION ||
         token_id == ML_DATA || token_id == ML_DEFAULT ||
         token_id == ML_DELAY_KEY_WRITE || token_id == ML_ENCRYPTION ||
         token_id == ML_ENGINE || token_id == ML_ENGINE_ATTRIBUTE ||
         token_id == ML_INDEX || token_id == ML_INSERT_METHOD ||
         token_id == ML_KEY_BLOCK_SIZE || token_id == ML_MAX_ROWS ||
         token_id == ML_MIN_ROWS || token_id == ML_PACK_KEYS ||
         token_id == ML_PASSWORD || token_id == ML_ROW_FORMAT ||
         token_id == ML_SECONDARY_ENGINE ||
         token_id == ML_SECONDARY_ENGINE_ATTRIBUTE || token_id == ML_START ||
         token_id == ML_STATS_AUTO_RECALC ||
         token_id == ML_STATS_PERSISTENT ||
         token_id == ML_STATS_SAMPLE_PAGES || token_id == ML_STORAGE ||
         token_id == ML_TABLESPACE || token_id == ML_UNION;
}

static void validate_create_table_tail_options(MyliteParseContext *ctx,
                                               MyliteToken start) {
  enum {
    CREATE_TABLE_TAIL_READY,
    CREATE_TABLE_TAIL_SKIP_BODY,
    CREATE_TABLE_TAIL_AFTER_DEFAULT,
    CREATE_TABLE_TAIL_AFTER_CHARACTER,
    CREATE_TABLE_TAIL_AFTER_DATA,
    CREATE_TABLE_TAIL_AFTER_INDEX,
    CREATE_TABLE_TAIL_AFTER_START,
    CREATE_TABLE_TAIL_EXPECT_VALUE,
    CREATE_TABLE_TAIL_AFTER_UNION,
    CREATE_TABLE_TAIL_UNION_BODY,
    CREATE_TABLE_TAIL_AFTER_CTAS_AS,
    CREATE_TABLE_TAIL_AFTER_CTAS_MODIFIER
  };
  enum {
    CREATE_TABLE_PARTITION_NONE,
    CREATE_TABLE_PARTITION_EXPECT_BY,
    CREATE_TABLE_PARTITION_EXPECT_METHOD,
    CREATE_TABLE_PARTITION_IN_METHOD,
    CREATE_TABLE_PARTITION_AFTER_METHOD,
    CREATE_TABLE_PARTITION_EXPECT_PARTITIONS_VALUE
  };
  enum {
    CREATE_TABLE_UNION_NEED_NAME_OR_END,
    CREATE_TABLE_UNION_NEED_NAME,
    CREATE_TABLE_UNION_AFTER_NAME,
    CREATE_TABLE_UNION_AFTER_DOT
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_start = 0;
  int if_not_exists_state = 0;
  int table_name_parts = 0;
  int table_dot_pending = 0;
  int table_ref_done = 0;
  int state = CREATE_TABLE_TAIL_READY;
  int depth = 0;
  int value_kind = CREATE_TABLE_OPTION_VALUE_NONE;
  int value_saw_equals = 0;
  int need_option_after_comma = 0;
  int saw_definition_body = 0;
  int option_tail_without_body = 0;
  int partition_tail = 0;
  int partition_state = CREATE_TABLE_PARTITION_NONE;
  int partition_skip_body = 0;
  int union_state = CREATE_TABLE_UNION_NEED_NAME_OR_END;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_start) {
      if (token.offset == start.offset) {
        saw_start = 1;
      }
      continue;
    }

    if (!table_ref_done) {
      if (if_not_exists_state == 1) {
        if (token_id != ML_NOT) {
          mylite_parser_reject(ctx, pending_token,
                               "malformed CREATE TABLE tail");
          return;
        }
        if_not_exists_state = 2;
        pending_token = token;
        continue;
      }
      if (if_not_exists_state == 2) {
        if (token_id != ML_EXISTS) {
          mylite_parser_reject(ctx, pending_token,
                               "malformed CREATE TABLE tail");
          return;
        }
        if_not_exists_state = 0;
        continue;
      }
      if (table_name_parts == 0 && token_id == ML_IF) {
        if_not_exists_state = 1;
        pending_token = token;
        continue;
      }
      if (table_dot_pending) {
        table_name_parts++;
        table_dot_pending = 0;
        continue;
      }
      if (table_name_parts == 0) {
        table_name_parts = 1;
        continue;
      }
      if (token_id == ML_DOT && table_name_parts == 1) {
        table_dot_pending = 1;
        continue;
      }
      table_ref_done = 1;
    }

    if (state == CREATE_TABLE_TAIL_SKIP_BODY) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0) {
          state = CREATE_TABLE_TAIL_READY;
          if (partition_skip_body) {
            partition_state = CREATE_TABLE_PARTITION_AFTER_METHOD;
            partition_skip_body = 0;
          }
        }
      }
      continue;
    }

    if (state == CREATE_TABLE_TAIL_UNION_BODY) {
      if (token_id == ML_RP) {
        if (union_state == CREATE_TABLE_UNION_NEED_NAME ||
            union_state == CREATE_TABLE_UNION_AFTER_DOT) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE UNION option");
          return;
        }
        state = CREATE_TABLE_TAIL_READY;
        union_state = CREATE_TABLE_UNION_NEED_NAME_OR_END;
        continue;
      }
      if (token_id == ML_COMMA) {
        if (union_state != CREATE_TABLE_UNION_AFTER_NAME) {
          mylite_parser_reject(ctx, token,
                               "incomplete CREATE TABLE UNION option");
          return;
        }
        union_state = CREATE_TABLE_UNION_NEED_NAME;
        pending_token = token;
        continue;
      }
      if (token_id == ML_DOT) {
        if (union_state != CREATE_TABLE_UNION_AFTER_NAME) {
          mylite_parser_reject(ctx, token,
                               "malformed CREATE TABLE UNION option");
          return;
        }
        union_state = CREATE_TABLE_UNION_AFTER_DOT;
        pending_token = token;
        continue;
      }
      if (dml_row_alias_token(token_id) &&
          union_state != CREATE_TABLE_UNION_AFTER_NAME) {
        union_state = CREATE_TABLE_UNION_AFTER_NAME;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed CREATE TABLE UNION option");
      return;
    }

    if (state == CREATE_TABLE_TAIL_EXPECT_VALUE) {
      if (token_id == ML_EQUALS && !value_saw_equals &&
          create_table_tail_option_value_allows_equals(value_kind)) {
        value_saw_equals = 1;
        continue;
      }
      if (!create_table_tail_option_value_token(value_kind, token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "invalid CREATE TABLE table option");
        return;
      }
      state = CREATE_TABLE_TAIL_READY;
      value_kind = CREATE_TABLE_OPTION_VALUE_NONE;
      value_saw_equals = 0;
      need_option_after_comma = 0;
      continue;
    }

    if (state == CREATE_TABLE_TAIL_AFTER_DEFAULT) {
      if (token_id == ML_CHARACTER) {
        state = CREATE_TABLE_TAIL_AFTER_CHARACTER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHARSET || token_id == ML_COLLATE) {
        state = CREATE_TABLE_TAIL_EXPECT_VALUE;
        value_kind = CREATE_TABLE_OPTION_VALUE_CHARSET;
        value_saw_equals = 0;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "invalid CREATE TABLE table option");
      return;
    }

    if (state == CREATE_TABLE_TAIL_AFTER_CHARACTER) {
      if (token_id != ML_SET) {
        mylite_parser_reject(ctx, pending_token,
                             "invalid CREATE TABLE table option");
        return;
      }
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_CHARSET;
      value_saw_equals = 0;
      pending_token = token;
      continue;
    }

    if (state == CREATE_TABLE_TAIL_AFTER_DATA ||
        state == CREATE_TABLE_TAIL_AFTER_INDEX) {
      if (token_id != ML_DIRECTORY) {
        mylite_parser_reject(ctx, pending_token,
                             "invalid CREATE TABLE table option");
        return;
      }
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_STRING;
      value_saw_equals = 0;
      pending_token = token;
      continue;
    }

    if (state == CREATE_TABLE_TAIL_AFTER_START) {
      if (token_id != ML_TRANSACTION) {
        mylite_parser_reject(ctx, pending_token,
                             "invalid CREATE TABLE table option");
        return;
      }
      state = CREATE_TABLE_TAIL_READY;
      need_option_after_comma = 0;
      continue;
    }

    if (state == CREATE_TABLE_TAIL_AFTER_UNION) {
      if (token_id == ML_EQUALS && !value_saw_equals) {
        value_saw_equals = 1;
        continue;
      }
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "invalid CREATE TABLE table option");
        return;
      }
      state = CREATE_TABLE_TAIL_UNION_BODY;
      depth = 1;
      value_saw_equals = 0;
      need_option_after_comma = 0;
      union_state = CREATE_TABLE_UNION_NEED_NAME_OR_END;
      continue;
    }

    if (state == CREATE_TABLE_TAIL_AFTER_CTAS_AS) {
      if (token_id == ML_LP && parenthesized_query_start_follows(ctx, token)) {
        mylite_parser_validate_parenthesized_statement(ctx, token);
        return;
      }
      if (!create_table_query_expression_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE query body");
        return;
      }
      return;
    }

    if (state == CREATE_TABLE_TAIL_AFTER_CTAS_MODIFIER) {
      if (token_id == ML_AS) {
        state = CREATE_TABLE_TAIL_AFTER_CTAS_AS;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LP && parenthesized_query_start_follows(ctx, token)) {
        mylite_parser_validate_parenthesized_statement(ctx, token);
        return;
      }
      if (create_table_query_expression_start(token_id)) {
        return;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete CREATE TABLE query body");
      return;
    }

    if (token_id == ML_LP) {
      state = CREATE_TABLE_TAIL_SKIP_BODY;
      depth = 1;
      if (partition_tail) {
        partition_skip_body = 1;
      }
      if (!option_tail_without_body) {
        saw_definition_body = 1;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if (need_option_after_comma) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE table option");
      } else if (option_tail_without_body) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE query body");
      }
      return;
    }

    if (token_id == ML_PARTITION) {
      if (!saw_definition_body) {
        option_tail_without_body = 1;
      }
      partition_tail = 1;
      partition_state = CREATE_TABLE_PARTITION_EXPECT_BY;
      continue;
    }

    if (token_id == ML_AS) {
      state = CREATE_TABLE_TAIL_AFTER_CTAS_AS;
      pending_token = token;
      continue;
    }

    if (token_id == ML_IGNORE || token_id == ML_REPLACE) {
      state = CREATE_TABLE_TAIL_AFTER_CTAS_MODIFIER;
      pending_token = token;
      continue;
    }

    if (create_table_query_expression_start(token_id) ||
        token_id == ML_LIKE) {
      if (need_option_after_comma) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE table option");
      } else if (token_id == ML_LP &&
                 parenthesized_query_start_follows(ctx, token)) {
        mylite_parser_validate_parenthesized_statement(ctx, token);
      }
      return;
    }

    if (partition_tail) {
      if (partition_state == CREATE_TABLE_PARTITION_EXPECT_BY) {
        if (token_id != ML_BY) {
          mylite_parser_reject(ctx, token,
                               "invalid CREATE TABLE partition option");
          return;
        }
        partition_state = CREATE_TABLE_PARTITION_EXPECT_METHOD;
        continue;
      }
      if (partition_state == CREATE_TABLE_PARTITION_EXPECT_METHOD) {
        if (token_ascii_equal(token, "linear")) {
          continue;
        }
        if (token_ascii_equal(token, "hash") ||
            token_id == ML_KEY || token_ascii_equal(token, "range") ||
            token_ascii_equal(token, "list")) {
          partition_state = CREATE_TABLE_PARTITION_IN_METHOD;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "invalid CREATE TABLE partition option");
        return;
      }
      if (partition_state == CREATE_TABLE_PARTITION_IN_METHOD) {
        if (token_id == ML_ALGORITHM || token_id == ML_COLUMNS ||
            token_id == ML_EQUALS ||
            create_table_tail_option_number_token(token_id)) {
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "invalid CREATE TABLE partition option");
        return;
      }
      if (partition_state == CREATE_TABLE_PARTITION_EXPECT_PARTITIONS_VALUE) {
        if (!create_table_tail_option_number_token(token_id)) {
          mylite_parser_reject(ctx, token,
                               "invalid CREATE TABLE partition option");
          return;
        }
        partition_state = CREATE_TABLE_PARTITION_AFTER_METHOD;
        continue;
      }
      if (token_id == ML_PARTITIONS ||
          token_ascii_equal(token, "subpartitions")) {
        partition_state = CREATE_TABLE_PARTITION_EXPECT_PARTITIONS_VALUE;
        continue;
      }
      if (token_ascii_equal(token, "subpartition")) {
        partition_state = CREATE_TABLE_PARTITION_EXPECT_BY;
        continue;
      }
      if (token_id == ML_COMMA) {
        mylite_parser_reject(ctx, token,
                             "invalid CREATE TABLE partition option");
        return;
      }
      mylite_parser_reject(ctx, token,
                           "invalid CREATE TABLE partition option");
      return;
    }

    if (token_id == ML_COMMA) {
      if (need_option_after_comma) {
        mylite_parser_reject(ctx, token,
                             "incomplete CREATE TABLE table option");
        return;
      }
      need_option_after_comma = 1;
      pending_token = token;
      continue;
    }

    if (token_id == ML_DEFAULT) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_AFTER_DEFAULT;
      pending_token = token;
      continue;
    }
    if (token_id == ML_CHARACTER) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_AFTER_CHARACTER;
      pending_token = token;
      continue;
    }
    if (token_id == ML_DATA) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_AFTER_DATA;
      pending_token = token;
      continue;
    }
    if (token_id == ML_INDEX) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_AFTER_INDEX;
      pending_token = token;
      continue;
    }
    if (token_id == ML_START) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_AFTER_START;
      pending_token = token;
      continue;
    }
    if (token_id == ML_UNION) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_AFTER_UNION;
      value_saw_equals = 0;
      pending_token = token;
      continue;
    }

    if (token_id == ML_AUTOEXTEND_SIZE || token_id == ML_AUTO_INCREMENT ||
        token_id == ML_AVG_ROW_LENGTH || token_id == ML_KEY_BLOCK_SIZE ||
        token_id == ML_MAX_ROWS || token_id == ML_MIN_ROWS) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = token_id == ML_AUTOEXTEND_SIZE
                       ? CREATE_TABLE_OPTION_VALUE_SIZE_NUMBER
                       : CREATE_TABLE_OPTION_VALUE_DECIMAL_NUMBER;
    } else if (token_id == ML_CHECKSUM || token_id == ML_DELAY_KEY_WRITE) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_DECIMAL_NUMBER;
    } else if (token_id == ML_PACK_KEYS ||
               token_id == ML_STATS_AUTO_RECALC ||
               token_id == ML_STATS_PERSISTENT) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_DEFAULT_BOOLEAN;
    } else if (token_id == ML_STATS_SAMPLE_PAGES) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_STATS_SAMPLE_PAGES;
    } else if (token_id == ML_COMMENT || token_id == ML_COMPRESSION ||
               token_id == ML_CONNECTION || token_id == ML_PASSWORD ||
               token_id == ML_ENGINE_ATTRIBUTE ||
               token_id == ML_SECONDARY_ENGINE_ATTRIBUTE) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_STRING;
    } else if (token_id == ML_ENCRYPTION) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_ENCRYPTION;
    } else if (token_id == ML_ENGINE || token_id == ML_SECONDARY_ENGINE ||
               token_id == ML_TABLESPACE) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_NAME;
    } else if (token_id == ML_CHARSET || token_id == ML_COLLATE) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_CHARSET;
    } else if (token_id == ML_INSERT_METHOD) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_INSERT_METHOD;
    } else if (token_id == ML_ROW_FORMAT) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_ROW_FORMAT;
    } else if (token_id == ML_STORAGE) {
      option_tail_without_body = !saw_definition_body;
      state = CREATE_TABLE_TAIL_EXPECT_VALUE;
      value_kind = CREATE_TABLE_OPTION_VALUE_STORAGE;
    } else {
      mylite_parser_reject(ctx, token, "invalid CREATE TABLE table option");
      return;
    }

    value_saw_equals = 0;
    pending_token = token;
  }

  if (state == CREATE_TABLE_TAIL_EXPECT_VALUE ||
      state == CREATE_TABLE_TAIL_AFTER_DEFAULT ||
      state == CREATE_TABLE_TAIL_AFTER_CHARACTER ||
      state == CREATE_TABLE_TAIL_AFTER_DATA ||
      state == CREATE_TABLE_TAIL_AFTER_INDEX ||
      state == CREATE_TABLE_TAIL_AFTER_START ||
      state == CREATE_TABLE_TAIL_AFTER_UNION ||
      state == CREATE_TABLE_TAIL_UNION_BODY ||
      state == CREATE_TABLE_TAIL_AFTER_CTAS_AS ||
      state == CREATE_TABLE_TAIL_AFTER_CTAS_MODIFIER ||
      need_option_after_comma) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE table option");
  } else if (option_tail_without_body) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE query body");
  }
}

static int create_table_tail_option_value_token(
    int kind, int token_id, MyliteToken token) {
  if (kind == CREATE_TABLE_OPTION_VALUE_DECIMAL_NUMBER) {
    return create_table_tail_option_decimal_number_token(token_id, token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_SIZE_NUMBER) {
    return create_table_tail_option_size_number_token(token_id, token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_DEFAULT_BOOLEAN) {
    return create_table_tail_option_default_boolean_token(token_id);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_STATS_SAMPLE_PAGES) {
    return create_table_tail_option_stats_sample_pages_token(token_id, token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_STRING) {
    return create_table_tail_option_string_token(token_id, token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_ENCRYPTION) {
    return create_table_tail_option_string_token(token_id, token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_NAME) {
    return dml_row_alias_token(token_id) ||
           create_table_tail_option_string_token(token_id, token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_CHARSET) {
    return column_definition_charset_name_token(token_id, token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_INSERT_METHOD) {
    return create_table_tail_option_insert_method_token(token_id);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_ROW_FORMAT) {
    return create_table_tail_option_row_format_token(token);
  }
  if (kind == CREATE_TABLE_OPTION_VALUE_STORAGE) {
    return create_table_tail_option_storage_token(token_id, token);
  }
  return 0;
}

static int create_table_tail_option_value_allows_equals(int kind) {
  return kind != CREATE_TABLE_OPTION_VALUE_STORAGE;
}

static int create_table_tail_option_number_token(int token_id) {
  return token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
         token_id == ML_NUMBER_LITERAL;
}

static int create_table_tail_option_decimal_number_token(int token_id,
                                                         MyliteToken token) {
  return create_table_tail_option_number_token(token_id) &&
         token_is_unsigned_decimal_literal(token);
}

static int create_table_tail_option_size_number_token(int token_id,
                                                      MyliteToken token) {
  return (create_table_tail_option_number_token(token_id) &&
          token_is_unsigned_size_literal(token)) ||
         (token_id == ML_STRING_LITERAL && token_is_quoted_hex_literal(token));
}

static int create_table_tail_option_default_boolean_token(int token_id) {
  return token_id == ML_BOOLEAN_NUMBER || token_id == ML_DEFAULT;
}

static int create_table_tail_option_stats_sample_pages_token(
    int token_id, MyliteToken token) {
  unsigned long value;

  if (token_id == ML_DEFAULT) {
    return 1;
  }
  if (!create_table_tail_option_number_token(token_id) ||
      token_has_leading_sign(token)) {
    return 0;
  }
  if (token_is_hex_literal(token, 0)) {
    return 1;
  }
  if (!token_is_unsigned_decimal_literal(token)) {
    return 0;
  }
  if (!token_is_plain_unsigned_integer(token, &value)) {
    return 1;
  }
  return value >= 1 && value <= 65535;
}

static int create_table_tail_option_string_token(int token_id,
                                                 MyliteToken token) {
  return !token_is_quoted_hex_or_bit_literal(token) &&
         (token_id == ML_DOUBLE_QUOTED_STRING ||
          token_id == ML_ENCRYPTION_VALUE || token_id == ML_SQLSTATE_VALUE ||
          token_id == ML_STRING_LITERAL);
}

static int create_table_tail_option_insert_method_token(int token_id) {
  return token_id == ML_FIRST || token_id == ML_LAST || token_id == ML_NO;
}

static int create_table_tail_option_row_format_token(MyliteToken token) {
  return token_ascii_equal(token, "default") ||
         token_ascii_equal(token, "fixed") ||
         token_ascii_equal(token, "dynamic") ||
         token_ascii_equal(token, "compressed") ||
         token_ascii_equal(token, "redundant") ||
         token_ascii_equal(token, "compact");
}

static int create_table_tail_option_storage_token(int token_id,
                                                  MyliteToken token) {
  return token_id == ML_MEMORY || token_ascii_equal(token, "disk");
}

static int token_is_unsigned_decimal_literal(MyliteToken token) {
  return !token_has_leading_sign(token) && token_is_decimal_literal(token, 0);
}

static int token_is_unsigned_size_literal(MyliteToken token) {
  size_t i = 0;

  if (token_has_leading_sign(token)) {
    return 0;
  }
  if (token_is_hex_literal(token, 0)) {
    return 1;
  }
  while (i < token.length && ascii_is_digit(token.start[i])) {
    i++;
  }
  if (i == 0) {
    return 0;
  }
  if (i == token.length) {
    return 1;
  }
  if (i + 1 == token.length) {
    char suffix = token.start[i];
    return suffix == 'k' || suffix == 'K' || suffix == 'm' ||
           suffix == 'M' || suffix == 'g' || suffix == 'G' ||
           suffix == 't' || suffix == 'T' || suffix == 'p' ||
           suffix == 'P';
  }
  return 0;
}

static int token_is_plain_unsigned_integer(MyliteToken token,
                                           unsigned long *value) {
  size_t i;
  unsigned long parsed = 0;

  if (token.length == 0) {
    return 0;
  }
  for (i = 0; i < token.length; i++) {
    unsigned int digit;
    if (!ascii_is_digit(token.start[i])) {
      return 0;
    }
    digit = (unsigned int)(token.start[i] - '0');
    if (parsed > 65536UL) {
      parsed = 65536UL;
    } else {
      parsed = parsed * 10UL + digit;
    }
  }
  *value = parsed;
  return 1;
}

static int token_is_quoted_hex_literal(MyliteToken token) {
  if (token.length < 3 || token.start[1] != '\'') {
    return 0;
  }

  return token.start[0] == 'X' || token.start[0] == 'x';
}

static int token_is_quoted_hex_or_bit_literal(MyliteToken token) {
  if (token_is_quoted_hex_literal(token)) {
    return 1;
  }
  if (token.length < 3 || token.start[1] != '\'') {
    return 0;
  }

  return token.start[0] == 'B' || token.start[0] == 'b';
}

static int token_has_leading_sign(MyliteToken token) {
  return token.length > 0 &&
         (token.start[0] == '+' || token.start[0] == '-');
}

static int create_table_column_name_needs_type_check(int token_id,
                                                     MyliteToken token) {
  size_t i;

  for (i = 0; i < token.length; i++) {
    if (token.start[i] == '$') {
      return 1;
    }
  }

  if ((token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
       token_id == ML_NUMBER_LITERAL) &&
      token_starts_numeric_literal(token)) {
    return 0;
  }

  return 1;
}

static int create_table_column_type_start(int token_id, MyliteToken token) {
  return token_id == ML_BINARY || token_id == ML_CHARACTER ||
         token_id == ML_DECIMAL || token_id == ML_INT ||
         token_id == ML_INTEGER || token_id == ML_JSON || token_id == ML_REAL ||
         token_id == ML_SET || token_ascii_equal(token, "bigint") ||
         token_ascii_equal(token, "bit") || token_ascii_equal(token, "blob") ||
         token_ascii_equal(token, "bool") ||
         token_ascii_equal(token, "boolean") ||
         token_ascii_equal(token, "char") || token_ascii_equal(token, "date") ||
         token_ascii_equal(token, "datetime") ||
         token_ascii_equal(token, "dec") || token_ascii_equal(token, "double") ||
         token_ascii_equal(token, "enum") || token_ascii_equal(token, "fixed") ||
         token_ascii_equal(token, "float") ||
         token_ascii_equal(token, "float4") ||
         token_ascii_equal(token, "float8") ||
         token_ascii_equal(token, "geometry") ||
         token_ascii_equal(token, "geometrycollection") ||
         token_ascii_equal(token, "geomcollection") ||
         token_ascii_equal(token, "int1") || token_ascii_equal(token, "int2") ||
         token_ascii_equal(token, "int3") || token_ascii_equal(token, "int4") ||
         token_ascii_equal(token, "int8") ||
         token_ascii_equal(token, "linestring") ||
         token_ascii_equal(token, "long") ||
         token_ascii_equal(token, "longblob") ||
         token_ascii_equal(token, "longtext") ||
         token_ascii_equal(token, "mediumblob") ||
         token_ascii_equal(token, "mediumint") ||
         token_ascii_equal(token, "mediumtext") ||
         token_ascii_equal(token, "middleint") ||
         token_ascii_equal(token, "multilinestring") ||
         token_ascii_equal(token, "multipoint") ||
         token_ascii_equal(token, "multipolygon") ||
         token_ascii_equal(token, "national") ||
         token_ascii_equal(token, "nchar") ||
         token_ascii_equal(token, "numeric") ||
         token_ascii_equal(token, "nvarchar") ||
         token_ascii_equal(token, "point") ||
         token_ascii_equal(token, "polygon") ||
         token_ascii_equal(token, "serial") ||
         token_ascii_equal(token, "smallint") ||
         token_ascii_equal(token, "text") || token_ascii_equal(token, "time") ||
         token_ascii_equal(token, "timestamp") ||
         token_ascii_equal(token, "tinyblob") ||
         token_ascii_equal(token, "tinyint") ||
         token_ascii_equal(token, "tinytext") ||
         token_ascii_equal(token, "varbinary") ||
         token_ascii_equal(token, "varchar") ||
         token_ascii_equal(token, "year");
}

static ColumnTypeParameterKind column_type_parameter_kind(int token_id,
                                                          MyliteToken token) {
  if (token_id == ML_SET || token_ascii_equal(token, "enum")) {
    return COLUMN_TYPE_PARAMETER_STRING_LIST;
  }

  if (token_ascii_equal(token, "year")) {
    return COLUMN_TYPE_PARAMETER_YEAR;
  }

  if (column_type_real_family_token(token_id, token)) {
    return COLUMN_TYPE_PARAMETER_NUMERIC_TWO;
  }

  if (token_id == ML_DECIMAL || token_ascii_equal(token, "dec") ||
      token_ascii_equal(token, "fixed") ||
      token_ascii_equal(token, "float") || token_ascii_equal(token, "float4") ||
      token_ascii_equal(token, "float8") ||
      token_ascii_equal(token, "numeric")) {
    return COLUMN_TYPE_PARAMETER_NUMERIC_ONE_OR_TWO;
  }

  if (token_id == ML_BINARY || token_id == ML_CHARACTER ||
      token_id == ML_INT || token_id == ML_INTEGER ||
      token_ascii_equal(token, "bigint") || token_ascii_equal(token, "bit") ||
      token_ascii_equal(token, "blob") || token_ascii_equal(token, "char") ||
      token_ascii_equal(token, "datetime") ||
      token_ascii_equal(token, "int1") || token_ascii_equal(token, "int2") ||
      token_ascii_equal(token, "int3") || token_ascii_equal(token, "int4") ||
      token_ascii_equal(token, "int8") || token_ascii_equal(token, "long") ||
      token_ascii_equal(token, "longblob") ||
      token_ascii_equal(token, "longtext") ||
      token_ascii_equal(token, "mediumblob") ||
      token_ascii_equal(token, "mediumint") ||
      token_ascii_equal(token, "mediumtext") ||
      token_ascii_equal(token, "nchar") ||
      token_ascii_equal(token, "nvarchar") ||
      token_ascii_equal(token, "smallint") ||
      token_ascii_equal(token, "text") || token_ascii_equal(token, "time") ||
      token_ascii_equal(token, "timestamp") ||
      token_ascii_equal(token, "tinyblob") ||
      token_ascii_equal(token, "tinyint") ||
      token_ascii_equal(token, "tinytext") ||
      token_ascii_equal(token, "varbinary") ||
      token_ascii_equal(token, "varchar") ||
      token_ascii_equal(token, "varying") ||
      token_ascii_equal(token, "year")) {
    return COLUMN_TYPE_PARAMETER_NUMERIC_ONE;
  }

  return COLUMN_TYPE_PARAMETER_NONE;
}

static ColumnTypeState column_type_state_after_start(int token_id,
                                                     MyliteToken token) {
  (void) token_id;
  if (token_ascii_equal(token, "long")) {
    return COLUMN_TYPE_STATE_LONG;
  }
  if (token_ascii_equal(token, "national")) {
    return COLUMN_TYPE_STATE_NATIONAL;
  }
  if (token_ascii_equal(token, "nchar")) {
    return COLUMN_TYPE_STATE_NCHAR;
  }
  if (column_type_character_token(token_id, token)) {
    return COLUMN_TYPE_STATE_CHAR;
  }
  return COLUMN_TYPE_STATE_COMPLETE;
}

static int column_type_modifier_mask_after_start(int token_id,
                                                 MyliteToken token) {
  if (column_type_integer_family_token(token_id, token) ||
      column_type_real_family_token(token_id, token) ||
      column_type_numeric_family_token(token_id, token) ||
      token_ascii_equal(token, "year")) {
    return COLUMN_TYPE_MODIFIER_NUMERIC;
  }
  if (column_type_national_family_token(token_id, token)) {
    return COLUMN_TYPE_MODIFIER_NATIONAL_BINARY;
  }
  if (column_type_character_family_token(token_id, token) ||
      column_type_text_family_token(token_id, token) ||
      token_id == ML_SET || token_ascii_equal(token, "enum") ||
      token_ascii_equal(token, "long")) {
    return COLUMN_TYPE_MODIFIER_CHARSET;
  }
  return 0;
}

static int column_type_incomplete(ColumnTypeState state) {
  return state == COLUMN_TYPE_STATE_NATIONAL ||
         state == COLUMN_TYPE_STATE_LONG_CHAR;
}

static int consume_column_type_tail_token_if_pending(
    MyliteParseContext *ctx, int token_id, MyliteToken token,
    ColumnTypeState *state, int *modifier_mask, int *parameter_pending,
    int *parameter_required, int *parameter_forbidden,
    ColumnTypeParameterKind *parameter_kind,
    ColumnTypeCharsetModifierState *charset_state, const char *message,
    int *consumed) {
  ColumnTypeState previous_state = *state;
  *consumed = 0;

  if (column_type_numeric_modifier_token(token_id, token)) {
    if ((*modifier_mask & COLUMN_TYPE_MODIFIER_NUMERIC) == 0) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    *state = COLUMN_TYPE_STATE_COMPLETE;
    *consumed = 1;
    return 1;
  }

  if (column_type_simple_charset_modifier_token(token_id, token)) {
    if ((*modifier_mask & COLUMN_TYPE_MODIFIER_CHARSET) != 0) {
      if (token_id == ML_BINARY) {
        if (*charset_state == COLUMN_TYPE_CHARSET_AVAILABLE) {
          *charset_state = COLUMN_TYPE_CHARSET_BINARY_PREFIX;
        } else if (*charset_state ==
                       COLUMN_TYPE_CHARSET_ASCII_OR_UNICODE ||
                   *charset_state == COLUMN_TYPE_CHARSET_CHARACTER_SET) {
          *charset_state = COLUMN_TYPE_CHARSET_DONE;
          *modifier_mask = 0;
        } else {
          mylite_parser_reject(ctx, token, message);
          return 0;
        }
      } else if (column_type_ascii_or_unicode_modifier_token(token_id,
                                                             token)) {
        if (*charset_state == COLUMN_TYPE_CHARSET_AVAILABLE) {
          *charset_state = COLUMN_TYPE_CHARSET_ASCII_OR_UNICODE;
        } else if (*charset_state == COLUMN_TYPE_CHARSET_BINARY_PREFIX) {
          *charset_state = COLUMN_TYPE_CHARSET_DONE;
          *modifier_mask = 0;
        } else {
          mylite_parser_reject(ctx, token, message);
          return 0;
        }
      } else if (*charset_state == COLUMN_TYPE_CHARSET_AVAILABLE) {
        *charset_state = COLUMN_TYPE_CHARSET_DONE;
        *modifier_mask = 0;
      } else {
        mylite_parser_reject(ctx, token, message);
        return 0;
      }
      *state = COLUMN_TYPE_STATE_COMPLETE;
      *consumed = 1;
      return 1;
    }
    if ((*modifier_mask & COLUMN_TYPE_MODIFIER_NATIONAL_BINARY) != 0 &&
        token_id == ML_BINARY) {
      *state = COLUMN_TYPE_STATE_COMPLETE;
      *modifier_mask = 0;
      *charset_state = COLUMN_TYPE_CHARSET_DONE;
      *consumed = 1;
      return 1;
    }
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (column_type_varying_token(token_id, token)) {
    if (previous_state != COLUMN_TYPE_STATE_CHAR &&
        previous_state != COLUMN_TYPE_STATE_NCHAR &&
        previous_state != COLUMN_TYPE_STATE_NATIONAL_CHAR &&
        previous_state != COLUMN_TYPE_STATE_LONG_CHAR) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    *state = COLUMN_TYPE_STATE_COMPLETE;
    *modifier_mask =
        previous_state == COLUMN_TYPE_STATE_LONG_CHAR
            ? COLUMN_TYPE_MODIFIER_CHARSET
            : previous_state == COLUMN_TYPE_STATE_NCHAR ||
                      previous_state == COLUMN_TYPE_STATE_NATIONAL_CHAR
                  ? COLUMN_TYPE_MODIFIER_NATIONAL_BINARY
                  : COLUMN_TYPE_MODIFIER_CHARSET;
    *charset_state = COLUMN_TYPE_CHARSET_AVAILABLE;
    *parameter_pending = 1;
    *parameter_required = previous_state != COLUMN_TYPE_STATE_LONG_CHAR;
    *parameter_forbidden = previous_state == COLUMN_TYPE_STATE_LONG_CHAR;
    *parameter_kind = COLUMN_TYPE_PARAMETER_NUMERIC_ONE;
    *consumed = 1;
    return 1;
  }

  if (token_id == ML_CHARACTER &&
      (previous_state == COLUMN_TYPE_STATE_LONG ||
       previous_state == COLUMN_TYPE_STATE_NATIONAL)) {
    *state = previous_state == COLUMN_TYPE_STATE_LONG
                 ? COLUMN_TYPE_STATE_LONG_CHAR
                 : COLUMN_TYPE_STATE_NATIONAL_CHAR;
    *modifier_mask = previous_state == COLUMN_TYPE_STATE_NATIONAL
                         ? COLUMN_TYPE_MODIFIER_NATIONAL_BINARY
                         : 0;
    *charset_state = COLUMN_TYPE_CHARSET_AVAILABLE;
    if (previous_state == COLUMN_TYPE_STATE_NATIONAL) {
      *parameter_pending = 1;
      *parameter_required = 0;
      *parameter_forbidden = 0;
      *parameter_kind = COLUMN_TYPE_PARAMETER_NUMERIC_ONE;
    }
    *consumed = 1;
    return 1;
  }

  if (column_type_charset_introducer_token(token_id, token)) {
    if ((*modifier_mask & COLUMN_TYPE_MODIFIER_CHARSET) == 0) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    if (*charset_state == COLUMN_TYPE_CHARSET_AVAILABLE) {
      *charset_state = COLUMN_TYPE_CHARSET_CHARACTER_SET;
    } else if (*charset_state == COLUMN_TYPE_CHARSET_BINARY_PREFIX) {
      *charset_state = COLUMN_TYPE_CHARSET_DONE;
      *modifier_mask = 0;
    } else {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    *state = COLUMN_TYPE_STATE_COMPLETE;
    return 1;
  }

  if (create_table_column_type_start(token_id, token)) {
    if (column_definition_serial_attribute_token(token_id, token)) {
      return 1;
    }
    if (previous_state == COLUMN_TYPE_STATE_LONG) {
      if (column_type_varchar_token(token_id, token)) {
        *modifier_mask = COLUMN_TYPE_MODIFIER_CHARSET;
        *charset_state = COLUMN_TYPE_CHARSET_AVAILABLE;
      } else if (column_type_varbinary_token(token_id, token)) {
        *modifier_mask = 0;
        *charset_state = COLUMN_TYPE_CHARSET_DONE;
      } else if (column_type_character_token(token_id, token)) {
        *state = COLUMN_TYPE_STATE_LONG_CHAR;
        *modifier_mask = 0;
        *charset_state = COLUMN_TYPE_CHARSET_AVAILABLE;
        *consumed = 1;
        return 1;
      } else {
        mylite_parser_reject(ctx, token, message);
        return 0;
      }
      *state = COLUMN_TYPE_STATE_COMPLETE;
      *parameter_pending = 1;
      *parameter_required = 0;
      *parameter_forbidden = 1;
      *parameter_kind = COLUMN_TYPE_PARAMETER_NUMERIC_ONE;
      *consumed = 1;
      return 1;
    }

    if (previous_state == COLUMN_TYPE_STATE_NATIONAL) {
      if (column_type_varchar_token(token_id, token)) {
        *state = COLUMN_TYPE_STATE_COMPLETE;
        *modifier_mask = COLUMN_TYPE_MODIFIER_NATIONAL_BINARY;
        *charset_state = COLUMN_TYPE_CHARSET_AVAILABLE;
        *parameter_pending = 1;
        *parameter_required = 1;
        *parameter_forbidden = 0;
        *parameter_kind = COLUMN_TYPE_PARAMETER_NUMERIC_ONE;
        *consumed = 1;
        return 1;
      }
      if (column_type_character_token(token_id, token)) {
        *state = COLUMN_TYPE_STATE_NATIONAL_CHAR;
        *modifier_mask = COLUMN_TYPE_MODIFIER_NATIONAL_BINARY;
        *charset_state = COLUMN_TYPE_CHARSET_AVAILABLE;
        *parameter_pending = 1;
        *parameter_required = 0;
        *parameter_forbidden = 0;
        *parameter_kind = COLUMN_TYPE_PARAMETER_NUMERIC_ONE;
        *consumed = 1;
        return 1;
      }
      mylite_parser_reject(ctx, token, message);
      return 0;
    }

    if (previous_state == COLUMN_TYPE_STATE_NCHAR &&
        column_type_varchar_token(token_id, token)) {
      *state = COLUMN_TYPE_STATE_COMPLETE;
      *modifier_mask = COLUMN_TYPE_MODIFIER_NATIONAL_BINARY;
      *charset_state = COLUMN_TYPE_CHARSET_AVAILABLE;
      *parameter_pending = 1;
      *parameter_required = 1;
      *parameter_forbidden = 0;
      *parameter_kind = COLUMN_TYPE_PARAMETER_NUMERIC_ONE;
      *consumed = 1;
      return 1;
    }

    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (column_definition_type_modifier(token_id, token)) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  return 1;
}

static int column_type_numeric_modifier_token(int token_id,
                                              MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "signed") ||
         token_ascii_equal(token, "unsigned") ||
         token_ascii_equal(token, "zerofill");
}

static int column_type_simple_charset_modifier_token(int token_id,
                                                     MyliteToken token) {
  return token_id == ML_BINARY || token_ascii_equal(token, "ascii") ||
         token_ascii_equal(token, "byte") ||
         token_ascii_equal(token, "unicode");
}

static int column_type_ascii_or_unicode_modifier_token(int token_id,
                                                       MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "ascii") ||
         token_ascii_equal(token, "unicode");
}

static int column_type_charset_introducer_token(int token_id,
                                                MyliteToken token) {
  (void) token;
  return token_id == ML_CHARACTER || token_id == ML_CHARSET;
}

static int column_type_varying_token(int token_id, MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "varying");
}

static int column_type_character_token(int token_id, MyliteToken token) {
  return token_id == ML_CHARACTER || token_ascii_equal(token, "char");
}

static int column_type_varchar_token(int token_id, MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "varchar");
}

static int column_type_varbinary_token(int token_id, MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "varbinary");
}

static int column_definition_serial_attribute_token(int token_id,
                                                    MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "serial");
}

static int column_definition_engine_attribute_token(int token_id) {
  return token_id == ML_ENGINE_ATTRIBUTE ||
         token_id == ML_SECONDARY_ENGINE_ATTRIBUTE;
}

static int column_definition_json_attribute_token(int token_id) {
  return token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_STRING_LITERAL;
}

static int column_type_integer_family_token(int token_id, MyliteToken token) {
  return token_id == ML_INT || token_id == ML_INTEGER ||
         token_ascii_equal(token, "bigint") ||
         token_ascii_equal(token, "int1") ||
         token_ascii_equal(token, "int2") ||
         token_ascii_equal(token, "int3") ||
         token_ascii_equal(token, "int4") ||
         token_ascii_equal(token, "int8") ||
         token_ascii_equal(token, "mediumint") ||
         token_ascii_equal(token, "middleint") ||
         token_ascii_equal(token, "smallint") ||
         token_ascii_equal(token, "tinyint");
}

static int column_type_real_family_token(int token_id, MyliteToken token) {
  return token_id == ML_REAL || token_ascii_equal(token, "double");
}

static int column_type_numeric_family_token(int token_id, MyliteToken token) {
  return token_id == ML_DECIMAL || token_ascii_equal(token, "dec") ||
         token_ascii_equal(token, "fixed") ||
         token_ascii_equal(token, "float") ||
         token_ascii_equal(token, "float4") ||
         token_ascii_equal(token, "float8") ||
         token_ascii_equal(token, "numeric");
}

static int column_type_character_family_token(int token_id,
                                              MyliteToken token) {
  return column_type_character_token(token_id, token) ||
         column_type_varchar_token(token_id, token);
}

static int column_type_national_family_token(int token_id,
                                             MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "nchar") ||
         token_ascii_equal(token, "nvarchar");
}

static int column_type_text_family_token(int token_id, MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "longtext") ||
         token_ascii_equal(token, "mediumtext") ||
         token_ascii_equal(token, "text") ||
         token_ascii_equal(token, "tinytext");
}

static int column_type_blob_family_parameter_forbidden(int token_id,
                                                       MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "longblob") ||
         token_ascii_equal(token, "longtext") ||
         token_ascii_equal(token, "mediumblob") ||
         token_ascii_equal(token, "mediumtext") ||
         token_ascii_equal(token, "tinyblob") ||
         token_ascii_equal(token, "tinytext");
}

static int column_type_numeric_parameter_token(int token_id,
                                               MyliteToken token) {
  size_t i;
  int saw_digit = 0;
  int saw_dot = 0;

  if (!create_table_tail_option_number_token(token_id)) {
    return 0;
  }

  for (i = 0; i < token.length; i++) {
    char ch = token.start[i];
    if (ch >= '0' && ch <= '9') {
      saw_digit = 1;
      continue;
    }
    if (ch == '.' && !saw_dot) {
      saw_dot = 1;
      continue;
    }
    return 0;
  }

  return saw_digit;
}

static int column_type_integer_parameter_token(int token_id,
                                               MyliteToken token) {
  size_t i;

  if (!column_type_numeric_parameter_token(token_id, token)) {
    return 0;
  }

  for (i = 0; i < token.length; i++) {
    if (token.start[i] == '.') {
      return 0;
    }
  }

  return 1;
}

static int column_type_year_length_token(int token_id, MyliteToken token) {
  size_t i = 0;
  unsigned int value = 0;

  if (!column_type_numeric_parameter_token(token_id, token)) {
    return 0;
  }

  while (i < token.length && token.start[i] >= '0' &&
         token.start[i] <= '9') {
    value = value * 10 + (unsigned int) (token.start[i] - '0');
    i++;
  }

  return value == 4;
}

static int column_type_string_parameter_token(int token_id,
                                              MyliteToken token) {
  return token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_ENCRYPTION_VALUE || token_id == ML_SQLSTATE_VALUE ||
         token_id == ML_STRING_LITERAL ||
         do_expression_hex_or_bit_literal_token(token);
}

static int validate_column_type_parameter_list(
    MyliteParseContext *ctx, MyliteLexer *lexer, MyliteToken start,
    ColumnTypeParameterKind kind, const char *message) {
  enum {
    COLUMN_TYPE_PARAMETER_NEED_VALUE,
    COLUMN_TYPE_PARAMETER_AFTER_FIRST_VALUE,
    COLUMN_TYPE_PARAMETER_NEED_SECOND_VALUE,
    COLUMN_TYPE_PARAMETER_AFTER_SECOND_VALUE
  };
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int state = COLUMN_TYPE_PARAMETER_NEED_VALUE;
  int first_numeric_parameter_integer = 0;

  while ((token_id = mylite_lexer_next(lexer, &token)) > 0) {
    if (token_id == ML_RP) {
      if (state == COLUMN_TYPE_PARAMETER_AFTER_SECOND_VALUE ||
          (state == COLUMN_TYPE_PARAMETER_AFTER_FIRST_VALUE &&
           kind != COLUMN_TYPE_PARAMETER_NUMERIC_TWO)) {
        return 1;
      }
      mylite_parser_reject(ctx, pending_token, message);
      return 0;
    }

    if (token_id == ML_COMMA) {
      if (kind == COLUMN_TYPE_PARAMETER_STRING_LIST &&
          (state == COLUMN_TYPE_PARAMETER_AFTER_FIRST_VALUE ||
           state == COLUMN_TYPE_PARAMETER_AFTER_SECOND_VALUE)) {
        state = COLUMN_TYPE_PARAMETER_NEED_SECOND_VALUE;
        pending_token = token;
        continue;
      }
      if (state != COLUMN_TYPE_PARAMETER_AFTER_FIRST_VALUE ||
          kind == COLUMN_TYPE_PARAMETER_NUMERIC_ONE ||
          kind == COLUMN_TYPE_PARAMETER_YEAR) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      if (kind == COLUMN_TYPE_PARAMETER_NUMERIC_ONE_OR_TWO &&
          !first_numeric_parameter_integer) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      state = COLUMN_TYPE_PARAMETER_NEED_SECOND_VALUE;
      pending_token = token;
      continue;
    }

    if (state == COLUMN_TYPE_PARAMETER_NEED_VALUE ||
        state == COLUMN_TYPE_PARAMETER_NEED_SECOND_VALUE) {
      if (kind == COLUMN_TYPE_PARAMETER_STRING_LIST) {
        if (!column_type_string_parameter_token(token_id, token)) {
          mylite_parser_reject(ctx, pending_token, message);
          return 0;
        }
      } else if (kind == COLUMN_TYPE_PARAMETER_YEAR) {
        if (!column_type_year_length_token(token_id, token)) {
          mylite_parser_reject(ctx, pending_token, message);
          return 0;
        }
      } else if (kind == COLUMN_TYPE_PARAMETER_NUMERIC_TWO ||
                 state == COLUMN_TYPE_PARAMETER_NEED_SECOND_VALUE) {
        if (!column_type_integer_parameter_token(token_id, token)) {
          mylite_parser_reject(ctx, pending_token, message);
          return 0;
        }
      } else if (!column_type_numeric_parameter_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      if (state == COLUMN_TYPE_PARAMETER_NEED_VALUE &&
          kind != COLUMN_TYPE_PARAMETER_STRING_LIST) {
        first_numeric_parameter_integer =
            column_type_integer_parameter_token(token_id, token);
      }
      state = state == COLUMN_TYPE_PARAMETER_NEED_VALUE
                  ? COLUMN_TYPE_PARAMETER_AFTER_FIRST_VALUE
                  : COLUMN_TYPE_PARAMETER_AFTER_SECOND_VALUE;
      continue;
    }

    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  mylite_parser_reject(ctx, pending_token, message);
  return 0;
}

static int consume_column_type_parameter_list_if_pending(
    MyliteParseContext *ctx, MyliteLexer *lexer, int token_id,
    MyliteToken token, int *pending, int *required,
    ColumnTypeParameterKind kind, const char *message, int *consumed) {
  *consumed = 0;
  if (!*pending) {
    return 1;
  }

  if (*required && token_id != ML_LP) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  *pending = 0;
  *required = 0;
  if (token_id != ML_LP) {
    return 1;
  }

  *consumed = 1;
  if (kind == COLUMN_TYPE_PARAMETER_NONE) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }
  return validate_column_type_parameter_list(ctx, lexer, token, kind, message);
}

static int consume_column_precision_modifier_if_pending(
    MyliteParseContext *ctx, int token_id, MyliteToken token, int *pending,
    int *parameter_pending, int *parameter_required,
    ColumnTypeParameterKind *parameter_kind, const char *message,
    int *consumed) {
  *consumed = 0;
  if (!column_type_precision_modifier_token(token_id, token)) {
    return 1;
  }
  if (!*pending) {
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  *consumed = 1;
  *pending = 0;
  *parameter_pending = 1;
  *parameter_required = 0;
  *parameter_kind = COLUMN_TYPE_PARAMETER_NUMERIC_TWO;
  return 1;
}

static int column_definition_tail_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token,
    ColumnDefinitionTailState *state, int *depth, int *check_pending,
    MyliteToken *pending_token, int *flags, int allow_position,
    const char *message) {
  if (token_id == ML_LP) {
    if (*state == COLUMN_DEFINITION_TAIL_READY ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_COMMENT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_COLLATE ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CHARACTER ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CHARSET ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_STORAGE ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_SRID ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE_EQUALS ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_AFTER ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_ON ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE_DOT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_SERIAL_ATTRIBUTE ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_SERIAL_DEFAULT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_GENERATED ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_ALWAYS) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    if (*state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_LIST_NEED_NAME;
      *pending_token = token;
      return 1;
    }
    if (*state == COLUMN_DEFINITION_TAIL_AFTER_AS) {
      *flags |= COLUMN_DEFINITION_FLAG_GENERATED_EXPRESSION;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    (*depth)++;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_NOT) {
    if (token_id != ML_NULL && !token_ascii_equal(token, "secondary")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_PRIMARY) {
    if (token_id != ML_KEY) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT) {
    if (token_id == ML_MINUS || token_is_plus(token)) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN;
      *pending_token = token;
      return 1;
    }
    if (token_id == ML_DOT) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_VALUE_DOT;
      *pending_token = token;
      return 1;
    }
    if (column_definition_default_introducer_token(token_id, token)) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_INTRODUCER;
      *pending_token = token;
      return 1;
    }
    if (!column_definition_value_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE) {
    if (column_definition_attribute_start(token_id, token, allow_position)) {
      *state = COLUMN_DEFINITION_TAIL_READY;
    } else {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_INTRODUCER) {
    if (!column_definition_value_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN) {
    if (token_id != ML_BOOLEAN_NUMBER && token_id != ML_FACTOR_NUMBER &&
        token_id != ML_NUMBER_LITERAL) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_VALUE_DOT) {
    if (token_id != ML_BOOLEAN_NUMBER && token_id != ML_FACTOR_NUMBER &&
        token_id != ML_NUMBER_LITERAL) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_COMMENT) {
    if (!create_table_tail_option_string_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_COLLATE ||
      *state == COLUMN_DEFINITION_TAIL_AFTER_CHARSET) {
    if (!column_definition_charset_name_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    if (*state == COLUMN_DEFINITION_TAIL_AFTER_COLLATE) {
      *flags |= COLUMN_DEFINITION_FLAG_COLLATE;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_CHARACTER) {
    if (token_ascii_equal(token, "varying")) {
      *state = COLUMN_DEFINITION_TAIL_READY;
      return 1;
    }
    if (token_id != ML_SET) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_CHARSET;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT) {
    if (token_id != ML_DEFAULT && !token_ascii_equal(token, "dynamic") &&
        !token_ascii_equal(token, "fixed")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_STORAGE) {
    if (token_id != ML_DEFAULT && token_id != ML_MEMORY &&
        !token_ascii_equal(token, "disk")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_SRID) {
    if (!column_type_integer_parameter_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE) {
    if (token_id == ML_EQUALS) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE_EQUALS;
      *pending_token = token;
      return 1;
    }
    if (!column_definition_json_attribute_token(token_id)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE_EQUALS) {
    if (!column_definition_json_attribute_token(token_id)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_AFTER ||
      *state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES) {
    if (!dml_row_alias_token(token_id)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    if (*state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE;
    } else {
      *state = COLUMN_DEFINITION_TAIL_READY;
    }
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE) {
    if (token_id == ML_DOT) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE_DOT;
      *pending_token = token;
      return 1;
    }
    if (token_ascii_equal(token, "match")) {
      if ((*flags & (REFERENCE_TAIL_FLAG_MATCH |
                     REFERENCE_TAIL_FLAG_ON_UPDATE |
                     REFERENCE_TAIL_FLAG_ON_DELETE)) != 0) {
        mylite_parser_reject(ctx, token, message);
        return 0;
      }
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_MATCH;
      *pending_token = token;
      return 1;
    }
    if (token_id == ML_ON) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_ON;
      *pending_token = token;
      return 1;
    }
    mylite_parser_reject(ctx, *pending_token, message);
    return 0;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE_DOT) {
    if (!dml_row_alias_token(token_id)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_LIST_NEED_NAME) {
    if (!dml_row_alias_token(token_id)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_NAME;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_NAME) {
    if (token_id == ML_DOT) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_DOT;
      *pending_token = token;
      return 1;
    }
    if (token_id == ML_COMMA) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_LIST_NEED_NAME;
      *pending_token = token;
      return 1;
    }
    if (token_id == ML_RP) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST;
      return 1;
    }
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_DOT) {
    if (!dml_row_alias_token(token_id)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_NAME;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST) {
    if (token_ascii_equal(token, "match")) {
      if ((*flags & (REFERENCE_TAIL_FLAG_MATCH |
                     REFERENCE_TAIL_FLAG_ON_UPDATE |
                     REFERENCE_TAIL_FLAG_ON_DELETE)) != 0) {
        mylite_parser_reject(ctx, token, message);
        return 0;
      }
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_MATCH;
      *pending_token = token;
      return 1;
    }
    if (token_id == ML_ON) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_ON;
      *pending_token = token;
      return 1;
    }
    mylite_parser_reject(ctx, token, message);
    return 0;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_MATCH) {
    if (!foreign_key_match_option(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *flags |= REFERENCE_TAIL_FLAG_MATCH;
    *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_ON) {
    if (token_id != ML_DELETE && token_id != ML_UPDATE) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    if (token_id == ML_DELETE) {
      if ((*flags & REFERENCE_TAIL_FLAG_ON_DELETE) != 0) {
        mylite_parser_reject(ctx, token, message);
        return 0;
      }
      *flags |= REFERENCE_TAIL_FLAG_ON_DELETE;
    } else {
      if ((*flags & REFERENCE_TAIL_FLAG_ON_UPDATE) != 0) {
        mylite_parser_reject(ctx, token, message);
        return 0;
      }
      *flags |= REFERENCE_TAIL_FLAG_ON_UPDATE;
    }
    *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_ON_ACTION;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_ON_ACTION) {
    if (foreign_key_reference_action_token(token_id)) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST;
      return 1;
    }
    if (token_id == ML_SET) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_SET;
      *pending_token = token;
      return 1;
    }
    if (token_id == ML_NO) {
      *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_NO;
      *pending_token = token;
      return 1;
    }
    mylite_parser_reject(ctx, *pending_token, message);
    return 0;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_SET) {
    if (token_id != ML_DEFAULT && token_id != ML_NULL) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_NO) {
    if (!token_ascii_equal(token, "action")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ON) {
    if (token_id != ML_UPDATE) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE) {
    if (!column_definition_on_update_value_token(token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT) {
    if (token_id == ML_CHECK) {
      *check_pending = 1;
      *state = COLUMN_DEFINITION_TAIL_READY;
      *pending_token = token;
      return 1;
    }
    if (dml_row_alias_token(token_id)) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME;
    } else {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME) {
    if (token_id == ML_CHECK) {
      *check_pending = 1;
      *state = COLUMN_DEFINITION_TAIL_READY;
      *pending_token = token;
      return 1;
    }
    mylite_parser_reject(ctx, *pending_token, message);
    return 0;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_SERIAL_ATTRIBUTE) {
    if (token_id != ML_DEFAULT) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_SERIAL_DEFAULT;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_SERIAL_DEFAULT) {
    if (token_id != ML_VALUE) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_GENERATED) {
    if (!token_ascii_equal(token, "always")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_ALWAYS;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ALWAYS) {
    if (token_id != ML_AS) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_AS;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_AS) {
    mylite_parser_reject(ctx, *pending_token, message);
    return 0;
  }

  if (token_id == ML_CHECK) {
    *check_pending = 1;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_NOT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_NOT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_PRIMARY) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_PRIMARY;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_DEFAULT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_DEFAULT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_COMMENT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_COMMENT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_COLLATE) {
    if ((*flags & COLUMN_DEFINITION_FLAG_COLLATE) != 0) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_COLLATE;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_CHARACTER) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_CHARACTER;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_CHARSET) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_CHARSET;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "column_format")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_STORAGE) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_STORAGE;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "srid")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_SRID;
    *pending_token = token;
    return 1;
  }

  if (column_definition_engine_attribute_token(token_id)) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_ENGINE_ATTRIBUTE;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_AFTER) {
    if (!allow_position) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_AFTER;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_ON) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_ON;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "references")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_REFERENCES;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_CONSTRAINT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT;
    *pending_token = token;
    return 1;
  }

  if (column_definition_serial_attribute_token(token_id, token)) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_SERIAL_ATTRIBUTE;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "generated")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_GENERATED;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_AS) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_AS;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "stored") || token_ascii_equal(token, "virtual")) {
    if ((*flags & COLUMN_DEFINITION_FLAG_GENERATED_EXPRESSION) == 0) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    if ((*flags & COLUMN_DEFINITION_FLAG_GENERATED_STORAGE) != 0) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    *flags |= COLUMN_DEFINITION_FLAG_GENERATED_STORAGE;
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (column_definition_type_modifier(token_id, token) ||
      create_table_column_type_start(token_id, token) ||
      token_id == ML_AUTO_INCREMENT ||
      (allow_position && token_id == ML_FIRST) ||
      token_id == ML_INVISIBLE || token_id == ML_KEY || token_id == ML_NULL ||
      token_id == ML_UNIQUE || token_id == ML_VISIBLE) {
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  mylite_parser_reject(ctx, token, message);
  return 0;
}

static int column_definition_tail_complete(ColumnDefinitionTailState state) {
  return state == COLUMN_DEFINITION_TAIL_READY ||
         state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE ||
         state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE ||
         state == COLUMN_DEFINITION_TAIL_REFERENCES_AFTER_LIST;
}

static int column_definition_tail_wants_boundary_token(
    ColumnDefinitionTailState state, int token_id) {
  if (token_id == ML_LP &&
      state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES_TABLE) {
    return 1;
  }
  if (token_id == ML_COMMA || token_id == ML_RP) {
    return state == COLUMN_DEFINITION_TAIL_REFERENCES_LIST_NEED_NAME ||
           state == COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_NAME ||
           state == COLUMN_DEFINITION_TAIL_REFERENCES_LIST_AFTER_DOT;
  }
  return 0;
}

static int column_definition_tail_parenthesized_expression(
    ColumnDefinitionTailState state) {
  return state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT ||
         state == COLUMN_DEFINITION_TAIL_AFTER_AS;
}

static void column_definition_tail_finish_parenthesized_expression(
    ColumnDefinitionTailState *state, int *flags) {
  if (*state == COLUMN_DEFINITION_TAIL_AFTER_AS) {
    *flags |= COLUMN_DEFINITION_FLAG_GENERATED_EXPRESSION;
  }
  *state = COLUMN_DEFINITION_TAIL_READY;
}

static int column_definition_value_token(int token_id, MyliteToken token) {
  return token_id == ML_ATOM || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_ENCRYPTION_VALUE || token_id == ML_FACTOR_NUMBER ||
         token_id == ML_LABEL ||
         token_id == ML_NULL || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID || token_id == ML_SQLSTATE_VALUE ||
         token_id == ML_STRING_LITERAL ||
         token_ascii_equal(token, "false") || token_ascii_equal(token, "true");
}

static int column_definition_default_introducer_token(int token_id,
                                                     MyliteToken token) {
  return token_id == ML_ATOM && token.length > 1 && token.start[0] == '_';
}

static int column_definition_on_update_value_token(MyliteToken token) {
  return column_definition_temporal_function_token(token);
}

static int column_definition_temporal_function_token(MyliteToken token) {
  return token_ascii_equal(token, "current_timestamp") ||
         token_ascii_equal(token, "localtime") ||
         token_ascii_equal(token, "localtimestamp") ||
         token_ascii_equal(token, "now");
}

static int validate_column_temporal_precision_list(MyliteParseContext *ctx,
                                                   MyliteLexer *lexer,
                                                   MyliteToken start,
                                                   const char *message) {
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id = mylite_lexer_next(lexer, &token);

  if (token_id == ML_RP) {
    return 1;
  }
  if (!create_table_tail_option_number_token(token_id)) {
    mylite_parser_reject(ctx, pending_token, message);
    return 0;
  }

  pending_token = token;
  token_id = mylite_lexer_next(lexer, &token);
  if (token_id != ML_RP) {
    mylite_parser_reject(ctx, pending_token, message);
    return 0;
  }
  return 1;
}

static int column_definition_charset_name_token(int token_id,
                                                MyliteToken token) {
  return token_id == ML_BINARY ||
         token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_STRING_LITERAL ||
         (token_id != ML_DEFAULT && dml_row_alias_token(token_id)) ||
         token_ascii_equal(token, "ascii") ||
         token_ascii_equal(token, "unicode");
}

static int column_definition_attribute_start(int token_id, MyliteToken token,
                                             int allow_position) {
  return (allow_position && token_id == ML_AFTER) || token_id == ML_AS ||
         token_id == ML_AUTO_INCREMENT || token_id == ML_CHARACTER ||
         token_id == ML_CHARSET || token_id == ML_CHECK ||
         token_id == ML_COLLATE || token_id == ML_COMMENT ||
         token_id == ML_CONSTRAINT || token_id == ML_DEFAULT ||
         column_definition_engine_attribute_token(token_id) ||
         (allow_position && token_id == ML_FIRST) || token_id == ML_INVISIBLE ||
         token_id == ML_KEY || token_id == ML_NOT || token_id == ML_NULL ||
         token_id == ML_ON || token_id == ML_PRIMARY ||
         token_id == ML_STORAGE || token_id == ML_UNIQUE ||
         token_id == ML_VISIBLE ||
         token_ascii_equal(token, "column_format") ||
         token_ascii_equal(token, "generated") ||
         token_ascii_equal(token, "references") ||
         column_definition_serial_attribute_token(token_id, token) ||
         token_ascii_equal(token, "srid") || token_ascii_equal(token, "stored") ||
         token_ascii_equal(token, "virtual");
}

static int column_definition_type_modifier(int token_id, MyliteToken token) {
  return token_id == ML_BINARY || token_id == ML_VALUE ||
         token_ascii_equal(token, "ascii") || token_ascii_equal(token, "byte") ||
         token_ascii_equal(token, "signed") ||
         token_ascii_equal(token, "unicode") ||
         token_ascii_equal(token, "unsigned") ||
         token_ascii_equal(token, "varying") ||
         token_ascii_equal(token, "zerofill");
}

static int column_type_allows_precision_modifier(int token_id,
                                                 MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "double");
}

static int column_type_precision_modifier_token(int token_id,
                                                MyliteToken token) {
  (void) token_id;
  return token_ascii_equal(token, "precision");
}

static int column_type_requires_parameter(int token_id, MyliteToken token,
                                          int long_prefix) {
  if (long_prefix) {
    return 0;
  }

  return token_id == ML_SET || token_ascii_equal(token, "enum") ||
         token_ascii_equal(token, "nvarchar") ||
         token_ascii_equal(token, "varbinary") ||
         token_ascii_equal(token, "varchar") ||
         token_ascii_equal(token, "varying");
}

static int column_type_forbids_parameter(int token_id, MyliteToken token,
                                         int long_prefix) {
  return token_ascii_equal(token, "long") ||
         column_type_blob_family_parameter_forbidden(token_id, token) ||
         (long_prefix && (token_ascii_equal(token, "varbinary") ||
                          token_ascii_equal(token, "varchar") ||
                          token_ascii_equal(token, "varying")));
}

static int foreign_key_match_option(int token_id, MyliteToken token) {
  return token_id == ML_FULL || token_ascii_equal(token, "partial") ||
         token_ascii_equal(token, "simple");
}

static int foreign_key_reference_action_token(int token_id) {
  return token_id == ML_CASCADE || token_id == ML_RESTRICT;
}

static int validate_parenthesized_identifier_list(MyliteParseContext *ctx,
                                                  MyliteLexer *lexer,
                                                  MyliteToken start,
                                                  const char *message) {
  enum {
    CREATE_TABLE_IDENTIFIER_NEED_NAME,
    CREATE_TABLE_IDENTIFIER_AFTER_NAME
  };
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int state = CREATE_TABLE_IDENTIFIER_NEED_NAME;

  while ((token_id = mylite_lexer_next(lexer, &token)) > 0) {
    if (token_id == ML_RP) {
      if (state != CREATE_TABLE_IDENTIFIER_AFTER_NAME) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      return 1;
    }

    if (token_id == ML_COMMA) {
      if (state != CREATE_TABLE_IDENTIFIER_AFTER_NAME) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      state = CREATE_TABLE_IDENTIFIER_NEED_NAME;
      pending_token = token;
      continue;
    }

    if (state != CREATE_TABLE_IDENTIFIER_NEED_NAME ||
        !dml_row_alias_token(token_id)) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }

    state = CREATE_TABLE_IDENTIFIER_AFTER_NAME;
  }

  mylite_parser_reject(ctx, pending_token, message);
  return 0;
}

static int validate_parenthesized_expression_body(MyliteParseContext *ctx,
                                                  MyliteLexer *lexer,
                                                  MyliteToken start,
                                                  const char *message) {
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int depth = 1;
  MyliteExpressionStack expression_stack = {0};

  query_expression_stack_open_list(&expression_stack, depth, start, 0, 0);

  while ((token_id = mylite_lexer_next(lexer, &token)) > 0) {
    if (token_id == ML_COMMA &&
        query_expression_stack_rejects_comma(&expression_stack, depth)) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }
    if (!query_expression_depth_token(ctx, token_id, token, &depth,
                                      &expression_stack, message)) {
      return 0;
    }
    if (depth == 0) {
      return 1;
    }
  }

  mylite_parser_reject(ctx, pending_token, message);
  return 0;
}

static int validate_create_table_index_key_list(MyliteParseContext *ctx,
                                                MyliteLexer *lexer,
                                                MyliteToken start) {
  enum {
    CREATE_TABLE_KEY_NEED_PART,
    CREATE_TABLE_KEY_AFTER_NAME,
    CREATE_TABLE_KEY_AFTER_DOT,
    CREATE_TABLE_KEY_PREFIX_VALUE,
    CREATE_TABLE_KEY_PREFIX_AFTER_VALUE,
    CREATE_TABLE_KEY_AFTER_PART,
    CREATE_TABLE_KEY_AFTER_DIRECTION,
    CREATE_TABLE_KEY_IN_FUNCTION
  };
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int depth = 1;
  int key_state = CREATE_TABLE_KEY_NEED_PART;

  while ((token_id = mylite_lexer_next(lexer, &token)) > 0) {
    if (depth > 1) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 1 && key_state == CREATE_TABLE_KEY_IN_FUNCTION) {
          key_state = CREATE_TABLE_KEY_AFTER_PART;
        }
      }
      continue;
    }

    if (key_state == CREATE_TABLE_KEY_PREFIX_VALUE) {
      if (!create_index_prefix_length_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_PREFIX_AFTER_VALUE;
      continue;
    }

    if (key_state == CREATE_TABLE_KEY_PREFIX_AFTER_VALUE) {
      if (token_id != ML_RP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_PART;
      continue;
    }

    if (token_id == ML_LP) {
      if (key_state == CREATE_TABLE_KEY_NEED_PART) {
        key_state = CREATE_TABLE_KEY_IN_FUNCTION;
        depth = 2;
        pending_token = token;
        continue;
      }
      if (key_state == CREATE_TABLE_KEY_AFTER_NAME) {
        key_state = CREATE_TABLE_KEY_PREFIX_VALUE;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed CREATE TABLE index key part");
      return 0;
    }

    if (token_id == ML_RP) {
      if (key_state == CREATE_TABLE_KEY_NEED_PART ||
          key_state == CREATE_TABLE_KEY_AFTER_DOT ||
          key_state == CREATE_TABLE_KEY_IN_FUNCTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      return 1;
    }

    if (token_id == ML_COMMA) {
      if (key_state != CREATE_TABLE_KEY_AFTER_NAME &&
          key_state != CREATE_TABLE_KEY_AFTER_PART &&
          key_state != CREATE_TABLE_KEY_AFTER_DIRECTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_NEED_PART;
      pending_token = token;
      continue;
    }

    if (token_id == ML_ASC || token_id == ML_DESC) {
      if (key_state != CREATE_TABLE_KEY_AFTER_NAME &&
          key_state != CREATE_TABLE_KEY_AFTER_PART) {
        mylite_parser_reject(ctx, token,
                             "malformed CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_DIRECTION;
      continue;
    }

    if (token_id == ML_DOT) {
      if (key_state != CREATE_TABLE_KEY_AFTER_NAME) {
        mylite_parser_reject(ctx, token,
                             "malformed CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_DOT;
      pending_token = token;
      continue;
    }

    if (key_state == CREATE_TABLE_KEY_NEED_PART ||
        key_state == CREATE_TABLE_KEY_AFTER_DOT) {
      if (!dml_row_alias_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_NAME;
      continue;
    }

    mylite_parser_reject(ctx, token, "malformed CREATE TABLE index key part");
    return 0;
  }

  mylite_parser_reject(ctx, pending_token,
                       "incomplete CREATE TABLE index key part");
  return 0;
}

static int create_index_prefix_length_token(int token_id) {
  return token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
         token_id == ML_NUMBER_LITERAL;
}

static int create_index_option_number_token(int token_id, MyliteToken token) {
  return create_index_prefix_length_token(token_id) ||
         (token_id == ML_STRING_LITERAL && token_is_quoted_hex_literal(token));
}

static int index_using_type_token(MyliteToken token) {
  return token_ascii_equal(token, "btree") || token_ascii_equal(token, "hash") ||
         token_ascii_equal(token, "rtree");
}

static int alter_table_add_index_marker(int token_id) {
  return token_id == ML_FULLTEXT || token_id == ML_INDEX ||
         token_id == ML_KEY || token_id == ML_PRIMARY ||
         token_id == ML_SPATIAL || token_id == ML_UNIQUE;
}

static int alter_table_add_non_index_marker(int token_id) {
  return token_id == ML_CHECK || token_id == ML_COLUMN ||
         token_id == ML_FOREIGN || token_id == ML_PARTITION;
}

static int event_interval_unit_token(MyliteToken token) {
  return token_ascii_equal(token, "year") ||
         token_ascii_equal(token, "quarter") ||
         token_ascii_equal(token, "month") ||
         token_ascii_equal(token, "week") ||
         token_ascii_equal(token, "day") ||
         token_ascii_equal(token, "hour") ||
         token_ascii_equal(token, "minute") ||
         token_ascii_equal(token, "second") ||
         token_ascii_equal(token, "microsecond") ||
         token_ascii_equal(token, "year_month") ||
         token_ascii_equal(token, "day_hour") ||
         token_ascii_equal(token, "day_minute") ||
         token_ascii_equal(token, "day_second") ||
         token_ascii_equal(token, "hour_minute") ||
         token_ascii_equal(token, "hour_second") ||
         token_ascii_equal(token, "minute_second") ||
         token_ascii_equal(token, "second_microsecond") ||
         token_ascii_equal(token, "minute_microsecond") ||
         token_ascii_equal(token, "hour_microsecond") ||
         token_ascii_equal(token, "day_microsecond");
}

static int event_schedule_boundary(int token_id) {
  return token_id == ML_COMMENT || token_id == ML_DISABLE ||
         token_id == ML_DO || token_id == ML_ENABLE || token_id == ML_ON ||
         token_id == ML_RENAME || token_id == ML_SEMI;
}

static int event_schedule_option_start(MyliteToken token) {
  return token_ascii_equal(token, "starts") || token_ascii_equal(token, "ends");
}

static int routine_body_statement_start_token(int token_id) {
  return token_id == ML_BEGIN || token_id == ML_CASE ||
         token_id == ML_CALL || token_id == ML_CLOSE ||
         token_id == ML_DECLARE || token_id == ML_DO || token_id == ML_FETCH ||
         token_id == ML_GET || token_id == ML_IF || token_id == ML_ITERATE ||
         token_id == ML_LEAVE || token_id == ML_LOOP || token_id == ML_OPEN ||
         token_id == ML_REPEAT ||
         token_id == ML_RESIGNAL || token_id == ML_RETURN ||
         token_id == ML_SET || token_id == ML_SIGNAL ||
         token_id == ML_UNTIL || token_id == ML_WHEN || token_id == ML_WHILE;
}

static int routine_direct_query_body_start_token(int token_id) {
  return token_id == ML_DELETE || token_id == ML_INSERT ||
         token_id == ML_REPLACE || token_id == ML_SELECT ||
         token_id == ML_TABLE || token_id == ML_UPDATE ||
         token_id == ML_VALUES || token_id == ML_WITH;
}

static int routine_compound_statement_start_token(int token_id) {
  return token_id == ML_BEGIN || token_id == ML_CASE || token_id == ML_IF ||
         token_id == ML_LOOP || token_id == ML_REPEAT || token_id == ML_WHILE;
}

static int routine_end_suffix_token(int token_id) {
  return token_id == ML_CASE || token_id == ML_IF || token_id == ML_LOOP ||
         token_id == ML_REPEAT || token_id == ML_WHILE;
}

static int diagnostics_item_name_token(int token_id) {
  return token_id == ML_CATALOG_NAME || token_id == ML_CLASS_ORIGIN ||
         token_id == ML_COLUMN_NAME || token_id == ML_CONSTRAINT_CATALOG ||
         token_id == ML_CONSTRAINT_NAME || token_id == ML_CONSTRAINT_SCHEMA ||
         token_id == ML_CURSOR_NAME || token_id == ML_MESSAGE_TEXT ||
         token_id == ML_MYSQL_ERRNO || token_id == ML_NUMBER ||
         token_id == ML_RETURNED_SQLSTATE || token_id == ML_ROW_COUNT ||
         token_id == ML_SCHEMA_NAME || token_id == ML_SUBCLASS_ORIGIN ||
         token_id == ML_TABLE_NAME;
}

static int routine_characteristic_token(MyliteParseContext *ctx,
                                        MyliteLexer *lexer, int *token_id,
                                        MyliteToken *token) {
  switch (*token_id) {
    case ML_COMMENT:
      {
        MyliteToken comment_token = *token;
        *token_id = mylite_lexer_next(lexer, token);
        if (*token_id <= 0 ||
            !create_table_tail_option_string_token(*token_id, *token)) {
          mylite_parser_reject(ctx, comment_token,
                               "invalid routine comment value");
        }
      }
      return 1;
    case ML_LANGUAGE:
    case ML_NOT:
      *token_id = mylite_lexer_next(lexer, token);
      return 1;
    case ML_CONTAINS:
    case ML_NO:
      *token_id = mylite_lexer_next(lexer, token);
      return 1;
    case ML_READS:
    case ML_MODIFIES:
      *token_id = mylite_lexer_next(lexer, token);
      if (*token_id > 0) {
        *token_id = mylite_lexer_next(lexer, token);
      }
      return 1;
    case ML_SQL:
      *token_id = mylite_lexer_next(lexer, token);
      if (*token_id == ML_SECURITY) {
        *token_id = mylite_lexer_next(lexer, token);
      }
      return 1;
    case ML_DETERMINISTIC:
      return 1;
    default:
      return 0;
  }
}

static int token_is_statement_terminator(int token_id, MyliteToken token) {
  return token_id == ML_SEMI || token_ascii_equal(token, ";");
}

static int token_is_plus(MyliteToken token) {
  return token.length == 1 && token.start[0] == '+';
}

static int dml_assignment_boundary(int mode, int token_id) {
  if (mode == DML_ASSIGNMENT_UPDATE) {
    return token_id == ML_LIMIT || token_id == ML_ORDER || token_id == ML_WHERE;
  }
  if (mode == DML_ASSIGNMENT_INSERT_SET) {
    return token_id == ML_AS || token_id == ML_ON;
  }
  if (mode == DML_ASSIGNMENT_REPLACE_SET) {
    return token_id == ML_AS;
  }

  return 0;
}

static int dml_assignment_operator(int token_id) {
  return token_id == ML_ASSIGN || token_id == ML_EQUALS;
}

static int dml_assignment_target_token(int token_id) {
  return token_id != ML_ASSIGN && token_id != ML_COMMA &&
         token_id != ML_DOT && token_id != ML_DUPLICATE &&
         token_id != ML_EQUALS && token_id != ML_KEY &&
         token_id != ML_LB && token_id != ML_LC && token_id != ML_LIMIT &&
         token_id != ML_LP && token_id != ML_ON && token_id != ML_ORDER &&
         token_id != ML_RB && token_id != ML_RC && token_id != ML_RP &&
         token_id != ML_SEMI && token_id != ML_UPDATE &&
         token_id != ML_WHERE;
}

static int dml_assignment_value_allows_function(int token_id,
                                                MyliteToken token) {
  if (token_id == ML_AND || token_id == ML_ASSIGN || token_id == ML_EQUALS ||
      token_id == ML_GE || token_id == ML_GT || token_id == ML_LE ||
      token_id == ML_LT || token_id == ML_MINUS || token_id == ML_OR) {
    return 1;
  }

  return token_id == ML_ATOM && token.length == 1 &&
         strchr("!%&*+/^|", token.start[0]) != NULL;
}

static int parenthesized_query_start_follows(MyliteParseContext *ctx,
                                             MyliteToken token) {
  MyliteLexer lexer;
  MyliteToken next;
  int token_id;
  size_t offset = token.offset + token.length;

  if (offset >= ctx->length) {
    return 0;
  }

  mylite_lexer_init(&lexer, ctx->sql + offset, ctx->length - offset, NULL);
  token_id = mylite_lexer_next(&lexer, &next);

  return token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_VALUES || token_id == ML_WITH;
}

static int insert_duplicate_clause_follows(MyliteParseContext *ctx,
                                           MyliteToken token) {
  MyliteLexer lexer;
  MyliteToken next;
  int token_id;
  size_t offset = token.offset + token.length;

  if (offset >= ctx->length) {
    return 0;
  }

  mylite_lexer_init(&lexer, ctx->sql + offset, ctx->length - offset, NULL);
  token_id = mylite_lexer_next(&lexer, &next);
  if (token_id != ML_DUPLICATE) {
    return 0;
  }
  token_id = mylite_lexer_next(&lexer, &next);
  if (token_id != ML_KEY) {
    return 0;
  }
  token_id = mylite_lexer_next(&lexer, &next);
  return token_id == ML_UPDATE;
}

static int view_query_order_boundary(int token_id) {
  return token_id == ML_COMMA || token_id == ML_LIMIT || token_id == ML_SEMI ||
         token_id == ML_WITH || select_clause_requires_by(token_id) ||
         select_clause_requires_operand(token_id) || select_set_operator(token_id);
}

static int dml_query_order_boundary(int token_id) {
  return token_id == ML_COMMA || token_id == ML_LIMIT || token_id == ML_ON ||
         token_id == ML_SEMI || select_clause_requires_by(token_id) ||
         select_clause_requires_operand(token_id) || select_set_operator(token_id);
}

static int dml_clause_operand_boundary(int token_id) {
  return token_id == ML_LIMIT || token_id == ML_ORDER ||
         token_id == ML_SEMI || token_id == ML_WHERE;
}

static int dml_limit_option_token(int token_id) {
  return token_id == ML_ATOM || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID;
}

static int dml_literal_token(int token_id, MyliteToken token) {
  return token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_DOUBLE_QUOTED_STRING ||
         ((token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL) &&
          token_starts_numeric_literal(token)) ||
         token_id == ML_STRING_LITERAL;
}

static int set_statement_previous_value_terminal(int token_id,
                                                 MyliteToken token) {
  (void) token;
  return token_id == ML_AT_HOST || token_id == ML_AT_SIGN ||
         token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID || token_id == ML_RB ||
         token_id == ML_RC || token_id == ML_RP ||
         token_id == ML_STRING_LITERAL;
}

static int dml_row_alias_token(int token_id) {
  return token_id != ML_ASSIGN && token_id != ML_BOOLEAN_NUMBER &&
         token_id != ML_COMMA && token_id != ML_DOT &&
         token_id != ML_DOUBLE_QUOTED_STRING && token_id != ML_EQUALS &&
         token_id != ML_FACTOR_NUMBER && token_id != ML_LB &&
         token_id != ML_LC && token_id != ML_LP &&
         token_id != ML_NUMBER_LITERAL && token_id != ML_RB &&
         token_id != ML_RC && token_id != ML_RP && token_id != ML_SEMI &&
         token_id != ML_STRING_LITERAL;
}

static int dml_values_unclosed_string_fragment(int token_id,
                                               MyliteToken token) {
  char quote;
  size_t i;

  if (token_id != ML_DOUBLE_QUOTED_STRING && token_id != ML_STRING_LITERAL) {
    return 0;
  }
  if (token.length == 0) {
    return 0;
  }

  quote = token.start[0];
  if (quote != '\'' && quote != '"') {
    if (token.length < 2 || token.start[1] != '\'') {
      return 0;
    }
    quote = '\'';
    i = 2;
  } else {
    i = 1;
  }

  while (i < token.length) {
    char ch = token.start[i++];
    if (ch == '\\' && i < token.length) {
      i++;
      continue;
    }
    if (ch == quote) {
      if (i < token.length && token.start[i] == quote) {
        i++;
        continue;
      }
      return 0;
    }
  }

  return 1;
}

static int grant_object_start_token(int token_id, MyliteToken token,
                                    int proxy_grant) {
  if (proxy_grant && (token_id == ML_DOUBLE_QUOTED_STRING ||
                      token_id == ML_STRING_LITERAL)) {
    return 1;
  }

  if (token_id == ML_STAR || token_id == ML_FUNCTION ||
      token_id == ML_PROCEDURE || token_id == ML_TABLESPACE) {
    return 1;
  }

  return !token_is_invalid_identifier_atom(token, 0);
}

static int token_opens_nested_expression(int token_id) {
  return token_id == ML_LP || token_id == ML_LB || token_id == ML_LC;
}

static int token_closes_nested_expression(int token_id) {
  return token_id == ML_RP || token_id == ML_RB || token_id == ML_RC;
}

static void result_init(MyliteParseResult *result) {
  memset(result, 0, sizeof(*result));
  result->error_line = 1;
  result->error_column = 1;
}

static void set_parser_error(MyliteParseContext *ctx, const MyliteToken *token,
                             const char *message) {
  MyliteParseResult *result = ctx->result;
  result->error_offset = token->offset;
  result->error_line = token->line;
  result->error_column = token->column;
  snprintf(result->error_message, sizeof(result->error_message), "%s",
           message);
}

static void format_near_token(MyliteParseContext *ctx, int token_id,
                              const MyliteToken *token) {
  MyliteParseResult *result = ctx->result;
  size_t copy_length = token->length;
  char snippet[64];

  (void) token_id;

  if (copy_length >= sizeof(snippet)) {
    copy_length = sizeof(snippet) - 1;
  }
  memcpy(snippet, token->start, copy_length);
  snippet[copy_length] = '\0';

  result->error_offset = token->offset;
  result->error_line = token->line;
  result->error_column = token->column;
  snprintf(result->error_message, sizeof(result->error_message),
           "syntax error near '%s'", snippet);
}

#ifndef MYLITE_SQL_MYLITE_PARSER_INTERNAL_H
#define MYLITE_SQL_MYLITE_PARSER_INTERNAL_H

#include "mylite_ast.h"
#include "mylite_lexer.h"
#include "mylite_parser.h"

#include <stdbool.h>

struct mylite_sql_parser_state {
    struct mylite_sql_parse_result *result;
    unsigned int modes;
    bool accepted;
};

struct mylite_sql_update_statement_parts {
    struct mylite_sql_ast_node *target_table;
    struct mylite_sql_ast_node *assignment_list;
    struct mylite_sql_ast_node *where_clause;
    struct mylite_sql_ast_node *order_clause;
    struct mylite_sql_ast_node *limit_clause;
    struct mylite_sql_ast_node *low_priority_modifier;
    struct mylite_sql_ast_node *ignore_modifier;
};

struct mylite_sql_integer_type_name_tokens {
    struct mylite_sql_token type_token;
    enum mylite_sql_ast_integer_type integer_type;
};

struct mylite_sql_integer_display_width_tokens {
    struct mylite_sql_token width_token;
    struct mylite_sql_token end_token;
};

struct mylite_sql_integer_signedness_tokens {
    struct mylite_sql_token attribute_token;
    int is_unsigned;
};

struct mylite_sql_varchar_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token length_token;
    struct mylite_sql_token end_token;
    int is_national;
};

struct mylite_sql_char_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token length_token;
    struct mylite_sql_token end_token;
    int has_explicit_length;
    int is_national;
};

struct mylite_sql_text_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token length_token;
    struct mylite_sql_token end_token;
    enum mylite_sql_ast_text_type text_type;
    int has_length;
};

struct mylite_sql_binary_string_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token length_token;
    struct mylite_sql_token end_token;
    enum mylite_sql_ast_binary_string_type binary_string_type;
    int has_length;
};

struct mylite_sql_spatial_type_tokens {
    struct mylite_sql_token type_token;
    enum mylite_sql_ast_spatial_type spatial_type;
};

struct mylite_sql_multi_valued_index_part_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token cast_token;
    struct mylite_sql_token right_cast_paren;
    struct mylite_sql_token right_part_paren;
};

struct mylite_sql_bit_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token length_token;
    struct mylite_sql_token end_token;
    int has_length;
};

struct mylite_sql_year_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token width_token;
    struct mylite_sql_token end_token;
    int has_width;
};

struct mylite_sql_decimal_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token precision_token;
    struct mylite_sql_token scale_token;
    struct mylite_sql_token end_token;
    struct mylite_sql_token attribute_token;
    enum mylite_sql_ast_decimal_type decimal_type;
    int has_precision;
    int has_scale;
    int is_unsigned;
};

struct mylite_sql_approximate_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token precision_token;
    struct mylite_sql_token scale_token;
    struct mylite_sql_token end_token;
    struct mylite_sql_token attribute_token;
    enum mylite_sql_ast_approximate_type approximate_type;
    int has_precision;
    int has_scale;
    int is_unsigned;
};

struct mylite_sql_temporal_fractional_precision_tokens {
    struct mylite_sql_token precision_token;
    struct mylite_sql_token end_token;
    int has_precision;
};

struct mylite_sql_temporal_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token precision_token;
    struct mylite_sql_token end_token;
    int has_precision;
};

struct mylite_sql_show_count_warnings_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token count;
    struct mylite_sql_token left_paren;
    struct mylite_sql_token warnings;
};

struct mylite_sql_show_count_errors_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token count;
    struct mylite_sql_token left_paren;
    struct mylite_sql_token errors;
};

struct mylite_sql_select_modifiers {
    enum mylite_sql_ast_select_modifier duplicate_modifier;
    unsigned int options;
    int calc_found_rows;
};

struct mylite_sql_select_locking_clause {
    enum mylite_sql_ast_select_locking_clause kind;
    struct mylite_sql_source_span span;
};

struct mylite_sql_alter_algorithm_value {
    enum mylite_sql_ast_alter_algorithm kind;
    struct mylite_sql_source_span span;
};

struct mylite_sql_alter_lock_value {
    enum mylite_sql_ast_alter_lock kind;
    struct mylite_sql_source_span span;
};

struct mylite_sql_alter_table_options {
    enum mylite_sql_ast_alter_algorithm algorithm;
    enum mylite_sql_ast_alter_lock lock;
    struct mylite_sql_source_span span;
    int has_span;
};

struct mylite_sql_comparison_operator_tokens {
    struct mylite_sql_token token;
    enum mylite_sql_ast_operator operator_kind;
};

void mylite_sql_parser_state_set_root(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *root
);
void mylite_sql_parser_state_syntax_error(
    struct mylite_sql_parser_state *state,
    int parser_token,
    struct mylite_sql_token token
);
void mylite_sql_parser_state_parse_failed(struct mylite_sql_parser_state *state);
void mylite_sql_parser_state_accept(struct mylite_sql_parser_state *state);
void mylite_sql_parser_state_stack_overflow(struct mylite_sql_parser_state *state);

struct mylite_sql_ast_node *mylite_sql_parser_make_script(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *mylite_sql_parser_make_script_with_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement
);
struct mylite_sql_ast_node *mylite_sql_parser_append_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *script,
    struct mylite_sql_ast_node *statement
);
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
);
struct mylite_sql_ast_node *mylite_sql_parser_make_with_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_ast_node *union_terms,
    struct mylite_sql_ast_node *order_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token table_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_explain_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token explain_token,
    struct mylite_sql_ast_node *format,
    struct mylite_sql_ast_node *analyze,
    struct mylite_sql_ast_node *statement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_explain_format(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token format_token,
    struct mylite_sql_ast_node *format_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_explain_analyze(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token analyze_token
);
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
);
struct mylite_sql_ast_node *mylite_sql_parser_attach_select_window_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *window_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_attach_select_into_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *into_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_select_into_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *variable
);
struct mylite_sql_ast_node *mylite_sql_parser_append_select_into_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *variable
);
struct mylite_sql_ast_node *mylite_sql_parser_make_compound_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *first_select,
    struct mylite_sql_ast_node *terms
);
struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_query_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_parenthesis,
    struct mylite_sql_ast_node *inner_statement,
    struct mylite_sql_token right_parenthesis,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_union_term_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *term
);
struct mylite_sql_ast_node *mylite_sql_parser_append_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *terms,
    struct mylite_sql_ast_node *term
);
struct mylite_sql_ast_node *mylite_sql_parser_make_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    enum mylite_sql_ast_union_modifier modifier,
    struct mylite_sql_ast_node *select_statement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_operation_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_set_operator operator_kind,
    enum mylite_sql_ast_union_modifier modifier,
    struct mylite_sql_ast_node *select_statement
);
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
);
struct mylite_sql_ast_node *mylite_sql_parser_make_select_calc_found_rows_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_do_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token do_token,
    struct mylite_sql_ast_node *expression_list
);
struct mylite_sql_ast_node *mylite_sql_parser_make_do_expression_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression
);
struct mylite_sql_ast_node *mylite_sql_parser_append_do_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *expression
);
struct mylite_sql_ast_node *mylite_sql_parser_make_use_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token use_token,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    struct mylite_sql_ast_node *characteristics
);
struct mylite_sql_ast_node *mylite_sql_parser_make_begin_immediate_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token begin_token,
    struct mylite_sql_token immediate_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_transaction_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *characteristics
);
struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *characteristic
);
struct mylite_sql_ast_node *mylite_sql_parser_append_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *characteristic
);
struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_savepoint_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *savepoint_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_maintenance_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *table_names
);
struct mylite_sql_ast_node *mylite_sql_parser_make_lock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token lock_token,
    struct mylite_sql_ast_node *targets
);
struct mylite_sql_ast_node *mylite_sql_parser_make_unlock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unlock_token,
    struct mylite_sql_token table_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target
);
struct mylite_sql_ast_node *mylite_sql_parser_append_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *target
);
struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *lock_type
);
struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_type(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_names_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name,
    struct mylite_sql_ast_node *collation_name
);
struct mylite_sql_ast_node *mylite_sql_parser_attach_set_tail_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *assignment_list
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_default_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *assignments
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_append_set_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_system_variable_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_user_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_user_variable_assignment_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token prepare_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *source
);
struct mylite_sql_ast_node *mylite_sql_parser_make_execute_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token execute_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *using_list
);
struct mylite_sql_ast_node *mylite_sql_parser_make_execute_using_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *variable
);
struct mylite_sql_ast_node *mylite_sql_parser_append_execute_using_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *variable
);
struct mylite_sql_ast_node *mylite_sql_parser_make_deallocate_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_ast_node *select_statement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_ast_node *select_statement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *or_replace_clause,
    struct mylite_sql_ast_node *view_options,
    struct mylite_sql_ast_node *view_name,
    struct mylite_sql_ast_node *column_names,
    struct mylite_sql_ast_node *check_option,
    struct mylite_sql_ast_node *select_statement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *view_options,
    struct mylite_sql_ast_node *view_name,
    struct mylite_sql_ast_node *column_names,
    struct mylite_sql_ast_node *check_option,
    struct mylite_sql_ast_node *select_statement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_or_replace_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token or_token,
    struct mylite_sql_token replace_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_view_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
);
struct mylite_sql_ast_node *mylite_sql_parser_append_view_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
);
struct mylite_sql_ast_node *mylite_sql_parser_make_view_algorithm_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token algorithm_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_view_definer_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token definer_token,
    struct mylite_sql_ast_node *account
);
struct mylite_sql_ast_node *mylite_sql_parser_make_view_security_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token sql_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_view_definer_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *user,
    struct mylite_sql_ast_node *host
);
struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_view_definer_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_view_check_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_token option_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *procedure_name,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    bool is_unique,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_fulltext_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_spatial_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
);
struct mylite_sql_ast_node *mylite_sql_parser_append_table_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_engine_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token engine_token,
    struct mylite_sql_ast_node *engine_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_charset_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_collation_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_auto_increment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_row_format_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_format_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_key_block_size_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token key_block_size_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_pack_keys_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token pack_keys_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_checksum_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token checksum_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_persistent_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_persistent_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_auto_recalc_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_auto_recalc_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_sample_pages_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_sample_pages_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_min_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token min_rows_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_max_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token max_rows_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_avg_row_length_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token avg_row_length_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_delay_key_write_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delay_key_write_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_tablespace_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token tablespace_token,
    struct mylite_sql_ast_node *tablespace_name,
    struct mylite_sql_ast_node *storage
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_union_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    struct mylite_sql_ast_node *table_names,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_insert_method_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_method_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_storage_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token storage_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
);
struct mylite_sql_ast_node *mylite_sql_parser_append_index_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_type_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *type_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_visibility_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_schema_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *view_names
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *procedure_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_name_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_append_table_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_truncate_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token truncate_token,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_databases_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token databases_token,
    struct mylite_sql_ast_node *filter
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_variables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token variables_token,
    struct mylite_sql_ast_node *filter
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *filter
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    int is_full,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_table_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token collation_token,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_triggers_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token triggers_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_events_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token events_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_open_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_routine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_processlist_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token processlist_token,
    enum mylite_sql_ast_node_kind statement_kind
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_for_target_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_ast_node *role_list
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *user,
    struct mylite_sql_ast_node *host
);
struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_show_grants_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_role_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *role
);
struct mylite_sql_ast_node *mylite_sql_parser_append_show_grants_role(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *role
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token warnings_token,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_warnings_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token errors_token,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_errors_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_columns_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_full_columns_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *where_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *view_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *procedure_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_database_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_call_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token call_token,
    struct mylite_sql_ast_node *procedure_name,
    struct mylite_sql_ast_node *arguments
);
struct mylite_sql_ast_node *mylite_sql_parser_make_raw_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_source_span span
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_engines_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token engines_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_engine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *engine_name,
    struct mylite_sql_token status_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_plugins_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token plugins_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_privileges_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token privileges_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_log_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_logs_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token logs_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_replica_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_replicas_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token replicas_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token rename_token,
    struct mylite_sql_ast_node *pairs
);
struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *pair
);
struct mylite_sql_ast_node *mylite_sql_parser_append_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *pair
);
struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_token to_token,
    struct mylite_sql_ast_node *target_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_ast_node *target_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *column_definitions
);
struct mylite_sql_ast_node *mylite_sql_parser_append_alter_table_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_multi_action_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *actions,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *primary_key,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *secondary_index,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_constraint_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_index_name,
    struct mylite_sql_ast_node *new_index_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_index_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_constraint
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_alter_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name,
    struct mylite_sql_ast_node *enforcement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token key_token,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_auto_increment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *auto_increment_option
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *new_column_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_modify_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_change_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_first(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_after(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token after_token,
    struct mylite_sql_ast_node *column_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_set_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_ast_node *default_node
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token default_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_column_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_convert_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_comment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *comment_option,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_storage_statistics_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_order_by_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_items
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_force_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_disable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_enable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
);
struct mylite_sql_alter_table_options mylite_sql_parser_empty_alter_table_options(void);
struct mylite_sql_alter_algorithm_value mylite_sql_parser_make_alter_algorithm_value(
    struct mylite_sql_token token
);
struct mylite_sql_alter_lock_value mylite_sql_parser_make_alter_lock_value(
    struct mylite_sql_token token
);
struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_algorithm_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_algorithm_value value
);
struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_lock_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_lock_value value
);
struct mylite_sql_alter_table_options mylite_sql_parser_append_alter_table_option(
    struct mylite_sql_alter_table_options list,
    struct mylite_sql_alter_table_options option
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
);
struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_infile_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token load_token,
    struct mylite_sql_ast_node *file_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *ignore_lines,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *local_modifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_local_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_high_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_ignore_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_update_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_update_ignore_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_replace_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_replace_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_replace_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_replace_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
);
struct mylite_sql_ast_node *mylite_sql_parser_make_replace_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_append_insert_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_update_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *assignments
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_append_insert_duplicate_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_values_reference(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_token close_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_joined_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *where_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_update_statement_parts parts
);
struct mylite_sql_ast_node *mylite_sql_parser_make_joined_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_append_update_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
);
struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token wildcard_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
);
struct mylite_sql_ast_node *mylite_sql_parser_append_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *item
);
struct mylite_sql_ast_node *mylite_sql_parser_make_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_ast_node *alias
);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_token dual_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_table(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
);
struct mylite_sql_ast_node *mylite_sql_parser_make_derived_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_parenthesis,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_parenthesis,
    struct mylite_sql_ast_node *alias
);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_join(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
);
struct mylite_sql_ast_node *mylite_sql_parser_make_join_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
);
struct mylite_sql_ast_node *mylite_sql_parser_make_join_using_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_parenthesis
);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *hint
);
struct mylite_sql_ast_node *mylite_sql_parser_append_index_hint(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *hint
);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *names,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_scope(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token for_token,
    struct mylite_sql_token last_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_where_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token where_token,
    struct mylite_sql_ast_node *predicate
);
struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_key_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *group_key
);
struct mylite_sql_ast_node *mylite_sql_parser_append_group_by_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *group_key
);
struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token group_token,
    struct mylite_sql_ast_node *group_keys
);
struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_rollup_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_token rollup_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_having_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token having_token,
    struct mylite_sql_ast_node *predicate
);
struct mylite_sql_ast_node *mylite_sql_parser_make_comparison_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
);

struct mylite_sql_parser_like_comparison_predicate_request {
    struct mylite_sql_ast_node *left;
    struct mylite_sql_token operator_token;
    enum mylite_sql_ast_operator operator_kind;
    struct mylite_sql_ast_node *right;
    struct mylite_sql_ast_node *escape;
};
struct mylite_sql_ast_node *mylite_sql_parser_make_like_comparison_predicate(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_parser_like_comparison_predicate_request *request
);
struct mylite_sql_ast_node *mylite_sql_parser_make_is_null_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token null_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_is_boolean_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token truth_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_between_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token between_token,
    struct mylite_sql_ast_node *lower,
    struct mylite_sql_ast_node *upper
);
struct mylite_sql_ast_node *mylite_sql_parser_make_in_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token in_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_exists_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token exists_token,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_predicate_value_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_append_predicate_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_and_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
);
struct mylite_sql_ast_node *mylite_sql_parser_make_xor_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
);
struct mylite_sql_ast_node *mylite_sql_parser_make_or_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
);
struct mylite_sql_ast_node *mylite_sql_parser_make_not_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *child
);
struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
);
struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause_from_item_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *item_list
);

struct mylite_sql_parser_select_order_by_parts {
    struct mylite_sql_ast_node *first_order_key;
    struct mylite_sql_ast_node *first_direction;
    struct mylite_sql_ast_node *tail_items;
};
struct mylite_sql_ast_node *mylite_sql_parser_make_select_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_parser_select_order_by_parts parts
);
struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
);
struct mylite_sql_ast_node *mylite_sql_parser_append_order_by_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *item
);
struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
);
struct mylite_sql_ast_node *mylite_sql_parser_make_order_direction(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token direction_token,
    enum mylite_sql_ast_order_direction direction
);
struct mylite_sql_ast_node *mylite_sql_parser_make_limit_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token limit_token,
    struct mylite_sql_ast_node *row_count,
    struct mylite_sql_ast_node *offset
);
struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_ignore_space_sensitive_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_ast_node *right
);
struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *qualifier,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_literal(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_literal_kind literal_kind
);
struct mylite_sql_ast_node *mylite_sql_parser_make_dml_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_system_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_unary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *operand
);
struct mylite_sql_ast_node *mylite_sql_parser_make_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
);
struct mylite_sql_ast_node *mylite_sql_parser_make_cast_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token cast_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_unary_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token binary_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_convert_binary_type_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_charset_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *charset,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_collate_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation
);
struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_scalar_subquery_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_searched_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_simple_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *case_value,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *when_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_append_case_when(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *when_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token when_token,
    struct mylite_sql_ast_node *condition,
    struct mylite_sql_ast_node *result
);
struct mylite_sql_ast_node *mylite_sql_parser_make_case_else_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token else_token,
    struct mylite_sql_ast_node *result
);
struct mylite_sql_ast_node *mylite_sql_parser_make_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_group_concat_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *separator,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_attach_function_window_clause(
    struct mylite_sql_ast_node *function,
    struct mylite_sql_ast_node *window_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_row_number_window_function_with_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *window_clause
);

struct mylite_sql_window_function_arguments {
    size_t count;
    struct mylite_sql_ast_node *items[3];
};

struct mylite_sql_ast_node *mylite_sql_parser_make_window_function_with_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_window_function_arguments arguments,
    struct mylite_sql_ast_node *window_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_window_function_arguments arguments,
    struct mylite_sql_ast_node *null_treatment,
    struct mylite_sql_ast_node *window_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_null_treatment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token treatment_token,
    enum mylite_sql_ast_node_kind treatment_kind,
    struct mylite_sql_token nulls_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_empty_window_spec(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token over_token,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_spec(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *window_reference,
    struct mylite_sql_ast_node *partition_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *frame_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_partition_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token partition_token,
    struct mylite_sql_ast_node *key_list
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_order_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *order_list
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_reference(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_definition_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *definition
);
struct mylite_sql_ast_node *mylite_sql_parser_append_window_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *definition
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *spec
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_frame_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token frame_token,
    struct mylite_sql_ast_node *first_bound,
    struct mylite_sql_ast_node *second_bound
);
struct mylite_sql_ast_node *mylite_sql_parser_make_window_frame_bound(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token bound_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_trim_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *remove_string,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_four_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_ast_node *fourth_argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_list_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_generic_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_generic_function_with_window_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *window_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_row_constructor(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_count_error(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind error_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_function_argument_count_error(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind error_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument
);
struct mylite_sql_ast_node *mylite_sql_parser_prepend_function_argument(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_ast_node *list
);
struct mylite_sql_ast_node *mylite_sql_parser_append_function_argument(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *argument
);
struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_current_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_timestamp_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_current_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_date_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_current_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_time_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_temporal_value_with_precision(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_temporal_fractional_precision_tokens precision
);
struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_temporal_value_with_precision(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_temporal_fractional_precision_tokens precision
);
struct mylite_sql_ast_node *mylite_sql_parser_make_utc_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_date_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_utc_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_time_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_utc_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_timestamp_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column
);
struct mylite_sql_ast_node *mylite_sql_parser_append_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *column
);
struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
);
struct mylite_sql_ast_node *mylite_sql_parser_append_primary_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
);
struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token index_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_unique_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_fulltext_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token fulltext_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
);
struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token spatial_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
);
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
);
struct mylite_sql_ast_node *mylite_sql_parser_make_check_constraint_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token check_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *enforcement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_check_enforcement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
);
struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
);
struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
);
struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_index_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
);
struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
);
struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    enum mylite_sql_ast_node_kind kind
);
struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
);
struct mylite_sql_ast_node *mylite_sql_parser_append_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
);
struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *prefix_length,
    struct mylite_sql_ast_node *direction
);
struct mylite_sql_ast_node *mylite_sql_parser_make_functional_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *direction
);
struct mylite_sql_ast_node *mylite_sql_parser_make_multi_valued_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_multi_valued_index_part_tokens tokens,
    struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_node_kind cast_target,
    struct mylite_sql_ast_node *direction
);
struct mylite_sql_ast_node *mylite_sql_parser_make_inline_primary_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    struct mylite_sql_token key_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_inline_unique_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_attribute_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *attribute
);
struct mylite_sql_ast_node *mylite_sql_parser_append_column_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *attribute
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_auto_increment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_on_update_current_timestamp(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *current_timestamp_value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_charset_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_collation_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_binary_collation_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token binary_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_comment_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *comment
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_visibility_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_srid_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token srid_token,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token as_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren_token,
    struct mylite_sql_ast_node *storage
);
struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_storage(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *nullability,
    struct mylite_sql_ast_node *default_null,
    struct mylite_sql_ast_node *primary_key
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_with_attributes(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *attributes
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_null(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_token null_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_ast_node *value
);
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
);
struct mylite_sql_ast_node *mylite_sql_parser_make_varchar_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_varchar_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_char_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_char_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_text_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_text_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_json_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_spatial_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_enum_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_enum_label_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token label_token
);
struct mylite_sql_ast_node *mylite_sql_parser_append_enum_label(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_token label_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_token end_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_member_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token member_token
);
struct mylite_sql_ast_node *mylite_sql_parser_append_set_member(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_token member_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_binary_string_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_binary_string_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_bit_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_bit_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_year_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_year_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_decimal_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_decimal_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_approximate_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_approximate_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_date_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token date_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_datetime_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_timestamp_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_time_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_temporal_type_tokens tokens
);
struct mylite_sql_ast_node *mylite_sql_parser_make_nullability(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_nullability nullability,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_empty_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_append_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *identifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
);
struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
);
struct mylite_sql_ast_node *mylite_sql_parser_append_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
);
struct mylite_sql_ast_node *mylite_sql_parser_append_values_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_append_insert_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_append_values_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_values_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
);

#endif

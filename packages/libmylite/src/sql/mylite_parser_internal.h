#ifndef MYLITE_SQL_MYLITE_PARSER_INTERNAL_H
#define MYLITE_SQL_MYLITE_PARSER_INTERNAL_H

#include "mylite_ast.h"
#include "mylite_lexer.h"
#include "mylite_parser.h"

#include <stdbool.h>
#include <stdint.h>

struct mylite_sql_parser_state {
    struct mylite_sql_parse_result *result;
    bool accepted;
};

struct mylite_sql_parser_select_duplicate_mode {
    enum mylite_sql_ast_select_duplicate_mode mode;
    struct mylite_sql_source_span first_span;
    struct mylite_sql_source_span last_span;
    struct mylite_sql_source_span conflict_span;
    size_t modifier_count;
    bool explicit_mode;
    bool conflict;
};

struct mylite_sql_parser_union_operator {
    enum mylite_sql_ast_set_duplicate_mode mode;
    struct mylite_sql_source_span span;
};

struct mylite_sql_parser_aggregate_star_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token star;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_substring_operands {
    struct mylite_sql_ast_node *text;
    struct mylite_sql_ast_node *position;
    struct mylite_sql_ast_node *length;
};

struct mylite_sql_parser_position_operands {
    struct mylite_sql_ast_node *substring;
    struct mylite_sql_ast_node *source;
};

struct mylite_sql_parser_char_function_call_parts {
    struct mylite_sql_token char_token;
    struct mylite_sql_token left_paren;
    struct mylite_sql_ast_node *arguments;
    struct mylite_sql_ast_node *charset;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_trim_operands {
    struct mylite_sql_ast_node *remove;
    struct mylite_sql_ast_node *source;
};

struct mylite_sql_parser_display_width_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token integer;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_column_length_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token integer;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_precision_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token precision;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_precision_scale_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token precision;
    struct mylite_sql_token scale;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_key_part_prefix_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token integer;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_index_key_block_size_tokens {
    struct mylite_sql_token key_block_size;
    struct mylite_sql_token integer;
};

struct mylite_sql_parser_index_string_option_tokens {
    struct mylite_sql_token option;
    struct mylite_sql_token string;
};

struct mylite_sql_parser_column_unique_key_attribute_tokens {
    struct mylite_sql_token unique_token;
    struct mylite_sql_token key_token;
};

struct mylite_sql_parser_table_string_option_tokens {
    struct mylite_sql_token option;
    struct mylite_sql_token string;
};

struct mylite_sql_parser_table_integer_option_tokens {
    struct mylite_sql_token option;
    struct mylite_sql_token integer;
};

struct mylite_sql_parser_create_index_tokens {
    struct mylite_sql_token create;
    struct mylite_sql_token class_token;
};

struct mylite_sql_parser_show_tables_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token extended;
    struct mylite_sql_token full;
    struct mylite_sql_token tables;
};

struct mylite_sql_parser_show_table_status_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token table;
    struct mylite_sql_token status;
};

struct mylite_sql_parser_show_columns_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token extended;
    struct mylite_sql_token full;
    struct mylite_sql_token columns;
};

struct mylite_sql_parser_show_variables_scope {
    struct mylite_sql_token token;
    enum mylite_sql_ast_show_variables_scope scope;
};

struct mylite_sql_parser_show_status_scope {
    struct mylite_sql_token token;
    enum mylite_sql_ast_show_status_scope scope;
};

struct mylite_sql_parser_show_engines_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token storage;
    struct mylite_sql_token engines;
};

struct mylite_sql_parser_show_index_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token extended;
    struct mylite_sql_token index;
};

struct mylite_sql_parser_show_create_table_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token table;
};

struct mylite_sql_parser_show_create_schema_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token schema;
};

struct mylite_sql_parser_show_diagnostics_kind {
    struct mylite_sql_token token;
    enum mylite_sql_ast_show_diagnostics_kind kind;
};

struct mylite_sql_parser_show_diagnostics_count_tokens {
    struct mylite_sql_token show;
    struct mylite_sql_token count;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_describe_table_tokens {
    struct mylite_sql_token keyword;
};

struct mylite_sql_parser_ddl_table_option_tokens {
    struct mylite_sql_token option;
    struct mylite_sql_token value;
};

struct mylite_sql_parser_alter_table_action_tokens {
    struct mylite_sql_token action;
    struct mylite_sql_token column;
};

struct mylite_sql_parser_index_class_token {
    struct mylite_sql_token token;
    enum mylite_sql_ast_index_class index_class;
};

struct mylite_sql_parser_alter_table_index_spelling_token {
    struct mylite_sql_token token;
    enum mylite_sql_ast_alter_table_index_spelling spelling;
};

struct mylite_sql_parser_alter_table_constraint_spelling_token {
    struct mylite_sql_token token;
    enum mylite_sql_ast_alter_table_constraint_spelling spelling;
};

struct mylite_sql_parser_constraint_enforcement {
    struct mylite_sql_token start;
    struct mylite_sql_token end;
    enum mylite_sql_ast_constraint_enforcement enforcement;
};

struct mylite_sql_parser_reference_action {
    struct mylite_sql_token start;
    struct mylite_sql_token end;
    enum mylite_sql_ast_reference_action action;
};

struct mylite_sql_parser_reference_match {
    struct mylite_sql_token token;
    enum mylite_sql_ast_reference_match match;
};

struct mylite_sql_parser_drop_table_tokens {
    struct mylite_sql_token drop;
    struct mylite_sql_token temporary;
    struct mylite_sql_token mode;
};

struct mylite_sql_parser_insert_tokens {
    struct mylite_sql_token insert;
    struct mylite_sql_token ignore;
};

struct mylite_sql_parser_replace_modifier {
    struct mylite_sql_token low_priority;
    struct mylite_sql_token delayed;
};

struct mylite_sql_parser_replace_tokens {
    struct mylite_sql_token replace;
    struct mylite_sql_parser_replace_modifier modifier;
};

struct mylite_sql_parser_completion_tokens {
    struct mylite_sql_token start;
    struct mylite_sql_token end;
};

struct mylite_sql_parser_statement_tokens {
    struct mylite_sql_token start;
    struct mylite_sql_token end;
};

struct mylite_sql_parser_join_operator {
    struct mylite_sql_token token;
    enum mylite_sql_ast_join_type join_type;
};

struct mylite_sql_parser_using_column_append {
    struct mylite_sql_ast_node *list;
    struct mylite_sql_ast_node *column;
};

struct mylite_sql_parser_subquery {
    struct mylite_sql_token left_paren;
    struct mylite_sql_ast_node *select_statement;
    struct mylite_sql_token right_paren;
};

struct mylite_sql_parser_comparison_operator {
    struct mylite_sql_token token;
    enum mylite_sql_ast_operator operator_kind;
};

struct mylite_sql_parser_row_constructor_elements {
    struct mylite_sql_ast_node *first_expression;
    struct mylite_sql_ast_node *remaining_expressions;
};

void mylite_sql_parser_state_set_root(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *root);
void mylite_sql_parser_state_syntax_error(struct mylite_sql_parser_state *state, int parser_token,
                                          struct mylite_sql_token token);
void mylite_sql_parser_state_parse_failed(struct mylite_sql_parser_state *state);
void mylite_sql_parser_state_accept(struct mylite_sql_parser_state *state);
void mylite_sql_parser_state_stack_overflow(struct mylite_sql_parser_state *state);

struct mylite_sql_ast_node *mylite_sql_parser_make_script(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_make_script_with_statement(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_ast_node *statement);
struct mylite_sql_ast_node *
mylite_sql_parser_append_statement(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *script,
                                   struct mylite_sql_ast_node *statement);
struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token select_token,
    struct mylite_sql_parser_select_duplicate_mode duplicate_mode,
    struct mylite_sql_ast_node *select_list, struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause, struct mylite_sql_ast_node *group_by_clause,
    struct mylite_sql_ast_node *having_clause, struct mylite_sql_ast_node *order_by_clause,
    struct mylite_sql_ast_node *limit_clause);
struct mylite_sql_ast_node *mylite_sql_parser_make_query_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *body,
    struct mylite_sql_ast_node *order_by_clause, struct mylite_sql_ast_node *limit_clause);
struct mylite_sql_ast_node *mylite_sql_parser_make_query_primary(
    struct mylite_sql_parser_state *state, struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *select_statement, struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *mylite_sql_parser_make_union_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *left,
    struct mylite_sql_parser_union_operator union_operator, struct mylite_sql_ast_node *right);
struct mylite_sql_parser_union_operator
mylite_sql_parser_make_default_union_operator(struct mylite_sql_token union_token);
struct mylite_sql_parser_union_operator
mylite_sql_parser_make_all_union_operator(struct mylite_sql_token union_token,
                                          struct mylite_sql_token all_token);
struct mylite_sql_parser_union_operator
mylite_sql_parser_make_distinct_union_operator(struct mylite_sql_token union_token,
                                               struct mylite_sql_token distinct_token);
struct mylite_sql_parser_select_duplicate_mode
mylite_sql_parser_make_implicit_select_duplicate_mode(void);
struct mylite_sql_parser_select_duplicate_mode
mylite_sql_parser_make_all_select_duplicate_mode(struct mylite_sql_token token);
struct mylite_sql_parser_select_duplicate_mode
mylite_sql_parser_make_distinct_select_duplicate_mode(struct mylite_sql_token token);
struct mylite_sql_parser_select_duplicate_mode
mylite_sql_parser_append_select_duplicate_mode(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_parser_select_duplicate_mode list,
                                               struct mylite_sql_parser_select_duplicate_mode item);
struct mylite_sql_ast_node *
mylite_sql_parser_make_where_clause(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_token where_token,
                                    struct mylite_sql_ast_node *expression);
struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_clause(
    struct mylite_sql_parser_state *state, struct mylite_sql_token group_token,
    struct mylite_sql_token by_token, struct mylite_sql_ast_node *items);
struct mylite_sql_ast_node *
mylite_sql_parser_make_group_item_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *
mylite_sql_parser_append_group_item(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *
mylite_sql_parser_make_group_item(struct mylite_sql_parser_state *state,
                                  struct mylite_sql_ast_node *expression,
                                  struct mylite_sql_token direction_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_having_clause(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_token having_token,
                                     struct mylite_sql_ast_node *expression);
struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause(
    struct mylite_sql_parser_state *state, struct mylite_sql_token order_token,
    struct mylite_sql_token by_token, struct mylite_sql_ast_node *items);
struct mylite_sql_ast_node *
mylite_sql_parser_make_order_item_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *
mylite_sql_parser_append_order_item(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *
mylite_sql_parser_make_order_item(struct mylite_sql_parser_state *state,
                                  struct mylite_sql_ast_node *expression,
                                  struct mylite_sql_token direction_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_limit_clause(
    struct mylite_sql_parser_state *state, struct mylite_sql_token limit_token,
    struct mylite_sql_ast_node *offset_bound, struct mylite_sql_ast_node *row_count_bound);
struct mylite_sql_ast_node *
mylite_sql_parser_make_limit_bound(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_token integer_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_use_statement(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_token use_token,
                                     struct mylite_sql_ast_node *schema_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists, struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_schema_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *schema_name, struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists, struct mylite_sql_ast_node *schema_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_table_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_drop_table_tokens tokens,
    struct mylite_sql_ast_node *if_exists, struct mylite_sql_ast_node *table_names);
struct mylite_sql_ast_node *
mylite_sql_parser_make_rename_table_statement(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_token rename_token,
                                              struct mylite_sql_ast_node *pairs);
struct mylite_sql_ast_node *
mylite_sql_parser_make_rename_table_pair_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *pair);
struct mylite_sql_ast_node *
mylite_sql_parser_append_rename_table_pair(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *pair);
struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *old_name,
    struct mylite_sql_token to_token, struct mylite_sql_ast_node *new_name);
struct mylite_sql_ast_node *
mylite_sql_parser_make_truncate_table_statement(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_token truncate_token,
                                                struct mylite_sql_ast_node *table_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *items);
struct mylite_sql_ast_node *
mylite_sql_parser_make_alter_table_item_list(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *
mylite_sql_parser_append_alter_table_item(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *list,
                                          struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_alter_table_action_tokens tokens,
    struct mylite_sql_ast_node *column_definition, struct mylite_sql_ast_node *position);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_column_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_alter_table_action_tokens tokens,
    struct mylite_sql_ast_node *column_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_column_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token rename_token,
    struct mylite_sql_ast_node *old_name, struct mylite_sql_token to_token,
    struct mylite_sql_ast_node *new_name);
struct mylite_sql_ast_node *
mylite_sql_parser_make_alter_table_rename_table_action(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_token rename_token,
                                                       struct mylite_sql_ast_node *new_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_change_column_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_alter_table_action_tokens tokens, struct mylite_sql_ast_node *old_name,
    struct mylite_sql_ast_node *column_definition, struct mylite_sql_ast_node *position);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_modify_column_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_alter_table_action_tokens tokens,
    struct mylite_sql_ast_node *column_definition, struct mylite_sql_ast_node *position);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_column_position(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    enum mylite_sql_ast_alter_table_column_position position,
    struct mylite_sql_ast_node *column_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_primary_key_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts, struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *
mylite_sql_parser_make_alter_table_drop_primary_key_action(struct mylite_sql_parser_state *state,
                                                           struct mylite_sql_token drop_token,
                                                           struct mylite_sql_token key_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_unique_index_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type, struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_secondary_index_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *index_name, struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts, struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_special_index_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_index_class_token index_class,
    struct mylite_sql_ast_node *index_name, struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_index_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token drop_token,
    struct mylite_sql_parser_alter_table_index_spelling_token spelling,
    struct mylite_sql_ast_node *index_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_index_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token rename_token,
    struct mylite_sql_parser_alter_table_index_spelling_token spelling,
    struct mylite_sql_ast_node *old_name, struct mylite_sql_token to_token,
    struct mylite_sql_ast_node *new_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_alter_index_visibility_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *index_name, struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_index_option visibility);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_check_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *expression,
    struct mylite_sql_parser_constraint_enforcement enforcement);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_check_or_constraint_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token drop_token,
    struct mylite_sql_parser_alter_table_constraint_spelling_token spelling,
    struct mylite_sql_ast_node *constraint_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_alter_check_or_constraint_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token alter_token,
    struct mylite_sql_parser_alter_table_constraint_spelling_token spelling,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_parser_constraint_enforcement enforcement);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_foreign_key_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *columns, struct mylite_sql_ast_node *reference);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_foreign_key_action(
    struct mylite_sql_parser_state *state, struct mylite_sql_token drop_token,
    struct mylite_sql_token key_token, struct mylite_sql_ast_node *constraint_name);
struct mylite_sql_ast_node *
mylite_sql_parser_make_table_name_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *table_name);
struct mylite_sql_ast_node *
mylite_sql_parser_append_table_name(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *table_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_values_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_insert_tokens tokens,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows, struct mylite_sql_ast_node *row_alias,
    struct mylite_sql_ast_node *duplicate_update);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_insert_tokens tokens,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *row_alias, struct mylite_sql_ast_node *duplicate_update);
struct mylite_sql_ast_node *mylite_sql_parser_make_replace_values_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_replace_tokens tokens,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows);
struct mylite_sql_ast_node *mylite_sql_parser_make_replace_set_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_replace_tokens tokens,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *assignments);
struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_column_list(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_column(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *list,
                                       struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_row_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *row);
struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_row(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *row);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_token start_token,
                                                              struct mylite_sql_ast_node *values,
                                                              struct mylite_sql_token end_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_value_list(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_value(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *list,
                                      struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_set_assignment_list(struct mylite_sql_parser_state *state,
                                                  struct mylite_sql_ast_node *assignment);
struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_set_assignment(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_ast_node *list,
                                               struct mylite_sql_ast_node *assignment);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_assignment(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *target,
    struct mylite_sql_token equal_token, struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_duplicate_update_clause(struct mylite_sql_parser_state *state,
                                                      struct mylite_sql_token on_token,
                                                      struct mylite_sql_ast_node *assignments);
struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_update_assignment_list(struct mylite_sql_parser_state *state,
                                                     struct mylite_sql_ast_node *assignment);
struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_update_assignment(struct mylite_sql_parser_state *state,
                                                  struct mylite_sql_ast_node *list,
                                                  struct mylite_sql_ast_node *assignment);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_update_assignment(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *target,
    struct mylite_sql_token equal_token, struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_alias(
    struct mylite_sql_parser_state *state, struct mylite_sql_token as_token,
    struct mylite_sql_ast_node *alias, struct mylite_sql_ast_node *columns);
struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_alias_column_list(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_alias_column(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_ast_node *list,
                                             struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *target, struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *where_clause, struct mylite_sql_ast_node *order_by_clause,
    struct mylite_sql_ast_node *limit_clause);
struct mylite_sql_ast_node *
mylite_sql_parser_make_update_target(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *table_name,
                                     struct mylite_sql_ast_node *alias);
struct mylite_sql_ast_node *
mylite_sql_parser_make_update_assignment_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *assignment);
struct mylite_sql_ast_node *
mylite_sql_parser_append_update_assignment(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *assignment);
struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *target,
    struct mylite_sql_token equal_token, struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *
mylite_sql_parser_make_update_limit_clause(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_token limit_token,
                                           struct mylite_sql_ast_node *row_count_bound);
struct mylite_sql_ast_node *mylite_sql_parser_make_delete_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *target, struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_by_clause, struct mylite_sql_ast_node *limit_clause);
struct mylite_sql_ast_node *
mylite_sql_parser_make_start_transaction_statement(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token start_token,
                                                   struct mylite_sql_ast_node *characteristics);
struct mylite_sql_ast_node *mylite_sql_parser_make_begin_transaction_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_statement_tokens tokens);
struct mylite_sql_ast_node *
mylite_sql_parser_make_transaction_characteristic_list(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_ast_node *characteristic);
struct mylite_sql_ast_node *
mylite_sql_parser_append_transaction_characteristic(struct mylite_sql_parser_state *state,
                                                    struct mylite_sql_ast_node *list,
                                                    struct mylite_sql_ast_node *characteristic);
struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_access_mode(
    struct mylite_sql_parser_state *state, struct mylite_sql_token read_token,
    struct mylite_sql_token end_token, enum mylite_sql_ast_transaction_access_mode access_mode);
struct mylite_sql_ast_node *
mylite_sql_parser_make_transaction_consistent_snapshot(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_token with_token,
                                                       struct mylite_sql_token snapshot_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_commit_statement(struct mylite_sql_parser_state *state,
                                        struct mylite_sql_parser_statement_tokens tokens,
                                        struct mylite_sql_ast_node *completion);
struct mylite_sql_ast_node *
mylite_sql_parser_make_rollback_statement(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_parser_statement_tokens tokens,
                                          struct mylite_sql_ast_node *completion);
struct mylite_sql_ast_node *
mylite_sql_parser_make_savepoint_statement(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_token savepoint_token,
                                           struct mylite_sql_ast_node *name);
struct mylite_sql_ast_node *
mylite_sql_parser_make_rollback_to_savepoint_statement(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_token rollback_token,
                                                       struct mylite_sql_ast_node *name);
struct mylite_sql_ast_node *
mylite_sql_parser_make_release_savepoint_statement(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token release_token,
                                                   struct mylite_sql_ast_node *name);
struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_completion(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_completion_tokens tokens,
    enum mylite_sql_ast_transaction_chain chain, enum mylite_sql_ast_transaction_release release);
struct mylite_sql_ast_node *
mylite_sql_parser_make_delete_target(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *table_name,
                                     struct mylite_sql_ast_node *alias);
struct mylite_sql_ast_node *
mylite_sql_parser_make_delete_limit_clause(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_token limit_token,
                                           struct mylite_sql_ast_node *row_count_bound);
struct mylite_sql_ast_node *
mylite_sql_parser_make_show_schemas_statement(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_token show_token,
                                              struct mylite_sql_token schemas_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_variables_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token show_token,
    struct mylite_sql_parser_show_variables_scope scope, struct mylite_sql_token variables_token,
    struct mylite_sql_ast_node *filter);
struct mylite_sql_parser_show_variables_scope
mylite_sql_parser_make_show_variables_scope(struct mylite_sql_token token,
                                            enum mylite_sql_ast_show_variables_scope scope);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_status_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token show_token,
    struct mylite_sql_parser_show_status_scope scope, struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *filter);
struct mylite_sql_parser_show_status_scope
mylite_sql_parser_make_show_status_scope(struct mylite_sql_token token,
                                         enum mylite_sql_ast_show_status_scope scope);
struct mylite_sql_ast_node *
mylite_sql_parser_make_show_engines_statement(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_parser_show_engines_tokens tokens);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_character_set_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token show_token,
    struct mylite_sql_token character_set_token, struct mylite_sql_ast_node *filter);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_collation_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token show_token,
    struct mylite_sql_token collation_token, struct mylite_sql_ast_node *filter);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_tables_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_show_tables_tokens tokens,
    struct mylite_sql_ast_node *schema_name, struct mylite_sql_ast_node *filter);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_table_status_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_show_table_status_tokens tokens,
    struct mylite_sql_ast_node *schema_name, struct mylite_sql_ast_node *filter);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_columns_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_show_columns_tokens tokens,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_index_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_show_index_tokens tokens,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_table_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_show_create_table_tokens tokens,
    struct mylite_sql_ast_node *table_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_show_create_schema_tokens tokens,
    struct mylite_sql_ast_node *if_not_exists, struct mylite_sql_ast_node *schema_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_diagnostics_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token show_token,
    struct mylite_sql_parser_show_diagnostics_kind kind, struct mylite_sql_ast_node *limit_clause);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_diagnostics_count_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_show_diagnostics_count_tokens tokens,
    struct mylite_sql_parser_show_diagnostics_kind kind);
struct mylite_sql_parser_show_diagnostics_kind
mylite_sql_parser_make_show_diagnostics_kind(struct mylite_sql_token token,
                                             enum mylite_sql_ast_show_diagnostics_kind kind);
struct mylite_sql_ast_node *mylite_sql_parser_make_describe_table_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_describe_table_tokens tokens,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *filter);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_names_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *character_set, struct mylite_sql_ast_node *collation);
struct mylite_sql_ast_node *
mylite_sql_parser_make_set_character_set_statement(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token set_token,
                                                   struct mylite_sql_ast_node *character_set);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists, struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns, struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_index_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_create_index_tokens tokens,
    enum mylite_sql_ast_index_class index_class, struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *pre_index_type, struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *key_parts, struct mylite_sql_ast_node *options,
    struct mylite_sql_ast_node *ddl_options);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_index_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *index_name, struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *ddl_options);
struct mylite_sql_ast_node *
mylite_sql_parser_make_ddl_table_option_list(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_append_ddl_table_option(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *list,
                                          struct mylite_sql_ast_node *option);
struct mylite_sql_ast_node *
mylite_sql_parser_make_ddl_table_option(struct mylite_sql_parser_state *state,
                                        struct mylite_sql_parser_ddl_table_option_tokens tokens,
                                        enum mylite_sql_ast_ddl_table_option option_kind);
struct mylite_sql_ast_node *
mylite_sql_parser_make_table_option_list(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_append_table_option(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *list,
                                      struct mylite_sql_ast_node *option);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_option(
    struct mylite_sql_parser_state *state, struct mylite_sql_token option_token,
    enum mylite_sql_ast_table_option option_kind, struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_table_string_option_tokens tokens);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_auto_increment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_table_integer_option_tokens tokens);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_definition_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_append_column_definition(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_constraint(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type, struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *index_name, struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts, struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *mylite_sql_parser_make_unique_index(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type, struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *
mylite_sql_parser_make_key_part_list(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *key_part);
struct mylite_sql_ast_node *mylite_sql_parser_append_key_part(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_ast_node *list,
                                                              struct mylite_sql_ast_node *key_part);
struct mylite_sql_ast_node *mylite_sql_parser_make_key_part(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *prefix, enum mylite_sql_ast_key_part_order order,
    struct mylite_sql_token order_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_key_part_prefix(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_parser_key_part_prefix_tokens tokens);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_type(
    struct mylite_sql_parser_state *state, struct mylite_sql_token using_token,
    struct mylite_sql_token algorithm_token, enum mylite_sql_ast_index_algorithm algorithm);
struct mylite_sql_ast_node *
mylite_sql_parser_make_index_option_list(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_append_index_option(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *list,
                                      struct mylite_sql_ast_node *option);
struct mylite_sql_ast_node *
mylite_sql_parser_make_index_using_option(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *index_type);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_key_block_size_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_index_key_block_size_tokens tokens);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_index_string_option_tokens tokens);
struct mylite_sql_ast_node *
mylite_sql_parser_make_index_visibility_option(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_token visibility_token,
                                               enum mylite_sql_ast_index_option visibility);
struct mylite_sql_ast_node *mylite_sql_parser_make_index_attribute_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_index_string_option_tokens tokens,
    enum mylite_sql_ast_index_option option);
struct mylite_sql_ast_node *
mylite_sql_parser_make_index_with_parser_option(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_token with_token,
                                                struct mylite_sql_ast_node *parser_name);
struct mylite_sql_ast_node *
mylite_sql_parser_make_identifier_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *identifier);
struct mylite_sql_ast_node *
mylite_sql_parser_append_identifier(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *identifier);
struct mylite_sql_ast_node *mylite_sql_parser_make_reference_definition(
    struct mylite_sql_parser_state *state, struct mylite_sql_token references_token,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *options);
struct mylite_sql_ast_node *
mylite_sql_parser_make_reference_option_list(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_append_reference_option(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *list,
                                          struct mylite_sql_ast_node *option);
struct mylite_sql_ast_node *
mylite_sql_parser_make_reference_action_option(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_token on_token,
                                               enum mylite_sql_ast_reference_option option_kind,
                                               struct mylite_sql_parser_reference_action action);
struct mylite_sql_ast_node *
mylite_sql_parser_make_reference_match_option(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_token match_token,
                                              struct mylite_sql_parser_reference_match match);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type, struct mylite_sql_ast_node *attributes);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_type(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_token type_token,
                                   enum mylite_sql_ast_column_type column_type);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_display_width(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *column_type,
                                           struct mylite_sql_ast_node *display_width);
struct mylite_sql_ast_node *
mylite_sql_parser_make_integer_display_width(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_parser_display_width_tokens tokens);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_length(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_parser_column_length_tokens tokens);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_length(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *column_type,
                                    struct mylite_sql_ast_node *length);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_precision(struct mylite_sql_parser_state *state,
                                        struct mylite_sql_parser_precision_tokens tokens);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_precision_scale(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_precision_scale_tokens tokens);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_precision_scale(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_ast_node *column_type,
                                             struct mylite_sql_ast_node *precision_scale);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_signed(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *column_type,
                                         struct mylite_sql_token signed_token);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_unsigned(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *column_type,
                                           struct mylite_sql_token unsigned_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_type_attribute_list(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_character_set(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_ast_node *attributes,
                                                struct mylite_sql_ast_node *character_set);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_collation(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_ast_node *attributes,
                                            struct mylite_sql_ast_node *collation);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_binary_attribute(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_ast_node *attributes,
                                                   struct mylite_sql_token binary_token);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_byte_attribute(struct mylite_sql_parser_state *state,
                                                 struct mylite_sql_ast_node *attributes,
                                                 struct mylite_sql_token byte_token);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_zerofill_attribute(struct mylite_sql_parser_state *state,
                                                     struct mylite_sql_ast_node *attributes,
                                                     struct mylite_sql_token zerofill_token);
struct mylite_sql_ast_node *
mylite_sql_parser_apply_column_type_attributes(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_ast_node *column_type,
                                               struct mylite_sql_ast_node *attributes);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_national(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *column_type,
                                           struct mylite_sql_token national_token);
struct mylite_sql_ast_node *
mylite_sql_parser_validate_column_type(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *column_type);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_attribute_list(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_append_column_attribute(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *list,
                                          struct mylite_sql_ast_node *attribute);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_null_attribute(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_token null_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_not_null_attribute(struct mylite_sql_parser_state *state,
                                                 struct mylite_sql_token not_token,
                                                 struct mylite_sql_token null_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_default_attribute(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_token default_token,
                                                struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_on_update_attribute(
    struct mylite_sql_parser_state *state, struct mylite_sql_token on_token,
    struct mylite_sql_token update_token, struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_comment_attribute(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_token comment_token,
                                                struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_visibility_attribute(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token visibility_token,
                                                   enum mylite_sql_ast_column_attribute visibility);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_format_attribute(
    struct mylite_sql_parser_state *state, struct mylite_sql_token column_format_token,
    struct mylite_sql_token value_token, enum mylite_sql_ast_column_format format);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_storage_attribute(
    struct mylite_sql_parser_state *state, struct mylite_sql_token storage_token,
    struct mylite_sql_token value_token, enum mylite_sql_ast_column_storage storage);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_auto_increment_attribute(
    struct mylite_sql_parser_state *state, struct mylite_sql_token auto_increment_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_primary_key_attribute(struct mylite_sql_parser_state *state,
                                                    struct mylite_sql_token start_token,
                                                    struct mylite_sql_token key_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_column_unique_key_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_column_unique_key_attribute_tokens tokens);
struct mylite_sql_ast_node *
mylite_sql_parser_make_current_timestamp(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_token current_timestamp_token,
                                         struct mylite_sql_ast_node *precision);
struct mylite_sql_ast_node *mylite_sql_parser_make_current_timestamp_empty_parens(
    struct mylite_sql_parser_state *state, struct mylite_sql_token current_timestamp_token,
    struct mylite_sql_token left_paren, struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *mylite_sql_parser_make_if_exists(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token if_token,
                                                             struct mylite_sql_token exists_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_if_not_exists(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_token if_token,
                                     struct mylite_sql_token exists_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_schema_option_list(struct mylite_sql_parser_state *state);
struct mylite_sql_ast_node *
mylite_sql_parser_append_schema_option(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *list,
                                       struct mylite_sql_ast_node *option);
struct mylite_sql_ast_node *mylite_sql_parser_make_schema_option(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    enum mylite_sql_ast_schema_option schema_option, struct mylite_sql_ast_node *value);
struct mylite_sql_ast_node *
mylite_sql_parser_make_wildcard_select_list(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_token wildcard_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_select_list(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *
mylite_sql_parser_append_select_item(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *list,
                                     struct mylite_sql_ast_node *item);
struct mylite_sql_ast_node *
mylite_sql_parser_make_select_item(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *expression);
struct mylite_sql_ast_node *
mylite_sql_parser_make_aliased_select_item(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *expression,
                                           struct mylite_sql_ast_node *alias);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token from_token,
                                                             struct mylite_sql_token dual_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_table(
    struct mylite_sql_parser_state *state, struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *alias);
struct mylite_sql_ast_node *
mylite_sql_parser_make_from_table_references(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_token from_token,
                                             struct mylite_sql_ast_node *references);
struct mylite_sql_ast_node *
mylite_sql_parser_make_table_reference_list(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_ast_node *reference);
struct mylite_sql_ast_node *
mylite_sql_parser_append_table_reference(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *list,
                                         struct mylite_sql_ast_node *reference);
struct mylite_sql_ast_node *mylite_sql_parser_make_join_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *left,
    struct mylite_sql_parser_join_operator join_operator, struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition);
struct mylite_sql_parser_join_operator
mylite_sql_parser_make_join_operator(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_token token,
                                     enum mylite_sql_ast_join_type join_type);
struct mylite_sql_ast_node *
mylite_sql_parser_make_join_on_condition(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_token on_token,
                                         struct mylite_sql_ast_node *expression);
struct mylite_sql_ast_node *mylite_sql_parser_make_join_using_condition(
    struct mylite_sql_parser_state *state, struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *columns, struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *
mylite_sql_parser_make_using_column_list(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_append_using_column(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_parser_using_column_append append);
struct mylite_sql_ast_node *
mylite_sql_parser_make_using_column(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_make_table_factor(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *table_name,
                                    struct mylite_sql_ast_node *alias);
struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_token token);
struct mylite_sql_ast_node *mylite_sql_parser_make_default(struct mylite_sql_parser_state *state,
                                                           struct mylite_sql_token token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_qualified_identifier(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_ast_node *left,
                                            struct mylite_sql_ast_node *right);
struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_wildcard(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *first,
    struct mylite_sql_ast_node *second, struct mylite_sql_token wildcard_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard(struct mylite_sql_parser_state *state,
                                                            struct mylite_sql_token token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_literal(struct mylite_sql_parser_state *state, struct mylite_sql_token token,
                               enum mylite_sql_ast_literal_kind literal_kind);
struct mylite_sql_ast_node *
mylite_sql_parser_make_bare_function_call(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_token name_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_function_call(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_token left_paren, struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *
mylite_sql_parser_make_char_function_call(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_parser_char_function_call_parts parts);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_function_call(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_token left_paren, struct mylite_sql_ast_node *first,
    struct mylite_sql_ast_node *second, struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *mylite_sql_parser_make_position_function_call(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_token left_paren, struct mylite_sql_parser_position_operands operands,
    struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *mylite_sql_parser_make_substring_for_function_call(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_token left_paren, struct mylite_sql_parser_substring_operands operands,
    struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *mylite_sql_parser_make_trim_direction_function_call(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_token left_paren, enum mylite_sql_ast_trim_direction direction,
    struct mylite_sql_parser_trim_operands operands, struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *
mylite_sql_parser_make_aggregate_star_call(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *name,
                                           struct mylite_sql_parser_aggregate_star_tokens tokens);
struct mylite_sql_ast_node *mylite_sql_parser_make_cast_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token cast_token,
    struct mylite_sql_ast_node *expression, struct mylite_sql_ast_node *target_type,
    struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *mylite_sql_parser_make_simple_case_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *base, struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_expression, struct mylite_sql_token end_token);
struct mylite_sql_ast_node *mylite_sql_parser_make_searched_case_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *when_list, struct mylite_sql_ast_node *else_expression,
    struct mylite_sql_token end_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_case_when_list(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *case_when);
struct mylite_sql_ast_node *
mylite_sql_parser_append_case_when(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *list,
                                   struct mylite_sql_ast_node *case_when);
struct mylite_sql_ast_node *mylite_sql_parser_make_case_when(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token when_token,
                                                             struct mylite_sql_ast_node *condition,
                                                             struct mylite_sql_ast_node *result);
struct mylite_sql_ast_node *
mylite_sql_parser_make_empty_function_argument_list(struct mylite_sql_parser_state *state,
                                                    struct mylite_sql_token left_paren,
                                                    struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *
mylite_sql_parser_make_function_argument_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *argument);
struct mylite_sql_ast_node *
mylite_sql_parser_append_function_argument(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *argument);
struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_column_default_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression, struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *mylite_sql_parser_make_unary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind, struct mylite_sql_ast_node *operand);
struct mylite_sql_ast_node *mylite_sql_parser_make_binary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token, enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right);
struct mylite_sql_ast_node *mylite_sql_parser_make_in_subquery_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token, enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_parser_subquery subquery);
struct mylite_sql_ast_node *mylite_sql_parser_make_quantified_comparison(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token, enum mylite_sql_ast_operator operator_kind,
    enum mylite_sql_ast_subquery_quantifier quantifier, struct mylite_sql_parser_subquery subquery);
struct mylite_sql_ast_node *mylite_sql_parser_make_ternary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *first,
    struct mylite_sql_token operator_token, enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *second, struct mylite_sql_ast_node *third);
struct mylite_sql_ast_node *
mylite_sql_parser_make_expression_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *expression);
struct mylite_sql_ast_node *
mylite_sql_parser_append_expression(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *expression);
struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression, struct mylite_sql_token right_paren);
struct mylite_sql_ast_node *
mylite_sql_parser_make_scalar_subquery_expression(struct mylite_sql_parser_state *state,
                                                  struct mylite_sql_parser_subquery subquery);
struct mylite_sql_ast_node *
mylite_sql_parser_make_exists_expression(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_token start_token,
                                         struct mylite_sql_parser_subquery subquery, bool negated);
struct mylite_sql_ast_node *
mylite_sql_parser_make_row_constructor(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_token start_token,
                                       struct mylite_sql_parser_row_constructor_elements elements,
                                       struct mylite_sql_token right_paren);

#endif

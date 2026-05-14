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
};

struct mylite_sql_char_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token length_token;
    struct mylite_sql_token end_token;
    int has_explicit_length;
};

struct mylite_sql_text_type_tokens {
    struct mylite_sql_token type_token;
    enum mylite_sql_ast_text_type text_type;
};

struct mylite_sql_binary_string_type_tokens {
    struct mylite_sql_token type_token;
    struct mylite_sql_token length_token;
    struct mylite_sql_token end_token;
    enum mylite_sql_ast_binary_string_type binary_string_type;
    int has_length;
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
    struct mylite_sql_token end_token;
    struct mylite_sql_token attribute_token;
    enum mylite_sql_ast_approximate_type approximate_type;
    int has_precision;
    int is_unsigned;
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
    struct mylite_sql_token last_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_set_names_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name,
    struct mylite_sql_ast_node *collation_name
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
struct mylite_sql_ast_node *mylite_sql_parser_make_set_system_variable_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *target,
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
struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *select_statement
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    bool is_unique,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list
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
struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
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
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_variables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token variables_token,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_table_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
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
struct mylite_sql_ast_node *mylite_sql_parser_make_show_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_database_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_engines_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token engines_token
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
    struct mylite_sql_ast_node *column
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *primary_key
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *secondary_index
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token key_token
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
    struct mylite_sql_ast_node *column_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *new_column_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_modify_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_change_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *column
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
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_order_by_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_items
);
struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_force_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name
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
    struct mylite_sql_ast_node *modifier
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
struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *table_name,
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
    struct mylite_sql_ast_node *alias
);
struct mylite_sql_ast_node *mylite_sql_parser_make_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
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
struct mylite_sql_ast_node *mylite_sql_parser_make_where_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token where_token,
    struct mylite_sql_ast_node *predicate
);
struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token group_token,
    struct mylite_sql_ast_node *group_key
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
struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
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
struct mylite_sql_ast_node *mylite_sql_parser_make_list_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
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
struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument
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
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren
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
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_unique_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token foreign_token,
    struct mylite_sql_ast_node *child_parts,
    struct mylite_sql_ast_node *referenced_table,
    struct mylite_sql_ast_node *referenced_parts,
    struct mylite_sql_token right_paren
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
    struct mylite_sql_ast_node *prefix_length
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
    struct mylite_sql_token datetime_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_timestamp_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token timestamp_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_time_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token time_token
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
struct mylite_sql_ast_node *mylite_sql_parser_append_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *identifier
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
);
struct mylite_sql_ast_node *mylite_sql_parser_append_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
);
struct mylite_sql_ast_node *mylite_sql_parser_append_insert_value(
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

#endif

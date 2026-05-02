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

struct mylite_sql_parser_drop_table_tokens {
    struct mylite_sql_token drop;
    struct mylite_sql_token temporary;
    struct mylite_sql_token mode;
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
    struct mylite_sql_ast_node *select_list, struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause, struct mylite_sql_ast_node *order_by_clause,
    struct mylite_sql_ast_node *limit_clause);
struct mylite_sql_ast_node *
mylite_sql_parser_make_where_clause(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_token where_token,
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
mylite_sql_parser_make_table_name_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *table_name);
struct mylite_sql_ast_node *
mylite_sql_parser_append_table_name(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *table_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_values_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token insert_token,
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
struct mylite_sql_ast_node *mylite_sql_parser_make_unary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind, struct mylite_sql_ast_node *operand);
struct mylite_sql_ast_node *mylite_sql_parser_make_binary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token, enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right);
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

#endif

#ifndef MYLITE_SQL_MYLITE_PARSER_INTERNAL_H
#define MYLITE_SQL_MYLITE_PARSER_INTERNAL_H

#include "mylite_ast.h"
#include "mylite_lexer.h"
#include "mylite_parser.h"

#include <stdbool.h>

struct mylite_sql_parser_state {
    struct mylite_sql_parse_result *result;
    bool accepted;
};

struct mylite_sql_parser_display_width_tokens {
    struct mylite_sql_token left_paren;
    struct mylite_sql_token integer;
    struct mylite_sql_token right_paren;
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
    struct mylite_sql_ast_node *select_list, struct mylite_sql_ast_node *from_clause);
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
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *columns);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_definition_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_append_column_definition(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *column);
struct mylite_sql_ast_node *
mylite_sql_parser_make_column_definition(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *name,
                                         struct mylite_sql_ast_node *column_type);
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
mylite_sql_parser_set_column_type_signed(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *column_type,
                                         struct mylite_sql_token signed_token);
struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_unsigned(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *column_type,
                                           struct mylite_sql_token unsigned_token);
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
struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token from_token,
                                                             struct mylite_sql_token dual_token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_from_table(struct mylite_sql_parser_state *state,
                                  struct mylite_sql_token from_token,
                                  struct mylite_sql_ast_node *table_name);
struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_token token);
struct mylite_sql_ast_node *mylite_sql_parser_make_default(struct mylite_sql_parser_state *state,
                                                           struct mylite_sql_token token);
struct mylite_sql_ast_node *
mylite_sql_parser_make_qualified_identifier(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_ast_node *left,
                                            struct mylite_sql_ast_node *right);
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
struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression, struct mylite_sql_token right_paren);

#endif

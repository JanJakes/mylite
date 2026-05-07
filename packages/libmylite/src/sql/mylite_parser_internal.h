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
    struct mylite_sql_ast_node *from_clause
);
struct mylite_sql_ast_node *mylite_sql_parser_make_use_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token use_token,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren
);
struct mylite_sql_ast_node *mylite_sql_parser_make_drop_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_show_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    struct mylite_sql_ast_node *schema_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token rename_token,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_ast_node *target_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_insert_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows
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
    struct mylite_sql_ast_node *expression
);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_token dual_token
);
struct mylite_sql_ast_node *mylite_sql_parser_make_from_table(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *table_name
);
struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(
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
struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren
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
struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *integer_type,
    struct mylite_sql_ast_node *nullability
);
struct mylite_sql_ast_node *mylite_sql_parser_make_integer_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    enum mylite_sql_ast_integer_type integer_type,
    struct mylite_sql_token unsigned_token
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

%name mylite_sql_lemon
%token_prefix MYLITE_SQL_PARSE_
%token_type { struct mylite_sql_token }
%default_type { struct mylite_sql_ast_node * }
%extra_argument { struct mylite_sql_parser_state *state }

%include {
#define YYNOERRORRECOVERY 1
#include "mylite_parser_internal.h"
}

%syntax_error {
    mylite_sql_parser_state_syntax_error(state, yymajor, TOKEN);
}

%parse_failure {
    mylite_sql_parser_state_parse_failed(state);
}

%parse_accept {
    mylite_sql_parser_state_accept(state);
}

%stack_overflow {
    mylite_sql_parser_state_stack_overflow(state);
}

%left PLUS MINUS.
%left STAR SLASH.
%right UPLUS UMINUS.

input ::= statement_list(A). {
    mylite_sql_parser_state_set_root(state, A);
}

statement_list(A) ::= . {
    A = mylite_sql_parser_make_script(state);
}
statement_list(A) ::= statements(B). {
    A = B;
}

statements(A) ::= statement(B). {
    A = mylite_sql_parser_make_script_with_statement(state, B);
}
statements(A) ::= statements(B) SEMICOLON. {
    A = B;
}
statements(A) ::= statements(B) SEMICOLON statement(C). {
    A = mylite_sql_parser_append_statement(state, B, C);
}

statement(A) ::= select_statement(B). {
    A = B;
}
statement(A) ::= use_statement(B). {
    A = B;
}
statement(A) ::= create_table_statement(B). {
    A = B;
}
statement(A) ::= drop_table_statement(B). {
    A = B;
}
statement(A) ::= show_tables_statement(B). {
    A = B;
}
statement(A) ::= rename_table_statement(B). {
    A = B;
}
statement(A) ::= insert_values_statement(B). {
    A = B;
}

use_statement(A) ::= USE(T) identifier(B). {
    A = mylite_sql_parser_make_use_statement(state, T, B);
}

create_table_statement(A) ::= CREATE(C) TABLE table_name(T) LPAREN column_definition_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_create_table_statement(state, C, T, L, R);
}

drop_table_statement(A) ::= DROP(D) TABLE table_name(T). {
    A = mylite_sql_parser_make_drop_table_statement(state, D, T);
}

show_tables_statement(A) ::= SHOW(S) TABLES(T). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, NULL);
}
show_tables_statement(A) ::= SHOW(S) TABLES(T) FROM identifier(D). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, D);
}
show_tables_statement(A) ::= SHOW(S) TABLES(T) IN identifier(D). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, D);
}

rename_table_statement(A) ::= RENAME(R) TABLE table_name(S) TO table_name(T). {
    A = mylite_sql_parser_make_rename_table_statement(state, R, S, T);
}

insert_values_statement(A) ::=
    INSERT(I) INTO table_name(T) insert_column_list_opt(C) VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R);
}

insert_column_list_opt(A) ::= . {
    A = mylite_sql_parser_make_identifier_list(state, NULL);
}
insert_column_list_opt(A) ::= LPAREN identifier_list(L) RPAREN. {
    A = L;
}

identifier_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_identifier_list(state, B);
}
identifier_list(A) ::= identifier_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_identifier(state, B, C);
}

insert_row_list(A) ::= insert_row(B). {
    A = mylite_sql_parser_make_insert_row_list(state, B);
}
insert_row_list(A) ::= insert_row_list(B) COMMA insert_row(C). {
    A = mylite_sql_parser_append_insert_row(state, B, C);
}

insert_row(A) ::= LPAREN(L) insert_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(state, L, V, R);
}

insert_value_list(A) ::= insert_value(B). {
    A = mylite_sql_parser_make_insert_row_values(state, B);
}
insert_value_list(A) ::= insert_value_list(B) COMMA insert_value(C). {
    A = mylite_sql_parser_append_insert_value(state, B, C);
}

insert_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
insert_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}

select_statement(A) ::= SELECT(T) select_item_list(B). {
    A = mylite_sql_parser_make_select_statement(state, T, B, NULL);
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_dual(state, F, D));
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) table_name(N). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, N));
}
select_statement(A) ::= SELECT(T) STAR(S). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S), NULL);
}
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D));
}
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) table_name(N). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, N));
}

select_item_list(A) ::= select_item(B). {
    A = mylite_sql_parser_make_select_list(state, B);
}
select_item_list(A) ::= select_item_list(B) COMMA select_item(C). {
    A = mylite_sql_parser_append_select_item(state, B, C);
}

select_item(A) ::= expression(B). {
    A = mylite_sql_parser_make_select_item(state, B);
}

expression(A) ::= literal(B). {
    A = B;
}
expression(A) ::= qualified_identifier(B). {
    A = B;
}
expression(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
expression(A) ::= PLUS(T) expression(B). [UPLUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_POSITIVE, B);
}
expression(A) ::= MINUS(T) expression(B). [UMINUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_NEGATIVE, B);
}
expression(A) ::= expression(B) PLUS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
expression(A) ::= expression(B) MINUS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
expression(A) ::= expression(B) STAR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
expression(A) ::= expression(B) SLASH(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_DIVIDE, C);
}

literal(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
literal(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
literal(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
literal(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
literal(A) ::= NATIONAL_STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NATIONAL_STRING);
}
literal(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
literal(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
literal(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
literal(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
literal(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}

qualified_identifier(A) ::= identifier(B). {
    A = B;
}
qualified_identifier(A) ::= qualified_identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
}

table_name(A) ::= identifier(B). {
    A = B;
}
table_name(A) ::= identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
}

identifier(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

column_definition_list(A) ::= column_definition(B). {
    A = mylite_sql_parser_make_column_definition_list(state, B);
}
column_definition_list(A) ::= column_definition_list(B) COMMA column_definition(C). {
    A = mylite_sql_parser_append_column_definition(state, B, C);
}

column_definition(A) ::= identifier(N) integer_type(T) nullability_opt(U). {
    A = mylite_sql_parser_make_column_definition(state, N, T, U);
}

integer_type(A) ::= INT(T). {
    A = mylite_sql_parser_make_integer_type(
        state, T, MYLITE_SQL_AST_INTEGER_TYPE_INT, (struct mylite_sql_token){0});
}
integer_type(A) ::= INTEGER_TYPE(T). {
    A = mylite_sql_parser_make_integer_type(
        state, T, MYLITE_SQL_AST_INTEGER_TYPE_INT, (struct mylite_sql_token){0});
}
integer_type(A) ::= BIGINT(T). {
    A = mylite_sql_parser_make_integer_type(
        state, T, MYLITE_SQL_AST_INTEGER_TYPE_BIGINT, (struct mylite_sql_token){0});
}
integer_type(A) ::= INT(T) UNSIGNED(U). {
    A = mylite_sql_parser_make_integer_type(
        state, T, MYLITE_SQL_AST_INTEGER_TYPE_INT, U);
}
integer_type(A) ::= INTEGER_TYPE(T) UNSIGNED(U). {
    A = mylite_sql_parser_make_integer_type(
        state, T, MYLITE_SQL_AST_INTEGER_TYPE_INT, U);
}
integer_type(A) ::= BIGINT(T) UNSIGNED(U). {
    A = mylite_sql_parser_make_integer_type(
        state, T, MYLITE_SQL_AST_INTEGER_TYPE_BIGINT, U);
}

nullability_opt(A) ::= . {
    A = NULL;
}
nullability_opt(A) ::= NULL(T). {
    A = mylite_sql_parser_make_nullability(
        state, MYLITE_SQL_AST_NULLABILITY_NULL, T, T);
}
nullability_opt(A) ::= NOT(N) NULL(T). {
    A = mylite_sql_parser_make_nullability(
        state, MYLITE_SQL_AST_NULLABILITY_NOT_NULL, N, T);
}

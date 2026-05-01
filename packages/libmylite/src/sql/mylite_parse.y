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
%fallback IDENTIFIER CHARSET ENCRYPTION ONLY.

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
statement(A) ::= create_schema_statement(B). {
    A = B;
}
statement(A) ::= alter_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_schema_statement(B). {
    A = B;
}
statement(A) ::= show_schemas_statement(B). {
    A = B;
}

use_statement(A) ::= USE(T) identifier(B). {
    A = mylite_sql_parser_make_use_statement(state, T, B);
}

create_schema_statement(A) ::= CREATE(T) DATABASE opt_if_not_exists(B) identifier(C) schema_create_option_list(D). {
    A = mylite_sql_parser_make_create_schema_statement(state, T, B, C, D);
}
create_schema_statement(A) ::= CREATE(T) SCHEMA opt_if_not_exists(B) identifier(C) schema_create_option_list(D). {
    A = mylite_sql_parser_make_create_schema_statement(state, T, B, C, D);
}

alter_schema_statement(A) ::= ALTER(T) DATABASE schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, NULL, C);
}
alter_schema_statement(A) ::= ALTER(T) DATABASE identifier(B) schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, B, C);
}
alter_schema_statement(A) ::= ALTER(T) SCHEMA schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, NULL, C);
}
alter_schema_statement(A) ::= ALTER(T) SCHEMA identifier(B) schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, B, C);
}

drop_schema_statement(A) ::= DROP(T) DATABASE opt_if_exists(B) identifier(C). {
    A = mylite_sql_parser_make_drop_schema_statement(state, T, B, C);
}
drop_schema_statement(A) ::= DROP(T) SCHEMA opt_if_exists(B) identifier(C). {
    A = mylite_sql_parser_make_drop_schema_statement(state, T, B, C);
}

show_schemas_statement(A) ::= SHOW(T) DATABASES(D). {
    A = mylite_sql_parser_make_show_schemas_statement(state, T, D);
}
show_schemas_statement(A) ::= SHOW(T) SCHEMAS(D). {
    A = mylite_sql_parser_make_show_schemas_statement(state, T, D);
}

opt_if_not_exists(A) ::= . {
    A = NULL;
}
opt_if_not_exists(A) ::= IF(I) NOT EXISTS(E). {
    A = mylite_sql_parser_make_if_not_exists(state, I, E);
}

opt_if_exists(A) ::= . {
    A = NULL;
}
opt_if_exists(A) ::= IF(I) EXISTS(E). {
    A = mylite_sql_parser_make_if_exists(state, I, E);
}

schema_create_option_list(A) ::= . {
    A = mylite_sql_parser_make_schema_option_list(state);
}
schema_create_option_list(A) ::= schema_create_option_list(B) schema_create_option(C). {
    A = mylite_sql_parser_append_schema_option(state, B, C);
}

schema_alter_option_list(A) ::= schema_alter_option(B). {
    A = mylite_sql_parser_append_schema_option(
        state, mylite_sql_parser_make_schema_option_list(state), B);
}
schema_alter_option_list(A) ::= schema_alter_option_list(B) schema_alter_option(C). {
    A = mylite_sql_parser_append_schema_option(state, B, C);
}

schema_create_option(A) ::= schema_common_option(B). {
    A = B;
}

schema_alter_option(A) ::= schema_common_option(B). {
    A = B;
}
schema_alter_option(A) ::= READ(R) ONLY opt_equal read_only_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, R, MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY, B);
}

schema_common_option(A) ::= opt_default CHARACTER(T) SET opt_equal schema_option_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET, B);
}
schema_common_option(A) ::= opt_default CHARSET(T) opt_equal schema_option_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET, B);
}
schema_common_option(A) ::= opt_default COLLATE(T) opt_equal schema_option_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE, B);
}
schema_common_option(A) ::= opt_default ENCRYPTION(T) opt_equal STRING(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION,
        mylite_sql_parser_make_literal(state, B, MYLITE_SQL_AST_LITERAL_STRING));
}

schema_option_value(A) ::= identifier(B). {
    A = B;
}
schema_option_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

read_only_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
read_only_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}

opt_default ::= .
opt_default ::= DEFAULT.

opt_equal ::= .
opt_equal ::= EQ.

select_statement(A) ::= SELECT(T) select_item_list(B). {
    A = mylite_sql_parser_make_select_statement(state, T, B, NULL);
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_dual(state, F, D));
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) table_name(C). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, C));
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
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) table_name(C). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, C));
}

table_name(A) ::= qualified_identifier(B). {
    A = B;
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

identifier(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

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
statement(A) ::= create_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_table_statement(B). {
    A = B;
}
statement(A) ::= drop_schema_statement(B). {
    A = B;
}
statement(A) ::= truncate_table_statement(B). {
    A = B;
}
statement(A) ::= show_tables_statement(B). {
    A = B;
}
statement(A) ::= show_databases_statement(B). {
    A = B;
}
statement(A) ::= rename_table_statement(B). {
    A = B;
}
statement(A) ::= insert_values_statement(B). {
    A = B;
}
statement(A) ::= delete_statement(B). {
    A = B;
}
statement(A) ::= update_statement(B). {
    A = B;
}

use_statement(A) ::= USE(T) identifier(B). {
    A = mylite_sql_parser_make_use_statement(state, T, B);
}

create_table_statement(A) ::= CREATE(C) TABLE table_name(T) LPAREN column_definition_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_create_table_statement(state, C, T, L, R);
}

create_schema_statement(A) ::= CREATE(C) DATABASE identifier(S). {
    A = mylite_sql_parser_make_create_schema_statement(state, C, S);
}
create_schema_statement(A) ::= CREATE(C) SCHEMA identifier(S). {
    A = mylite_sql_parser_make_create_schema_statement(state, C, S);
}

drop_table_statement(A) ::= DROP(D) TABLE table_name(T). {
    A = mylite_sql_parser_make_drop_table_statement(state, D, T);
}

drop_schema_statement(A) ::= DROP(D) DATABASE identifier(S). {
    A = mylite_sql_parser_make_drop_schema_statement(state, D, S);
}
drop_schema_statement(A) ::= DROP(D) SCHEMA identifier(S). {
    A = mylite_sql_parser_make_drop_schema_statement(state, D, S);
}

truncate_table_statement(A) ::= TRUNCATE(T) table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
}
truncate_table_statement(A) ::= TRUNCATE(T) TABLE table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
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

show_databases_statement(A) ::= SHOW(S) DATABASES(D). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D);
}
show_databases_statement(A) ::= SHOW(S) SCHEMAS(D). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D);
}

rename_table_statement(A) ::= RENAME(R) TABLE table_name(S) TO table_name(T). {
    A = mylite_sql_parser_make_rename_table_statement(state, R, S, T);
}

insert_values_statement(A) ::=
    INSERT(I) INTO table_name(T) insert_column_list_opt(C) VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R);
}

delete_statement(A) ::=
    DELETE(D) FROM table_name(T) where_clause_opt(W) order_clause_opt(O) delete_limit_clause_opt(L). {
    A = mylite_sql_parser_make_delete_statement(state, D, T, W, O, L);
}

update_statement(A) ::=
    UPDATE(U) table_name(T) SET update_assignment_list(S) where_clause_opt(W)
    order_clause_opt(O) update_limit_clause_opt(L). {
    A = mylite_sql_parser_make_update_statement(state, U, T, S, W, O, L);
}

update_assignment_list(A) ::= update_assignment(B). {
    A = mylite_sql_parser_make_update_assignment_list(state, B);
}
update_assignment_list(A) ::= update_assignment_list(B) COMMA update_assignment(C). {
    A = mylite_sql_parser_append_update_assignment(state, B, C);
}

update_assignment(A) ::= qualified_identifier(T) EQUAL(E) update_value(V). {
    A = mylite_sql_parser_make_update_assignment(state, T, E, V);
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

update_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
update_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}

select_statement(A) ::= SELECT(T) select_item_list(B). {
    A = mylite_sql_parser_make_select_statement(state, T, B, NULL, NULL, NULL, NULL);
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL);
}
select_statement(A) ::=
    SELECT(T) select_item_list(B) FROM(F) table_name(N) where_clause_opt(W)
    order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, N), W, O, L);
}
select_statement(A) ::= SELECT(T) STAR(S). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S), NULL, NULL, NULL, NULL);
}
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL);
}
select_statement(A) ::=
    SELECT(T) STAR(S) FROM(F) table_name(N) where_clause_opt(W)
    order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, N), W, O, L);
}

where_clause_opt(A) ::= . {
    A = NULL;
}
where_clause_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

predicate(A) ::= predicate_atom(B). {
    A = B;
}
predicate(A) ::= LPAREN(L) predicate(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

predicate_atom(A) ::= qualified_identifier(C) EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) NULL_SAFE_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) NOT_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) LESS(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
predicate_atom(A) ::= qualified_identifier(C) LESS_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) GREATER(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
predicate_atom(A) ::= qualified_identifier(C) GREATER_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}

predicate_integer_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
predicate_integer_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
predicate_integer_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}

order_clause_opt(A) ::= . {
    A = NULL;
}
order_clause_opt(A) ::= ORDER(O) BY qualified_identifier(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_clause(state, O, K, D);
}

order_direction_opt(A) ::= . {
    A = NULL;
}
order_direction_opt(A) ::= ASC(T). {
    A = mylite_sql_parser_make_order_direction(
        state, T, MYLITE_SQL_AST_ORDER_DIRECTION_ASC);
}
order_direction_opt(A) ::= DESC(T). {
    A = mylite_sql_parser_make_order_direction(
        state, T, MYLITE_SQL_AST_ORDER_DIRECTION_DESC);
}

limit_clause_opt(A) ::= . {
    A = NULL;
}
limit_clause_opt(A) ::= LIMIT(L) limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, NULL);
}
limit_clause_opt(A) ::= LIMIT(L) limit_integer(C) OFFSET limit_integer(O). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, O);
}
limit_clause_opt(A) ::= LIMIT(L) limit_integer(O) COMMA limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, O);
}

delete_limit_clause_opt(A) ::= . {
    A = NULL;
}
delete_limit_clause_opt(A) ::= LIMIT(L) limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, NULL);
}

update_limit_clause_opt(A) ::= . {
    A = NULL;
}
update_limit_clause_opt(A) ::= LIMIT(L) limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, NULL);
}

limit_integer(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
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
expression(A) ::= DATABASE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_DATABASE_FUNCTION, R);
}
expression(A) ::= SCHEMA(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_SCHEMA_FUNCTION, R);
}
expression(A) ::= USER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_USER_FUNCTION, R);
}
expression(A) ::= SESSION_USER(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_SESSION_USER_FUNCTION, R);
}
expression(A) ::= SYSTEM_USER(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_SYSTEM_USER_FUNCTION, R);
}
expression(A) ::= CURRENT_USER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_USER_FUNCTION, R);
}
expression(A) ::= CURRENT_USER(T). {
    A = mylite_sql_parser_make_current_user_keyword(state, T);
}
expression(A) ::= VERSION(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_VERSION_FUNCTION, R);
}
expression(A) ::= VERSION(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= ROW_COUNT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_ROW_COUNT_FUNCTION, R);
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

function_argument_list(A) ::= expression(B). {
    A = mylite_sql_parser_make_function_argument_list(state, B);
}
function_argument_list(A) ::= function_argument_list(B) COMMA expression(C). {
    A = mylite_sql_parser_append_function_argument(state, B, C);
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
identifier(A) ::= USER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SESSION_USER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SYSTEM_USER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= VERSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ROW_COUNT(T). {
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

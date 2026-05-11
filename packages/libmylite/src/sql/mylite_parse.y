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

%left OR.
%left XOR.
%left AND.
%right NOT.
%left EQUAL NULL_SAFE_EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL IS.
%left BITWISE_OR.
%left BITWISE_AND.
%left LEFT_SHIFT RIGHT_SHIFT.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%left BITWISE_XOR.
%right UPLUS UMINUS BITWISE_NOT.

%type integer_type_name { struct mylite_sql_integer_type_name_tokens }
%type integer_display_width_opt { struct mylite_sql_integer_display_width_tokens }
%type integer_signedness_opt { struct mylite_sql_integer_signedness_tokens }
%type select_modifiers { struct mylite_sql_select_modifiers }
%type select_duplicate_modifier_opt { enum mylite_sql_ast_select_modifier }
%type select_high_priority_opt { int }
%type select_straight_join_opt { int }
%type select_sql_small_result_opt { int }
%type select_sql_big_result_opt { int }
%type select_sql_buffer_result_opt { int }
%type select_sql_no_cache_opt { int }
%type select_sql_calc_found_rows_opt { int }
%type select_locking_clause_opt { struct mylite_sql_select_locking_clause }

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
statement(A) ::= set_connection_charset_statement(B). {
    A = B;
}
statement(A) ::= set_system_variable_statement(B). {
    A = B;
}
statement(A) ::= create_table_statement(B). {
    A = B;
}
statement(A) ::= create_table_like_statement(B). {
    A = B;
}
statement(A) ::= create_table_select_statement(B). {
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
statement(A) ::= show_table_status_statement(B). {
    A = B;
}
statement(A) ::= show_character_set_statement(B). {
    A = B;
}
statement(A) ::= show_collation_statement(B). {
    A = B;
}
statement(A) ::= show_triggers_statement(B). {
    A = B;
}
statement(A) ::= show_events_statement(B). {
    A = B;
}
statement(A) ::= show_open_tables_statement(B). {
    A = B;
}
statement(A) ::= show_routine_status_statement(B). {
    A = B;
}
statement(A) ::= show_processlist_statement(B). {
    A = B;
}
statement(A) ::= show_warnings_statement(B). {
    A = B;
}
statement(A) ::= show_errors_statement(B). {
    A = B;
}
statement(A) ::= show_columns_statement(B). {
    A = B;
}
statement(A) ::= show_index_statement(B). {
    A = B;
}
statement(A) ::= show_create_table_statement(B). {
    A = B;
}
statement(A) ::= show_create_database_statement(B). {
    A = B;
}
statement(A) ::= show_engines_statement(B). {
    A = B;
}
statement(A) ::= show_databases_statement(B). {
    A = B;
}
statement(A) ::= show_variables_statement(B). {
    A = B;
}
statement(A) ::= describe_table_statement(B). {
    A = B;
}
statement(A) ::= explain_table_statement(B). {
    A = B;
}
statement(A) ::= rename_table_statement(B). {
    A = B;
}
statement(A) ::= alter_table_rename_statement(B). {
    A = B;
}
statement(A) ::= alter_table_add_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_rename_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_modify_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_change_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_set_default_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_default_statement(B). {
    A = B;
}
statement(A) ::= alter_table_column_visibility_statement(B). {
    A = B;
}
statement(A) ::= alter_table_default_charset_collation_statement(B). {
    A = B;
}
statement(A) ::= alter_table_order_by_statement(B). {
    A = B;
}
statement(A) ::= alter_table_force_statement(B). {
    A = B;
}
statement(A) ::= insert_values_statement(B). {
    A = B;
}
statement(A) ::= insert_select_statement(B). {
    A = B;
}
statement(A) ::= replace_values_statement(B). {
    A = B;
}
statement(A) ::= replace_select_statement(B). {
    A = B;
}
statement(A) ::= replace_set_statement(B). {
    A = B;
}
statement(A) ::= insert_set_statement(B). {
    A = B;
}
statement(A) ::= delete_statement(B). {
    A = B;
}
statement(A) ::= update_statement(B). {
    A = B;
}
statement(A) ::= do_statement(B). {
    A = B;
}

use_statement(A) ::= USE(T) identifier(B). {
    A = mylite_sql_parser_make_use_statement(state, T, B);
}

set_connection_charset_statement(A) ::= SET(S) NAMES option_name(C) set_names_collate_opt(L). {
    A = mylite_sql_parser_make_set_names_statement(state, S, C, L);
}
set_connection_charset_statement(A) ::= SET(S) NAMES DEFAULT(D). {
    A = mylite_sql_parser_make_set_names_statement(
        state,
        S,
        mylite_sql_parser_make_set_character_set_default_target(state, D),
        NULL);
}
set_connection_charset_statement(A) ::= SET(S) CHARACTER SET option_name(C). {
    A = mylite_sql_parser_make_set_character_set_statement(state, S, C);
}
set_connection_charset_statement(A) ::= SET(S) CHARACTER SET DEFAULT(D). {
    A = mylite_sql_parser_make_set_character_set_statement(
        state,
        S,
        mylite_sql_parser_make_set_character_set_default_target(state, D));
}
set_connection_charset_statement(A) ::= SET(S) CHARSET option_name(C). {
    A = mylite_sql_parser_make_set_character_set_statement(state, S, C);
}
set_connection_charset_statement(A) ::= SET(S) CHARSET DEFAULT(D). {
    A = mylite_sql_parser_make_set_character_set_statement(
        state,
        S,
        mylite_sql_parser_make_set_character_set_default_target(state, D));
}

set_names_collate_opt(A) ::= . {
    A = NULL;
}
set_names_collate_opt(A) ::= COLLATE option_name(C). {
    A = C;
}

set_system_variable_statement(A) ::=
    SET(S) set_system_variable_target(T) EQUAL set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_system_variable_statement(state, S, T, V);
}

set_system_variable_target(A) ::= identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(state, NULL, N);
}
set_system_variable_target(A) ::= SESSION(S) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, S),
        N);
}
set_system_variable_target(A) ::= LOCAL(L) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, L),
        N);
}
set_system_variable_target(A) ::= GLOBAL(G) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, G),
        N);
}
set_system_variable_target(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        NULL,
        mylite_sql_parser_make_system_variable(state, T));
}

set_system_variable_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_set_default_value(state, T);
}
set_system_variable_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
set_system_variable_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
set_system_variable_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
set_system_variable_value(A) ::= ON(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
set_system_variable_value(A) ::= OFF(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
set_system_variable_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

create_table_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LPAREN
    column_definition_list(L) RPAREN(R) table_option_list_opt(O). {
    A = mylite_sql_parser_make_create_table_statement(state, C, E, T, L, R, O);
}
create_table_like_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LIKE table_name(S). {
    A = mylite_sql_parser_make_create_table_like_statement(state, C, E, T, S);
}
create_table_like_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LPAREN LIKE table_name(S)
    RPAREN. {
    A = mylite_sql_parser_make_create_table_like_statement(state, C, E, T, S);
}
create_table_select_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) create_table_select_as_opt
    select_statement(S). {
    A = mylite_sql_parser_make_create_table_select_statement(state, C, E, T, S);
}

create_table_select_as_opt ::= .
create_table_select_as_opt ::= AS.

create_if_not_exists_opt(A) ::= . {
    A = NULL;
}
create_if_not_exists_opt(A) ::= IF(I) NOT EXISTS(E). {
    A = mylite_sql_parser_make_create_if_not_exists_clause(state, I, E);
}

table_option_list_opt(A) ::= . {
    A = NULL;
}
table_option_list_opt(A) ::= table_option_list(B). {
    A = B;
}

table_option_list(A) ::= table_option(B). {
    A = mylite_sql_parser_make_table_option_list(state, B);
}
table_option_list(A) ::= table_option_list(B) table_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

table_option(A) ::= ENGINE(E) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_engine_option(state, E, N);
}
table_option(A) ::= default_opt CHARSET(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
table_option(A) ::= default_opt CHARACTER(C) SET equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
table_option(A) ::= default_opt COLLATE(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}

default_opt ::= .
default_opt ::= DEFAULT.

equal_opt ::= .
equal_opt ::= EQUAL.

option_name(A) ::= identifier(B). {
    A = B;
}
option_name(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

create_schema_statement(A) ::= CREATE(C) DATABASE create_schema_if_not_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_create_schema_statement(state, C, E, S);
}
create_schema_statement(A) ::= CREATE(C) SCHEMA create_schema_if_not_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_create_schema_statement(state, C, E, S);
}

create_schema_if_not_exists_opt(A) ::= . {
    A = NULL;
}
create_schema_if_not_exists_opt(A) ::= IF(I) NOT EXISTS(E). {
    A = mylite_sql_parser_make_create_schema_if_not_exists_clause(state, I, E);
}

drop_table_statement(A) ::= DROP(D) TABLE drop_if_exists_opt(E) table_name_list(T). {
    A = mylite_sql_parser_make_drop_table_statement(state, D, E, T);
}

drop_if_exists_opt(A) ::= . {
    A = NULL;
}
drop_if_exists_opt(A) ::= IF(I) EXISTS(E). {
    A = mylite_sql_parser_make_drop_if_exists_clause(state, I, E);
}

table_name_list(A) ::= table_name(T). {
    A = mylite_sql_parser_make_table_name_list(state, T);
}
table_name_list(A) ::= table_name_list(L) COMMA table_name(T). {
    A = mylite_sql_parser_append_table_name(state, L, T);
}

drop_schema_statement(A) ::= DROP(D) DATABASE drop_schema_if_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_drop_schema_statement(state, D, E, S);
}
drop_schema_statement(A) ::= DROP(D) SCHEMA drop_schema_if_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_drop_schema_statement(state, D, E, S);
}

drop_schema_if_exists_opt(A) ::= . {
    A = NULL;
}
drop_schema_if_exists_opt(A) ::= IF(I) EXISTS(E). {
    A = mylite_sql_parser_make_drop_schema_if_exists_clause(state, I, E);
}

truncate_table_statement(A) ::= TRUNCATE(T) table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
}
truncate_table_statement(A) ::= TRUNCATE(T) TABLE table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
}

show_tables_statement(A) ::= SHOW(S) TABLES(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, NULL, L);
}
show_tables_statement(A) ::= SHOW(S) TABLES(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, D, L);
}
show_tables_statement(A) ::= SHOW(S) TABLES(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, D, L);
}

show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, NULL, L);
}
show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, L);
}
show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, L);
}

show_character_set_statement(A) ::= SHOW(S) CHARACTER SET(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_character_set_statement(state, S, T, L);
}
show_character_set_statement(A) ::= SHOW(S) CHARSET(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_character_set_statement(state, S, T, L);
}

show_collation_statement(A) ::= SHOW(S) COLLATION(C) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_collation_statement(state, S, C, L);
}

show_triggers_statement(A) ::= SHOW(S) TRIGGERS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, NULL, L);
}
show_triggers_statement(A) ::= SHOW(S) FULL TRIGGERS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, NULL, L);
}
show_triggers_statement(A) ::= SHOW(S) TRIGGERS(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}
show_triggers_statement(A) ::= SHOW(S) TRIGGERS(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}
show_triggers_statement(A) ::= SHOW(S) FULL TRIGGERS(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}
show_triggers_statement(A) ::= SHOW(S) FULL TRIGGERS(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}

show_events_statement(A) ::= SHOW(S) EVENTS(E) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, NULL, L);
}
show_events_statement(A) ::= SHOW(S) EVENTS(E) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, D, L);
}
show_events_statement(A) ::= SHOW(S) EVENTS(E) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, D, L);
}

show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, NULL, L);
}
show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, D, L);
}
show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, D, L);
}

show_routine_status_statement(A) ::= SHOW(S) PROCEDURE STATUS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_routine_status_statement(
        state, S, T, MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT, L);
}
show_routine_status_statement(A) ::= SHOW(S) FUNCTION STATUS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_routine_status_statement(
        state, S, T, MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT, L);
}

show_processlist_statement(A) ::= SHOW(S) PROCESSLIST(P). {
    A = mylite_sql_parser_make_show_processlist_statement(
        state, S, P, MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT);
}
show_processlist_statement(A) ::= SHOW(S) FULL PROCESSLIST(P). {
    A = mylite_sql_parser_make_show_processlist_statement(
        state, S, P, MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT);
}

show_warnings_statement(A) ::= SHOW(S) WARNINGS(W) limit_clause_opt(L). {
    A = mylite_sql_parser_make_show_warnings_statement(state, S, W, L);
}
show_warnings_statement(A) ::= SHOW(S) COUNT(C) LPAREN(L) STAR RPAREN WARNINGS(W). {
    A = mylite_sql_parser_make_show_count_warnings_statement(
        state,
        (struct mylite_sql_show_count_warnings_tokens){
            .show = S,
            .count = C,
            .left_paren = L,
            .warnings = W,
        });
}

show_errors_statement(A) ::= SHOW(S) ERRORS(E) limit_clause_opt(L). {
    A = mylite_sql_parser_make_show_errors_statement(state, S, E, L);
}
show_errors_statement(A) ::= SHOW(S) COUNT(C) LPAREN(L) STAR RPAREN ERRORS(E). {
    A = mylite_sql_parser_make_show_count_errors_statement(
        state,
        (struct mylite_sql_show_count_errors_tokens){
            .show = S,
            .count = C,
            .left_paren = L,
            .errors = E,
        });
}

show_columns_statement(A) ::= SHOW(S) COLUMNS FROM table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS IN table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS FROM table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS IN table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS FROM table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS FROM table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS IN table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS IN table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS FROM table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS FROM table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS IN table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS IN table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}

show_index_statement(A) ::= SHOW(S) show_index_keyword FROM table_name(T). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, NULL);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword IN table_name(T). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, NULL);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword FROM table_name(T) FROM identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword FROM table_name(T) IN identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword IN table_name(T) FROM identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword IN table_name(T) IN identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}

show_index_keyword ::= INDEX.
show_index_keyword ::= INDEXES.
show_index_keyword ::= KEYS.

show_databases_statement(A) ::= SHOW(S) DATABASES(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, L);
}
show_databases_statement(A) ::= SHOW(S) SCHEMAS(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, L);
}

show_variables_statement(A) ::= SHOW(S) show_variables_scope_opt(O) VARIABLES(V) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, L);
}

show_variables_scope_opt(A) ::= . {
    A = NULL;
}
show_variables_scope_opt(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_variables_scope_opt(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_variables_scope_opt(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

show_create_table_statement(A) ::= SHOW(S) CREATE TABLE table_name(T). {
    A = mylite_sql_parser_make_show_create_table_statement(state, S, T);
}

show_create_database_statement(A) ::= SHOW(S) CREATE DATABASE identifier(D). {
    A = mylite_sql_parser_make_show_create_database_statement(state, S, D);
}
show_create_database_statement(A) ::= SHOW(S) CREATE SCHEMA identifier(D). {
    A = mylite_sql_parser_make_show_create_database_statement(state, S, D);
}

show_engines_statement(A) ::= SHOW(S) ENGINES(E). {
    A = mylite_sql_parser_make_show_engines_statement(state, S, E);
}
show_engines_statement(A) ::= SHOW(S) STORAGE ENGINES(E). {
    A = mylite_sql_parser_make_show_engines_statement(state, S, E);
}

describe_table_statement(A) ::= DESCRIBE(D) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, D, T, NULL, NULL);
}
describe_table_statement(A) ::= DESC(D) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, D, T, NULL, NULL);
}

explain_table_statement(A) ::= EXPLAIN(E) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, E, T, NULL, NULL);
}

show_like_clause_opt(A) ::= . {
    A = NULL;
}
show_like_clause_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}

rename_table_statement(A) ::= RENAME(R) TABLE rename_table_pair_list(L). {
    A = mylite_sql_parser_make_rename_table_statement(state, R, L);
}

rename_table_pair_list(A) ::= rename_table_pair(P). {
    A = mylite_sql_parser_make_rename_table_pair_list(state, P);
}
rename_table_pair_list(A) ::= rename_table_pair_list(L) COMMA rename_table_pair(P). {
    A = mylite_sql_parser_append_rename_table_pair(state, L, P);
}

rename_table_pair(A) ::= table_name(S) TO(T) table_name(N). {
    A = mylite_sql_parser_make_rename_table_pair(state, S, T, N);
}

alter_table_rename_statement(A) ::=
    ALTER(A1) TABLE table_name(S) RENAME table_rename_connector_opt table_name(T). {
    A = mylite_sql_parser_make_alter_table_rename_statement(state, A1, S, T);
}

alter_table_add_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD column_keyword_opt column_definition(C). {
    A = mylite_sql_parser_make_alter_table_add_column_statement(state, A1, T, C);
}

alter_table_drop_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP column_keyword_opt identifier(C). {
    A = mylite_sql_parser_make_alter_table_drop_column_statement(state, A1, T, C);
}

alter_table_rename_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) RENAME COLUMN identifier(O) TO identifier(N). {
    A = mylite_sql_parser_make_alter_table_rename_column_statement(state, A1, T, O, N);
}

alter_table_modify_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) MODIFY column_keyword_opt column_definition(C). {
    A = mylite_sql_parser_make_alter_table_modify_column_statement(state, A1, T, C);
}

alter_table_change_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CHANGE column_keyword_opt identifier(O) column_definition(C). {
    A = mylite_sql_parser_make_alter_table_change_column_statement(state, A1, T, O, C);
}

alter_table_set_default_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, A1, T, C, mylite_sql_parser_make_column_default_null(state, D, N));
}
alter_table_set_default_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET DEFAULT(D)
    column_default_value(V). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, A1, T, C, mylite_sql_parser_make_column_default_value(state, D, V));
}

alter_table_drop_default_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) DROP DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_drop_default_statement(state, A1, T, C, D);
}

alter_table_column_visibility_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET VISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_column_visibility_statement(
        state, A1, T, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE);
}
alter_table_column_visibility_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET INVISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_column_visibility_statement(
        state, A1, T, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE);
}

alter_table_default_charset_collation_statement(A) ::=
    ALTER(A1) TABLE table_name(T) alter_table_default_charset_collation_option_list(O). {
    A = mylite_sql_parser_make_alter_table_default_charset_collation_statement(
        state, A1, T, O);
}

alter_table_default_charset_collation_option_list(A) ::=
    alter_table_default_charset_collation_option(B). {
    A = mylite_sql_parser_make_table_option_list(state, B);
}
alter_table_default_charset_collation_option_list(A) ::=
    alter_table_default_charset_collation_option_list(B)
    alter_table_default_charset_collation_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

alter_table_default_charset_collation_option(A) ::=
    default_opt CHARSET(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
alter_table_default_charset_collation_option(A) ::=
    default_opt CHARACTER(C) SET equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
alter_table_default_charset_collation_option(A) ::=
    default_opt COLLATE(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}

alter_table_order_by_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ORDER BY alter_table_order_item_list(O). {
    A = mylite_sql_parser_make_alter_table_order_by_statement(state, A1, T, O);
}

alter_table_order_item_list(A) ::= alter_table_order_item(I). {
    A = mylite_sql_parser_make_order_by_item_list(state, I);
}
alter_table_order_item_list(A) ::= alter_table_order_item_list(L) COMMA alter_table_order_item(I). {
    A = mylite_sql_parser_append_order_by_item(state, L, I);
}

alter_table_order_item(A) ::= qualified_identifier(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_item(state, K, D);
}

alter_table_force_statement(A) ::= ALTER(A1) TABLE table_name(T) FORCE. {
    A = mylite_sql_parser_make_alter_table_force_statement(state, A1, T);
}

column_keyword_opt(A) ::= . {
    A = NULL;
}
column_keyword_opt(A) ::= COLUMN. {
    A = NULL;
}

table_rename_connector_opt(A) ::= . {
    A = NULL;
}
table_rename_connector_opt(A) ::= TO. {
    A = NULL;
}
table_rename_connector_opt(A) ::= AS. {
    A = NULL;
}

insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G)
    );
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G)
    );
}

insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M);
}

insert_modifier_opt(A) ::= . {
    A = NULL;
}
insert_modifier_opt(A) ::= LOW_PRIORITY(T). {
    A = mylite_sql_parser_make_insert_low_priority_modifier(state, T);
}
insert_modifier_opt(A) ::= HIGH_PRIORITY(T). {
    A = mylite_sql_parser_make_insert_high_priority_modifier(state, T);
}
insert_modifier_opt(A) ::= DELAYED(T). {
    A = mylite_sql_parser_make_insert_delayed_modifier(state, T);
}

replace_values_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V, M);
}
replace_values_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V, M);
}

replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S, M);
}
replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S, M);
}

replace_set_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_replace_set_statement(state, R, T, S, M);
}
replace_set_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_replace_set_statement(state, R, T, S, M);
}

replace_modifier_opt(A) ::= . {
    A = NULL;
}
replace_modifier_opt(A) ::= LOW_PRIORITY(T). {
    A = mylite_sql_parser_make_replace_low_priority_modifier(state, T);
}
replace_modifier_opt(A) ::= DELAYED(T). {
    A = mylite_sql_parser_make_replace_delayed_modifier(state, T);
}

insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, NULL);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(
        state, I, T, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G)
    );
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, NULL);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(
        state, I, T, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G)
    );
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

do_statement(A) ::= DO(T) do_expression_list(E). {
    A = mylite_sql_parser_make_do_statement(state, T, E);
}

do_expression_list(A) ::= expression(B). {
    A = mylite_sql_parser_make_do_expression_list(state, B);
}
do_expression_list(A) ::= do_expression_list(B) COMMA expression(C). {
    A = mylite_sql_parser_append_do_expression(state, B, C);
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

insert_assignment_list(A) ::= insert_assignment(B). {
    A = mylite_sql_parser_make_insert_assignment_list(state, B);
}
insert_assignment_list(A) ::= insert_assignment_list(B) COMMA insert_assignment(C). {
    A = mylite_sql_parser_append_insert_assignment(state, B, C);
}

insert_assignment(A) ::= qualified_identifier(T) EQUAL(E) insert_value(V). {
    A = mylite_sql_parser_make_insert_assignment(state, T, E, V);
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
insert_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
insert_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
insert_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
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
update_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
update_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
update_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}

select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
    select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, NULL, NULL, NULL, NULL, NULL, NULL, K);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) DUAL(D)
    select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL, NULL,
        NULL, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) table_name(N) table_alias_opt(AL)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) order_clause_opt(O)
    limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL), W, G, H, O, L, K);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) STAR(S) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        NULL, NULL, NULL, NULL, NULL, NULL, K);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) STAR(S) FROM(F) DUAL(D)
    select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL, NULL, NULL, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) STAR(S) FROM(F) table_name(N) table_alias_opt(AL)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) order_clause_opt(O)
    limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, N, AL), W, G, H, O, L, K);
}

select_modifiers(A) ::=
    select_duplicate_modifier_opt(D) select_high_priority_opt(HP) select_straight_join_opt(SJ)
    select_sql_small_result_opt(SM) select_sql_big_result_opt(BG)
    select_sql_buffer_result_opt(BF) select_sql_no_cache_opt(NC)
    select_sql_calc_found_rows_opt(CF). {
    A = (struct mylite_sql_select_modifiers){
        .duplicate_modifier = D,
        .options =
            (HP ? MYLITE_SQL_AST_SELECT_OPTION_HIGH_PRIORITY : 0U) |
            (SJ ? MYLITE_SQL_AST_SELECT_OPTION_STRAIGHT_JOIN : 0U) |
            (SM ? MYLITE_SQL_AST_SELECT_OPTION_SQL_SMALL_RESULT : 0U) |
            (BG ? MYLITE_SQL_AST_SELECT_OPTION_SQL_BIG_RESULT : 0U) |
            (BF ? MYLITE_SQL_AST_SELECT_OPTION_SQL_BUFFER_RESULT : 0U) |
            (NC ? MYLITE_SQL_AST_SELECT_OPTION_SQL_NO_CACHE : 0U),
        .calc_found_rows = CF,
    };
}
select_duplicate_modifier_opt(A) ::= . {
    A = MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT;
}
select_duplicate_modifier_opt(A) ::= ALL. {
    A = MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT;
}
select_duplicate_modifier_opt(A) ::= DISTINCT. {
    A = MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
}
select_duplicate_modifier_opt(A) ::= DISTINCTROW. {
    A = MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
}
select_high_priority_opt(A) ::= . {
    A = 0;
}
select_high_priority_opt(A) ::= HIGH_PRIORITY. {
    A = 1;
}
select_straight_join_opt(A) ::= . {
    A = 0;
}
select_straight_join_opt(A) ::= STRAIGHT_JOIN. {
    A = 1;
}
select_sql_small_result_opt(A) ::= . {
    A = 0;
}
select_sql_small_result_opt(A) ::= SQL_SMALL_RESULT. {
    A = 1;
}
select_sql_big_result_opt(A) ::= . {
    A = 0;
}
select_sql_big_result_opt(A) ::= SQL_BIG_RESULT. {
    A = 1;
}
select_sql_buffer_result_opt(A) ::= . {
    A = 0;
}
select_sql_buffer_result_opt(A) ::= SQL_BUFFER_RESULT. {
    A = 1;
}
select_sql_no_cache_opt(A) ::= . {
    A = 0;
}
select_sql_no_cache_opt(A) ::= SQL_NO_CACHE. {
    A = 1;
}
select_sql_calc_found_rows_opt(A) ::= . {
    A = 0;
}
select_sql_calc_found_rows_opt(A) ::= SQL_CALC_FOUND_ROWS. {
    A = 1;
}

select_locking_clause_opt(A) ::= . {
    A = (struct mylite_sql_select_locking_clause){
        .kind = MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE,
        .span = {0},
    };
}
select_locking_clause_opt(A) ::= FOR(F) UPDATE(U). {
    A = (struct mylite_sql_select_locking_clause){
        .kind = MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        .span = {
            .text = F.text,
            .length = (U.offset + U.length) - F.offset,
            .offset = F.offset,
            .line = F.line,
            .column = F.column,
        },
    };
}
select_locking_clause_opt(A) ::= FOR(F) SHARE(S). {
    A = (struct mylite_sql_select_locking_clause){
        .kind = MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        .span = {
            .text = F.text,
            .length = (S.offset + S.length) - F.offset,
            .offset = F.offset,
            .line = F.line,
            .column = F.column,
        },
    };
}
select_locking_clause_opt(A) ::= LOCK(L) IN SHARE MODE(M). {
    A = (struct mylite_sql_select_locking_clause){
        .kind = MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_LOCK_IN_SHARE_MODE,
        .span = {
            .text = L.text,
            .length = (M.offset + M.length) - L.offset,
            .offset = L.offset,
            .line = L.line,
            .column = L.column,
        },
    };
}

table_alias_opt(A) ::= . {
    A = NULL;
}
table_alias_opt(A) ::= AS identifier(B). {
    A = B;
}
table_alias_opt(A) ::= identifier(B). {
    A = B;
}

where_clause_opt(A) ::= . {
    A = NULL;
}
where_clause_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

group_clause_opt(A) ::= . {
    A = NULL;
}
group_clause_opt(A) ::= GROUP(G) BY qualified_identifier(K). {
    A = mylite_sql_parser_make_group_by_clause(state, G, K);
}

having_clause_opt(A) ::= . {
    A = NULL;
}
having_clause_opt(A) ::= HAVING(H) having_predicate(P). {
    A = mylite_sql_parser_make_having_clause(state, H, P);
}

having_predicate(A) ::= having_predicate_atom(B). {
    A = B;
}
having_predicate(A) ::= LPAREN(L) having_predicate(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

having_predicate_atom(A) ::= having_operand(C) EQUAL(O) having_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
having_predicate_atom(A) ::= having_operand(C) NULL_SAFE_EQUAL(O) having_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
}
having_predicate_atom(A) ::= having_operand(C) NOT_EQUAL(O) having_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
having_predicate_atom(A) ::= having_operand(C) LESS(O) having_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
having_predicate_atom(A) ::= having_operand(C) LESS_EQUAL(O) having_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
having_predicate_atom(A) ::= having_operand(C) GREATER(O) having_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
having_predicate_atom(A) ::= having_operand(C) GREATER_EQUAL(O) having_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
having_predicate_atom(A) ::= having_operand(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
having_predicate_atom(A) ::= having_operand(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}

having_operand(A) ::= qualified_identifier(B). {
    A = B;
}
having_operand(A) ::= selected_grouped_aggregate_expression(B). {
    A = B;
}

selected_grouped_aggregate_expression(A) ::= COUNT(T) LPAREN(L) STAR RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, R);
}
selected_grouped_aggregate_expression(A) ::= COUNT(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION, B, R);
}
selected_grouped_aggregate_expression(A) ::= MIN(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, B, R);
}
selected_grouped_aggregate_expression(A) ::= MAX(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, B, R);
}
selected_grouped_aggregate_expression(A) ::= SUM(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, B, R);
}
selected_grouped_aggregate_expression(A) ::= AVG(T) LPAREN qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, B, R);
}

having_integer_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
having_integer_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
having_integer_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
having_integer_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
having_integer_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}

predicate(A) ::= predicate_disjunction(B). {
    A = B;
}

predicate_disjunction(A) ::= predicate_xor(B). {
    A = B;
}
predicate_disjunction(A) ::= predicate_disjunction(B) OR(O) predicate_xor(C). {
    A = mylite_sql_parser_make_or_predicate(
        state, B, O, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, C);
}
predicate_disjunction(A) ::= predicate_disjunction(B) LOGICAL_OR(O) predicate_xor(C). {
    A = mylite_sql_parser_make_or_predicate(
        state, B, O, MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR, C);
}

predicate_xor(A) ::= predicate_conjunction(B). {
    A = B;
}
predicate_xor(A) ::= predicate_xor(B) XOR(O) predicate_conjunction(C). {
    A = mylite_sql_parser_make_xor_predicate(
        state, B, O, MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, C);
}

predicate_conjunction(A) ::= predicate_negation(B). {
    A = B;
}
predicate_conjunction(A) ::= predicate_conjunction(B) AND(O) predicate_negation(C). {
    A = mylite_sql_parser_make_and_predicate(
        state, B, O, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, C);
}
predicate_conjunction(A) ::= predicate_conjunction(B) LOGICAL_AND(O) predicate_negation(C). {
    A = mylite_sql_parser_make_and_predicate(
        state, B, O, MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND, C);
}

predicate_negation(A) ::= predicate_primary(B). {
    A = B;
}
predicate_negation(A) ::= NOT(O) predicate_negation(B). {
    A = mylite_sql_parser_make_not_predicate(state, O, B);
}

predicate_primary(A) ::= predicate_atom(B). {
    A = B;
}
predicate_primary(A) ::= LPAREN(L) predicate(B) RPAREN(R). {
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
predicate_atom(A) ::= qualified_identifier(C) IS(I) TRUE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_TRUE, T);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) NOT TRUE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE, T);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) FALSE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_FALSE, T);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) NOT FALSE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE, T);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) UNKNOWN(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN, T);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) NOT UNKNOWN(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN, T);
}
predicate_atom(A) ::= qualified_identifier(C) BETWEEN(B) predicate_integer_value(L) AND
        predicate_integer_value(U). {
    A = mylite_sql_parser_make_between_predicate(state, C, B, L, U);
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) BETWEEN(B) predicate_integer_value(L) AND
        predicate_integer_value(U). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, C, B, L, U));
}
predicate_atom(A) ::= qualified_identifier(C) IN(I) LPAREN predicate_in_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_in_predicate(state, C, I, V, R);
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) IN(I) LPAREN predicate_in_value_list(V)
        RPAREN(R). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_in_predicate(state, C, I, V, R));
}

predicate_in_value_list(A) ::= predicate_in_value(V). {
    A = mylite_sql_parser_make_predicate_value_list(state, V);
}
predicate_in_value_list(A) ::= predicate_in_value_list(L) COMMA predicate_in_value(V). {
    A = mylite_sql_parser_append_predicate_value(state, L, V);
}

predicate_in_value(A) ::= predicate_integer_value(V). {
    A = V;
}
predicate_in_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
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
predicate_integer_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
predicate_integer_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
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
    A = mylite_sql_parser_make_select_item(state, B, NULL);
}
select_item(A) ::= expression(B) AS select_alias(C). {
    A = mylite_sql_parser_make_select_item(state, B, C);
}
select_item(A) ::= expression(B) select_alias(C). {
    A = mylite_sql_parser_make_select_item(state, B, C);
}

select_alias(A) ::= identifier(B). {
    A = B;
}
select_alias(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

expression(A) ::= literal(B). {
    A = B;
}
expression(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
expression(A) ::= qualified_identifier(B). {
    A = B;
}
expression(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
expression(A) ::= CASE(T) searched_case_when_list(W) case_else_opt(E) END(R). {
    A = mylite_sql_parser_make_searched_case_expression(state, T, W, E, R);
}
expression(A) ::= CASE(T) expression(V) simple_case_when_list(W) case_else_opt(E) END(R). {
    A = mylite_sql_parser_make_simple_case_expression(state, T, V, W, E, R);
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
expression(A) ::= CURRENT_ROLE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION, R);
}
expression(A) ::= CURRENT_ROLE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= IF(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_IF_FUNCTION, B, C, D, R);
}
expression(A) ::= IFNULL(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_IFNULL_FUNCTION, B, C, R);
}
expression(A) ::= COALESCE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_COALESCE_FUNCTION, B, R);
}
expression(A) ::= NULLIF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_FUNCTION, B, C, R);
}
expression(A) ::= MOD(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_MOD_FUNCTION, B, C, R);
}
expression(A) ::= BIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIN_FUNCTION, B, R);
}
expression(A) ::= OCT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_OCT_FUNCTION, B, R);
}
expression(A) ::= CONV(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_CONV_FUNCTION, B, C, D, R);
}
expression(A) ::= PI(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(state, T, MYLITE_SQL_AST_PI_FUNCTION, R);
}
expression(A) ::= PI(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= SQRT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SQRT_FUNCTION, B, R);
}
expression(A) ::= DEGREES(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DEGREES_FUNCTION, B, R);
}
expression(A) ::= RADIANS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RADIANS_FUNCTION, B, R);
}
expression(A) ::= ABS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ABS_FUNCTION, B, R);
}
expression(A) ::= SIGN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SIGN_FUNCTION, B, R);
}
expression(A) ::= CEIL(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CEIL_FUNCTION, B, R);
}
expression(A) ::= CEILING(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CEILING_FUNCTION, B, R);
}
expression(A) ::= FLOOR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FLOOR_FUNCTION, B, R);
}
expression(A) ::= ROUND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ROUND_FUNCTION, B, R);
}
expression(A) ::= ROUND(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ROUND_FUNCTION, B, C, R);
}
expression(A) ::= BIT_COUNT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, B, R);
}
expression(A) ::= ISNULL(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ISNULL_FUNCTION, B, R);
}
expression(A) ::= IFNULL(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= IFNULL(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    IFNULL(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= NULLIF(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= NULLIF(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    NULLIF(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= ISNULL(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ISNULL(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= BIN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= BIN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= OCT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= OCT(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= CONV(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CONV(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= CONV(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::=
    CONV(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, E, R);
}
expression(A) ::= ABS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ABS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= SIGN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SIGN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= SQRT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SQRT(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= DEGREES(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= DEGREES(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= RADIANS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= RADIANS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= CEIL(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CEIL(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= CEILING(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CEILING(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= FLOOR(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FLOOR(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= ROUND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ROUND(T) LPAREN
    expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= BIT_COUNT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= BIT_COUNT(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= CONNECTION_ID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CONNECTION_ID_FUNCTION, R);
}
expression(A) ::= CONNECTION_ID(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= COUNT(T) LPAREN(L) STAR RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, R);
}
expression(A) ::= COUNT(T) LPAREN(L) DISTINCT qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION, B, R);
}
expression(A) ::= COUNT(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION, B, R);
}
expression(A) ::= COUNT(T) LPAREN(L) count_literal(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION, B, R);
}
expression(A) ::= MIN(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= MAX(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= SUM(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= AVG(T) LPAREN qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= BIT_AND(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= BIT_OR(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= BIT_XOR(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION, B, R);
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
expression(A) ::= FOUND_ROWS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_FOUND_ROWS_FUNCTION, R);
}
expression(A) ::= FOUND_ROWS(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= LAST_INSERT_ID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION, R);
}
expression(A) ::= PLUS(T) expression(B). [UPLUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_POSITIVE, B);
}
expression(A) ::= MINUS(T) expression(B). [UMINUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_NEGATIVE, B);
}
expression(A) ::= NOT(T) expression(B). [NOT] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, B);
}
expression(A) ::= BITWISE_NOT(T) expression(B). [BITWISE_NOT] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, B);
}
expression(A) ::= expression(B) AND(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, C);
}
expression(A) ::= expression(B) XOR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, C);
}
expression(A) ::= expression(B) OR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, C);
}
expression(A) ::= expression(B) EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_EQUAL, C);
}
expression(A) ::= expression(B) NULL_SAFE_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, C);
}
expression(A) ::= expression(B) NOT_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, C);
}
expression(A) ::= expression(B) LESS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS, C);
}
expression(A) ::= expression(B) LESS_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, C);
}
expression(A) ::= expression(B) GREATER(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER, C);
}
expression(A) ::= expression(B) GREATER_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, C);
}
expression(A) ::= expression(B) BITWISE_OR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_OR, C);
}
expression(A) ::= expression(B) BITWISE_AND(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_AND, C);
}
expression(A) ::= expression(B) LEFT_SHIFT(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT, C);
}
expression(A) ::= expression(B) RIGHT_SHIFT(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT, C);
}
expression(A) ::= expression(B) IS(T) NULL(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NULL,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_NULL));
}
expression(A) ::= expression(B) IS(T) NOT NULL(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_NULL));
}
expression(A) ::= expression(B) IS(T) TRUE(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_TRUE));
}
expression(A) ::= expression(B) IS(T) NOT TRUE(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_TRUE));
}
expression(A) ::= expression(B) IS(T) FALSE(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_FALSE,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_FALSE));
}
expression(A) ::= expression(B) IS(T) NOT FALSE(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_FALSE));
}
expression(A) ::= expression(B) IS(T) UNKNOWN(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN,
        mylite_sql_parser_make_identifier(state, C));
}
expression(A) ::= expression(B) IS(T) NOT UNKNOWN(C). [IS] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN,
        mylite_sql_parser_make_identifier(state, C));
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
expression(A) ::= expression(B) DIV(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE, C);
}
expression(A) ::= expression(B) PERCENT(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}
expression(A) ::= expression(B) MOD(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}
expression(A) ::= expression(B) BITWISE_XOR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_XOR, C);
}

searched_case_when_list(A) ::= searched_case_when(B). {
    A = mylite_sql_parser_make_case_when_list(state, B);
}
searched_case_when_list(A) ::= searched_case_when_list(B) searched_case_when(C). {
    A = mylite_sql_parser_append_case_when(state, B, C);
}
searched_case_when(A) ::= WHEN(W) expression(C) THEN expression(R). {
    A = mylite_sql_parser_make_case_when_clause(state, W, C, R);
}

simple_case_when_list(A) ::= simple_case_when(B). {
    A = mylite_sql_parser_make_case_when_list(state, B);
}
simple_case_when_list(A) ::= simple_case_when_list(B) simple_case_when(C). {
    A = mylite_sql_parser_append_case_when(state, B, C);
}
simple_case_when(A) ::= WHEN(W) expression(C) THEN expression(R). {
    A = mylite_sql_parser_make_case_when_clause(state, W, C, R);
}

case_else_opt(A) ::= . {
    A = NULL;
}
case_else_opt(A) ::= ELSE(E) expression(B). {
    A = mylite_sql_parser_make_case_else_clause(state, E, B);
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

count_literal(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
count_literal(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
count_literal(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
count_literal(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
count_literal(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
count_literal(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
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
identifier(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= OFF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= VARIABLES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= IFNULL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COALESCE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= NULLIF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ISNULL(T). {
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
identifier(A) ::= BOOL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BOOLEAN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UNKNOWN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CONNECTION_ID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CURRENT_ROLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DO(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= END(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MAX(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SUM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= AVG(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= OCT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CONV(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= PI(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ABS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SIGN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQRT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DEGREES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= RADIANS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CEIL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CEILING(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FLOOR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ROUND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIT_AND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIT_OR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIT_XOR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIT_COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= VERSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ROW_COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FOUND_ROWS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LAST_INSERT_ID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COLUMNS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIELDS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= INDEXES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FULL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TRIGGERS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= EVENTS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= OPEN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= PROCESSLIST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= WARNINGS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ERRORS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ENGINE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ENGINES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STATUS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STORAGE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHARSET(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= NAMES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MODE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SHARE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHARACTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COLLATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COLLATION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= VISIBLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= INVISIBLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

column_definition_list(A) ::= column_definition(B). {
    A = mylite_sql_parser_make_column_definition_list(state, B);
}
column_definition_list(A) ::= column_definition_list(B) COMMA column_definition(C). {
    A = mylite_sql_parser_append_column_definition(state, B, C);
}

column_definition(A) ::= identifier(N) integer_type(T) nullability_opt(U) column_default_opt(D). {
    A = mylite_sql_parser_make_column_definition(state, N, T, U, D);
}

integer_type(A) ::= integer_type_name(T) integer_display_width_opt(W) integer_signedness_opt(S). {
    A = mylite_sql_parser_make_integer_type(
        state,
        T.type_token,
        T.integer_type,
        W.width_token,
        W.end_token,
        S.attribute_token,
        S.is_unsigned,
        0);
}
integer_type(A) ::= BOOL(T). {
    A = mylite_sql_parser_make_integer_type(
        state,
        T,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        0,
        1);
}
integer_type(A) ::= BOOLEAN(T). {
    A = mylite_sql_parser_make_integer_type(
        state,
        T,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        0,
        1);
}

integer_type_name(A) ::= INT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_INT,
    };
}
integer_type_name(A) ::= TINYINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
    };
}
integer_type_name(A) ::= SMALLINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
    };
}
integer_type_name(A) ::= MEDIUMINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
    };
}
integer_type_name(A) ::= INTEGER_TYPE(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_INT,
    };
}
integer_type_name(A) ::= BIGINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
    };
}
integer_type_name(A) ::= INT1(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
    };
}
integer_type_name(A) ::= INT2(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
    };
}
integer_type_name(A) ::= INT3(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
    };
}
integer_type_name(A) ::= INT4(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_INT,
    };
}
integer_type_name(A) ::= INT8(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
    };
}

integer_display_width_opt(A) ::= . {
    A = (struct mylite_sql_integer_display_width_tokens){0};
}
integer_display_width_opt(A) ::= LPAREN INTEGER(W) RPAREN(R). {
    A = (struct mylite_sql_integer_display_width_tokens){
        .width_token = W,
        .end_token = R,
    };
}

integer_signedness_opt(A) ::= . {
    A = (struct mylite_sql_integer_signedness_tokens){0};
}
integer_signedness_opt(A) ::= SIGNED(S). {
    A = (struct mylite_sql_integer_signedness_tokens){
        .attribute_token = S,
        .is_unsigned = 0,
    };
}
integer_signedness_opt(A) ::= UNSIGNED(U). {
    A = (struct mylite_sql_integer_signedness_tokens){
        .attribute_token = U,
        .is_unsigned = 1,
    };
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

column_default_opt(A) ::= . {
    A = NULL;
}
column_default_opt(A) ::= DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_column_default_null(state, D, N);
}
column_default_opt(A) ::= DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_column_default_value(state, D, V);
}

column_default_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
column_default_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
column_default_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
column_default_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
column_default_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}

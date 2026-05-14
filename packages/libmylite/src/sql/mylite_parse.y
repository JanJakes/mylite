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

%fallback IDENTIFIER SAVEPOINT.

%type integer_type_name { struct mylite_sql_integer_type_name_tokens }
%type text_type_name { struct mylite_sql_text_type_tokens }
%type binary_string_type_name { struct mylite_sql_binary_string_type_tokens }
%type decimal_type_name { struct mylite_sql_decimal_type_tokens }
%type decimal_unsigned_opt { struct mylite_sql_decimal_type_tokens }
%type approximate_precision_opt { struct mylite_sql_approximate_type_tokens }
%type approximate_unsigned_opt { struct mylite_sql_approximate_type_tokens }
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
%type join_operator { enum mylite_sql_ast_join_kind }

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
statement(A) ::= create_temporary_table_statement(B). {
    A = B;
}
statement(A) ::= create_table_like_statement(B). {
    A = B;
}
statement(A) ::= create_table_select_statement(B). {
    A = B;
}
statement(A) ::= create_index_statement(B). {
    A = B;
}
statement(A) ::= create_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_table_statement(B). {
    A = B;
}
statement(A) ::= drop_temporary_table_statement(B). {
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
statement(A) ::= alter_table_add_primary_key_statement(B). {
    A = B;
}
statement(A) ::= alter_table_add_index_statement(B). {
    A = B;
}
statement(A) ::= alter_table_add_foreign_key_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_foreign_key_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_index_statement(B). {
    A = B;
}
statement(A) ::= drop_index_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_primary_key_statement(B). {
    A = B;
}
statement(A) ::= alter_table_auto_increment_statement(B). {
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
statement(A) ::= transaction_control_statement(B). {
    A = B;
}
statement(A) ::= table_maintenance_statement(B). {
    A = B;
}
statement(A) ::= do_statement(B). {
    A = B;
}

transaction_control_statement(A) ::= START(S) TRANSACTION(T). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, S, T);
}
transaction_control_statement(A) ::= BEGIN(B). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, B, B);
}
transaction_control_statement(A) ::= BEGIN(B) WORK(W). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, B, W);
}
transaction_control_statement(A) ::= COMMIT(C). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_COMMIT_STATEMENT, C, C);
}
transaction_control_statement(A) ::= COMMIT(C) WORK(W). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_COMMIT_STATEMENT, C, W);
}
transaction_control_statement(A) ::= ROLLBACK(R). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT, R, R);
}
transaction_control_statement(A) ::= ROLLBACK(R) WORK(W). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT, R, W);
}
transaction_control_statement(A) ::= SAVEPOINT(S) identifier(N). {
    A = mylite_sql_parser_make_savepoint_control_statement(
        state, MYLITE_SQL_AST_SAVEPOINT_STATEMENT, S, N);
}
transaction_control_statement(A) ::= ROLLBACK(R) rollback_work_opt TO rollback_savepoint_opt identifier(N). {
    A = mylite_sql_parser_make_savepoint_control_statement(
        state, MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT, R, N);
}
transaction_control_statement(A) ::= RELEASE(R) SAVEPOINT identifier(N). {
    A = mylite_sql_parser_make_savepoint_control_statement(
        state, MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT, R, N);
}

rollback_work_opt ::= .
rollback_work_opt ::= WORK.

rollback_savepoint_opt ::= .
rollback_savepoint_opt ::= SAVEPOINT.

table_maintenance_statement(A) ::=
    ANALYZE(T) maintenance_binlog_opt TABLE table_name_list(N). {
    A = mylite_sql_parser_make_table_maintenance_statement(
        state, MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT, T, N);
}
table_maintenance_statement(A) ::=
    CHECK(T) TABLE table_name_list(N) check_table_option_list_opt. {
    A = mylite_sql_parser_make_table_maintenance_statement(
        state, MYLITE_SQL_AST_CHECK_TABLE_STATEMENT, T, N);
}
table_maintenance_statement(A) ::=
    OPTIMIZE(T) maintenance_binlog_opt TABLE table_name_list(N). {
    A = mylite_sql_parser_make_table_maintenance_statement(
        state, MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT, T, N);
}
table_maintenance_statement(A) ::=
    REPAIR(T) maintenance_binlog_opt TABLE table_name_list(N) repair_table_option_list_opt. {
    A = mylite_sql_parser_make_table_maintenance_statement(
        state, MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT, T, N);
}

maintenance_binlog_opt ::= .
maintenance_binlog_opt ::= NO_WRITE_TO_BINLOG.
maintenance_binlog_opt ::= LOCAL.

check_table_option_list_opt ::= .
check_table_option_list_opt ::= check_table_option_list.

check_table_option_list ::= check_table_option.
check_table_option_list ::= check_table_option_list check_table_option.

check_table_option ::= FOR UPGRADE.
check_table_option ::= QUICK.
check_table_option ::= FAST.
check_table_option ::= MEDIUM.
check_table_option ::= EXTENDED.
check_table_option ::= CHANGED.

repair_table_option_list_opt ::= .
repair_table_option_list_opt ::= repair_table_option_list.

repair_table_option_list ::= repair_table_option.
repair_table_option_list ::= repair_table_option_list repair_table_option.

repair_table_option ::= QUICK.
repair_table_option ::= EXTENDED.
repair_table_option ::= USE_FRM.

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
set_system_variable_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
set_system_variable_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
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
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LPAREN(P)
    create_table_item_list(L) RPAREN(R) table_option_list_opt(O). {
    A = mylite_sql_parser_make_create_table_statement(state, C, E, T, P, L, R, O);
}
create_temporary_table_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T) LPAREN(P)
    create_table_item_list(L) RPAREN(R) table_option_list_opt(O). {
    A = mylite_sql_parser_make_create_temporary_table_statement(state, C, E, T, P, L, R, O);
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

create_index_statement(A) ::=
    CREATE(C) INDEX identifier(N) ON table_name(T) LPAREN secondary_index_part_list(L) RPAREN. {
    A = mylite_sql_parser_make_create_index_statement(state, C, false, N, T, L);
}
create_index_statement(A) ::=
    CREATE(C) UNIQUE INDEX identifier(N) ON table_name(T) LPAREN secondary_index_part_list(L)
    RPAREN. {
    A = mylite_sql_parser_make_create_index_statement(state, C, true, N, T, L);
}

drop_index_statement(A) ::= DROP(D) INDEX identifier(I) ON table_name(T). {
    A = mylite_sql_parser_make_drop_index_statement(state, D, I, T);
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
table_option(A) ::= AUTO_INCREMENT(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_auto_increment_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
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
drop_temporary_table_statement(A) ::=
    DROP(D) TEMPORARY TABLE drop_if_exists_opt(E) table_name_list(T). {
    A = mylite_sql_parser_make_drop_temporary_table_statement(state, D, E, T);
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
show_columns_statement(A) ::= SHOW(S) FULL COLUMNS FROM table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FULL COLUMNS IN table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FULL FIELDS FROM table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FULL FIELDS IN table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FULL COLUMNS FROM table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FULL COLUMNS FROM table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FULL COLUMNS IN table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FULL COLUMNS IN table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FULL FIELDS FROM table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FULL FIELDS FROM table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FULL FIELDS IN table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FULL FIELDS IN table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, L);
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

alter_table_add_primary_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD primary_key_definition(P). {
    A = mylite_sql_parser_make_alter_table_add_primary_key_statement(state, A1, T, P);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD secondary_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD unique_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I);
}

alter_table_add_foreign_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD CONSTRAINT identifier(N) FOREIGN(F) KEY LPAREN
    foreign_key_part_list(C) RPAREN REFERENCES table_name(P) LPAREN foreign_key_part_list(RL)
    RPAREN(R). {
    A = mylite_sql_parser_make_alter_table_add_foreign_key_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_foreign_key_definition(state, N, F, C, P, RL, R));
}

alter_table_drop_foreign_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP FOREIGN KEY identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_foreign_key_statement(state, A1, T, I);
}

alter_table_drop_index_statement(A) ::= ALTER(A1) TABLE table_name(T) DROP INDEX identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(state, A1, T, I);
}

alter_table_drop_index_statement(A) ::= ALTER(A1) TABLE table_name(T) DROP KEY identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(state, A1, T, I);
}

alter_table_drop_primary_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP PRIMARY KEY(K). {
    A = mylite_sql_parser_make_alter_table_drop_primary_key_statement(state, A1, T, K);
}

alter_table_auto_increment_statement(A) ::=
    ALTER(A1) TABLE table_name(T) AUTO_INCREMENT(O) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_alter_table_auto_increment_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_auto_increment_option(
            state,
            O,
            mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER)));
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
    VALUES insert_row_list(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL, D);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL, D);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) table_name(T) insert_column_list_opt(C)
    VALUES insert_row_list(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
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
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) SET insert_assignment_list(S)
    on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, NULL, D);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO table_name(T) SET insert_assignment_list(S)
    on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(
        state, I, T, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) SET insert_assignment_list(S)
    on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, NULL, D);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) table_name(T) SET insert_assignment_list(S)
    on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(
        state, I, T, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
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

on_duplicate_key_update_opt(A) ::= . {
    A = NULL;
}
on_duplicate_key_update_opt(A) ::= ON(O) DUPLICATE KEY UPDATE duplicate_assignment_list(L). {
    A = mylite_sql_parser_make_insert_duplicate_update_clause(state, O, L);
}

duplicate_assignment_list(A) ::= duplicate_assignment(B). {
    A = mylite_sql_parser_make_insert_duplicate_assignment_list(state, B);
}
duplicate_assignment_list(A) ::= duplicate_assignment_list(B) COMMA duplicate_assignment(C). {
    A = mylite_sql_parser_append_insert_duplicate_assignment(state, B, C);
}

duplicate_assignment(A) ::= qualified_identifier(T) EQUAL(E) duplicate_update_value(V). {
    A = mylite_sql_parser_make_insert_duplicate_assignment(state, T, E, V);
}

duplicate_update_value(A) ::= insert_value(V). {
    A = V;
}
duplicate_update_value(A) ::= VALUES(V) LPAREN qualified_identifier(I) RPAREN(R). {
    A = mylite_sql_parser_make_insert_values_reference(state, V, I, R);
}

insert_column_list_opt(A) ::= . {
    A = mylite_sql_parser_make_identifier_list(state, NULL);
}
insert_column_list_opt(A) ::= LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_empty_identifier_list(state, L, R);
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
insert_row(A) ::= LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(
        state,
        L,
        mylite_sql_parser_make_insert_row_values(state, NULL),
        R
    );
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
insert_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
insert_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
insert_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_value(A) ::= PLUS(P) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
insert_value(A) ::= PLUS(P) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
insert_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_value(A) ::= MINUS(M) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
insert_value(A) ::= MINUS(M) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
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
insert_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
insert_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
insert_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
insert_value(A) ::= current_timestamp_value(T). {
    A = T;
}
insert_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}

update_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
update_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
update_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
update_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= PLUS(P) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
update_value(A) ::= PLUS(P) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
update_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= MINUS(M) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
update_value(A) ::= MINUS(M) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
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
update_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
update_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
update_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
update_value(A) ::= current_timestamp_value(T). {
    A = T;
}
update_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}
update_value(A) ::= LPAREN(L) select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R);
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
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) table_source(LT) join_operator(JO)
    table_source(RT) join_condition_opt(J) where_clause_opt(W) group_clause_opt(G)
    having_clause_opt(H) order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_join(state, F, LT, JO, RT, J), W, G, H, O, L, K);
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
select_statement(A) ::=
    SELECT(T) select_modifiers(M) STAR(S) FROM(F) table_source(LT) join_operator(JO)
    table_source(RT) join_condition_opt(J) where_clause_opt(W) group_clause_opt(G)
    having_clause_opt(H) order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_join(state, F, LT, JO, RT, J), W, G, H, O, L, K);
}

table_source(A) ::= table_name(N) table_alias_opt(AL). {
    A = mylite_sql_parser_make_table_source(state, N, AL);
}

join_operator(A) ::= JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_INNER;
}
join_operator(A) ::= INNER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_INNER;
}
join_operator(A) ::= CROSS JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_INNER;
}
join_operator(A) ::= LEFT JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER;
}
join_operator(A) ::= LEFT OUTER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER;
}

join_condition_opt(A) ::= . {
    A = NULL;
}
join_condition_opt(A) ::= ON join_condition(C). {
    A = C;
}

join_condition(A) ::= qualified_identifier(L) EQUAL(O) qualified_identifier(R). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, L, O, MYLITE_SQL_AST_OPERATOR_EQUAL, R);
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

predicate_atom(A) ::= qualified_identifier(C) EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) NULL_SAFE_EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) NULL_SAFE_EQUAL(O) NULL(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL));
}
predicate_atom(A) ::= qualified_identifier(C) NOT_EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) LESS(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
predicate_atom(A) ::= qualified_identifier(C) LESS_EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) GREATER(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
predicate_atom(A) ::= qualified_identifier(C) GREATER_EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) LIKE(O) STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LIKE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) LIKE(O) STRING(T). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_LIKE,
            mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING)));
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
predicate_atom(A) ::= qualified_identifier(C) BETWEEN(B) predicate_range_value(L) AND
        predicate_range_value(U). {
    A = mylite_sql_parser_make_between_predicate(state, C, B, L, U);
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) BETWEEN(B) predicate_range_value(L) AND
        predicate_range_value(U). {
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
predicate_in_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
predicate_in_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
predicate_in_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}

predicate_range_value(A) ::= predicate_integer_value(V). {
    A = V;
}
predicate_range_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
predicate_range_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}

predicate_comparison_value(A) ::= predicate_integer_value(V). {
    A = V;
}
predicate_comparison_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
predicate_comparison_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
predicate_comparison_value(A) ::= DATABASE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_DATABASE_FUNCTION, R);
}
predicate_comparison_value(A) ::= SCHEMA(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_SCHEMA_FUNCTION, R);
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
select_alias(A) ::= BINARY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
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
expression(A) ::= LPAREN(L) select_statement(B) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, B, R);
}
expression(A) ::= CAST(T) LPAREN expression(V) AS BINARY RPAREN(R). {
    A = mylite_sql_parser_make_cast_binary_expression(state, T, V, R);
}
expression(A) ::= DATE_ADD(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I) SECOND RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_DATE_ADD_FUNCTION, V, I, R);
}
expression(A) ::= DATE_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_DATE_FORMAT_FUNCTION, B, C, R);
}
expression(A) ::= current_timestamp_value(T). {
    A = T;
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
current_timestamp_value(A) ::= CURRENT_TIMESTAMP(T). {
    A = mylite_sql_parser_make_current_timestamp_keyword(state, T);
}
current_timestamp_value(A) ::= CURRENT_TIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE, R);
}
current_timestamp_value(A) ::= CURRENT_TIMESTAMP(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR, B, R);
}
current_timestamp_value(A) ::= NOW(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE, R);
}
current_timestamp_value(A) ::= NOW(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR, B, R);
}
current_timestamp_value(A) ::= LOCALTIME(T). {
    A = mylite_sql_parser_make_current_timestamp_keyword(state, T);
}
current_timestamp_value(A) ::= LOCALTIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE, R);
}
current_timestamp_value(A) ::= LOCALTIME(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR, B, R);
}
current_timestamp_value(A) ::= LOCALTIMESTAMP(T). {
    A = mylite_sql_parser_make_current_timestamp_keyword(state, T);
}
current_timestamp_value(A) ::= LOCALTIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE, R);
}
current_timestamp_value(A) ::= LOCALTIMESTAMP(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR, B, R);
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
expression(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
}
expression(A) ::= FIELD(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_FIELD_FUNCTION, B, R);
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
expression(A) ::= RAND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(state, T, MYLITE_SQL_AST_RAND_FUNCTION, R);
}
expression(A) ::= RAND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RAND_SEED_UNSUPPORTED, B, R);
}
expression(A) ::= RAND(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= ACOS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ACOS_FUNCTION, B, R);
}
expression(A) ::= ASIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ASIN_FUNCTION, B, R);
}
expression(A) ::= ATAN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ATAN_FUNCTION, B, R);
}
expression(A) ::= ATAN(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ATAN_FUNCTION, B, C, R);
}
expression(A) ::= ATAN2(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ATAN2_FUNCTION, B, R);
}
expression(A) ::= ATAN2(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ATAN2_FUNCTION, B, C, R);
}
expression(A) ::= EXP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_EXP_FUNCTION, B, R);
}
expression(A) ::= LN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LN_FUNCTION, B, R);
}
expression(A) ::= LOG(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG_FUNCTION, B, R);
}
expression(A) ::= LOG(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_LOG_FUNCTION, B, C, R);
}
expression(A) ::= LOG10(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG10_FUNCTION, B, R);
}
expression(A) ::= LOG2(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG2_FUNCTION, B, R);
}
expression(A) ::= POW(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_POW_FUNCTION, B, C, R);
}
expression(A) ::= POWER(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_POWER_FUNCTION, B, C, R);
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
expression(A) ::= LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LENGTH_FUNCTION, B, R);
}
expression(A) ::= OCTET_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION, B, R);
}
expression(A) ::= BIT_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIT_LENGTH_FUNCTION, B, R);
}
expression(A) ::= CHAR_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION, B, R);
}
expression(A) ::= CHARACTER_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION, B, R);
}
expression(A) ::= LOWER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOWER_FUNCTION, B, R);
}
expression(A) ::= LCASE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LCASE_FUNCTION, B, R);
}
expression(A) ::= UPPER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UPPER_FUNCTION, B, R);
}
expression(A) ::= UCASE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UCASE_FUNCTION, B, R);
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
expression(A) ::= CONCAT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FIELD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= DATE_FORMAT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= DATE_FORMAT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    DATE_FORMAT(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= ACOS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ACOS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= ASIN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ASIN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= ATAN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    ATAN(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= ATAN2(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    ATAN2(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= EXP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= EXP(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LOG10(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LOG10(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LOG2(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LOG2(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= POW(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= POW(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    POW(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= POWER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= POWER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    POWER(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= LENGTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LENGTH(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= OCTET_LENGTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= OCTET_LENGTH(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= BIT_LENGTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= BIT_LENGTH(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= CHAR_LENGTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CHAR_LENGTH(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= CHARACTER_LENGTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    CHARACTER_LENGTH(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LOWER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LOWER(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LCASE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LCASE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= UPPER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= UPPER(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= UCASE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= UCASE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR, C, R);
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
identifier(A) ::= CONCAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIELD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATE_FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CAST(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= DATE_ADD(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
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
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= SYSTEM_USER(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
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
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= MIN(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= MAX(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= SUM(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
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
identifier(A) ::= RAND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= NOW(T). {
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
identifier(A) ::= ACOS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ASIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ATAN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ATAN2(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= EXP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LOG(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LOG10(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LOG2(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= POW(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= POWER(T). {
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
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= BIT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIT_OR(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= BIT_XOR(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= BIT_COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= OCTET_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIT_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHAR_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHARACTER_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LOWER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LCASE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UPPER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UCASE(T). {
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
identifier(A) ::= TABLES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TEMPORARY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TEXT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIXED(T). {
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
identifier(A) ::= ENUM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SET(T). {
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
identifier(A) ::= NATIONAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= NCHAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= NVARCHAR(T). {
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
identifier(A) ::= AUTO_INCREMENT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SERIAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATETIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIMESTAMP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DUPLICATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= START(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TRANSACTION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BEGIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= WORK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COMMIT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ROLLBACK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ANALYZE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHECK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= OPTIMIZE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= REPAIR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= NO_WRITE_TO_BINLOG(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUICK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FAST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MEDIUM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= EXTENDED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHANGED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UPGRADE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= USE_FRM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

create_table_item_list(A) ::= create_table_item(B). {
    A = mylite_sql_parser_make_column_definition_list(state, B);
}
create_table_item_list(A) ::= create_table_item_list(B) COMMA create_table_item(C). {
    A = mylite_sql_parser_append_column_definition(state, B, C);
}

create_table_item(A) ::= column_definition(B). {
    A = B;
}
create_table_item(A) ::= primary_key_definition(B). {
    A = B;
}
create_table_item(A) ::= secondary_index_definition(B). {
    A = B;
}
create_table_item(A) ::= unique_index_definition(B). {
    A = B;
}
create_table_item(A) ::= foreign_key_definition(B). {
    A = B;
}

primary_key_definition(A) ::= PRIMARY(P) KEY LPAREN primary_key_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_primary_key_definition(state, P, L, R);
}

primary_key_part_list(A) ::= primary_key_part(B). {
    A = mylite_sql_parser_make_primary_key_part_list(state, B);
}
primary_key_part_list(A) ::= primary_key_part_list(B) COMMA primary_key_part(C). {
    A = mylite_sql_parser_append_primary_key_part(state, B, C);
}

primary_key_part(A) ::= qualified_identifier(B). {
    A = B;
}

secondary_index_definition(A) ::= KEY(K) index_name_opt(N) LPAREN secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_secondary_index_definition(state, K, N, L, R);
}
secondary_index_definition(A) ::= INDEX(K) index_name_opt(N) LPAREN secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_secondary_index_definition(state, K, N, L, R);
}

unique_index_definition(A) ::= UNIQUE(U) unique_index_keyword_opt index_name_opt(N) LPAREN secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_unique_index_definition(state, U, N, L, R);
}

unique_index_keyword_opt ::= .
unique_index_keyword_opt ::= KEY.
unique_index_keyword_opt ::= INDEX.

foreign_key_definition(A) ::=
    constraint_name_opt(N) FOREIGN(F) KEY LPAREN foreign_key_part_list(C) RPAREN
    REFERENCES table_name(P) LPAREN foreign_key_part_list(RL) RPAREN(R). {
    A = mylite_sql_parser_make_foreign_key_definition(state, N, F, C, P, RL, R);
}

constraint_name_opt(A) ::= . {
    A = NULL;
}
constraint_name_opt(A) ::= CONSTRAINT identifier(B). {
    A = B;
}

foreign_key_part_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_foreign_key_part_list(state, B);
}
foreign_key_part_list(A) ::= foreign_key_part_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_foreign_key_part(state, B, C);
}

index_name_opt(A) ::= . {
    A = NULL;
}
index_name_opt(A) ::= identifier(B). {
    A = B;
}

secondary_index_part_list(A) ::= secondary_index_part(B). {
    A = mylite_sql_parser_make_secondary_index_part_list(state, B);
}
secondary_index_part_list(A) ::= secondary_index_part_list(B) COMMA secondary_index_part(C). {
    A = mylite_sql_parser_append_secondary_index_part(state, B, C);
}

secondary_index_part(A) ::= identifier(B). {
    A = mylite_sql_parser_make_secondary_index_part(state, B, NULL);
}
secondary_index_part(A) ::= identifier(B) LPAREN INTEGER(L) RPAREN. {
    A = mylite_sql_parser_make_secondary_index_part(
        state,
        B,
        mylite_sql_parser_make_literal(state, L, MYLITE_SQL_AST_LITERAL_INTEGER));
}

column_definition(A) ::= identifier(N) column_type(T) column_attribute_list_opt(L). {
    A = mylite_sql_parser_make_column_definition_with_attributes(state, N, T, L);
}

column_attribute_list_opt(A) ::= . {
    A = NULL;
}
column_attribute_list_opt(A) ::= column_attribute_list(B). {
    A = B;
}

column_attribute_list(A) ::= column_attribute(B). {
    A = mylite_sql_parser_make_column_attribute_list(state, B);
}
column_attribute_list(A) ::= column_attribute_list(B) column_attribute(C). {
    A = mylite_sql_parser_append_column_attribute(state, B, C);
}

column_attribute(A) ::= nullability(B). {
    A = B;
}
column_attribute(A) ::= column_default(B). {
    A = B;
}
column_attribute(A) ::= ON(O) UPDATE current_timestamp_value(T). {
    A = mylite_sql_parser_make_column_on_update_current_timestamp(state, O, T);
}
column_attribute(A) ::= PRIMARY(P) KEY(K). {
    A = mylite_sql_parser_make_inline_primary_key(state, P, K);
}
column_attribute(A) ::= UNIQUE(U). {
    A = mylite_sql_parser_make_inline_unique_key(state, U, U);
}
column_attribute(A) ::= UNIQUE(U) KEY(K). {
    A = mylite_sql_parser_make_inline_unique_key(state, U, K);
}
column_attribute(A) ::= AUTO_INCREMENT(T). {
    A = mylite_sql_parser_make_column_auto_increment(state, T);
}

column_type(A) ::= integer_type(T). {
    A = T;
}
column_type(A) ::= serial_type(T). {
    A = T;
}
column_type(A) ::= varchar_type(T). {
    A = T;
}
column_type(A) ::= char_type(T). {
    A = T;
}
column_type(A) ::= text_type(T). {
    A = T;
}
column_type(A) ::= json_type(T). {
    A = T;
}
column_type(A) ::= enum_type(T). {
    A = T;
}
column_type(A) ::= set_type(T). {
    A = T;
}
column_type(A) ::= binary_string_type(T). {
    A = T;
}
column_type(A) ::= bit_type(T). {
    A = T;
}
column_type(A) ::= decimal_type(T). {
    A = T;
}
column_type(A) ::= approximate_type(T). {
    A = T;
}
column_type(A) ::= date_type(T). {
    A = T;
}
column_type(A) ::= datetime_type(T). {
    A = T;
}
column_type(A) ::= time_type(T). {
    A = T;
}
column_type(A) ::= timestamp_type(T). {
    A = T;
}
column_type(A) ::= year_type(T). {
    A = T;
}

serial_type(A) ::= SERIAL(T). {
    A = mylite_sql_parser_make_integer_type(
        state,
        T,
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        1,
        0,
        1);
}

varchar_type(A) ::= VARCHAR(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 0,
        });
}
varchar_type(A) ::= CHARACTER(T) VARYING LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 0,
        });
}
varchar_type(A) ::= CHAR(T) VARYING LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 0,
        });
}
varchar_type(A) ::= NVARCHAR(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 1,
        });
}
varchar_type(A) ::= NATIONAL(T) VARCHAR LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 1,
        });
}
varchar_type(A) ::= NCHAR(T) VARCHAR LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 1,
        });
}
varchar_type(A) ::= NCHAR(T) VARYING LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 1,
        });
}
varchar_type(A) ::= NATIONAL(T) CHAR VARYING LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 1,
        });
}
varchar_type(A) ::= NATIONAL(T) CHARACTER VARYING LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_varchar_type(
        state,
        (struct mylite_sql_varchar_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .is_national = 1,
        });
}

char_type(A) ::= CHAR(T). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = (struct mylite_sql_token){0},
            .end_token = T,
            .has_explicit_length = 0,
            .is_national = 0,
        });
}
char_type(A) ::= CHAR(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .has_explicit_length = 1,
            .is_national = 0,
        });
}
char_type(A) ::= CHARACTER(T). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = (struct mylite_sql_token){0},
            .end_token = T,
            .has_explicit_length = 0,
            .is_national = 0,
        });
}
char_type(A) ::= CHARACTER(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .has_explicit_length = 1,
            .is_national = 0,
        });
}
char_type(A) ::= NCHAR(T). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = (struct mylite_sql_token){0},
            .end_token = T,
            .has_explicit_length = 0,
            .is_national = 1,
        });
}
char_type(A) ::= NCHAR(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .has_explicit_length = 1,
            .is_national = 1,
        });
}
char_type(A) ::= NATIONAL(T) CHAR(E). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = (struct mylite_sql_token){0},
            .end_token = E,
            .has_explicit_length = 0,
            .is_national = 1,
        });
}
char_type(A) ::= NATIONAL(T) CHAR LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .has_explicit_length = 1,
            .is_national = 1,
        });
}
char_type(A) ::= NATIONAL(T) CHARACTER(E). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = (struct mylite_sql_token){0},
            .end_token = E,
            .has_explicit_length = 0,
            .is_national = 1,
        });
}
char_type(A) ::= NATIONAL(T) CHARACTER LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_char_type(
        state,
        (struct mylite_sql_char_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .has_explicit_length = 1,
            .is_national = 1,
        });
}

text_type(A) ::= text_type_name(T). {
    A = mylite_sql_parser_make_text_type(state, T);
}

text_type_name(A) ::= TINYTEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT,
    };
}
text_type_name(A) ::= TEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_TEXT,
    };
}
text_type_name(A) ::= MEDIUMTEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
    };
}
text_type_name(A) ::= LONGTEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT,
    };
}

json_type(A) ::= JSON(T). {
    A = mylite_sql_parser_make_json_type(state, T);
}

enum_type(A) ::= ENUM(T) LPAREN enum_label_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_enum_type(state, T, L, R);
}

enum_label_list(A) ::= STRING(T). {
    A = mylite_sql_parser_make_enum_label_list(state, T);
}
enum_label_list(A) ::= enum_label_list(L) COMMA STRING(T). {
    A = mylite_sql_parser_append_enum_label(state, L, T);
}

set_type(A) ::= SET(T) LPAREN set_member_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_set_type(state, T, L, R);
}

set_member_list(A) ::= STRING(T). {
    A = mylite_sql_parser_make_set_member_list(state, T);
}
set_member_list(A) ::= set_member_list(L) COMMA STRING(T). {
    A = mylite_sql_parser_append_set_member(state, L, T);
}

binary_string_type(A) ::= BINARY(T). {
    A = mylite_sql_parser_make_binary_string_type(
        state,
        (struct mylite_sql_binary_string_type_tokens){
            .type_token = T,
            .end_token = T,
            .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY,
            .has_length = 0,
        });
}
binary_string_type(A) ::= BINARY(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_binary_string_type(
        state,
        (struct mylite_sql_binary_string_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY,
            .has_length = 1,
        });
}
binary_string_type(A) ::= VARBINARY(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_binary_string_type(
        state,
        (struct mylite_sql_binary_string_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY,
            .has_length = 1,
        });
}
binary_string_type(A) ::= CHAR(T) BYTE(B). {
    A = mylite_sql_parser_make_binary_string_type(
        state,
        (struct mylite_sql_binary_string_type_tokens){
            .type_token = T,
            .end_token = B,
            .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY,
            .has_length = 0,
        });
}
binary_string_type(A) ::= CHAR(T) LPAREN INTEGER(L) RPAREN BYTE(B). {
    A = mylite_sql_parser_make_binary_string_type(
        state,
        (struct mylite_sql_binary_string_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = B,
            .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY,
            .has_length = 1,
        });
}
binary_string_type(A) ::= binary_string_type_name(T). {
    A = mylite_sql_parser_make_binary_string_type(state, T);
}

bit_type(A) ::= BIT(T). {
    A = mylite_sql_parser_make_bit_type(
        state,
        (struct mylite_sql_bit_type_tokens){
            .type_token = T,
            .end_token = T,
            .has_length = 0,
        });
}
bit_type(A) ::= BIT(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_bit_type(
        state,
        (struct mylite_sql_bit_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .has_length = 1,
        });
}

binary_string_type_name(A) ::= TINYBLOB(T). {
    A = (struct mylite_sql_binary_string_type_tokens){
        .type_token = T,
        .end_token = T,
        .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_TINYBLOB,
        .has_length = 0,
    };
}
binary_string_type_name(A) ::= BLOB(T). {
    A = (struct mylite_sql_binary_string_type_tokens){
        .type_token = T,
        .end_token = T,
        .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB,
        .has_length = 0,
    };
}
binary_string_type_name(A) ::= BLOB(T) LPAREN INTEGER(L) RPAREN(R). {
    A = (struct mylite_sql_binary_string_type_tokens){
        .type_token = T,
        .length_token = L,
        .end_token = R,
        .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB,
        .has_length = 1,
    };
}
binary_string_type_name(A) ::= MEDIUMBLOB(T). {
    A = (struct mylite_sql_binary_string_type_tokens){
        .type_token = T,
        .end_token = T,
        .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB,
        .has_length = 0,
    };
}
binary_string_type_name(A) ::= LONGBLOB(T). {
    A = (struct mylite_sql_binary_string_type_tokens){
        .type_token = T,
        .end_token = T,
        .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_LONGBLOB,
        .has_length = 0,
    };
}

decimal_type(A) ::= decimal_type_name(T) decimal_unsigned_opt(U). {
    T.attribute_token = U.attribute_token;
    T.is_unsigned = U.is_unsigned;
    T.end_token = T.attribute_token.text == NULL ? T.type_token : T.attribute_token;
    A = mylite_sql_parser_make_decimal_type(state, T);
}
decimal_type(A) ::= decimal_type_name(T) LPAREN INTEGER(P) RPAREN(R) decimal_unsigned_opt(U). {
    T.precision_token = P;
    T.end_token = R;
    T.has_precision = 1;
    T.attribute_token = U.attribute_token;
    T.is_unsigned = U.is_unsigned;
    if (T.attribute_token.text != NULL) {
        T.end_token = T.attribute_token;
    }
    A = mylite_sql_parser_make_decimal_type(state, T);
}
decimal_type(A) ::= decimal_type_name(T) LPAREN INTEGER(P) COMMA INTEGER(S) RPAREN(R)
    decimal_unsigned_opt(U). {
    T.precision_token = P;
    T.scale_token = S;
    T.end_token = R;
    T.has_precision = 1;
    T.has_scale = 1;
    T.attribute_token = U.attribute_token;
    T.is_unsigned = U.is_unsigned;
    if (T.attribute_token.text != NULL) {
        T.end_token = T.attribute_token;
    }
    A = mylite_sql_parser_make_decimal_type(state, T);
}

decimal_type_name(A) ::= DECIMAL_TYPE(T). {
    A = (struct mylite_sql_decimal_type_tokens){
        .type_token = T,
        .end_token = T,
        .decimal_type = MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
    };
}
decimal_type_name(A) ::= DEC(T). {
    A = (struct mylite_sql_decimal_type_tokens){
        .type_token = T,
        .end_token = T,
        .decimal_type = MYLITE_SQL_AST_DECIMAL_TYPE_DEC,
    };
}
decimal_type_name(A) ::= NUMERIC(T). {
    A = (struct mylite_sql_decimal_type_tokens){
        .type_token = T,
        .end_token = T,
        .decimal_type = MYLITE_SQL_AST_DECIMAL_TYPE_NUMERIC,
    };
}
decimal_type_name(A) ::= FIXED(T). {
    A = (struct mylite_sql_decimal_type_tokens){
        .type_token = T,
        .end_token = T,
        .decimal_type = MYLITE_SQL_AST_DECIMAL_TYPE_FIXED,
    };
}

decimal_unsigned_opt(A) ::= . {
    A = (struct mylite_sql_decimal_type_tokens){0};
}
decimal_unsigned_opt(A) ::= UNSIGNED(U). {
    A = (struct mylite_sql_decimal_type_tokens){
        .attribute_token = U,
        .is_unsigned = 1,
    };
}

approximate_type(A) ::= FLOAT_TYPE(T) approximate_precision_opt(P)
    approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = T,
            .precision_token = P.precision_token,
            .end_token = U.attribute_token.text == NULL
                ? (P.end_token.text == NULL ? T : P.end_token)
                : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
            .has_precision = P.has_precision,
            .is_unsigned = U.is_unsigned,
        });
}
approximate_type(A) ::= FLOAT4(T) approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = T,
            .end_token = U.attribute_token.text == NULL ? T : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT4,
            .is_unsigned = U.is_unsigned,
        });
}
approximate_type(A) ::= FLOAT8(T) approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = T,
            .end_token = U.attribute_token.text == NULL ? T : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT8,
            .is_unsigned = U.is_unsigned,
        });
}
approximate_type(A) ::= DOUBLE(T) approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = T,
            .end_token = U.attribute_token.text == NULL ? T : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = U.is_unsigned,
        });
}
approximate_type(A) ::= DOUBLE(D) PRECISION(P) approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = D,
            .end_token = U.attribute_token.text == NULL ? P : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = U.is_unsigned,
        });
}
approximate_type(A) ::= REAL(T) approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = T,
            .end_token = U.attribute_token.text == NULL ? T : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL,
            .is_unsigned = U.is_unsigned,
        });
}

approximate_precision_opt(A) ::= . {
    A = (struct mylite_sql_approximate_type_tokens){0};
}
approximate_precision_opt(A) ::= LPAREN INTEGER(P) RPAREN(R). {
    A = (struct mylite_sql_approximate_type_tokens){
        .precision_token = P,
        .end_token = R,
        .has_precision = 1,
    };
}

approximate_unsigned_opt(A) ::= . {
    A = (struct mylite_sql_approximate_type_tokens){0};
}
approximate_unsigned_opt(A) ::= UNSIGNED(U). {
    A = (struct mylite_sql_approximate_type_tokens){
        .attribute_token = U,
        .is_unsigned = 1,
    };
}

date_type(A) ::= DATE(T). {
    A = mylite_sql_parser_make_date_type(state, T);
}

datetime_type(A) ::= DATETIME(T). {
    A = mylite_sql_parser_make_datetime_type(state, T);
}

time_type(A) ::= TIME(T). {
    A = mylite_sql_parser_make_time_type(state, T);
}

timestamp_type(A) ::= TIMESTAMP(T). {
    A = mylite_sql_parser_make_timestamp_type(state, T);
}

year_type(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_year_type(
        state,
        (struct mylite_sql_year_type_tokens){
            .type_token = T,
            .end_token = T,
            .has_width = 0,
        });
}
year_type(A) ::= YEAR(T) LPAREN INTEGER(W) RPAREN(R). {
    A = mylite_sql_parser_make_year_type(
        state,
        (struct mylite_sql_year_type_tokens){
            .type_token = T,
            .width_token = W,
            .end_token = R,
            .has_width = 1,
        });
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
        0,
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
        1,
        0);
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
        1,
        0);
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

nullability(A) ::= NULL(T). {
    A = mylite_sql_parser_make_nullability(
        state, MYLITE_SQL_AST_NULLABILITY_NULL, T, T);
}
nullability(A) ::= NOT(N) NULL(T). {
    A = mylite_sql_parser_make_nullability(
        state, MYLITE_SQL_AST_NULLABILITY_NOT_NULL, N, T);
}

column_default(A) ::= DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_column_default_null(state, D, N);
}
column_default(A) ::= DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_column_default_value(state, D, V);
}

column_default_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
column_default_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
column_default_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
column_default_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
column_default_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
column_default_value(A) ::= current_timestamp_value(T). {
    A = T;
}
column_default_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
column_default_value(A) ::= PLUS(P) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
column_default_value(A) ::= PLUS(P) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
column_default_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
column_default_value(A) ::= MINUS(M) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
column_default_value(A) ::= MINUS(M) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
column_default_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
column_default_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
column_default_value(A) ::= LPAREN(L) expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, E, R);
}

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
%right ON.
%left EQUAL NULL_SAFE_EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL IS.
%left BITWISE_OR.
%left BITWISE_AND.
%left LEFT_SHIFT RIGHT_SHIFT.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%left BITWISE_XOR.
%left CONCAT_OPERATOR.
%left JSON_EXTRACT_OPERATOR JSON_UNQUOTE_EXTRACT_OPERATOR.
%right UPLUS UMINUS BITWISE_NOT.

%fallback IDENTIFIER SAVEPOINT ENFORCED NO ACTION ALGORITHM COMMENT.

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
%type union_modifier_opt { enum mylite_sql_ast_union_modifier }
%type show_full_opt { int }
%type join_operator { enum mylite_sql_ast_join_kind }
%type inner_join_operator { enum mylite_sql_ast_join_kind }
%type outer_join_operator { enum mylite_sql_ast_join_kind }
%type table_or_tables { struct mylite_sql_token }
%type trim_direction { enum mylite_sql_ast_node_kind }
%type cast_basic_target { enum mylite_sql_ast_node_kind }
%type alter_table_option_tail_opt { struct mylite_sql_alter_table_options }
%type alter_table_algorithm_lock_option_list { struct mylite_sql_alter_table_options }
%type alter_table_algorithm_lock_option { struct mylite_sql_alter_table_options }
%type alter_algorithm_value { struct mylite_sql_alter_algorithm_value }
%type alter_lock_value { struct mylite_sql_alter_lock_value }
%type predicate_comparison_operator { struct mylite_sql_comparison_operator_tokens }

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

statement(A) ::= compound_select_statement(B). {
    A = B;
}
statement(A) ::= select_statement(B). {
    A = B;
}
statement(A) ::= table_statement(B). {
    A = B;
}
statement(A) ::= use_statement(B). {
    A = B;
}
statement(A) ::= set_connection_charset_statement(B). {
    A = B;
}
statement(A) ::= set_transaction_statement(B). {
    A = B;
}
statement(A) ::= set_assignment_statement(B). {
    A = B;
}
statement(A) ::= prepare_statement(B). {
    A = B;
}
statement(A) ::= execute_statement(B). {
    A = B;
}
statement(A) ::= deallocate_prepare_statement(B). {
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
statement(A) ::= create_temporary_table_like_statement(B). {
    A = B;
}
statement(A) ::= create_table_select_statement(B). {
    A = B;
}
statement(A) ::= create_temporary_table_select_statement(B). {
    A = B;
}
statement(A) ::= create_view_statement(B). {
    A = B;
}
statement(A) ::= create_index_statement(B). {
    A = B;
}
statement(A) ::= create_schema_statement(B). {
    A = B;
}
statement(A) ::= alter_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_table_statement(B). {
    A = B;
}
statement(A) ::= drop_temporary_table_statement(B). {
    A = B;
}
statement(A) ::= drop_view_statement(B). {
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
statement(A) ::= show_grants_statement(B). {
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
statement(A) ::= show_create_view_statement(B). {
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
statement(A) ::= show_status_statement(B). {
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
statement(A) ::= alter_table_multi_action_statement(B). {
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
statement(A) ::= alter_table_drop_constraint_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_index_statement(B). {
    A = B;
}
statement(A) ::= alter_table_rename_index_statement(B). {
    A = B;
}
statement(A) ::= alter_table_index_visibility_statement(B). {
    A = B;
}
statement(A) ::= alter_table_add_check_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_check_statement(B). {
    A = B;
}
statement(A) ::= alter_table_alter_check_statement(B). {
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
statement(A) ::= alter_table_convert_character_set_statement(B). {
    A = B;
}
statement(A) ::= alter_table_comment_statement(B). {
    A = B;
}
statement(A) ::= alter_table_order_by_statement(B). {
    A = B;
}
statement(A) ::= alter_table_force_statement(B). {
    A = B;
}
statement(A) ::= alter_table_disable_keys_statement(B). {
    A = B;
}
statement(A) ::= alter_table_enable_keys_statement(B). {
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
statement(A) ::= joined_delete_statement(B). {
    A = B;
}
statement(A) ::= update_statement(B). {
    A = B;
}
statement(A) ::= joined_update_statement(B). {
    A = B;
}
statement(A) ::= transaction_control_statement(B). {
    A = B;
}
statement(A) ::= table_lock_statement(B). {
    A = B;
}
statement(A) ::= table_maintenance_statement(B). {
    A = B;
}
statement(A) ::= do_statement(B). {
    A = B;
}

transaction_control_statement(A) ::= START(S) TRANSACTION(T) start_transaction_characteristics_opt(C). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, S, T, C);
}
transaction_control_statement(A) ::= BEGIN(B). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, B, B, NULL);
}
transaction_control_statement(A) ::= BEGIN(B) IDENTIFIER(I). {
    A = mylite_sql_parser_make_begin_immediate_statement(state, B, I);
}
transaction_control_statement(A) ::= BEGIN(B) WORK(W). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, B, W, NULL);
}
transaction_control_statement(A) ::= COMMIT(C). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_COMMIT_STATEMENT, C, C, NULL);
}
transaction_control_statement(A) ::= COMMIT(C) WORK(W). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_COMMIT_STATEMENT, C, W, NULL);
}
transaction_control_statement(A) ::= ROLLBACK(R). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT, R, R, NULL);
}
transaction_control_statement(A) ::= ROLLBACK(R) WORK(W). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT, R, W, NULL);
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

start_transaction_characteristics_opt(A) ::= . {
    A = NULL;
}
start_transaction_characteristics_opt(A) ::= start_transaction_characteristics(C). {
    A = C;
}

start_transaction_characteristics(A) ::= start_transaction_characteristic(C). {
    A = mylite_sql_parser_make_transaction_characteristic_list(state, C);
}
start_transaction_characteristics(A) ::= start_transaction_characteristics(L) COMMA start_transaction_characteristic(C). {
    A = mylite_sql_parser_append_transaction_characteristic(state, L, C);
}

start_transaction_characteristic(A) ::= WITH(W) CONSISTENT SNAPSHOT(S). {
    A = mylite_sql_parser_make_transaction_characteristic(
        state, MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT, W, S);
}
start_transaction_characteristic(A) ::= set_transaction_access_mode(C). {
    A = C;
}

set_transaction_statement(A) ::= SET(S) TRANSACTION set_transaction_characteristics(C). {
    A = mylite_sql_parser_make_set_transaction_statement(state, S, NULL, C);
}
set_transaction_statement(A) ::= SET(S) SESSION(Q) TRANSACTION set_transaction_characteristics(C). {
    A = mylite_sql_parser_make_set_transaction_statement(
        state, S, mylite_sql_parser_make_identifier(state, Q), C);
}
set_transaction_statement(A) ::= SET(S) GLOBAL(Q) TRANSACTION set_transaction_characteristics(C). {
    A = mylite_sql_parser_make_set_transaction_statement(
        state, S, mylite_sql_parser_make_identifier(state, Q), C);
}

set_transaction_characteristics(A) ::= set_transaction_isolation(C). {
    A = mylite_sql_parser_make_transaction_characteristic_list(state, C);
}
set_transaction_characteristics(A) ::= set_transaction_access_mode(C). {
    A = mylite_sql_parser_make_transaction_characteristic_list(state, C);
}
set_transaction_characteristics(A) ::= set_transaction_isolation(I) COMMA set_transaction_access_mode(M). {
    A = mylite_sql_parser_append_transaction_characteristic(
        state,
        mylite_sql_parser_make_transaction_characteristic_list(state, I),
        M);
}
set_transaction_characteristics(A) ::= set_transaction_access_mode(M) COMMA set_transaction_isolation(I). {
    A = mylite_sql_parser_append_transaction_characteristic(
        state,
        mylite_sql_parser_make_transaction_characteristic_list(state, M),
        I);
}

set_transaction_isolation(A) ::= ISOLATION LEVEL transaction_isolation_level(L). {
    A = L;
}

transaction_isolation_level(A) ::= REPEATABLE(R) READ(D). {
    A = mylite_sql_parser_make_transaction_characteristic(
        state, MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ, R, D);
}
transaction_isolation_level(A) ::= READ(R) COMMITTED(C). {
    A = mylite_sql_parser_make_transaction_characteristic(
        state, MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED, R, C);
}
transaction_isolation_level(A) ::= READ(R) UNCOMMITTED(U). {
    A = mylite_sql_parser_make_transaction_characteristic(
        state, MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_UNCOMMITTED, R, U);
}
transaction_isolation_level(A) ::= SERIALIZABLE(S). {
    A = mylite_sql_parser_make_transaction_characteristic(
        state, MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE, S, S);
}

set_transaction_access_mode(A) ::= READ(R) WRITE(W). {
    A = mylite_sql_parser_make_transaction_characteristic(
        state, MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE, R, W);
}
set_transaction_access_mode(A) ::= READ(R) ONLY(O). {
    A = mylite_sql_parser_make_transaction_characteristic(
        state, MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY, R, O);
}

rollback_work_opt ::= .
rollback_work_opt ::= WORK.

rollback_savepoint_opt ::= .
rollback_savepoint_opt ::= SAVEPOINT.

table_lock_statement(A) ::= LOCK(L) table_or_tables lock_table_target_list(T). {
    A = mylite_sql_parser_make_lock_tables_statement(state, L, T);
}
table_lock_statement(A) ::= UNLOCK(U) table_or_tables(T). {
    A = mylite_sql_parser_make_unlock_tables_statement(state, U, T);
}

table_or_tables(A) ::= TABLE(T). {
    A = T;
}
table_or_tables(A) ::= TABLES(T). {
    A = T;
}

lock_table_target_list(A) ::= lock_table_target(T). {
    A = mylite_sql_parser_make_lock_table_target_list(state, T);
}
lock_table_target_list(A) ::= lock_table_target_list(L) COMMA lock_table_target(T). {
    A = mylite_sql_parser_append_lock_table_target(state, L, T);
}

lock_table_target(A) ::= table_name(T) lock_table_alias_opt(B) lock_table_type(M). {
    A = mylite_sql_parser_make_lock_table_target(state, T, B, M);
}

lock_table_alias_opt(A) ::= . {
    A = NULL;
}
lock_table_alias_opt(A) ::= AS identifier(I). {
    A = I;
}
lock_table_alias_opt(A) ::= identifier(I). {
    A = I;
}

lock_table_type(A) ::= READ(R). {
    A = mylite_sql_parser_make_lock_table_type(
        state, MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK, R, R);
}
lock_table_type(A) ::= READ(R) LOCAL(L). {
    A = mylite_sql_parser_make_lock_table_type(
        state, MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK, R, L);
}
lock_table_type(A) ::= WRITE(W). {
    A = mylite_sql_parser_make_lock_table_type(
        state, MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK, W, W);
}

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

set_assignment_statement(A) ::= SET(S) set_assignment_list(L). {
    A = mylite_sql_parser_make_set_statement(state, S, L);
}

set_assignment_list(A) ::= set_assignment(B). {
    A = mylite_sql_parser_make_set_assignment_list(state, B);
}
set_assignment_list(A) ::= set_assignment_list(B) COMMA set_assignment(C). {
    A = mylite_sql_parser_append_set_assignment(state, B, C);
}

set_assignment(A) ::= set_system_variable_target(T) EQUAL(E) set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_assignment(state, T, E, V);
}
set_assignment(A) ::= user_variable(T) EQUAL(E) user_variable_set_value(V). {
    A = mylite_sql_parser_make_set_assignment(state, T, E, V);
}
set_assignment(A) ::= user_variable(T) ASSIGN(O) user_variable_set_value(V). {
    A = mylite_sql_parser_make_set_assignment(state, T, O, V);
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
set_system_variable_value(A) ::= SYSTEM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
set_system_variable_value(A) ::= UTC(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
set_system_variable_value(A) ::= SERIALIZABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
set_system_variable_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
set_system_variable_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
set_system_variable_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
set_system_variable_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
set_system_variable_value(A) ::= PLUS(P) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
set_system_variable_value(A) ::= PLUS(P) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
set_system_variable_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
set_system_variable_value(A) ::= MINUS(M) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
set_system_variable_value(A) ::= MINUS(M) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
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
set_system_variable_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
set_system_variable_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
set_system_variable_value(A) ::= user_variable(T). {
    A = T;
}
set_system_variable_value(A) ::= LPAREN(L) set_system_variable_value(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

user_variable_set_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
user_variable_set_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
user_variable_set_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
user_variable_set_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
user_variable_set_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
user_variable_set_value(A) ::= ON(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
user_variable_set_value(A) ::= OFF(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
user_variable_set_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
user_variable_set_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
user_variable_set_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
user_variable_set_value(A) ::= user_variable(T). {
    A = T;
}
user_variable_set_value(A) ::= LPAREN(L) user_variable_set_value(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

user_variable(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_user_variable(state, T);
}

prepare_statement(A) ::= PREPARE(P) identifier(N) FROM prepare_source(S). {
    A = mylite_sql_parser_make_prepare_statement(state, P, N, S);
}

prepare_source(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
prepare_source(A) ::= user_variable(T). {
    A = T;
}

execute_statement(A) ::= EXECUTE(E) identifier(N) execute_using_opt(U). {
    A = mylite_sql_parser_make_execute_statement(state, E, N, U);
}

execute_using_opt(A) ::= . {
    A = NULL;
}
execute_using_opt(A) ::= USING execute_using_list(L). {
    A = L;
}

execute_using_list(A) ::= user_variable(V). {
    A = mylite_sql_parser_make_execute_using_list(state, V);
}
execute_using_list(A) ::= execute_using_list(L) COMMA user_variable(V). {
    A = mylite_sql_parser_append_execute_using_variable(state, L, V);
}

deallocate_prepare_statement(A) ::= DEALLOCATE(D) PREPARE identifier(N). {
    A = mylite_sql_parser_make_deallocate_prepare_statement(state, D, N);
}
deallocate_prepare_statement(A) ::= DROP(D) PREPARE identifier(N). {
    A = mylite_sql_parser_make_deallocate_prepare_statement(state, D, N);
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
create_temporary_table_like_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T) LIKE table_name(S). {
    A = mylite_sql_parser_make_create_temporary_table_like_statement(state, C, E, T, S);
}
create_temporary_table_like_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T) LPAREN LIKE table_name(S)
    RPAREN. {
    A = mylite_sql_parser_make_create_temporary_table_like_statement(state, C, E, T, S);
}
create_table_select_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) create_table_select_as_opt
    select_statement(S). {
    A = mylite_sql_parser_make_create_table_select_statement(state, C, E, T, S);
}
create_temporary_table_select_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T)
    create_table_select_as_opt select_statement(S). {
    A = mylite_sql_parser_make_create_temporary_table_select_statement(state, C, E, T, S);
}
create_view_statement(A) ::= CREATE(C) VIEW table_name(T) AS select_statement(S). {
    A = mylite_sql_parser_make_create_view_statement(state, C, T, S);
}

create_index_statement(A) ::=
    CREATE(C) INDEX identifier(N) index_type_opt(Y) ON table_name(T) LPAREN
    secondary_index_part_list(L) RPAREN index_option_list_opt(O). {
    A = mylite_sql_parser_make_create_index_statement(state, C, false, N, Y, T, L, O);
}
create_index_statement(A) ::=
    CREATE(C) UNIQUE INDEX identifier(N) index_type_opt(Y) ON table_name(T) LPAREN
    secondary_index_part_list(L) RPAREN index_option_list_opt(O). {
    A = mylite_sql_parser_make_create_index_statement(state, C, true, N, Y, T, L, O);
}
create_index_statement(A) ::=
    CREATE(C) FULLTEXT INDEX identifier(N) ON table_name(T) LPAREN secondary_index_part_list(L)
    RPAREN index_option_list_opt(O). {
    A = mylite_sql_parser_make_create_fulltext_index_statement(state, C, N, T, L, O);
}
create_index_statement(A) ::=
    CREATE(C) SPATIAL INDEX identifier(N) ON table_name(T) LPAREN secondary_index_part_list(L)
    RPAREN index_option_list_opt(O). {
    A = mylite_sql_parser_make_create_spatial_index_statement(state, C, N, T, L, O);
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
table_option_list(A) ::= table_option_list(B) COMMA table_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

table_option(A) ::= ENGINE(E) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_engine_option(state, E, N);
}
table_option(A) ::= default_opt CHARSET(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
table_option(A) ::= default_opt CHARSET(C) equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_charset_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
table_option(A) ::= default_opt CHARACTER(C) SET equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
table_option(A) ::= default_opt CHARACTER(C) SET equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_charset_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
table_option(A) ::= default_opt COLLATE(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}
table_option(A) ::= default_opt COLLATE(C) equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_collation_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
table_option(A) ::= AUTO_INCREMENT(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_auto_increment_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= COMMENT(T) equal_opt STRING(V). {
    A = mylite_sql_parser_make_table_comment_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}
table_option(A) ::= ROW_FORMAT(T) equal_opt row_format_option_value(V). {
    A = mylite_sql_parser_make_table_row_format_option(state, T, V);
}
table_option(A) ::= KEY_BLOCK_SIZE(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_key_block_size_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= PACK_KEYS(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_pack_keys_option(state, T, V);
}
table_option(A) ::= CHECKSUM(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_checksum_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= STATS_PERSISTENT(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_stats_persistent_option(state, T, V);
}
table_option(A) ::= STATS_AUTO_RECALC(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_stats_auto_recalc_option(state, T, V);
}
table_option(A) ::= STATS_SAMPLE_PAGES(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_stats_sample_pages_option(state, T, V);
}

row_format_option_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
row_format_option_value(A) ::= DYNAMIC(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
row_format_option_value(A) ::= COMPACT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
row_format_option_value(A) ::= REDUNDANT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
row_format_option_value(A) ::= COMPRESSED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
row_format_option_value(A) ::= FIXED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

table_default_or_integer_option_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
table_default_or_integer_option_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
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

create_schema_statement(A) ::=
    CREATE(C) DATABASE create_schema_if_not_exists_opt(E) identifier(S) schema_option_list_opt(O).
{
    A = mylite_sql_parser_make_create_schema_statement(state, C, E, S, O);
}
create_schema_statement(A) ::=
    CREATE(C) SCHEMA create_schema_if_not_exists_opt(E) identifier(S) schema_option_list_opt(O).
{
    A = mylite_sql_parser_make_create_schema_statement(state, C, E, S, O);
}

create_schema_if_not_exists_opt(A) ::= . {
    A = NULL;
}
create_schema_if_not_exists_opt(A) ::= IF(I) NOT EXISTS(E). {
    A = mylite_sql_parser_make_create_schema_if_not_exists_clause(state, I, E);
}

alter_schema_statement(A) ::= ALTER(T) DATABASE alter_schema_name_opt(S) schema_option_list(O). {
    A = mylite_sql_parser_make_alter_schema_default_charset_collation_statement(state, T, S, O);
}
alter_schema_statement(A) ::= ALTER(T) SCHEMA alter_schema_name_opt(S) schema_option_list(O). {
    A = mylite_sql_parser_make_alter_schema_default_charset_collation_statement(state, T, S, O);
}

alter_schema_name_opt(A) ::= . {
    A = NULL;
}
alter_schema_name_opt(A) ::= IDENTIFIER(S). {
    A = mylite_sql_parser_make_identifier(state, S);
}
alter_schema_name_opt(A) ::= QUOTED_IDENTIFIER(S). {
    A = mylite_sql_parser_make_identifier(state, S);
}

schema_option_list_opt(A) ::= . {
    A = NULL;
}
schema_option_list_opt(A) ::= schema_option_list(B). {
    A = B;
}

schema_option_list(A) ::= schema_option(B). {
    A = mylite_sql_parser_make_table_option_list(state, B);
}
schema_option_list(A) ::= schema_option_list(B) schema_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

schema_option(A) ::= default_opt CHARSET(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
schema_option(A) ::= default_opt CHARSET(C) equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_charset_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
schema_option(A) ::= default_opt CHARACTER(C) SET equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
schema_option(A) ::= default_opt CHARACTER(C) SET equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_charset_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
schema_option(A) ::= default_opt COLLATE(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}
schema_option(A) ::= default_opt COLLATE(C) equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_collation_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}

drop_table_statement(A) ::= DROP(D) TABLE drop_if_exists_opt(E) table_name_list(T). {
    A = mylite_sql_parser_make_drop_table_statement(state, D, E, T);
}
drop_temporary_table_statement(A) ::=
    DROP(D) TEMPORARY TABLE drop_if_exists_opt(E) table_name_list(T). {
    A = mylite_sql_parser_make_drop_temporary_table_statement(state, D, E, T);
}
drop_view_statement(A) ::= DROP(D) VIEW drop_if_exists_opt(E) table_name_list(T). {
    A = mylite_sql_parser_make_drop_view_statement(state, D, E, T);
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

show_tables_statement(A) ::= SHOW(S) show_full_opt(F) TABLES(T) show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, NULL, L);
}
show_tables_statement(A) ::= SHOW(S) show_full_opt(F) TABLES(T) FROM identifier(D) show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, D, L);
}
show_tables_statement(A) ::= SHOW(S) show_full_opt(F) TABLES(T) IN identifier(D) show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, D, L);
}

show_full_opt(A) ::= . {
    A = 0;
}
show_full_opt(A) ::= FULL. {
    A = 1;
}

show_tables_filter_opt(A) ::= . {
    A = NULL;
}
show_tables_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_tables_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) show_table_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, NULL, F);
}
show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) FROM identifier(D) show_table_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, F);
}
show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) IN identifier(D) show_table_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, F);
}

show_table_status_filter_opt(A) ::= . {
    A = NULL;
}
show_table_status_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_table_status_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
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

show_grants_statement(A) ::= SHOW(S) GRANTS(G). {
    A = mylite_sql_parser_make_show_grants_statement(state, S, G);
}
show_grants_statement(A) ::= SHOW(S) GRANTS FOR CURRENT_USER(C). {
    A = mylite_sql_parser_make_show_grants_statement(state, S, C);
}
show_grants_statement(A) ::= SHOW(S) GRANTS FOR CURRENT_USER LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_show_grants_statement(state, S, R);
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

show_columns_statement(A) ::= SHOW(S) show_columns_keyword show_columns_table_keyword table_name(T)
        show_columns_filter_opt(F). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, F);
}
show_columns_statement(A) ::= SHOW(S) show_columns_keyword show_columns_table_keyword table_name(T)
        show_columns_schema_keyword identifier(D) show_columns_filter_opt(F). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, F);
}
show_columns_statement(A) ::= SHOW(S) FULL show_columns_keyword show_columns_table_keyword table_name(T)
        show_columns_filter_opt(F). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, NULL, F);
}
show_columns_statement(A) ::= SHOW(S) FULL show_columns_keyword show_columns_table_keyword table_name(T)
        show_columns_schema_keyword identifier(D) show_columns_filter_opt(F). {
    A = mylite_sql_parser_make_show_full_columns_statement(state, S, T, D, F);
}

show_columns_keyword ::= COLUMNS.
show_columns_keyword ::= FIELDS.
show_columns_table_keyword ::= FROM.
show_columns_table_keyword ::= IN.
show_columns_schema_keyword ::= FROM.
show_columns_schema_keyword ::= IN.

show_columns_filter_opt(A) ::= . {
    A = NULL;
}
show_columns_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_columns_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

show_index_statement(A) ::= SHOW(S) show_index_keyword show_index_table_keyword table_name(T)
        show_index_filter_opt(F). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, NULL, F);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword show_index_table_keyword table_name(T)
        show_index_schema_keyword identifier(D) show_index_filter_opt(F). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D, F);
}

show_index_keyword ::= INDEX.
show_index_keyword ::= INDEXES.
show_index_keyword ::= KEYS.

show_index_table_keyword ::= FROM.
show_index_table_keyword ::= IN.
show_index_schema_keyword ::= FROM.
show_index_schema_keyword ::= IN.

show_index_filter_opt(A) ::= . {
    A = NULL;
}
show_index_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

show_databases_statement(A) ::= SHOW(S) DATABASES(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, L);
}
show_databases_statement(A) ::= SHOW(S) SCHEMAS(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, L);
}

show_variables_statement(A) ::= SHOW(S) show_variables_scope_opt(O) VARIABLES(V)
        show_variables_filter_opt(F). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, F);
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

show_variables_filter_opt(A) ::= . {
    A = NULL;
}
show_variables_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_variables_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

show_status_statement(A) ::= SHOW(S) show_status_scope_opt(O) STATUS(T)
        show_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_status_statement(state, S, O, T, F);
}

show_status_scope_opt(A) ::= . {
    A = NULL;
}
show_status_scope_opt(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_status_scope_opt(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_status_scope_opt(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

show_status_filter_opt(A) ::= . {
    A = NULL;
}
show_status_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}

show_create_table_statement(A) ::= SHOW(S) CREATE TABLE table_name(T). {
    A = mylite_sql_parser_make_show_create_table_statement(state, S, T);
}
show_create_view_statement(A) ::= SHOW(S) CREATE VIEW table_name(T). {
    A = mylite_sql_parser_make_show_create_view_statement(state, S, T);
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

alter_table_multi_action_statement(A) ::=
    ALTER(A1) TABLE table_name(T) alter_table_multi_action_list(L). {
    A = mylite_sql_parser_make_alter_table_multi_action_statement(state, A1, T, L);
}

alter_table_multi_action_list(A) ::= alter_table_multi_first_action(L) alter_table_multi_action(N). {
    A = mylite_sql_parser_append_alter_table_action(state, L, N);
}
alter_table_multi_action_list(A) ::=
    alter_table_multi_action_list(L) COMMA alter_table_multi_action(N). {
    A = mylite_sql_parser_append_alter_table_action(state, L, N);
}

alter_table_multi_first_action(A) ::=
    ADD(A1) column_keyword_opt column_definition(C) column_position_opt(P) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_column_statement(
            state,
            A1,
            NULL,
            C,
            P,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) secondary_index_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) unique_index_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) named_unique_constraint_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) primary_key_definition(P) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_primary_key_statement(
            state,
            A1,
            NULL,
            P,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= DROP(A1) INDEX identifier(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_index_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= DROP(A1) KEY identifier(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_index_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= DROP(A1) PRIMARY KEY(K) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_primary_key_statement(
            state,
            A1,
            NULL,
            K,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::=
    ALTER(A1) column_keyword_opt identifier(C) SET DEFAULT(D) NULL(N) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_set_default_statement(
            state, A1, NULL, C, mylite_sql_parser_make_column_default_null(state, D, N)));
}
alter_table_multi_first_action(A) ::=
    ALTER(A1) column_keyword_opt identifier(C) SET DEFAULT(D) column_default_value(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_set_default_statement(
            state, A1, NULL, C, mylite_sql_parser_make_column_default_value(state, D, V)));
}
alter_table_multi_first_action(A) ::=
    ALTER(A1) column_keyword_opt identifier(C) DROP DEFAULT(D) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_default_statement(state, A1, NULL, C, D));
}

alter_table_multi_action(A) ::=
    ADD(A1) column_keyword_opt column_definition(C) column_position_opt(P). {
    A = mylite_sql_parser_make_alter_table_add_column_statement(
        state,
        A1,
        NULL,
        C,
        P,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) secondary_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) unique_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) named_unique_constraint_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) primary_key_definition(P). {
    A = mylite_sql_parser_make_alter_table_add_primary_key_statement(
        state,
        A1,
        NULL,
        P,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= DROP(A1) INDEX identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= DROP(A1) KEY identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= DROP(A1) PRIMARY KEY(K). {
    A = mylite_sql_parser_make_alter_table_drop_primary_key_statement(
        state,
        A1,
        NULL,
        K,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::=
    ALTER(A1) column_keyword_opt identifier(C) SET DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, A1, NULL, C, mylite_sql_parser_make_column_default_null(state, D, N));
}
alter_table_multi_action(A) ::=
    ALTER(A1) column_keyword_opt identifier(C) SET DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, A1, NULL, C, mylite_sql_parser_make_column_default_value(state, D, V));
}
alter_table_multi_action(A) ::= ALTER(A1) column_keyword_opt identifier(C) DROP DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_drop_default_statement(state, A1, NULL, C, D);
}

alter_table_add_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD column_keyword_opt column_definition(C)
    column_position_opt(P) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_column_statement(state, A1, T, C, P, O);
}

alter_table_add_primary_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD primary_key_definition(P) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_primary_key_statement(state, A1, T, P, O);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD secondary_index_definition(I) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I, O);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD unique_index_definition(I) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I, O);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD named_unique_constraint_definition(I)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I, O);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD fulltext_index_definition(I) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I, O);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD spatial_index_definition(I) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I, O);
}

alter_table_add_foreign_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD foreign_key_definition(FK) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_foreign_key_statement(state, A1, T, FK, O);
}

alter_table_drop_foreign_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP FOREIGN KEY identifier(I) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_drop_foreign_key_statement(state, A1, T, I, O);
}

alter_table_drop_constraint_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP CONSTRAINT identifier(C) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_drop_constraint_statement(state, A1, T, C, O);
}

alter_table_drop_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP INDEX identifier(I) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(state, A1, T, I, O);
}

alter_table_drop_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP KEY identifier(I) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(state, A1, T, I, O);
}

alter_table_rename_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) RENAME INDEX old_identifier(O) TO new_identifier(N)
    alter_table_option_tail_opt(P). {
    A = mylite_sql_parser_make_alter_table_rename_index_statement(state, A1, T, O, N, P);
}

alter_table_rename_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) RENAME KEY old_identifier(O) TO new_identifier(N)
    alter_table_option_tail_opt(P). {
    A = mylite_sql_parser_make_alter_table_rename_index_statement(state, A1, T, O, N, P);
}

alter_table_index_visibility_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER INDEX identifier(I) VISIBLE(V)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_index_visibility_statement(
        state, A1, T, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE, O);
}

alter_table_index_visibility_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER INDEX identifier(I) INVISIBLE(V)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_index_visibility_statement(
        state, A1, T, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE, O);
}

alter_table_add_check_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD check_constraint_definition(C). {
    A = mylite_sql_parser_make_alter_table_add_check_statement(state, A1, T, C);
}

alter_table_drop_check_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP CHECK identifier(C). {
    A = mylite_sql_parser_make_alter_table_drop_check_statement(state, A1, T, C);
}

alter_table_alter_check_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER CHECK identifier(C) check_enforcement_required(E). {
    A = mylite_sql_parser_make_alter_table_alter_check_statement(state, A1, T, C, E);
}

old_identifier(A) ::= identifier(B). {
    A = B;
}

new_identifier(A) ::= identifier(B). {
    A = B;
}

alter_table_drop_primary_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP PRIMARY KEY(K) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_drop_primary_key_statement(state, A1, T, K, O);
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
    ALTER(A1) TABLE table_name(T) DROP column_keyword_opt identifier(C)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_drop_column_statement(state, A1, T, C, O);
}

alter_table_rename_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) RENAME COLUMN identifier(O) TO identifier(N)
    alter_table_option_tail_opt(P). {
    A = mylite_sql_parser_make_alter_table_rename_column_statement(state, A1, T, O, N, P);
}

alter_table_modify_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) MODIFY column_keyword_opt column_definition(C)
    column_position_opt(P). {
    A = mylite_sql_parser_make_alter_table_modify_column_statement(state, A1, T, C, P);
}

alter_table_change_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CHANGE column_keyword_opt identifier(O) column_definition(C)
    column_position_opt(P). {
    A = mylite_sql_parser_make_alter_table_change_column_statement(state, A1, T, O, C, P);
}

column_position_opt(A) ::= . {
    A = NULL;
}
column_position_opt(A) ::= FIRST(T). {
    A = mylite_sql_parser_make_column_position_first(state, T);
}
column_position_opt(A) ::= AFTER(T) identifier(C). {
    A = mylite_sql_parser_make_column_position_after(state, T, C);
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

alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARACTER(C) SET
    option_name(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(state, C, N)),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARACTER(C) SET
    BINARY(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(
                    state,
                    C,
                    mylite_sql_parser_make_identifier(state, N))),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARSET(C)
    option_name(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(state, C, N)),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARSET(C)
    BINARY(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(
                    state,
                    C,
                    mylite_sql_parser_make_identifier(state, N))),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARACTER(C) SET DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_option_list(
            state,
            mylite_sql_parser_make_table_charset_option(
                state,
                C,
                mylite_sql_parser_make_identifier(state, D))));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARSET(C) DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_option_list(
            state,
            mylite_sql_parser_make_table_charset_option(
                state,
                C,
                mylite_sql_parser_make_identifier(state, D))));
}

convert_character_set_collate_opt(A) ::= . {
    A = NULL;
}
convert_character_set_collate_opt(A) ::= COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}
convert_character_set_collate_opt(A) ::= COLLATE(C) BINARY(N). {
    A = mylite_sql_parser_make_table_collation_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}

alter_table_comment_statement(A) ::=
    ALTER(A1) TABLE table_name(T) COMMENT(C) equal_opt STRING(V) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_comment_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_comment_option(
            state,
            C,
            mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING)),
        O);
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

alter_table_force_statement(A) ::= ALTER(A1) TABLE table_name(T) FORCE alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_force_statement(state, A1, T, O);
}

alter_table_disable_keys_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DISABLE KEYS alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_disable_keys_statement(state, A1, T, O);
}

alter_table_enable_keys_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ENABLE KEYS alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_enable_keys_statement(state, A1, T, O);
}

alter_table_option_tail_opt(A) ::= . {
    A = mylite_sql_parser_empty_alter_table_options();
}
alter_table_option_tail_opt(A) ::= COMMA alter_table_algorithm_lock_option_list(O). {
    A = O;
}

alter_table_algorithm_lock_option_list(A) ::= alter_table_algorithm_lock_option(O). {
    A = O;
}
alter_table_algorithm_lock_option_list(A) ::=
    alter_table_algorithm_lock_option_list(L) COMMA alter_table_algorithm_lock_option(O). {
    A = mylite_sql_parser_append_alter_table_option(L, O);
}

alter_table_algorithm_lock_option(A) ::= ALGORITHM(T) equal_opt alter_algorithm_value(V). {
    A = mylite_sql_parser_make_alter_table_algorithm_option(T, V);
}
alter_table_algorithm_lock_option(A) ::= LOCK(T) equal_opt alter_lock_value(V). {
    A = mylite_sql_parser_make_alter_table_lock_option(T, V);
}

alter_algorithm_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_alter_algorithm_value(T);
}
alter_algorithm_value(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_alter_algorithm_value(T);
}

alter_lock_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_alter_lock_value(T);
}
alter_lock_value(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_alter_lock_value(T);
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
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL, D);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO table_name(T) insert_column_list_opt(C)
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL, D);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) table_name(T) insert_column_list_opt(C)
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}

insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL, D);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO table_name(T)
    insert_column_list_opt(C) insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(
        state, I, T, C, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL, D);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(
        state, I, T, C, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}

insert_select_source_statement(A) ::= select_statement(S). {
    A = S;
}
insert_select_source_statement(A) ::= compound_select_statement(S). {
    A = S;
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
    insert_values_source(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V, M);
}
replace_values_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    insert_values_source(V). {
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
joined_delete_statement(A) ::=
    DELETE(D) table_name(T) FROM(F) table_source(LT) join_operator(JO) table_source(RT)
    join_condition_opt(J) where_clause_opt(W). {
    A = mylite_sql_parser_make_joined_delete_statement(
        state,
        D,
        T,
        mylite_sql_parser_make_from_join(state, F, LT, JO, RT, J),
        W);
}
joined_delete_statement(A) ::=
    DELETE(D) FROM table_name(T) USING(U) table_source(LT) join_operator(JO) table_source(RT)
    join_condition_opt(J) where_clause_opt(W). {
    A = mylite_sql_parser_make_joined_delete_statement(
        state,
        D,
        T,
        mylite_sql_parser_make_from_join(state, U, LT, JO, RT, J),
        W);
}

update_statement(A) ::=
    UPDATE(U) update_table_source(T) SET update_assignment_list(S)
    where_clause_opt(W) order_clause_opt(O) update_limit_clause_opt(L). {
    A = mylite_sql_parser_make_update_statement(
        state,
        U,
        (struct mylite_sql_update_statement_parts){
            .target_table = T,
            .assignment_list = S,
            .where_clause = W,
            .order_clause = O,
            .limit_clause = L,
        });
}
joined_update_statement(A) ::=
    UPDATE(U) joined_update_table_source(LT) join_operator(JO) joined_update_table_source(RT)
    SET update_assignment_list(S) where_clause_opt(W) order_clause_opt(O)
    update_limit_clause_opt(L). {
    A = mylite_sql_parser_make_joined_update_statement(
        state,
        U,
        mylite_sql_parser_make_from_join(state, U, LT, JO, RT, NULL),
        S,
        W,
        O,
        L);
}
joined_update_statement(A) ::=
    UPDATE(U) joined_update_table_source(LT) join_operator(JO) joined_update_table_source(RT)
    ON join_condition(J)
    SET update_assignment_list(S) where_clause_opt(W) order_clause_opt(O)
    update_limit_clause_opt(L). {
    A = mylite_sql_parser_make_joined_update_statement(
        state,
        U,
        mylite_sql_parser_make_from_join(state, U, LT, JO, RT, J),
        S,
        W,
        O,
        L);
}

joined_update_table_source(A) ::=
    update_table_source(S). {
    A = S;
}
joined_update_table_source(A) ::=
    table_name(N) AS identifier(AL) table_index_hints_opt(IH). {
    A = mylite_sql_parser_make_table_source(state, N, AL, IH);
}

update_table_source(A) ::= table_name(N) table_index_hints_opt(IH). {
    A = mylite_sql_parser_make_table_source(state, N, NULL, IH);
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

insert_values_source(A) ::= VALUES insert_row_list(R). {
    A = R;
}
insert_values_source(A) ::= VALUE insert_row_list(R). {
    A = R;
}
insert_values_source(A) ::= VALUES insert_row_constructor_list(R). {
    A = R;
}

insert_row_list(A) ::= insert_row(B). {
    A = mylite_sql_parser_make_insert_row_list(state, B);
}
insert_row_list(A) ::= insert_row_list(B) COMMA insert_row(C). {
    A = mylite_sql_parser_append_insert_row(state, B, C);
}

insert_row_constructor_list(A) ::= insert_row_constructor(B). {
    A = mylite_sql_parser_make_insert_row_list(state, B);
}
insert_row_constructor_list(A) ::= insert_row_constructor_list(B) COMMA insert_row_constructor(C). {
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

insert_row_constructor(A) ::= ROW(T) LPAREN insert_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(state, T, V, R);
}
insert_row_constructor(A) ::= ROW(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(
        state,
        T,
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
insert_value(A) ::= current_date_value(T). {
    A = T;
}
insert_value(A) ::= current_time_value(T). {
    A = T;
}
insert_value(A) ::= utc_date_value(T). {
    A = T;
}
insert_value(A) ::= utc_time_value(T). {
    A = T;
}
insert_value(A) ::= utc_timestamp_value(T). {
    A = T;
}
insert_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}
insert_value(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DEFAULT_FUNCTION, C, R);
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
update_value(A) ::= current_date_value(T). {
    A = T;
}
update_value(A) ::= current_time_value(T). {
    A = T;
}
update_value(A) ::= utc_date_value(T). {
    A = T;
}
update_value(A) ::= utc_time_value(T). {
    A = T;
}
update_value(A) ::= utc_timestamp_value(T). {
    A = T;
}
update_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}
update_value(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DEFAULT_FUNCTION, C, R);
}
update_value(A) ::= arithmetic_update_source_column(B) PLUS(T) INTEGER(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= arithmetic_update_source_column(B) MINUS(T) INTEGER(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= update_constant_arithmetic_value(B). {
    A = B;
}
update_value(A) ::= LPAREN(L) select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R);
}

arithmetic_update_source_column(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
arithmetic_update_source_column(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

update_constant_arithmetic_value(A) ::= update_constant_arithmetic_expr(B). {
    A = B;
}
update_constant_arithmetic_value(A) ::= update_constant_arithmetic_multiplicative(B). {
    A = B;
}
update_constant_arithmetic_value(A) ::= LPAREN(L) update_constant_arithmetic_expr(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
update_constant_arithmetic_value(A) ::= LPAREN(L) update_constant_arithmetic_multiplicative(B)
    RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

update_constant_arithmetic_expr(A) ::= update_constant_arithmetic_expr(B) PLUS(T)
    update_constant_arithmetic_operand(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
update_constant_arithmetic_expr(A) ::= update_constant_arithmetic_operand(B) PLUS(T)
    update_constant_arithmetic_operand(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
update_constant_arithmetic_expr(A) ::= update_constant_arithmetic_expr(B) MINUS(T)
    update_constant_arithmetic_operand(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
update_constant_arithmetic_expr(A) ::= update_constant_arithmetic_operand(B) MINUS(T)
    update_constant_arithmetic_operand(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}

update_constant_arithmetic_operand(A) ::= update_constant_arithmetic_multiplicative(B). {
    A = B;
}
update_constant_arithmetic_operand(A) ::= update_constant_arithmetic_factor(B). {
    A = B;
}

update_constant_arithmetic_multiplicative(A) ::=
    update_constant_arithmetic_multiplicative(B) STAR(T) update_constant_arithmetic_factor(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
update_constant_arithmetic_multiplicative(A) ::=
    update_constant_arithmetic_factor(B) STAR(T) update_constant_arithmetic_factor(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}

update_constant_arithmetic_factor(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
update_constant_arithmetic_factor(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
update_constant_arithmetic_factor(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
update_constant_arithmetic_factor(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
update_constant_arithmetic_factor(A) ::= PLUS(P) update_constant_arithmetic_factor(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE, B);
}
update_constant_arithmetic_factor(A) ::= MINUS(M) update_constant_arithmetic_factor(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE, B);
}
update_constant_arithmetic_factor(A) ::= LPAREN(L) update_constant_arithmetic_expr(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
update_constant_arithmetic_factor(A) ::= LPAREN(L) update_constant_arithmetic_multiplicative(B)
    RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
    select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, NULL, NULL, NULL, NULL, NULL, NULL, K);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) DUAL(D)
    where_clause_opt(W) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D), W, NULL, NULL, NULL,
        NULL, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) table_name(N) table_alias_opt(AL)
    table_index_hints_opt(IH) where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL, IH), W, G, H, O,
        L, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM inner_join_table_source(JT)
    where_clause_opt(W) group_clause_opt(G)
    having_clause_opt(H) select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, JT, W, G, H, O, L, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) table_source(LT)
    outer_join_operator(JO) table_source(RT) ON join_condition(J) where_clause_opt(W) group_clause_opt(G)
    having_clause_opt(H) select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_join(state, F, LT, JO, RT, J), W, G, H, O, L, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM comma_table_sources(CT)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, CT, W, G, H, O, L, K);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) STAR(S) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        NULL, NULL, NULL, NULL, NULL, NULL, K);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) STAR(S) FROM(F) DUAL(D)
    where_clause_opt(W) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D), W, NULL, NULL, NULL, NULL, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) STAR(S) FROM(F) table_name(N) table_alias_opt(AL)
    table_index_hints_opt(IH) where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, N, AL, IH), W, G, H, O, L, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) STAR(S) FROM inner_join_table_source(JT)
    where_clause_opt(W) group_clause_opt(G)
    having_clause_opt(H) select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        JT, W, G, H, O, L, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) STAR(S) FROM(F) table_source(LT) outer_join_operator(JO)
    table_source(RT) ON join_condition(J) where_clause_opt(W) group_clause_opt(G)
    having_clause_opt(H) select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_join(state, F, LT, JO, RT, J), W, G, H, O, L, K);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) STAR(S) FROM comma_table_sources(CT)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) select_order_clause_opt(O)
    limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        CT, W, G, H, O, L, K);
}

compound_select_statement(A) ::= select_statement(S) union_term_list(T). {
    A = mylite_sql_parser_make_compound_select_statement(state, S, T);
}

union_term_list(A) ::= union_term(T). {
    A = mylite_sql_parser_make_union_term_list(state, T);
}
union_term_list(A) ::= union_term_list(L) union_term(T). {
    A = mylite_sql_parser_append_union_term(state, L, T);
}

union_term(A) ::= UNION(U) union_modifier_opt(M) select_statement(S). {
    A = mylite_sql_parser_make_union_term(state, U, M, S);
}

union_modifier_opt(A) ::= . {
    A = MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
}
union_modifier_opt(A) ::= DISTINCT. {
    A = MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
}
union_modifier_opt(A) ::= ALL. {
    A = MYLITE_SQL_AST_UNION_MODIFIER_ALL;
}

table_source(A) ::= table_name(N) table_alias_opt(AL) table_index_hints_opt(IH). {
    A = mylite_sql_parser_make_table_source(state, N, AL, IH);
}

inner_join_table_source(A) ::=
    table_source(LT) inner_join_operator(JO) table_source(RT) join_condition_opt(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}
inner_join_table_source(A) ::=
    inner_join_table_source(LT) inner_join_operator(JO) table_source(RT) join_condition_opt(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}

comma_table_sources(A) ::= table_source(LT) COMMA table_source(RT). {
    A = mylite_sql_parser_make_join_source(
        state, LT, MYLITE_SQL_AST_JOIN_KIND_INNER, RT, NULL);
}
comma_table_sources(A) ::= comma_table_sources(LT) COMMA table_source(RT). {
    A = mylite_sql_parser_make_join_source(
        state, LT, MYLITE_SQL_AST_JOIN_KIND_INNER, RT, NULL);
}

table_statement(A) ::= TABLE(T) table_name(N) table_order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_table_statement(state, T, N, O, L);
}

table_index_hints_opt(A) ::= . {
    A = NULL;
}
table_index_hints_opt(A) ::= table_index_hint_list(H). {
    A = H;
}

table_index_hint_list(A) ::= table_index_hint(H). {
    A = mylite_sql_parser_make_index_hint_list(state, H);
}
table_index_hint_list(A) ::= table_index_hint_list(L) table_index_hint(H). {
    A = mylite_sql_parser_append_index_hint(state, L, H);
}

table_index_hint(A) ::= USE(T) index_hint_keyword index_hint_scope_opt(S)
    LPAREN index_hint_name_list(N) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_USE_INDEX_HINT, T, S, N, R);
}
table_index_hint(A) ::= USE(T) index_hint_keyword index_hint_scope_opt(S) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_USE_INDEX_HINT, T, S,
        mylite_sql_parser_make_empty_identifier_list(state, L, R), R);
}
table_index_hint(A) ::= FORCE(T) index_hint_keyword index_hint_scope_opt(S)
    LPAREN index_hint_name_list(N) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_FORCE_INDEX_HINT, T, S, N, R);
}
table_index_hint(A) ::= IGNORE(T) index_hint_keyword index_hint_scope_opt(S)
    LPAREN index_hint_name_list(N) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_IGNORE_INDEX_HINT, T, S, N, R);
}

index_hint_keyword ::= INDEX.
index_hint_keyword ::= KEY.

index_hint_scope_opt(A) ::= . {
    A = NULL;
}
index_hint_scope_opt(A) ::= FOR(F) JOIN(J). {
    A = mylite_sql_parser_make_index_hint_scope(
        state, MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN, F, J);
}
index_hint_scope_opt(A) ::= FOR(F) ORDER BY(B). {
    A = mylite_sql_parser_make_index_hint_scope(
        state, MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY, F, B);
}
index_hint_scope_opt(A) ::= FOR(F) GROUP BY(B). {
    A = mylite_sql_parser_make_index_hint_scope(
        state, MYLITE_SQL_AST_INDEX_HINT_FOR_GROUP_BY, F, B);
}

index_hint_name_list(A) ::= index_hint_name(N). {
    A = mylite_sql_parser_make_identifier_list(state, N);
}
index_hint_name_list(A) ::= index_hint_name_list(L) COMMA index_hint_name(N). {
    A = mylite_sql_parser_append_identifier(state, L, N);
}

index_hint_name(A) ::= identifier(I). {
    A = I;
}
index_hint_name(A) ::= PRIMARY(P). {
    A = mylite_sql_parser_make_identifier(state, P);
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
join_operator(A) ::= RIGHT JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER;
}
join_operator(A) ::= RIGHT OUTER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER;
}

inner_join_operator(A) ::= JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_INNER;
}
inner_join_operator(A) ::= INNER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_INNER;
}
inner_join_operator(A) ::= CROSS JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_INNER;
}

outer_join_operator(A) ::= LEFT JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER;
}
outer_join_operator(A) ::= LEFT OUTER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER;
}
outer_join_operator(A) ::= RIGHT JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER;
}
outer_join_operator(A) ::= RIGHT OUTER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER;
}

join_condition_opt(A) ::= . [ON] {
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
group_clause_opt(A) ::= GROUP(G) BY group_key_list(K). {
    A = mylite_sql_parser_make_group_by_clause(state, G, K);
}

group_key_list(A) ::= qualified_identifier(K). {
    A = mylite_sql_parser_make_group_by_key_list(state, K);
}
group_key_list(A) ::= group_key_list(L) COMMA qualified_identifier(K). {
    A = mylite_sql_parser_append_group_by_key(state, L, K);
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

predicate_atom(A) ::= EXISTS(E) LPAREN select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_exists_predicate(state, E, S, R);
}
predicate_atom(A) ::= predicate_scalar_literal(V). {
    A = V;
}
predicate_atom(A) ::= predicate_scalar_literal(L) predicate_comparison_operator(O)
        predicate_scalar_literal(R). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, L, O.token, O.operator_kind, R);
}
predicate_atom(A) ::= predicate_scalar_literal(L) predicate_comparison_operator(O)
        qualified_identifier(C). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, L, O.token, O.operator_kind, C);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) TRUE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_TRUE, T);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT TRUE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE, T);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) FALSE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_FALSE, T);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT FALSE(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE, T);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) UNKNOWN(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN, T);
}
predicate_atom(A) ::= predicate_scalar_literal(V) IS(I) NOT UNKNOWN(T). {
    A = mylite_sql_parser_make_is_boolean_predicate(
        state, V, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN, T);
}
predicate_atom(A) ::= find_in_set_expression(C). {
    A = C;
}
predicate_atom(A) ::= json_valid_expression(C). {
    A = C;
}
predicate_atom(A) ::= json_contains_predicate_expression(C). {
    A = C;
}
predicate_atom(A) ::= regexp_like_expression(C). {
    A = C;
}
predicate_atom(A) ::= find_in_set_expression(C) EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= json_valid_expression(C) EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= json_contains_predicate_expression(C) EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= regexp_like_expression(C) EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= json_valid_expression(C) NULL_SAFE_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
}
predicate_atom(A) ::=
    json_contains_predicate_expression(C) NULL_SAFE_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
}
predicate_atom(A) ::= regexp_like_expression(C) NULL_SAFE_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
}
predicate_atom(A) ::= find_in_set_expression(C) NOT_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
predicate_atom(A) ::= json_valid_expression(C) NOT_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
predicate_atom(A) ::=
    json_contains_predicate_expression(C) NOT_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
predicate_atom(A) ::= regexp_like_expression(C) NOT_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
predicate_atom(A) ::= find_in_set_expression(C) LESS(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
predicate_atom(A) ::= json_valid_expression(C) LESS(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
predicate_atom(A) ::= json_contains_predicate_expression(C) LESS(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
predicate_atom(A) ::= regexp_like_expression(C) LESS(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
predicate_atom(A) ::= find_in_set_expression(C) LESS_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
predicate_atom(A) ::= json_valid_expression(C) LESS_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
predicate_atom(A) ::=
    json_contains_predicate_expression(C) LESS_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
predicate_atom(A) ::= regexp_like_expression(C) LESS_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
predicate_atom(A) ::= find_in_set_expression(C) GREATER(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
predicate_atom(A) ::= json_valid_expression(C) GREATER(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
predicate_atom(A) ::=
    json_contains_predicate_expression(C) GREATER(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
predicate_atom(A) ::= regexp_like_expression(C) GREATER(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
predicate_atom(A) ::= find_in_set_expression(C) GREATER_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
predicate_atom(A) ::= json_valid_expression(C) GREATER_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
predicate_atom(A) ::=
    json_contains_predicate_expression(C) GREATER_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
predicate_atom(A) ::= regexp_like_expression(C) GREATER_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
predicate_atom(A) ::= find_in_set_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= json_valid_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= json_contains_predicate_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= regexp_like_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= find_in_set_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= json_valid_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= json_contains_predicate_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= regexp_like_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= qualified_identifier(C) EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) NULL_SAFE_EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
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
predicate_atom(A) ::= qualified_identifier(C) REGEXP(O) STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
predicate_atom(A) ::= qualified_identifier(C) RLIKE(O) STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) REGEXP(O) STRING(T). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP,
            mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING)));
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) RLIKE(O) STRING(T). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE,
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
predicate_atom(A) ::= qualified_identifier(C) IN(I) LPAREN select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_in_predicate(state, C, I, S, R);
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) IN(I) LPAREN select_statement(S)
        RPAREN(R). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_in_predicate(state, C, I, S, R));
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
predicate_comparison_value(A) ::= qualified_identifier(V). {
    A = V;
}
predicate_comparison_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
predicate_comparison_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
predicate_comparison_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
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

predicate_scalar_literal(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
predicate_scalar_literal(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
predicate_scalar_literal(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
predicate_scalar_literal(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
predicate_scalar_literal(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
predicate_scalar_literal(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}

predicate_comparison_operator(A) ::= EQUAL(T). {
    A = (struct mylite_sql_comparison_operator_tokens){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_EQUAL,
    };
}
predicate_comparison_operator(A) ::= NULL_SAFE_EQUAL(T). {
    A = (struct mylite_sql_comparison_operator_tokens){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
    };
}
predicate_comparison_operator(A) ::= NOT_EQUAL(T). {
    A = (struct mylite_sql_comparison_operator_tokens){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
    };
}
predicate_comparison_operator(A) ::= LESS(T). {
    A = (struct mylite_sql_comparison_operator_tokens){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LESS,
    };
}
predicate_comparison_operator(A) ::= LESS_EQUAL(T). {
    A = (struct mylite_sql_comparison_operator_tokens){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
    };
}
predicate_comparison_operator(A) ::= GREATER(T). {
    A = (struct mylite_sql_comparison_operator_tokens){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_GREATER,
    };
}
predicate_comparison_operator(A) ::= GREATER_EQUAL(T). {
    A = (struct mylite_sql_comparison_operator_tokens){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
    };
}

order_clause_opt(A) ::= . {
    A = NULL;
}
order_clause_opt(A) ::= ORDER(O) BY qualified_identifier(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_clause(state, O, K, D);
}

select_order_clause_opt(A) ::= . {
    A = NULL;
}
select_order_clause_opt(A) ::=
    ORDER(O) BY select_order_key(K) order_direction_opt(D) select_order_tail_opt(T). {
    A = mylite_sql_parser_make_select_order_by_clause(
        state,
        O,
        (struct mylite_sql_parser_select_order_by_parts){
            .first_order_key = K,
            .first_direction = D,
            .tail_items = T,
        }
    );
}

select_order_tail_opt(A) ::= . {
    A = NULL;
}
select_order_tail_opt(A) ::= COMMA select_order_item_list(L). {
    A = L;
}

select_order_item_list(A) ::= select_order_item(I). {
    A = mylite_sql_parser_make_order_by_item_list(state, I);
}
select_order_item_list(A) ::= select_order_item_list(L) COMMA select_order_item(I). {
    A = mylite_sql_parser_append_order_by_item(state, L, I);
}

select_order_item(A) ::= select_order_key(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_item(state, K, D);
}

select_order_key(A) ::= qualified_identifier(K). {
    A = K;
}
select_order_key(A) ::= select_field_order_expression(K). {
    A = K;
}

select_field_order_expression(A) ::= FIELD(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_FIELD_FUNCTION, B, R);
}
select_field_order_expression(A) ::= FIELD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR, NULL, R);
}
select_field_order_expression(A) ::= LPAREN(L) select_field_order_expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

table_order_clause_opt(A) ::= . {
    A = NULL;
}
table_order_clause_opt(A) ::=
    ORDER(O) BY qualified_identifier(K) order_direction_opt(D) table_order_tail_opt(T). {
    A = mylite_sql_parser_make_select_order_by_clause(
        state,
        O,
        (struct mylite_sql_parser_select_order_by_parts){
            .first_order_key = K,
            .first_direction = D,
            .tail_items = T,
        }
    );
}

table_order_tail_opt(A) ::= . {
    A = NULL;
}
table_order_tail_opt(A) ::= COMMA table_order_item_list(L). {
    A = L;
}

table_order_item_list(A) ::= table_order_item(I). {
    A = mylite_sql_parser_make_order_by_item_list(state, I);
}
table_order_item_list(A) ::= table_order_item_list(L) COMMA table_order_item(I). {
    A = mylite_sql_parser_append_order_by_item(state, L, I);
}

table_order_item(A) ::= qualified_identifier(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_item(state, K, D);
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
select_item(A) ::= qualified_wildcard(B). {
    A = mylite_sql_parser_make_select_item(state, B, NULL);
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
expression(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_user_variable(state, T);
}
expression(A) ::= qualified_identifier(B). {
    A = B;
}
expression(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DEFAULT_FUNCTION, C, R);
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
expression(A) ::= CAST(T) LPAREN expression(V) AS cast_basic_target(K) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(state, T, K, V, R);
}
expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA BINARY RPAREN(R). {
    A = mylite_sql_parser_make_convert_binary_type_expression(state, T, V, R);
}
expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA cast_basic_target(K) RPAREN(R). {
    switch (K) {
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
        A = mylite_sql_parser_make_one_argument_function(
            state, T, MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION, V, R);
        break;
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
        A = mylite_sql_parser_make_one_argument_function(
            state, T, MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION, V, R);
        break;
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
        A = mylite_sql_parser_make_one_argument_function(
            state, T, MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION, V, R);
        break;
    default:
        A = NULL;
        break;
    }
}
expression(A) ::= CONVERT(T) LPAREN expression(V) USING BINARY RPAREN(R). {
    A = mylite_sql_parser_make_convert_using_binary_expression(state, T, V, R);
}
expression(A) ::= CONVERT(T) LPAREN expression(V) USING option_name(C) RPAREN(R). {
    A = mylite_sql_parser_make_convert_using_charset_expression(state, T, V, C, R);
}

cast_basic_target(A) ::= CHAR. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= SIGNED. {
    A = MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION;
}
cast_basic_target(A) ::= SIGNED INTEGER_TYPE. {
    A = MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION;
}
cast_basic_target(A) ::= SIGNED INT. {
    A = MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION;
}
cast_basic_target(A) ::= UNSIGNED. {
    A = MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION;
}
cast_basic_target(A) ::= UNSIGNED INTEGER_TYPE. {
    A = MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION;
}
cast_basic_target(A) ::= UNSIGNED INT. {
    A = MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION;
}
expression(A) ::= DATE_ADD(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I) SECOND RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_DATE_ADD_FUNCTION, V, I, R);
}
expression(A) ::= DATE_SUB(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I) SECOND RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_DATE_SUB_FUNCTION, V, I, R);
}
expression(A) ::= ADDDATE(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I) SECOND RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_ADDDATE_FUNCTION, V, I, R);
}
expression(A) ::= SUBDATE(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I) SECOND RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBDATE_FUNCTION, V, I, R);
}
expression(A) ::= ADDTIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ADDTIME_FUNCTION, B, C, R);
}
expression(A) ::= SUBTIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_SUBTIME_FUNCTION, B, C, R);
}
expression(A) ::= DATE_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_DATE_FORMAT_FUNCTION, B, C, R);
}
expression(A) ::= TIME_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TIME_FORMAT_FUNCTION, B, C, R);
}
expression(A) ::= DATEDIFF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_DATEDIFF_FUNCTION, B, C, R);
}
expression(A) ::=
    TIMESTAMPDIFF(T) LPAREN timestampdiff_unit(U) COMMA expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION, U, B, C, R);
}
expression(A) ::= UNIX_TIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION, R);
}
expression(A) ::= UNIX_TIMESTAMP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION, B, R);
}
expression(A) ::= FROM_UNIXTIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION, B, R);
}
expression(A) ::= FROM_UNIXTIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION, B, C, R);
}
expression(A) ::= DATE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DATE_FUNCTION, B, R);
}
expression(A) ::= TIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TIME_FUNCTION, B, R);
}
expression(A) ::= TIME_TO_SEC(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION, B, R);
}
expression(A) ::= SEC_TO_TIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION, B, R);
}
expression(A) ::= EXTRACT(T) LPAREN extract_unit(U) FROM expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_EXTRACT_FUNCTION, U, B, R);
}
expression(A) ::= WEEK(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEEK_FUNCTION, B, R);
}
expression(A) ::= WEEK(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_WEEK_FUNCTION, B, C, R);
}
expression(A) ::= WEEKDAY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEEKDAY_FUNCTION, B, R);
}
expression(A) ::= WEEKOFYEAR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION, B, R);
}
expression(A) ::= YEARWEEK(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_YEARWEEK_FUNCTION, B, R);
}
expression(A) ::= YEARWEEK(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_YEARWEEK_FUNCTION, B, C, R);
}
expression(A) ::= QUARTER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_QUARTER_FUNCTION, B, R);
}
expression(A) ::= YEAR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_YEAR_FUNCTION, B, R);
}
expression(A) ::= MONTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MONTH_FUNCTION, B, R);
}
expression(A) ::= DAY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAY_FUNCTION, B, R);
}
expression(A) ::= DAYOFMONTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYOFMONTH_FUNCTION, B, R);
}
expression(A) ::= DAYOFWEEK(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYOFWEEK_FUNCTION, B, R);
}
expression(A) ::= DAYOFYEAR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYOFYEAR_FUNCTION, B, R);
}
expression(A) ::= LAST_DAY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LAST_DAY_FUNCTION, B, R);
}
expression(A) ::= HOUR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_HOUR_FUNCTION, B, R);
}
expression(A) ::= MINUTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MINUTE_FUNCTION, B, R);
}
expression(A) ::= SECOND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SECOND_FUNCTION, B, R);
}

extract_unit(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= QUARTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= MONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= DAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= HOUR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= WEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= YEAR_MONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= DAY_HOUR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= DAY_MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= DAY_SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= HOUR_MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= HOUR_SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= MINUTE_SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= DAY_MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= HOUR_MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= MINUTE_MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
extract_unit(A) ::= SECOND_MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

timestampdiff_unit(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= QUARTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_QUARTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= MONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_MONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= WEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_WEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= DAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_DAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= HOUR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_HOUR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= SQL_TSI_SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
timestampdiff_unit(A) ::= MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
expression(A) ::= current_timestamp_value(T). {
    A = T;
}
expression(A) ::= current_date_value(T). {
    A = T;
}
expression(A) ::= current_time_value(T). {
    A = T;
}
expression(A) ::= utc_date_value(T). {
    A = T;
}
expression(A) ::= utc_time_value(T). {
    A = T;
}
expression(A) ::= utc_timestamp_value(T). {
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
current_date_value(A) ::= CURRENT_DATE(T). {
    A = mylite_sql_parser_make_current_date_keyword(state, T);
}
current_date_value(A) ::= CURRENT_DATE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_DATE_VALUE, R);
}
current_date_value(A) ::= CURDATE(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_CURRENT_DATE_VALUE, R);
}
current_time_value(A) ::= CURRENT_TIME(T). {
    A = mylite_sql_parser_make_current_time_keyword(state, T);
}
current_time_value(A) ::= CURRENT_TIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_TIME_VALUE, R);
}
current_time_value(A) ::= CURTIME(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_CURRENT_TIME_VALUE, R);
}
utc_date_value(A) ::= UTC_DATE(T). {
    A = mylite_sql_parser_make_utc_date_keyword(state, T);
}
utc_date_value(A) ::= UTC_DATE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UTC_DATE_VALUE, R);
}
utc_time_value(A) ::= UTC_TIME(T). {
    A = mylite_sql_parser_make_utc_time_keyword(state, T);
}
utc_time_value(A) ::= UTC_TIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UTC_TIME_VALUE, R);
}
utc_timestamp_value(A) ::= UTC_TIMESTAMP(T). {
    A = mylite_sql_parser_make_utc_timestamp_keyword(state, T);
}
utc_timestamp_value(A) ::= UTC_TIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE, R);
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
expression(A) ::= CONCAT_WS(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, B, R);
}
expression(A) ::= CHAR(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CHAR_FUNCTION, B, R);
}
expression(A) ::= REPLACE(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_REPLACE_FUNCTION, B, C, D, R);
}
expression(A) ::= REVERSE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_REVERSE_FUNCTION, B, R);
}
expression(A) ::= QUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_QUOTE_FUNCTION, B, R);
}
expression(A) ::= FIELD(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_FIELD_FUNCTION, B, R);
}
expression(A) ::= GREATEST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_GREATEST_FUNCTION, B, R);
}
expression(A) ::= LEAST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_LEAST_FUNCTION, B, R);
}
expression(A) ::= JSON_ARRAY(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_FUNCTION, R);
}
expression(A) ::= JSON_ARRAY(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_FUNCTION, B, R);
}
expression(A) ::= JSON_OBJECT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_JSON_OBJECT_FUNCTION, R);
}
expression(A) ::= JSON_OBJECT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_OBJECT_FUNCTION, B, R);
}
expression(A) ::= json_contains_predicate_expression(B). {
    A = B;
}
json_contains_predicate_expression(A) ::= json_contains_expression(B). {
    A = B;
}
json_contains_predicate_expression(A) ::= json_contains_path_expression(B). {
    A = B;
}
json_contains_expression(A) ::= JSON_CONTAINS(T) LPAREN expression(B) COMMA
                                expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION, B, C, R);
}
json_contains_expression(A) ::= JSON_CONTAINS(T) LPAREN expression(B) COMMA
                                expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION, B, C, D, R);
}
json_contains_path_expression(A) ::= JSON_CONTAINS_PATH(T) LPAREN
                                     function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION, B, R);
}
expression(A) ::= json_valid_expression(B). {
    A = B;
}
json_valid_expression(A) ::= JSON_VALID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_VALID_FUNCTION, B, R);
}
expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_FUNCTION, B, R);
}
expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_FUNCTION, B, C, R);
}
expression(A) ::= JSON_TYPE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_TYPE_FUNCTION, B, R);
}
expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION, B, C, R);
}
expression(A) ::= JSON_UNQUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION, B, R);
}
expression(A) ::= find_in_set_expression(B). {
    A = B;
}
find_in_set_expression(A) ::= FIND_IN_SET(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_FIND_IN_SET_FUNCTION, B, C, R);
}
expression(A) ::= STRCMP(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_STRCMP_FUNCTION, B, C, R);
}
expression(A) ::= regexp_like_expression(B). {
    A = B;
}
regexp_like_expression(A) ::= REGEXP_LIKE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION, B, C, R);
}
regexp_like_expression(A) ::= REGEXP_LIKE(T) LPAREN expression(B) COMMA expression(C)
                              COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION, B, C, D, R);
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
expression(A) ::= CRC32(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CRC32_FUNCTION, B, R);
}
expression(A) ::= HEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_HEX_FUNCTION, B, R);
}
expression(A) ::= UNHEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNHEX_FUNCTION, B, R);
}
expression(A) ::= IS_UUID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_IS_UUID_FUNCTION, B, R);
}
expression(A) ::= UUID_TO_BIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION, B, R);
}
expression(A) ::= UUID_TO_BIN(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION, B, C, R);
}
expression(A) ::= BIN_TO_UUID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION, B, R);
}
expression(A) ::= BIN_TO_UUID(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION, B, C, R);
}
expression(A) ::= CHARSET(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CHARSET_FUNCTION, B, R);
}
expression(A) ::= COLLATION(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_COLLATION_FUNCTION, B, R);
}
expression(A) ::= COERCIBILITY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_COERCIBILITY_FUNCTION, B, R);
}
expression(A) ::= FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_FORMAT_FUNCTION, B, C, R);
}
expression(A) ::= FORMAT(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED, B, C, D, R);
}
expression(A) ::= TRUNCATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TRUNCATE_FUNCTION, B, C, R);
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
        state, T, MYLITE_SQL_AST_RAND_SEED_FUNCTION, B, R);
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
expression(A) ::= SIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SIN_FUNCTION, B, R);
}
expression(A) ::= COS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_COS_FUNCTION, B, R);
}
expression(A) ::= TAN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TAN_FUNCTION, B, R);
}
expression(A) ::= COT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_COT_FUNCTION, B, R);
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
expression(A) ::= ASCII(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ASCII_FUNCTION, B, R);
}
expression(A) ::= ORD(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ORD_FUNCTION, B, R);
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
expression(A) ::= LEFT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_LEFT_FUNCTION, B, C, R);
}
expression(A) ::= RIGHT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_RIGHT_FUNCTION, B, C, R);
}
expression(A) ::=
    LPAD(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_LPAD_FUNCTION, B, C, D, R);
}
expression(A) ::=
    RPAD(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_RPAD_FUNCTION, B, C, D, R);
}
expression(A) ::= REPEAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_REPEAT_FUNCTION, B, C, R);
}
expression(A) ::= SPACE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SPACE_FUNCTION, B, R);
}
expression(A) ::= LOCATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_LOCATE_FUNCTION, B, C, R);
}
expression(A) ::=
    LOCATE(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_LOCATE_FUNCTION, B, C, D, R);
}
expression(A) ::= INSTR(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_INSTR_FUNCTION, B, C, R);
}
expression(A) ::= POSITION(T) LPAREN(L) expression(B) IN expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_POSITION_FUNCTION, B, C, R);
}
expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, R);
}
expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, D, R);
}
expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, R);
}
expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, D, R);
}
expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, R);
}
expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, D, R);
}
expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, R);
}
expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, D, R);
}
expression(A) ::= MID(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, R);
}
expression(A) ::= MID(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, D, R);
}
expression(A) ::= MID(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, R);
}
expression(A) ::= MID(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, D, R);
}
expression(A) ::=
    SUBSTRING_INDEX(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION, B, C, D, R);
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
expression(A) ::= LTRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LTRIM_FUNCTION, B, R);
}
expression(A) ::= RTRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RTRIM_FUNCTION, B, R);
}
expression(A) ::= TRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(
        state, T, MYLITE_SQL_AST_TRIM_FUNCTION, NULL, B, R);
}
expression(A) ::= TRIM(T) LPAREN expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(
        state, T, MYLITE_SQL_AST_TRIM_FUNCTION, B, C, R);
}
expression(A) ::= TRIM(T) LPAREN trim_direction(D) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(state, T, D, B, C, R);
}
expression(A) ::= TRIM(T) LPAREN trim_direction(D) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(state, T, D, NULL, C, R);
}
trim_direction(A) ::= LEADING. {
    A = MYLITE_SQL_AST_TRIM_LEADING_FUNCTION;
}
trim_direction(A) ::= TRAILING. {
    A = MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION;
}
trim_direction(A) ::= BOTH. {
    A = MYLITE_SQL_AST_TRIM_FUNCTION;
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
expression(A) ::= REGEXP_LIKE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= REGEXP_LIKE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= REGEXP_LIKE(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR, E, R);
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
expression(A) ::= CRC32(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CRC32(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= HEX(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= HEX(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= UNHEX(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= UNHEX(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= IS_UUID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= IS_UUID(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= UUID_TO_BIN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= UUID_TO_BIN(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= BIN_TO_UUID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= BIN_TO_UUID(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= QUOTE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= QUOTE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= FORMAT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FORMAT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    FORMAT(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR, E, R);
}
expression(A) ::= TRUNCATE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= TRUNCATE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    TRUNCATE(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= CONCAT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CONCAT_WS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FIELD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= GREATEST(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LEAST(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_VALID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_VALID(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= JSON_CONTAINS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_CONTAINS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= JSON_CONTAINS(T) LPAREN expression(B) COMMA expression(C) COMMA
                  expression(D) COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR, E, R);
}
expression(A) ::= JSON_CONTAINS_PATH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_EXTRACT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) COMMA expression(C) COMMA
    function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION, D, R);
}
expression(A) ::= JSON_LENGTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) COMMA expression(C) COMMA
    function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= JSON_TYPE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_TYPE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= JSON_UNQUOTE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_UNQUOTE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= FIND_IN_SET(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FIND_IN_SET(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    FIND_IN_SET(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D)
    RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= STRCMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= STRCMP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    STRCMP(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D)
    RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= SUBSTRING_INDEX(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SUBSTRING_INDEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= SUBSTRING_INDEX(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::=
    SUBSTRING_INDEX(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D)
    COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR, E, R);
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
expression(A) ::= TIME_FORMAT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= TIME_FORMAT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    TIME_FORMAT(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= DATEDIFF(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= DATEDIFF(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    DATEDIFF(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= ADDTIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ADDTIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    ADDTIME(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= SUBTIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SUBTIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    SUBTIME(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::=
    UNIX_TIMESTAMP(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= FROM_UNIXTIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    FROM_UNIXTIME(T) LPAREN expression(B) COMMA expression(C) COMMA
    function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= TIME_TO_SEC(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    TIME_TO_SEC(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= SEC_TO_TIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    SEC_TO_TIME(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= DAYOFMONTH(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    DAYOFMONTH(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= DAYOFWEEK(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    DAYOFWEEK(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= DAYOFYEAR(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    DAYOFYEAR(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LAST_DAY(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    LAST_DAY(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= WEEKDAY(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= WEEKDAY(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= WEEKOFYEAR(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= WEEKOFYEAR(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= YEARWEEK(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    YEARWEEK(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D)
    RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= SIN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SIN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= COS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= COS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= TAN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= TAN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= COT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= COT(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= ORD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ORD(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LOCATE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LOCATE(T) LPAREN expression(B) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    LOCATE(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D)
    COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR, E, R);
}
expression(A) ::= INSTR(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= INSTR(T) LPAREN expression(B) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    INSTR(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= LTRIM(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LTRIM(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= RTRIM(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= RTRIM(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= LPAD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= LPAD(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= LPAD(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::=
    LPAD(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA
    function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR, E, R);
}
expression(A) ::= RPAD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= RPAD(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= RPAD(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::=
    RPAD(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA
    function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR, E, R);
}
expression(A) ::= SPACE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SPACE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= GROUP_CONCAT(T) LPAREN(L) qualified_identifier(B)
    group_concat_order_opt(O) group_concat_separator_opt(S) RPAREN(R). {
    A = mylite_sql_parser_make_group_concat_function(state, T, L, B, O, S, R);
}

group_concat_order_opt(A) ::= . {
    A = NULL;
}
group_concat_order_opt(A) ::= ORDER(O) BY qualified_identifier(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_clause(state, O, K, D);
}

group_concat_separator_opt(A) ::= . {
    A = NULL;
}
group_concat_separator_opt(A) ::= SEPARATOR STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
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
expression(A) ::= LAST_INSERT_ID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION, B, R);
}
expression(A) ::= LAST_INSERT_ID(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= expression(B) CONCAT_OPERATOR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_CONCAT, C);
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
expression(A) ::= qualified_identifier(B) JSON_EXTRACT_OPERATOR(T) STRING(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_STRING));
}
expression(A) ::= qualified_identifier(B) JSON_UNQUOTE_EXTRACT_OPERATOR(T) STRING(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_STRING));
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

qualified_wildcard(A) ::= qualified_identifier(B) DOT STAR(S). {
    A = mylite_sql_parser_make_qualified_wildcard(state, B, S);
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
identifier(A) ::= ISOLATION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LEVEL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COMMITTED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UNCOMMITTED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= REPEATABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SERIALIZABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ONLY(T). {
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
identifier(A) ::= CONCAT_WS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= REVERSE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUOTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LPAD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= RPAD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SPACE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIELD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= GREATEST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LEAST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_ARRAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_OBJECT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_CONTAINS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_CONTAINS_PATH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_VALID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_EXTRACT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_TYPE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_UNQUOTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATE_FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIME_FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATEDIFF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIMESTAMPDIFF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_DAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_HOUR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_MONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_QUARTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_WEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SQL_TSI_YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UNIX_TIMESTAMP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FROM_UNIXTIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIME_TO_SEC(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SEC_TO_TIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CAST(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= DATE_ADD(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= DATE_SUB(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= ADDDATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SUBDATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ADDTIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SUBTIME(T). {
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
identifier(A) ::= CRC32(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= HEX(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UNHEX(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= IS_UUID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UUID_TO_BIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIN_TO_UUID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FORMAT(T). {
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
identifier(A) ::= CURDATE(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= CURTIME(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
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
identifier(A) ::= SIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TAN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COT(T). {
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
identifier(A) ::= TRUNCATE(T). {
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
identifier(A) ::= GROUP_CONCAT(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= BIT_COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ASCII(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ORD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SUBSTRING(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= SUBSTR(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= MID(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
}
identifier(A) ::= SUBSTRING_INDEX(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STRCMP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LOCATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= INSTR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= POSITION(T). {
    A = mylite_sql_parser_make_ignore_space_sensitive_identifier(state, T);
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
identifier(A) ::= LTRIM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= RTRIM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TRIM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UTC(T). {
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
identifier(A) ::= VIEW(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TEMPORARY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= AFTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIRST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TEXT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LONG(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= GEOMETRY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= GEOMETRYCOLLECTION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LINESTRING(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MULTILINESTRING(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MULTIPOINT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MULTIPOLYGON(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= POINT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= POLYGON(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIXED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ROW_FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= KEY_BLOCK_SIZE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= PACK_KEYS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DISABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ENABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHECKSUM(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STATS_PERSISTENT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STATS_AUTO_RECALC(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STATS_SAMPLE_PAGES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DYNAMIC(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COMPACT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= REDUNDANT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COMPRESSED(T). {
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
identifier(A) ::= GRANTS(T). {
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
identifier(A) ::= COERCIBILITY(T). {
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
identifier(A) ::= DAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DAYOFMONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DAYOFWEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DAYOFYEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= EXTRACT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= HOUR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LAST_DAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUARTER(T). {
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
identifier(A) ::= WEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= WEEKDAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= WEEKOFYEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= YEARWEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DUPLICATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= VALUE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= START(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TRANSACTION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CONSISTENT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SNAPSHOT(T). {
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
create_table_item(A) ::= fulltext_index_definition(B). {
    A = B;
}
create_table_item(A) ::= spatial_index_definition(B). {
    A = B;
}
create_table_item(A) ::= named_unique_constraint_definition(B). {
    A = B;
}
create_table_item(A) ::= foreign_key_definition(B). {
    A = B;
}
create_table_item(A) ::= check_constraint_definition(B). {
    A = B;
}

primary_key_definition(A) ::=
    PRIMARY(P) KEY index_type_opt(Y) LPAREN primary_key_part_list(L) RPAREN(R)
    index_option_list_opt(O). {
    A = mylite_sql_parser_make_primary_key_definition(state, P, Y, L, R, O);
}

primary_key_part_list(A) ::= primary_key_part(B). {
    A = mylite_sql_parser_make_primary_key_part_list(state, B);
}
primary_key_part_list(A) ::= primary_key_part_list(B) COMMA primary_key_part(C). {
    A = mylite_sql_parser_append_primary_key_part(state, B, C);
}

primary_key_part(A) ::= identifier(B) index_key_direction_opt(D). {
    A = mylite_sql_parser_make_secondary_index_part(state, B, NULL, D);
}

secondary_index_definition(A) ::=
    KEY(K) index_name_opt(N) index_type_opt(Y) LPAREN secondary_index_part_list(L) RPAREN(R)
    index_option_list_opt(O). {
    A = mylite_sql_parser_make_secondary_index_definition(state, K, N, Y, L, R, O);
}
secondary_index_definition(A) ::=
    INDEX(K) index_name_opt(N) index_type_opt(Y) LPAREN secondary_index_part_list(L) RPAREN(R)
    index_option_list_opt(O). {
    A = mylite_sql_parser_make_secondary_index_definition(state, K, N, Y, L, R, O);
}

unique_index_definition(A) ::=
    UNIQUE(U) unique_index_keyword_opt index_name_opt(N) index_type_opt(Y) LPAREN
    secondary_index_part_list(L) RPAREN(R) index_option_list_opt(O). {
    A = mylite_sql_parser_make_unique_index_definition(state, U, N, Y, L, R, O);
}

fulltext_index_definition(A) ::=
    FULLTEXT(F) fulltext_index_keyword_opt index_name_opt(N) LPAREN secondary_index_part_list(L)
    RPAREN(R) index_option_list_opt(O). {
    A = mylite_sql_parser_make_fulltext_index_definition(state, F, N, L, R, O);
}

fulltext_index_keyword_opt ::= .
fulltext_index_keyword_opt ::= KEY.
fulltext_index_keyword_opt ::= INDEX.

spatial_index_definition(A) ::=
    SPATIAL(S) spatial_index_keyword_opt index_name_opt(N) LPAREN secondary_index_part_list(L)
    RPAREN(R) index_option_list_opt(O). {
    A = mylite_sql_parser_make_spatial_index_definition(state, S, N, L, R, O);
}

spatial_index_keyword_opt ::= .
spatial_index_keyword_opt ::= KEY.
spatial_index_keyword_opt ::= INDEX.

named_unique_constraint_definition(A) ::=
    CONSTRAINT identifier(N) UNIQUE(U) unique_index_keyword_opt LPAREN
    secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_unique_index_definition(state, U, N, NULL, L, R, NULL);
}
named_unique_constraint_definition(A) ::=
    CONSTRAINT UNIQUE(U) unique_index_keyword_opt index_name_opt(N) LPAREN
    secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_unique_index_definition(state, U, N, NULL, L, R, NULL);
}
named_unique_constraint_definition(A) ::=
    CONSTRAINT identifier UNIQUE(U) unique_index_keyword_required identifier(N) LPAREN
    secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_unique_index_definition(state, U, N, NULL, L, R, NULL);
}
named_unique_constraint_definition(A) ::=
    CONSTRAINT identifier UNIQUE(U) identifier(N) LPAREN secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_unique_index_definition(state, U, N, NULL, L, R, NULL);
}

index_type_opt(A) ::= . {
    A = NULL;
}
index_type_opt(A) ::= index_type_option(B). {
    A = B;
}

index_type_option(A) ::= USING(U) identifier(T). {
    A = mylite_sql_parser_make_index_type_option(state, U, T);
}

index_option_list_opt(A) ::= . {
    A = NULL;
}
index_option_list_opt(A) ::= index_option_list(B). {
    A = B;
}
index_option_list(A) ::= index_option(B). {
    A = mylite_sql_parser_make_index_option_list(state, B);
}
index_option_list(A) ::= index_option_list(B) index_option(C). {
    A = mylite_sql_parser_append_index_option(state, B, C);
}
index_option(A) ::= index_type_option(B). {
    A = B;
}
index_option(A) ::= index_comment_option(B). {
    A = B;
}
index_option(A) ::= index_visibility_option(B). {
    A = B;
}

index_comment_option(A) ::= COMMENT(T) STRING(V). {
    A = mylite_sql_parser_make_index_comment_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}
index_visibility_option(A) ::= VISIBLE(T). {
    A = mylite_sql_parser_make_index_visibility_option(
        state,
        T,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE);
}
index_visibility_option(A) ::= INVISIBLE(T). {
    A = mylite_sql_parser_make_index_visibility_option(
        state,
        T,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE);
}

unique_index_keyword_opt ::= .
unique_index_keyword_opt ::= KEY.
unique_index_keyword_opt ::= INDEX.

unique_index_keyword_required ::= KEY.
unique_index_keyword_required ::= INDEX.

foreign_key_definition(A) ::=
    constraint_name_opt(N) FOREIGN(F) KEY foreign_key_index_name_opt(I) LPAREN
    foreign_key_part_list(C) RPAREN REFERENCES table_name(P) LPAREN foreign_key_part_list(RL)
    RPAREN(R) foreign_key_action_clause_list_opt(O). {
    A = mylite_sql_parser_make_foreign_key_definition(state, N, F, I, C, P, RL, R, O);
}

check_constraint_definition(A) ::=
    check_constraint_name_opt(N) CHECK(C) LPAREN expression(E) RPAREN(R) check_enforcement_opt(O). {
    A = mylite_sql_parser_make_check_constraint_definition(state, N, C, E, R, O);
}

column_check_constraint_definition(A) ::=
    check_constraint_name_opt(N) CHECK(C) LPAREN expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_check_constraint_definition(state, N, C, E, R, NULL);
}

check_constraint_name_opt(A) ::= . {
    A = NULL;
}
check_constraint_name_opt(A) ::= CONSTRAINT. {
    A = NULL;
}
check_constraint_name_opt(A) ::= CONSTRAINT identifier(B). {
    A = B;
}

check_enforcement_opt(A) ::= . {
    A = NULL;
}
check_enforcement_opt(A) ::= ENFORCED(E). {
    A = mylite_sql_parser_make_check_enforcement(
        state, E, MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED);
}
check_enforcement_opt(A) ::= NOT(N) ENFORCED. {
    A = mylite_sql_parser_make_check_enforcement(
        state, N, MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED);
}

check_enforcement_required(A) ::= ENFORCED(E). {
    A = mylite_sql_parser_make_check_enforcement(
        state, E, MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED);
}
check_enforcement_required(A) ::= NOT(N) ENFORCED. {
    A = mylite_sql_parser_make_check_enforcement(
        state, N, MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED);
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

foreign_key_index_name_opt(A) ::= . {
    A = NULL;
}
foreign_key_index_name_opt(A) ::= identifier(B). {
    A = mylite_sql_parser_make_foreign_key_index_name(state, B);
}

foreign_key_action_clause_list_opt(A) ::= . {
    A = NULL;
}
foreign_key_action_clause_list_opt(A) ::= foreign_key_action_clause_list(B). {
    A = B;
}

foreign_key_action_clause_list(A) ::= foreign_key_action_clause(B). {
    A = mylite_sql_parser_make_foreign_key_action_list(state, B);
}
foreign_key_action_clause_list(A) ::= foreign_key_action_clause_list(B) foreign_key_action_clause(C). {
    A = mylite_sql_parser_append_foreign_key_action(state, B, C);
}

foreign_key_action_clause(A) ::= ON(O) DELETE CASCADE(C). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, C, MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE);
}
foreign_key_action_clause(A) ::= ON(O) DELETE RESTRICT(R). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, R, MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_RESTRICT);
}
foreign_key_action_clause(A) ::= ON(O) DELETE NO ACTION(A1). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, A1, MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION);
}
foreign_key_action_clause(A) ::= ON(O) DELETE SET NULL(N). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, N, MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL);
}
foreign_key_action_clause(A) ::= ON(O) UPDATE CASCADE(C). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, C, MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE);
}
foreign_key_action_clause(A) ::= ON(O) UPDATE RESTRICT(R). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, R, MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT);
}
foreign_key_action_clause(A) ::= ON(O) UPDATE NO ACTION(A1). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, A1, MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_NO_ACTION);
}
foreign_key_action_clause(A) ::= ON(O) UPDATE SET NULL(N). {
    A = mylite_sql_parser_make_foreign_key_action(
        state, O, N, MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL);
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

secondary_index_part(A) ::= identifier(B) index_key_direction_opt(D). {
    A = mylite_sql_parser_make_secondary_index_part(state, B, NULL, D);
}
secondary_index_part(A) ::= identifier(B) LPAREN INTEGER(L) RPAREN index_key_direction_opt(D). {
    A = mylite_sql_parser_make_secondary_index_part(
        state,
        B,
        mylite_sql_parser_make_literal(state, L, MYLITE_SQL_AST_LITERAL_INTEGER),
        D);
}

index_key_direction_opt(A) ::= . {
    A = NULL;
}
index_key_direction_opt(A) ::= ASC(T). {
    A = mylite_sql_parser_make_order_direction(
        state, T, MYLITE_SQL_AST_ORDER_DIRECTION_ASC);
}
index_key_direction_opt(A) ::= DESC(T). {
    A = mylite_sql_parser_make_order_direction(
        state, T, MYLITE_SQL_AST_ORDER_DIRECTION_DESC);
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
column_attribute(A) ::= CHARACTER(C) SET option_name(N). {
    A = mylite_sql_parser_make_column_charset_attribute(state, C, N);
}
column_attribute(A) ::= CHARACTER(C) SET BINARY(N). {
    A = mylite_sql_parser_make_column_charset_attribute(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
column_attribute(A) ::= CHARSET(C) option_name(N). {
    A = mylite_sql_parser_make_column_charset_attribute(state, C, N);
}
column_attribute(A) ::= CHARSET(C) BINARY(N). {
    A = mylite_sql_parser_make_column_charset_attribute(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
column_attribute(A) ::= COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_column_collation_attribute(state, C, N);
}
column_attribute(A) ::= COLLATE(C) BINARY(N). {
    A = mylite_sql_parser_make_column_collation_attribute(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
}
column_attribute(A) ::= COMMENT(C) STRING(V). {
    A = mylite_sql_parser_make_column_comment_attribute(
        state,
        C,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}
column_attribute(A) ::= generated_column_clause(B). {
    A = B;
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
column_attribute(A) ::= column_check_constraint_definition(B). {
    A = B;
}
column_attribute(A) ::= ENFORCED(E). {
    A = mylite_sql_parser_make_check_enforcement(
        state, E, MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED);
}
column_attribute(A) ::= NOT(N) ENFORCED. {
    A = mylite_sql_parser_make_check_enforcement(
        state, N, MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED);
}

generated_column_clause(A) ::=
    generated_always_opt AS(T) LPAREN expression(E) RPAREN(R) generated_storage_opt(S). {
    A = mylite_sql_parser_make_generated_column_clause(state, T, E, R, S);
}

generated_always_opt ::= .
generated_always_opt ::= GENERATED ALWAYS.

generated_storage_opt(A) ::= . {
    A = NULL;
}
generated_storage_opt(A) ::= VIRTUAL(T). {
    A = mylite_sql_parser_make_generated_column_storage(
        state,
        T,
        MYLITE_SQL_AST_GENERATED_COLUMN_VIRTUAL);
}
generated_storage_opt(A) ::= STORED(T). {
    A = mylite_sql_parser_make_generated_column_storage(
        state,
        T,
        MYLITE_SQL_AST_GENERATED_COLUMN_STORED);
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
column_type(A) ::= spatial_type(T). {
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
text_type(A) ::= TEXT(T) LPAREN INTEGER(L) RPAREN(R). {
    A = mylite_sql_parser_make_text_type(
        state,
        (struct mylite_sql_text_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = R,
            .text_type = MYLITE_SQL_AST_TEXT_TYPE_TEXT,
            .has_length = 1,
        });
}

text_type_name(A) ::= TINYTEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .end_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT,
    };
}
text_type_name(A) ::= TEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .end_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_TEXT,
    };
}
text_type_name(A) ::= MEDIUMTEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .end_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
    };
}
text_type_name(A) ::= LONGTEXT(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .end_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT,
    };
}
text_type_name(A) ::= LONG(T). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .end_token = T,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
    };
}
text_type_name(A) ::= LONG(T) VARCHAR(V). {
    A = (struct mylite_sql_text_type_tokens){
        .type_token = T,
        .end_token = V,
        .text_type = MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
    };
}

json_type(A) ::= JSON(T). {
    A = mylite_sql_parser_make_json_type(state, T);
}

spatial_type(A) ::= GEOMETRY(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_GEOMETRY,
        });
}
spatial_type(A) ::= POINT(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_POINT,
        });
}
spatial_type(A) ::= LINESTRING(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_LINESTRING,
        });
}
spatial_type(A) ::= POLYGON(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_POLYGON,
        });
}
spatial_type(A) ::= MULTIPOINT(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_MULTIPOINT,
        });
}
spatial_type(A) ::= MULTILINESTRING(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_MULTILINESTRING,
        });
}
spatial_type(A) ::= MULTIPOLYGON(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_MULTIPOLYGON,
        });
}
spatial_type(A) ::= GEOMETRYCOLLECTION(T). {
    A = mylite_sql_parser_make_spatial_type(
        state,
        (struct mylite_sql_spatial_type_tokens){
            .type_token = T,
            .spatial_type = MYLITE_SQL_AST_SPATIAL_TYPE_GEOMETRYCOLLECTION,
        });
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
binary_string_type_name(A) ::= LONG(T) VARBINARY(V). {
    A = (struct mylite_sql_binary_string_type_tokens){
        .type_token = T,
        .end_token = V,
        .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB,
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
column_default_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
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

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

%right DEFAULT.
%right WITH.
%right ASSIGN.
%right KEY.
%left OR.
%left XOR.
%left AND.
%right NOT.
%right ON.
%left EQUAL NULL_SAFE_EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL IS LIKE REGEXP RLIKE
    BETWEEN.
%left ESCAPE.
%left BITWISE_OR.
%left BITWISE_AND.
%left LEFT_SHIFT RIGHT_SHIFT.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%left BITWISE_XOR.
%left CONCAT_OPERATOR.
%left JSON_EXTRACT_OPERATOR JSON_UNQUOTE_EXTRACT_OPERATOR.
%left COLUMN_BINARY_ATTRIBUTE.
%left ASCII UNICODE.
%left STRING_LITERAL_REDUCE.
%right STRING.
%left INTRODUCED_LITERAL_VALUE.
%left COLLATE.
%right UPLUS UMINUS LOGICAL_NOT BITWISE_NOT BINARY.

%fallback IDENTIFIER SAVEPOINT BINLOG CHAIN ENFORCED NO ACTION ALGORITHM COMMENT CASCADED DEFINER
    INVOKER DISK INSERT_METHOD LAST MEMORY MERGE NULLS RESPECT SECURITY SRID TABLESPACE TEMPTABLE
    UNDEFINED.

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
%type select_lock_wait_opt { struct mylite_sql_token }
%type union_modifier_opt { enum mylite_sql_ast_union_modifier }
%type show_full_opt { int }
%type join_operator { enum mylite_sql_ast_join_kind }
%type inner_join_operator { enum mylite_sql_ast_join_kind }
%type outer_join_operator { enum mylite_sql_ast_join_kind }
%type natural_join_operator { enum mylite_sql_ast_join_kind }
%type table_or_tables { struct mylite_sql_token }
%type trim_direction { enum mylite_sql_ast_node_kind }
%type cast_basic_target { enum mylite_sql_ast_node_kind }
%type cast_length_opt { int }
%type cast_character_set_opt { int }
%type cast_binary_attribute_opt { int }
%type cast_decimal_precision_opt { int }
%type cast_float_precision_opt { int }
%type window_frame_unit { struct mylite_sql_token }
%type aggregate_window_opt { struct mylite_sql_ast_node * }
%type alter_table_option_tail_opt { struct mylite_sql_alter_table_options }
%type alter_table_algorithm_lock_option_list { struct mylite_sql_alter_table_options }
%type alter_table_algorithm_lock_option { struct mylite_sql_alter_table_options }
%type alter_algorithm_value { struct mylite_sql_alter_algorithm_value }
%type alter_lock_value { struct mylite_sql_alter_lock_value }
%type predicate_comparison_operator { struct mylite_sql_comparison_operator_tokens }
%type dml_function_token { struct mylite_sql_token }
%type keyword_function_token { struct mylite_sql_token }
%type merge_insert_method { struct mylite_sql_ast_node * }
%type transaction_completion { struct mylite_sql_transaction_completion }
%type transaction_chain_completion { struct mylite_sql_transaction_completion }
%type transaction_release_completion { struct mylite_sql_transaction_completion }

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
statement(A) ::= query_compound_statement(B). {
    A = B;
}
statement(A) ::= parenthesized_query_expression(B). {
    A = B;
}
statement(A) ::= select_statement(B). {
    A = B;
}
statement(A) ::= with_select_statement(B). {
    A = B;
}
statement(A) ::= table_statement(B). {
    A = B;
}
statement(A) ::= values_statement(B). {
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
statement(A) ::= alter_view_statement(B). {
    A = B;
}
statement(A) ::= create_procedure_statement(B). {
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
statement(A) ::= drop_procedure_statement(B). {
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
statement(A) ::= show_create_procedure_statement(B). {
    A = B;
}
statement(A) ::= show_create_database_statement(B). {
    A = B;
}
statement(A) ::= show_engines_statement(B). {
    A = B;
}
statement(A) ::= show_engine_status_statement(B). {
    A = B;
}
statement(A) ::= show_plugins_statement(B). {
    A = B;
}
statement(A) ::= show_privileges_statement(B). {
    A = B;
}
statement(A) ::= show_binary_log_status_statement(B). {
    A = B;
}
statement(A) ::= show_binary_logs_statement(B). {
    A = B;
}
statement(A) ::= show_binlog_events_statement(B). {
    A = B;
}
statement(A) ::= show_replica_status_statement(B). {
    A = B;
}
statement(A) ::= show_replicas_statement(B). {
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
statement(A) ::= explain_statement(B). {
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
statement(A) ::= alter_table_storage_statistics_statement(B). {
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
statement(A) ::= load_data_infile_statement(B). {
    A = B;
}
statement(A) ::= call_statement(B). {
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
transaction_control_statement(A) ::= COMMIT(C) transaction_completion(O). {
    A = mylite_sql_parser_make_transaction_control_statement_with_completion(
        state, MYLITE_SQL_AST_COMMIT_STATEMENT,
        (struct mylite_sql_transaction_control_tokens){.statement_token = C}, O);
}
transaction_control_statement(A) ::= COMMIT(C) WORK(W) transaction_completion(O). {
    A = mylite_sql_parser_make_transaction_control_statement_with_completion(
        state, MYLITE_SQL_AST_COMMIT_STATEMENT,
        (struct mylite_sql_transaction_control_tokens){.statement_token = C, .work_token = W}, O);
}
transaction_control_statement(A) ::= ROLLBACK(R). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT, R, R, NULL);
}
transaction_control_statement(A) ::= ROLLBACK(R) WORK(W). {
    A = mylite_sql_parser_make_transaction_control_statement(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT, R, W, NULL);
}
transaction_control_statement(A) ::= ROLLBACK(R) transaction_completion(O). {
    A = mylite_sql_parser_make_transaction_control_statement_with_completion(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT,
        (struct mylite_sql_transaction_control_tokens){.statement_token = R}, O);
}
transaction_control_statement(A) ::= ROLLBACK(R) WORK(W) transaction_completion(O). {
    A = mylite_sql_parser_make_transaction_control_statement_with_completion(
        state, MYLITE_SQL_AST_ROLLBACK_STATEMENT,
        (struct mylite_sql_transaction_control_tokens){.statement_token = R, .work_token = W}, O);
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

transaction_completion(A) ::= transaction_chain_completion(C). {
    A = C;
}
transaction_completion(A) ::= transaction_release_completion(R). {
    A = R;
}
transaction_completion(A) ::= transaction_chain_completion(C) transaction_release_completion(R). {
    A = C;
    A.last_token = R.last_token;
    A.has_completion = true;
}

transaction_chain_completion(A) ::= AND(S) CHAIN(C). {
    A = (struct mylite_sql_transaction_completion){
        .last_token = C,
        .chain_start_token = S,
        .chain_end_token = C,
        .has_completion = true,
        .chain = true,
    };
}
transaction_chain_completion(A) ::= AND(S) NO CHAIN(C). {
    A = (struct mylite_sql_transaction_completion){
        .last_token = C,
        .chain_start_token = S,
        .chain_end_token = C,
        .has_completion = true,
        .chain = false,
    };
}

transaction_release_completion(A) ::= RELEASE(R). {
    A = (struct mylite_sql_transaction_completion){
        .last_token = R,
        .has_completion = true,
        .chain = false,
    };
}
transaction_release_completion(A) ::= NO RELEASE(R). {
    A = (struct mylite_sql_transaction_completion){
        .last_token = R,
        .has_completion = true,
        .chain = false,
    };
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
    CHECK(T) TABLES table_name_list(N) check_table_option_list_opt. {
    A = mylite_sql_parser_make_table_maintenance_statement(
        state, MYLITE_SQL_AST_CHECK_TABLE_STATEMENT, T, N);
}
table_maintenance_statement(A) ::=
    CHECKSUM(T) TABLE table_name_list(N) checksum_table_option_opt. {
    A = mylite_sql_parser_make_table_maintenance_statement(
        state, MYLITE_SQL_AST_CHECKSUM_TABLE_STATEMENT, T, N);
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

checksum_table_option_opt ::= .
checksum_table_option_opt ::= QUICK.
checksum_table_option_opt ::= EXTENDED.

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

set_connection_charset_statement(A) ::=
    SET(S) NAMES set_connection_charset_target(C) set_names_collate_opt(L) set_tail_opt(T). {
    A = mylite_sql_parser_attach_set_tail_assignment_list(
        state,
        mylite_sql_parser_make_set_names_statement(state, S, C, L),
        T);
}
set_connection_charset_statement(A) ::=
    SET(S) CHARACTER SET set_connection_charset_target(C) set_tail_opt(T). {
    A = mylite_sql_parser_attach_set_tail_assignment_list(
        state,
        mylite_sql_parser_make_set_character_set_statement(state, S, C),
        T);
}
set_connection_charset_statement(A) ::=
    SET(S) CHARSET set_connection_charset_target(C) set_tail_opt(T). {
    A = mylite_sql_parser_attach_set_tail_assignment_list(
        state,
        mylite_sql_parser_make_set_character_set_statement(state, S, C),
        T);
}

set_connection_charset_target(A) ::= option_name(C). {
    A = C;
}
set_connection_charset_target(A) ::= BINARY(B). {
    A = mylite_sql_parser_make_identifier(state, B);
}
set_connection_charset_target(A) ::= DEFAULT(D). {
    A = mylite_sql_parser_make_set_character_set_default_target(state, D);
}

set_names_collate_opt(A) ::= . {
    A = NULL;
}
set_names_collate_opt(A) ::= COLLATE option_name(C). {
    A = C;
}

set_tail_opt(A) ::= . {
    A = NULL;
}
set_tail_opt(A) ::= COMMA set_assignment_list(L). {
    A = L;
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
set_system_variable_value(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
set_system_variable_value(A) ::= BINARY(T). {
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
set_system_variable_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
set_system_variable_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
set_system_variable_value(A) ::= charset_introducer STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
set_system_variable_value(A) ::= charset_introducer STRING(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
set_system_variable_value(A) ::= charset_introducer HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
set_system_variable_value(A) ::= charset_introducer HEX_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
set_system_variable_value(A) ::= charset_introducer BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
set_system_variable_value(A) ::= charset_introducer BIT_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
set_system_variable_value(A) ::= user_variable(T). {
    A = T;
}
set_system_variable_value(A) ::= LPAREN(L) set_system_variable_value(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
set_system_variable_value(A) ::= set_expression_value(B). {
    A = B;
}
set_system_variable_value(A) ::= variable_value_expression(B). {
    A = B;
}

user_variable_set_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
user_variable_set_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
user_variable_set_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
user_variable_set_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
user_variable_set_value(A) ::= PLUS(P) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
user_variable_set_value(A) ::= PLUS(P) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
user_variable_set_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
user_variable_set_value(A) ::= MINUS(M) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
user_variable_set_value(A) ::= MINUS(M) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
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
user_variable_set_value(A) ::= TEMPORAL_LITERAL_INTRODUCER STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
user_variable_set_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
user_variable_set_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
user_variable_set_value(A) ::= charset_introducer STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
user_variable_set_value(A) ::= charset_introducer STRING(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
user_variable_set_value(A) ::= charset_introducer HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
user_variable_set_value(A) ::= charset_introducer HEX_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
user_variable_set_value(A) ::= charset_introducer BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
user_variable_set_value(A) ::= charset_introducer BIT_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
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
user_variable_set_value(A) ::= set_expression_value(B). {
    A = B;
}
user_variable_set_value(A) ::= variable_value_expression(B). {
    A = B;
}

set_expression_value(A) ::= set_function_value(B). {
    A = B;
}
set_expression_value(A) ::= set_function_value(B) PLUS(T) set_expression_literal(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
set_expression_value(A) ::= set_function_value(B) MINUS(T) set_expression_literal(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
set_expression_value(A) ::= LPAREN(L) select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R);
}

set_function_value(A) ::= dml_function_call(B). {
    A = B;
}
set_function_value(A) ::= current_timestamp_value(B). {
    A = B;
}
set_function_value(A) ::= current_date_value(B). {
    A = B;
}
set_function_value(A) ::= current_time_value(B). {
    A = B;
}
set_function_value(A) ::= utc_date_value(B). {
    A = B;
}
set_function_value(A) ::= utc_time_value(B). {
    A = B;
}
set_function_value(A) ::= utc_timestamp_value(B). {
    A = B;
}
set_function_value(A) ::= sysdate_value(B). {
    A = B;
}
set_function_value(A) ::= session_state_scalar_function_value(B). {
    A = B;
}
set_function_value(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
}
set_function_value(A) ::= REPEAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_REPEAT_FUNCTION, B, C, R);
}
set_function_value(A) ::= REPLACE(T) LPAREN expression(B) COMMA expression(C)
    COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_REPLACE_FUNCTION, B, C, D, R);
}
set_function_value(A) ::= REGEXP_REPLACE(T) LPAREN expression(B) COMMA expression(C)
    COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION, B, C, D, R);
}
set_function_value(A) ::= IF(T) LPAREN expression(B) COMMA expression(C)
    COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_IF_FUNCTION, B, C, D, R);
}
set_function_value(A) ::= IFNULL(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_IFNULL_FUNCTION, B, C, R);
}
set_function_value(A) ::= NULLIF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_FUNCTION, B, C, R);
}
set_function_value(A) ::= COALESCE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_COALESCE_FUNCTION, B, R);
}
set_function_value(A) ::= CONCAT_WS(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, B, R);
}
set_function_value(A) ::= GREATEST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_GREATEST_FUNCTION, B, R);
}
set_function_value(A) ::= LEAST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_LEAST_FUNCTION, B, R);
}
set_function_value(A) ::= LOG10(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG10_FUNCTION, B, R);
}
set_function_value(A) ::= insert_unix_timestamp_now(B). {
    A = B;
}
set_function_value(A) ::= UNIX_TIMESTAMP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION, B, R);
}
set_function_value(A) ::= cast_convert_expression(B). {
    A = B;
}

set_expression_literal(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
set_expression_literal(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
set_expression_literal(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}

variable_value_expression(A) ::= variable_value_head(B) PLUS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
variable_value_expression(A) ::= variable_value_head(B) MINUS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
variable_value_expression(A) ::= variable_value_head(B) STAR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
variable_value_expression(A) ::= variable_value_head(B) DIV(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE, C);
}
variable_value_expression(A) ::= variable_value_head(B) PERCENT(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}
variable_value_expression(A) ::= variable_value_head(B) MOD(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}

variable_value_head(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
variable_value_head(A) ::= user_variable(T). {
    A = T;
}

user_variable(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_user_variable(state, T);
}

select_into_opt(A) ::= . {
    A = NULL;
}
select_into_opt(A) ::= INTO select_into_list(L). {
    A = L;
}

select_into_list(A) ::= user_variable(V). {
    A = mylite_sql_parser_make_select_into_list(state, V);
}
select_into_list(A) ::= select_into_list(L) COMMA user_variable(V). {
    A = mylite_sql_parser_append_select_into_variable(state, L, V);
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
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) table_option_list_opt(O)
    create_table_select_as_opt select_statement(S). {
    A = mylite_sql_parser_make_create_table_select_statement(state, C, E, T, O, S);
}
create_temporary_table_select_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T) table_option_list_opt(O)
    create_table_select_as_opt select_statement(S). {
    A = mylite_sql_parser_make_create_temporary_table_select_statement(state, C, E, T, O, S);
}
create_view_statement(A) ::=
    CREATE(C) create_or_replace_opt(R) view_option_list_opt(O) VIEW table_name(T)
    view_column_list_opt(L) AS view_query_expression(S) view_check_option_opt(K). {
    A = mylite_sql_parser_make_create_view_statement(state, C, R, O, T, L, K, S);
}

alter_view_statement(A) ::=
    ALTER(C) view_option_list_opt(O) VIEW table_name(T) view_column_list_opt(L)
    AS view_query_expression(S) view_check_option_opt(K). {
    A = mylite_sql_parser_make_alter_view_statement(state, C, O, T, L, K, S);
}

create_or_replace_opt(A) ::= . {
    A = NULL;
}
create_or_replace_opt(A) ::= OR(O) REPLACE(R). {
    A = mylite_sql_parser_make_create_or_replace_clause(state, O, R);
}

view_option_list_opt(A) ::= . {
    A = NULL;
}
view_option_list_opt(A) ::= view_option_list(L). {
    A = L;
}
view_option_list(A) ::= view_option(O). {
    A = mylite_sql_parser_make_view_option_list(state, O);
}
view_option_list(A) ::= view_option_list(L) view_option(O). {
    A = mylite_sql_parser_append_view_option(state, L, O);
}

view_option(A) ::= ALGORITHM(T) equal_opt view_algorithm_value(V). {
    A = mylite_sql_parser_make_view_algorithm_option(state, T, V);
}
view_option(A) ::= DEFINER(T) equal_opt view_definer_account(D). {
    A = mylite_sql_parser_make_view_definer_option(state, T, D);
}
view_option(A) ::= SQL(S) SECURITY view_security_value(V). {
    A = mylite_sql_parser_make_view_security_option(state, S, V);
}

view_algorithm_value(A) ::= UNDEFINED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
view_algorithm_value(A) ::= MERGE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
view_algorithm_value(A) ::= TEMPTABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

view_security_value(A) ::= DEFINER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
view_security_value(A) ::= INVOKER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

view_definer_account(A) ::= identifier(U). {
    A = mylite_sql_parser_make_view_definer_account(state, U, NULL);
}
view_definer_account(A) ::= STRING(U). {
    A = mylite_sql_parser_make_view_definer_account(
        state, mylite_sql_parser_make_literal(state, U, MYLITE_SQL_AST_LITERAL_STRING), NULL);
}
view_definer_account(A) ::= identifier(U) user_variable(H). {
    A = mylite_sql_parser_make_view_definer_account(state, U, H);
}
view_definer_account(A) ::= STRING(U) user_variable(H). {
    A = mylite_sql_parser_make_view_definer_account(
        state, mylite_sql_parser_make_literal(state, U, MYLITE_SQL_AST_LITERAL_STRING), H);
}
view_definer_account(A) ::= CURRENT_USER(T). {
    A = mylite_sql_parser_make_current_user_view_definer_account(state, T, T);
}
view_definer_account(A) ::= CURRENT_USER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_current_user_view_definer_account(state, T, R);
}

view_column_list_opt(A) ::= . {
    A = NULL;
}
view_column_list_opt(A) ::= LPAREN identifier_list(L) RPAREN. {
    A = L;
}

view_check_option_opt(A) ::= . {
    A = NULL;
}
view_check_option_opt(A) ::= WITH(W) CHECK OPTION(O). {
    A = mylite_sql_parser_make_view_check_option(state, W, O);
}
view_check_option_opt(A) ::= WITH(W) LOCAL CHECK OPTION(O). {
    A = mylite_sql_parser_make_view_check_option(state, W, O);
}
view_check_option_opt(A) ::= WITH(W) CASCADED CHECK OPTION(O). {
    A = mylite_sql_parser_make_view_check_option(state, W, O);
}

create_procedure_statement(A) ::=
    CREATE(C) PROCEDURE table_name(P) LPAREN RPAREN BEGIN select_statement(S) SEMICOLON END(E). {
    A = mylite_sql_parser_make_create_procedure_statement(state, C, P, S, E);
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

drop_index_statement(A) ::= DROP(D) INDEX drop_index_name(I) ON table_name(T). {
    A = mylite_sql_parser_make_drop_index_statement(state, D, I, T);
}

drop_index_name(A) ::= identifier(I). {
    A = I;
}
drop_index_name(A) ::= PRIMARY(P). {
    A = mylite_sql_parser_make_identifier(state, P);
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
table_option(A) ::= MIN_ROWS(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_min_rows_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= MAX_ROWS(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_max_rows_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= AVG_ROW_LENGTH(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_avg_row_length_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= DELAY_KEY_WRITE(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_delay_key_write_option(state, T, V);
}
table_option(A) ::= TABLESPACE(T) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_tablespace_option(state, T, N, NULL);
}
table_option(A) ::= STORAGE(T) MEMORY(V). {
    A = mylite_sql_parser_make_table_storage_option(
        state,
        T,
        mylite_sql_parser_make_identifier(state, V));
}
table_option(A) ::= STORAGE(T) DISK(V). {
    A = mylite_sql_parser_make_table_storage_option(
        state,
        T,
        mylite_sql_parser_make_identifier(state, V));
}
table_option(A) ::= UNION(T) equal_opt LPAREN table_name_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_table_union_option(state, T, L, R);
}
table_option(A) ::= UNION(T) equal_opt LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_table_union_option(state, T, NULL, R);
}
table_option(A) ::= INSERT_METHOD(T) equal_opt merge_insert_method(V). {
    A = mylite_sql_parser_make_table_insert_method_option(state, T, V);
}

merge_insert_method(A) ::= NO(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
merge_insert_method(A) ::= FIRST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
merge_insert_method(A) ::= LAST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
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

drop_table_statement(A) ::= DROP(D) table_or_tables drop_if_exists_opt(E) table_name_list(T)
        drop_table_tail_opt. {
    A = mylite_sql_parser_make_drop_table_statement(state, D, E, T);
}
drop_temporary_table_statement(A) ::=
    DROP(D) TEMPORARY table_or_tables drop_if_exists_opt(E) table_name_list(T)
        drop_table_tail_opt. {
    A = mylite_sql_parser_make_drop_temporary_table_statement(state, D, E, T);
}
drop_view_statement(A) ::= DROP(D) VIEW drop_if_exists_opt(E) table_name_list(T)
        view_drop_tail_opt. {
    A = mylite_sql_parser_make_drop_view_statement(state, D, E, T);
}
drop_procedure_statement(A) ::= DROP(D) PROCEDURE drop_if_exists_opt(E) table_name(T). {
    A = mylite_sql_parser_make_drop_procedure_statement(state, D, E, T);
}

view_drop_tail_opt ::= .
view_drop_tail_opt ::= RESTRICT.
view_drop_tail_opt ::= CASCADE.

drop_table_tail_opt ::= .
drop_table_tail_opt ::= RESTRICT.
drop_table_tail_opt ::= CASCADE.

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

show_tables_statement(A) ::=
    SHOW(S) show_full_opt(F) TABLES(T) show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, NULL, L);
}
show_tables_statement(A) ::=
    SHOW(S) show_full_opt(F) TABLES(T) FROM identifier(D)
    show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, D, L);
}
show_tables_statement(A) ::=
    SHOW(S) show_full_opt(F) TABLES(T) IN identifier(D)
    show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, D, L);
}
show_tables_statement(A) ::=
    SHOW(S) EXTENDED show_full_opt(F) TABLES(T) show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, NULL, L);
}
show_tables_statement(A) ::=
    SHOW(S) EXTENDED show_full_opt(F) TABLES(T) FROM identifier(D)
    show_tables_filter_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, F, D, L);
}
show_tables_statement(A) ::=
    SHOW(S) EXTENDED show_full_opt(F) TABLES(T) IN identifier(D)
    show_tables_filter_opt(L). {
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

show_character_set_statement(A) ::= SHOW(S) CHARACTER SET(T) show_catalog_filter_opt(L). {
    A = mylite_sql_parser_make_show_character_set_statement(state, S, T, L);
}
show_character_set_statement(A) ::= SHOW(S) CHARSET(T) show_catalog_filter_opt(L). {
    A = mylite_sql_parser_make_show_character_set_statement(state, S, T, L);
}

show_collation_statement(A) ::= SHOW(S) COLLATION(C) show_catalog_filter_opt(L). {
    A = mylite_sql_parser_make_show_collation_statement(state, S, C, L);
}

show_catalog_filter_opt(A) ::= . {
    A = NULL;
}
show_catalog_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_catalog_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
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

show_events_statement(A) ::= SHOW(S) EVENTS(E) show_schema_object_filter_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, NULL, L);
}
show_events_statement(A) ::= SHOW(S) EVENTS(E) FROM identifier(D) show_schema_object_filter_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, D, L);
}
show_events_statement(A) ::= SHOW(S) EVENTS(E) IN identifier(D) show_schema_object_filter_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, D, L);
}

show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) show_schema_object_filter_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, NULL, L);
}
show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) FROM identifier(D) show_schema_object_filter_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, D, L);
}
show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) IN identifier(D) show_schema_object_filter_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, D, L);
}

show_schema_object_filter_opt(A) ::= . {
    A = NULL;
}
show_schema_object_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_schema_object_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

show_routine_status_statement(A) ::= SHOW(S) PROCEDURE STATUS(T) show_catalog_filter_opt(L). {
    A = mylite_sql_parser_make_show_routine_status_statement(
        state, S, T, MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT, L);
}
show_routine_status_statement(A) ::= SHOW(S) FUNCTION STATUS(T) show_catalog_filter_opt(L). {
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
show_grants_statement(A) ::= SHOW(S) GRANTS FOR show_grants_target(T)
                              show_grants_using_opt(R). {
    A = mylite_sql_parser_make_show_grants_for_target_statement(state, S, T, R);
}

show_grants_using_opt(A) ::= . {
    A = NULL;
}
show_grants_using_opt(A) ::= USING show_grants_role_list(R). {
    A = R;
}

show_grants_target(A) ::= CURRENT_USER(T). {
    A = mylite_sql_parser_make_current_user_keyword(state, T);
}
show_grants_target(A) ::= CURRENT_USER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_current_user_show_grants_target(state, T, R);
}
show_grants_target(A) ::= show_grants_account_name(N). {
    A = N;
}

show_grants_role_list(A) ::= show_grants_role_name(R). {
    A = mylite_sql_parser_make_show_grants_role_list(state, R);
}
show_grants_role_list(A) ::= show_grants_role_list(L) COMMA show_grants_role_name(R). {
    A = mylite_sql_parser_append_show_grants_role(state, L, R);
}

show_grants_role_name(A) ::= show_grants_account_name(N). {
    A = N;
}

show_grants_account_name(A) ::= show_grants_account_user(U). {
    A = mylite_sql_parser_make_show_grants_account(state, U, NULL);
}
show_grants_account_name(A) ::= show_grants_account_user(U) user_variable(H). {
    A = mylite_sql_parser_make_show_grants_account(state, U, H);
}

show_grants_account_user(A) ::= identifier(U). {
    A = U;
}
show_grants_account_user(A) ::= STRING(U). {
    A = mylite_sql_parser_make_literal(state, U, MYLITE_SQL_AST_LITERAL_STRING);
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

show_databases_statement(A) ::= SHOW(S) DATABASES(D) show_databases_filter_opt(F). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, F);
}
show_databases_statement(A) ::= SHOW(S) SCHEMAS(D) show_databases_filter_opt(F). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, F);
}

show_databases_filter_opt(A) ::= . {
    A = NULL;
}
show_databases_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_databases_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
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
show_status_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

show_create_table_statement(A) ::= SHOW(S) CREATE TABLE table_name(T). {
    A = mylite_sql_parser_make_show_create_table_statement(state, S, T);
}
show_create_view_statement(A) ::= SHOW(S) CREATE VIEW table_name(T). {
    A = mylite_sql_parser_make_show_create_view_statement(state, S, T);
}

show_create_procedure_statement(A) ::= SHOW(S) CREATE PROCEDURE table_name(T). {
    A = mylite_sql_parser_make_show_create_procedure_statement(state, S, T);
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

show_engine_status_statement(A) ::= SHOW(S) ENGINE option_name(N) STATUS(T). {
    A = mylite_sql_parser_make_show_engine_status_statement(state, S, N, T);
}

show_plugins_statement(A) ::= SHOW(S) PLUGINS(P). {
    A = mylite_sql_parser_make_show_plugins_statement(state, S, P);
}

show_privileges_statement(A) ::= SHOW(S) PRIVILEGES(P). {
    A = mylite_sql_parser_make_show_privileges_statement(state, S, P);
}

show_binary_log_status_statement(A) ::= SHOW(S) BINARY LOG STATUS(T). {
    A = mylite_sql_parser_make_show_binary_log_status_statement(state, S, T);
}

show_binary_logs_statement(A) ::= SHOW(S) BINARY LOGS(L). {
    A = mylite_sql_parser_make_show_binary_logs_statement(state, S, L);
}

show_binlog_events_statement(A) ::= SHOW(S) BINLOG EVENTS(E). {
    A = mylite_sql_parser_make_show_binlog_events_statement(state, S, E);
}

show_replica_status_statement(A) ::= SHOW(S) REPLICA STATUS(T). {
    A = mylite_sql_parser_make_show_replica_status_statement(state, S, T);
}

show_replicas_statement(A) ::= SHOW(S) REPLICAS(R). {
    A = mylite_sql_parser_make_show_replicas_statement(state, S, R);
}

describe_table_statement(A) ::= DESCRIBE(D) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, D, T, NULL, NULL);
}
describe_table_statement(A) ::= DESC(D) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, D, T, NULL, NULL);
}

explain_statement(A) ::= EXPLAIN(E) explain_format_opt(F) explainable_statement(S). {
    A = mylite_sql_parser_make_explain_statement(state, E, F, NULL, S);
}
explain_statement(A) ::= EXPLAIN(E) ANALYZE(Z) explain_format_opt(F)
    explainable_analyze_statement(S). {
    A = mylite_sql_parser_make_explain_statement(
        state,
        E,
        F,
        mylite_sql_parser_make_explain_analyze(state, Z),
        S);
}

explain_format_opt(A) ::= . {
    A = NULL;
}
explain_format_opt(A) ::= FORMAT(F) EQUAL explain_format_name(N). {
    A = mylite_sql_parser_make_explain_format(state, F, N);
}

explain_format_name(A) ::= identifier(N). {
    A = N;
}

explainable_statement(A) ::= select_statement(S). {
    A = S;
}
explainable_statement(A) ::= compound_select_statement(S). {
    A = S;
}
explainable_statement(A) ::= table_statement(S). {
    A = S;
}
explainable_statement(A) ::= values_statement(S). {
    A = S;
}
explainable_statement(A) ::= insert_values_statement(S). {
    A = S;
}
explainable_statement(A) ::= insert_select_statement(S). {
    A = S;
}
explainable_statement(A) ::= insert_set_statement(S). {
    A = S;
}
explainable_statement(A) ::= replace_values_statement(S). {
    A = S;
}
explainable_statement(A) ::= replace_select_statement(S). {
    A = S;
}
explainable_statement(A) ::= replace_set_statement(S). {
    A = S;
}
explainable_statement(A) ::= update_statement(S). {
    A = S;
}
explainable_statement(A) ::= delete_statement(S). {
    A = S;
}

explainable_analyze_statement(A) ::= select_statement(S). {
    A = S;
}
explainable_analyze_statement(A) ::= compound_select_statement(S). {
    A = S;
}
explainable_analyze_statement(A) ::= table_statement(S). {
    A = S;
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
rename_table_statement(A) ::= RENAME(R) TABLES rename_table_pair_list(L). {
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
    ALTER(A1) TABLE table_name(T) alter_table_multi_action_list(L) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_multi_action_statement(state, A1, T, L, O);
}
alter_table_multi_action_statement(A) ::=
    ALTER(A1) TABLE table_name(T) alter_table_algorithm_lock_option_list(P) COMMA
    alter_table_leading_option_action_list(L). {
    A = mylite_sql_parser_make_alter_table_multi_action_statement(
        state, A1, T, L, P);
}
alter_table_multi_action_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD(A2) column_keyword_opt LPAREN
    alter_table_parenthesized_add_column_list(L) RPAREN alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_multi_action_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_alter_table_add_column_action_list(state, A2, L),
        O);
}

alter_table_multi_action_list(A) ::= alter_table_multi_first_action(L) alter_table_multi_action(N). {
    A = mylite_sql_parser_append_alter_table_action(state, L, N);
}
alter_table_multi_action_list(A) ::=
    alter_table_multi_action_list(L) COMMA alter_table_multi_action(N). {
    A = mylite_sql_parser_append_alter_table_action(state, L, N);
}

alter_table_leading_option_action_list(A) ::= alter_table_multi_action(N). {
    A = mylite_sql_parser_make_alter_table_action_list(state, N);
}
alter_table_leading_option_action_list(A) ::=
    alter_table_leading_option_action_list(L) COMMA alter_table_multi_action(N). {
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
alter_table_multi_first_action(A) ::=
    ADD(A1) column_keyword_opt LPAREN column_definition(C) RPAREN COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_column_statement(
            state,
            A1,
            NULL,
            C,
            NULL,
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
alter_table_multi_first_action(A) ::= ADD(A1) named_primary_key_definition(P) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_primary_key_statement(
            state,
            A1,
            NULL,
            P,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) fulltext_index_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) spatial_index_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) foreign_key_definition(F) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_foreign_key_statement(
            state,
            A1,
            NULL,
            F,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ADD(A1) check_constraint_definition(C) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_check_statement(state, A1, NULL, C));
}
alter_table_multi_first_action(A) ::= DROP(A1) column_keyword_opt identifier(C) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_column_statement(
            state,
            A1,
            NULL,
            C,
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
alter_table_multi_first_action(A) ::= DROP(A1) FOREIGN KEY identifier(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_foreign_key_statement(
            state,
            A1,
            NULL,
            I,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= DROP(A1) CONSTRAINT identifier(C) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_constraint_statement(
            state,
            A1,
            NULL,
            C,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= DROP(A1) CHECK identifier(C) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_check_statement(state, A1, NULL, C));
}
alter_table_multi_first_action(A) ::=
    RENAME(A1) table_rename_connector_opt table_name(T) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_rename_statement(state, A1, NULL, T));
}
alter_table_multi_first_action(A) ::= RENAME(A1) COLUMN identifier(O) TO identifier(N) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_rename_column_statement(
            state,
            A1,
            NULL,
            O,
            N,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::=
    RENAME(A1) INDEX old_identifier(O) TO new_identifier(N) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_rename_index_statement(
            state,
            A1,
            NULL,
            O,
            N,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::=
    RENAME(A1) KEY old_identifier(O) TO new_identifier(N) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_rename_index_statement(
            state,
            A1,
            NULL,
            O,
            N,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::=
    MODIFY(A1) column_keyword_opt column_definition(C) column_position_opt(P) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_modify_column_statement(
            state,
            A1,
            NULL,
            C,
            P,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::=
    CHANGE(A1) column_keyword_opt identifier(O) column_definition(C) column_position_opt(P) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_change_column_statement(
            state,
            A1,
            NULL,
            O,
            C,
            P,
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
alter_table_multi_first_action(A) ::= ALTER(A1) INDEX identifier(I) VISIBLE(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_index_visibility_statement(
            state, A1, NULL, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ALTER(A1) INDEX identifier(I) INVISIBLE(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_index_visibility_statement(
            state, A1, NULL, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::=
    ALTER(A1) column_keyword_opt identifier(C) SET VISIBLE(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_column_visibility_statement(
            state, A1, NULL, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE));
}
alter_table_multi_first_action(A) ::=
    ALTER(A1) column_keyword_opt identifier(C) SET INVISIBLE(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_column_visibility_statement(
            state, A1, NULL, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE));
}
alter_table_multi_first_action(A) ::=
    ALTER(A1) CHECK identifier(C) check_enforcement_required(E) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_alter_check_statement(state, A1, NULL, C, E));
}
alter_table_multi_first_action(A) ::=
    ALTER(A1) CONSTRAINT identifier(C) check_enforcement_required(E) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_alter_check_statement(state, A1, NULL, C, E));
}
alter_table_multi_first_action(A) ::= AUTO_INCREMENT(T) equal_opt INTEGER(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_auto_increment_statement(
            state,
            T,
            NULL,
            mylite_sql_parser_make_table_auto_increment_option(
                state,
                T,
                mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER))));
}
alter_table_multi_first_action(A) ::= alter_table_default_charset_collation_option(O) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_default_charset_collation_statement(
            state,
            (struct mylite_sql_token){0},
            NULL,
            mylite_sql_parser_make_table_option_list(state, O)));
}
alter_table_multi_first_action(A) ::= alter_table_multi_convert_character_set_action(C) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(state, C);
}
alter_table_multi_first_action(A) ::= COMMENT(C) equal_opt STRING(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_comment_statement(
            state,
            C,
            NULL,
            mylite_sql_parser_make_table_comment_option(
                state,
                C,
                mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING)),
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= FORCE(A1) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_force_statement(
            state,
            A1,
            NULL,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= DISABLE(A1) KEYS COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_disable_keys_statement(
            state,
            A1,
            NULL,
            mylite_sql_parser_empty_alter_table_options()));
}
alter_table_multi_first_action(A) ::= ENABLE(A1) KEYS COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_enable_keys_statement(
            state,
            A1,
            NULL,
            mylite_sql_parser_empty_alter_table_options()));
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
alter_table_multi_action(A) ::=
    ADD(A1) column_keyword_opt LPAREN column_definition(C) RPAREN. {
    A = mylite_sql_parser_make_alter_table_add_column_statement(
        state,
        A1,
        NULL,
        C,
        NULL,
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
alter_table_multi_action(A) ::= ADD(A1) named_primary_key_definition(P). {
    A = mylite_sql_parser_make_alter_table_add_primary_key_statement(
        state,
        A1,
        NULL,
        P,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) fulltext_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) spatial_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) foreign_key_definition(F). {
    A = mylite_sql_parser_make_alter_table_add_foreign_key_statement(
        state,
        A1,
        NULL,
        F,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ADD(A1) check_constraint_definition(C). {
    A = mylite_sql_parser_make_alter_table_add_check_statement(state, A1, NULL, C);
}
alter_table_multi_action(A) ::= DROP(A1) column_keyword_opt identifier(C). {
    A = mylite_sql_parser_make_alter_table_drop_column_statement(
        state,
        A1,
        NULL,
        C,
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
alter_table_multi_action(A) ::= DROP(A1) FOREIGN KEY identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_foreign_key_statement(
        state,
        A1,
        NULL,
        I,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= DROP(A1) CONSTRAINT identifier(C). {
    A = mylite_sql_parser_make_alter_table_drop_constraint_statement(
        state,
        A1,
        NULL,
        C,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= DROP(A1) CHECK identifier(C). {
    A = mylite_sql_parser_make_alter_table_drop_check_statement(state, A1, NULL, C);
}
alter_table_multi_action(A) ::= RENAME(A1) table_rename_connector_opt table_name(T). {
    A = mylite_sql_parser_make_alter_table_rename_statement(state, A1, NULL, T);
}
alter_table_multi_action(A) ::= RENAME(A1) COLUMN identifier(O) TO identifier(N). {
    A = mylite_sql_parser_make_alter_table_rename_column_statement(
        state,
        A1,
        NULL,
        O,
        N,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= RENAME(A1) INDEX old_identifier(O) TO new_identifier(N). {
    A = mylite_sql_parser_make_alter_table_rename_index_statement(
        state,
        A1,
        NULL,
        O,
        N,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= RENAME(A1) KEY old_identifier(O) TO new_identifier(N). {
    A = mylite_sql_parser_make_alter_table_rename_index_statement(
        state,
        A1,
        NULL,
        O,
        N,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::=
    MODIFY(A1) column_keyword_opt column_definition(C) column_position_opt(P). {
    A = mylite_sql_parser_make_alter_table_modify_column_statement(
        state,
        A1,
        NULL,
        C,
        P,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::=
    CHANGE(A1) column_keyword_opt identifier(O) column_definition(C) column_position_opt(P). {
    A = mylite_sql_parser_make_alter_table_change_column_statement(
        state,
        A1,
        NULL,
        O,
        C,
        P,
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
alter_table_multi_action(A) ::= ALTER(A1) INDEX identifier(I) VISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_index_visibility_statement(
        state, A1, NULL, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ALTER(A1) INDEX identifier(I) INVISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_index_visibility_statement(
        state, A1, NULL, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ALTER(A1) column_keyword_opt identifier(C) SET VISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_column_visibility_statement(
        state, A1, NULL, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE);
}
alter_table_multi_action(A) ::= ALTER(A1) column_keyword_opt identifier(C) SET INVISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_column_visibility_statement(
        state, A1, NULL, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE);
}
alter_table_multi_action(A) ::= ALTER(A1) CHECK identifier(C) check_enforcement_required(E). {
    A = mylite_sql_parser_make_alter_table_alter_check_statement(state, A1, NULL, C, E);
}
alter_table_multi_action(A) ::=
    ALTER(A1) CONSTRAINT identifier(C) check_enforcement_required(E). {
    A = mylite_sql_parser_make_alter_table_alter_check_statement(state, A1, NULL, C, E);
}
alter_table_multi_action(A) ::= AUTO_INCREMENT(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_alter_table_auto_increment_statement(
        state,
        T,
        NULL,
        mylite_sql_parser_make_table_auto_increment_option(
            state,
            T,
            mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER)));
}
alter_table_multi_action(A) ::= alter_table_default_charset_collation_option_list(O). {
    A = mylite_sql_parser_make_alter_table_default_charset_collation_statement(
        state,
        (struct mylite_sql_token){0},
        NULL,
        O);
}
alter_table_multi_action(A) ::= alter_table_multi_convert_character_set_action(A1). {
    A = A1;
}
alter_table_multi_action(A) ::= COMMENT(C) equal_opt STRING(V). {
    A = mylite_sql_parser_make_alter_table_comment_statement(
        state,
        C,
        NULL,
        mylite_sql_parser_make_table_comment_option(
            state,
            C,
            mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING)),
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= alter_table_storage_statistics_option(O). {
    A = mylite_sql_parser_make_alter_table_storage_statistics_statement(
        state,
        (struct mylite_sql_token){0},
        NULL,
        mylite_sql_parser_make_table_option_list(state, O),
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= FORCE(A1). {
    A = mylite_sql_parser_make_alter_table_force_statement(
        state,
        A1,
        NULL,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= DISABLE(A1) KEYS. {
    A = mylite_sql_parser_make_alter_table_disable_keys_statement(
        state,
        A1,
        NULL,
        mylite_sql_parser_empty_alter_table_options());
}
alter_table_multi_action(A) ::= ENABLE(A1) KEYS. {
    A = mylite_sql_parser_make_alter_table_enable_keys_statement(
        state,
        A1,
        NULL,
        mylite_sql_parser_empty_alter_table_options());
}

alter_table_add_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD column_keyword_opt column_definition(C)
    column_position_opt(P) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_column_statement(state, A1, T, C, P, O);
}
alter_table_add_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD column_keyword_opt LPAREN column_definition(C) RPAREN
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_column_statement(state, A1, T, C, NULL, O);
}

alter_table_parenthesized_add_column_list(A) ::=
    column_definition(C) COMMA column_definition(N). {
    A = mylite_sql_parser_append_column_definition(
        state,
        mylite_sql_parser_make_column_definition_list(state, C),
        N);
}
alter_table_parenthesized_add_column_list(A) ::=
    alter_table_parenthesized_add_column_list(L) COMMA column_definition(C). {
    A = mylite_sql_parser_append_column_definition(state, L, C);
}

alter_table_add_primary_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD primary_key_definition(P) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_add_primary_key_statement(state, A1, T, P, O);
}

alter_table_add_primary_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD named_primary_key_definition(P)
    alter_table_option_tail_opt(O). {
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
alter_table_alter_check_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER CONSTRAINT identifier(C) check_enforcement_required(E). {
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
    column_position_opt(P) alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_modify_column_statement(state, A1, T, C, P, O);
}

alter_table_change_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CHANGE column_keyword_opt identifier(O) column_definition(C)
    column_position_opt(P) alter_table_option_tail_opt(P2). {
    A = mylite_sql_parser_make_alter_table_change_column_statement(state, A1, T, O, C, P, P2);
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
    default_opt CHARACTER(C) SET equal_opt BINARY(N). {
    A = mylite_sql_parser_make_table_charset_option(
        state,
        C,
        mylite_sql_parser_make_identifier(state, N));
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
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARACTER(C) SET DEFAULT(D)
    convert_character_set_collate_opt(O). {
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
                    mylite_sql_parser_make_identifier(state, D))),
            O));
}
alter_table_convert_character_set_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CONVERT TO CHARSET(C) DEFAULT(D)
    convert_character_set_collate_opt(O). {
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
                    mylite_sql_parser_make_identifier(state, D))),
            O));
}

alter_table_multi_convert_character_set_action(A) ::=
    CONVERT TO CHARACTER(C) SET option_name(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        C,
        NULL,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(state, C, N)),
            O));
}
alter_table_multi_convert_character_set_action(A) ::=
    CONVERT TO CHARACTER(C) SET BINARY(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        C,
        NULL,
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
alter_table_multi_convert_character_set_action(A) ::=
    CONVERT TO CHARSET(C) option_name(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        C,
        NULL,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(state, C, N)),
            O));
}
alter_table_multi_convert_character_set_action(A) ::=
    CONVERT TO CHARSET(C) BINARY(N) convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        C,
        NULL,
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
alter_table_multi_convert_character_set_action(A) ::= CONVERT TO CHARACTER(C) SET DEFAULT(D)
    convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        C,
        NULL,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(
                    state,
                    C,
                    mylite_sql_parser_make_identifier(state, D))),
            O));
}
alter_table_multi_convert_character_set_action(A) ::= CONVERT TO CHARSET(C) DEFAULT(D)
    convert_character_set_collate_opt(O). {
    A = mylite_sql_parser_make_alter_table_convert_character_set_statement(
        state,
        C,
        NULL,
        mylite_sql_parser_append_table_option(
            state,
            mylite_sql_parser_make_table_option_list(
                state,
                mylite_sql_parser_make_table_charset_option(
                    state,
                    C,
                    mylite_sql_parser_make_identifier(state, D))),
            O));
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

alter_table_storage_statistics_statement(A) ::=
    ALTER(A1) TABLE table_name(T) alter_table_storage_statistics_option_list(O)
    alter_table_option_tail_opt(P). {
    A = mylite_sql_parser_make_alter_table_storage_statistics_statement(state, A1, T, O, P);
}

alter_table_storage_statistics_option_list(A) ::= alter_table_storage_statistics_option(O). {
    A = mylite_sql_parser_make_table_option_list(state, O);
}
alter_table_storage_statistics_option_list(A) ::=
    alter_table_storage_statistics_option_list(L) alter_table_storage_statistics_statement_option(O). {
    A = mylite_sql_parser_append_table_option(state, L, O);
}
alter_table_storage_statistics_option_list(A) ::=
    alter_table_storage_statistics_option_list(L) COMMA
    alter_table_storage_statistics_statement_option(O). {
    A = mylite_sql_parser_append_table_option(state, L, O);
}

alter_table_storage_statistics_statement_option(A) ::= alter_table_storage_statistics_option(O). {
    A = O;
}
alter_table_storage_statistics_statement_option(A) ::= COMMENT(T) equal_opt STRING(V). {
    A = mylite_sql_parser_make_table_comment_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}

alter_table_storage_statistics_option(A) ::= ENGINE(E) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_engine_option(state, E, N);
}
alter_table_storage_statistics_option(A) ::= ROW_FORMAT(T) equal_opt row_format_option_value(V). {
    A = mylite_sql_parser_make_table_row_format_option(state, T, V);
}
alter_table_storage_statistics_option(A) ::= KEY_BLOCK_SIZE(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_key_block_size_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
alter_table_storage_statistics_option(A) ::=
    PACK_KEYS(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_pack_keys_option(state, T, V);
}
alter_table_storage_statistics_option(A) ::= CHECKSUM(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_checksum_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
alter_table_storage_statistics_option(A) ::=
    STATS_PERSISTENT(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_stats_persistent_option(state, T, V);
}
alter_table_storage_statistics_option(A) ::=
    STATS_AUTO_RECALC(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_stats_auto_recalc_option(state, T, V);
}
alter_table_storage_statistics_option(A) ::=
    STATS_SAMPLE_PAGES(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_stats_sample_pages_option(state, T, V);
}
alter_table_storage_statistics_option(A) ::= MIN_ROWS(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_min_rows_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
alter_table_storage_statistics_option(A) ::= MAX_ROWS(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_max_rows_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
alter_table_storage_statistics_option(A) ::= AVG_ROW_LENGTH(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_avg_row_length_option(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
alter_table_storage_statistics_option(A) ::=
    DELAY_KEY_WRITE(T) equal_opt table_default_or_integer_option_value(V). {
    A = mylite_sql_parser_make_table_delay_key_write_option(state, T, V);
}
alter_table_storage_statistics_option(A) ::= TABLESPACE(T) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_tablespace_option(state, T, N, NULL);
}
alter_table_storage_statistics_option(A) ::= STORAGE(T) MEMORY(V). {
    A = mylite_sql_parser_make_table_storage_option(
        state,
        T,
        mylite_sql_parser_make_identifier(state, V));
}
alter_table_storage_statistics_option(A) ::= STORAGE(T) DISK(V). {
    A = mylite_sql_parser_make_table_storage_option(
        state,
        T,
        mylite_sql_parser_make_identifier(state, V));
}
alter_table_storage_statistics_option(A) ::= UNION(T) equal_opt LPAREN table_name_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_table_union_option(state, T, L, R);
}
alter_table_storage_statistics_option(A) ::= UNION(T) equal_opt LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_table_union_option(state, T, NULL, R);
}
alter_table_storage_statistics_option(A) ::= INSERT_METHOD(T) equal_opt merge_insert_method(V). {
    A = mylite_sql_parser_make_table_insert_method_option(state, T, V);
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
    INSERT(I) insert_modifier_opt(M) INTO partitioned_table_name(T) insert_column_list_opt(C)
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL, D);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO partitioned_table_name(T)
    insert_column_list_opt(C)
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) partitioned_table_name(T) insert_column_list_opt(C)
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, NULL, D);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) partitioned_table_name(T) insert_column_list_opt(C)
    insert_values_source(R) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_statement(
        state, I, T, C, R, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}

insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO partitioned_table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL, D);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO partitioned_table_name(T)
    insert_column_list_opt(C) insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(
        state, I, T, C, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) partitioned_table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL, D);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) partitioned_table_name(T) insert_column_list_opt(C)
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
    REPLACE(R) replace_modifier_opt(M) INTO partitioned_table_name(T) insert_column_list_opt(C)
    insert_values_source(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V, M);
}
replace_values_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) partitioned_table_name(T) insert_column_list_opt(C)
    insert_values_source(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V, M);
}

replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO partitioned_table_name(T) insert_column_list_opt(C)
    select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S, M);
}
replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) partitioned_table_name(T) insert_column_list_opt(C)
    select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S, M);
}

replace_set_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO partitioned_table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_replace_set_statement(state, R, T, S, M);
}
replace_set_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) partitioned_table_name(T) SET insert_assignment_list(S). {
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
    INSERT(I) insert_modifier_opt(M) INTO partitioned_table_name(T) SET insert_assignment_list(S)
    on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, NULL, D);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) INTO partitioned_table_name(T)
    SET insert_assignment_list(S) on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(
        state, I, T, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) partitioned_table_name(T) SET insert_assignment_list(S)
    on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, NULL, D);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) IGNORE(G) partitioned_table_name(T)
    SET insert_assignment_list(S)
    on_duplicate_key_update_opt(D). {
    A = mylite_sql_parser_make_insert_set_statement(
        state, I, T, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G), D
    );
}

partitioned_table_name(A) ::= table_name(T). {
    A = T;
}
partitioned_table_name(A) ::= table_name(T) table_partition_selection(P). {
    (void)P;
    A = T;
}

table_partition_selection(A) ::= PARTITION LPAREN identifier_list(L) RPAREN. {
    A = L;
}

load_data_infile_statement(A) ::=
    LOAD(L) DATA INFILE STRING(F) INTO TABLE partitioned_table_name(T). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        NULL,
        NULL,
        NULL);
}
load_data_infile_statement(A) ::=
    LOAD(L) DATA INFILE STRING(F) INTO TABLE partitioned_table_name(T) load_data_ignore_lines(I). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        I,
        NULL,
        NULL);
}
load_data_infile_statement(A) ::=
    LOAD(L) DATA INFILE STRING(F) INTO TABLE partitioned_table_name(T) load_data_column_list(C). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        NULL,
        C,
        NULL);
}
load_data_infile_statement(A) ::=
    LOAD(L) DATA INFILE STRING(F) INTO TABLE partitioned_table_name(T)
    load_data_ignore_lines(I) load_data_column_list(C). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        I,
        C,
        NULL);
}
load_data_infile_statement(A) ::=
    LOAD(L) DATA LOCAL(LO) INFILE STRING(F) INTO TABLE partitioned_table_name(T). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        NULL,
        NULL,
        mylite_sql_parser_make_load_data_local_modifier(state, LO));
}
load_data_infile_statement(A) ::=
    LOAD(L) DATA LOCAL(LO) INFILE STRING(F) INTO TABLE partitioned_table_name(T)
    load_data_ignore_lines(I). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        I,
        NULL,
        mylite_sql_parser_make_load_data_local_modifier(state, LO));
}
load_data_infile_statement(A) ::=
    LOAD(L) DATA LOCAL(LO) INFILE STRING(F) INTO TABLE partitioned_table_name(T)
    load_data_column_list(C). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        NULL,
        C,
        mylite_sql_parser_make_load_data_local_modifier(state, LO));
}
load_data_infile_statement(A) ::=
    LOAD(L) DATA LOCAL(LO) INFILE STRING(F) INTO TABLE partitioned_table_name(T)
    load_data_ignore_lines(I) load_data_column_list(C). {
    A = mylite_sql_parser_make_load_data_infile_statement(
        state,
        L,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_STRING),
        T,
        I,
        C,
        mylite_sql_parser_make_load_data_local_modifier(state, LO));
}

load_data_ignore_lines(A) ::= IGNORE INTEGER(N) LINES. {
    A = mylite_sql_parser_make_literal(state, N, MYLITE_SQL_AST_LITERAL_INTEGER);
}
load_data_column_list(A) ::= LPAREN identifier_list(L) RPAREN. {
    A = L;
}

call_statement(A) ::= CALL(C) table_name(P). {
    A = mylite_sql_parser_make_call_statement(state, C, P, NULL);
}
call_statement(A) ::= CALL(C) table_name(P) LPAREN RPAREN. {
    A = mylite_sql_parser_make_call_statement(state, C, P, NULL);
}
call_statement(A) ::= CALL(C) table_name(P) LPAREN function_argument_list(L) RPAREN. {
    A = mylite_sql_parser_make_call_statement(state, C, P, L);
}

delete_statement(A) ::=
    DELETE(D) FROM delete_table_reference(T) where_clause_opt(W) order_clause_opt(O)
    delete_limit_clause_opt(L). {
    A = mylite_sql_parser_make_delete_statement(state, D, T, W, O, L);
}
delete_statement(A) ::=
    DELETE(D) LOW_PRIORITY FROM delete_table_reference(T) where_clause_opt(W) order_clause_opt(O)
    delete_limit_clause_opt(L). {
    A = mylite_sql_parser_make_delete_statement(state, D, T, W, O, L);
}
delete_statement(A) ::=
    DELETE(D) DELETE_QUICK_MODIFIER FROM delete_table_reference(T) where_clause_opt(W)
    order_clause_opt(O) delete_limit_clause_opt(L). {
    A = mylite_sql_parser_make_delete_statement(state, D, T, W, O, L);
}
delete_statement(A) ::=
    DELETE(D) LOW_PRIORITY DELETE_QUICK_MODIFIER FROM delete_table_reference(T) where_clause_opt(W)
    order_clause_opt(O) delete_limit_clause_opt(L). {
    A = mylite_sql_parser_make_delete_statement(state, D, T, W, O, L);
}
delete_table_reference(A) ::= table_name(T) table_alias_opt(AL). {
    A = AL == NULL ? T : mylite_sql_parser_make_table_source(state, T, AL, NULL);
}
delete_table_reference(A) ::= table_name(T) table_partition_selection(P) table_alias_opt(AL). {
    (void)P;
    A = AL == NULL ? T : mylite_sql_parser_make_table_source(state, T, AL, NULL);
}
joined_delete_statement(A) ::=
    DELETE(D) table_name_list(T) FROM(F) table_source(LT) join_operator(JO) table_source(RT)
    join_condition_opt(J) where_clause_opt(W). {
    A = mylite_sql_parser_make_joined_delete_statement(
        state,
        D,
        T,
        mylite_sql_parser_make_from_join(state, F, LT, JO, RT, J),
        W);
}
joined_delete_statement(A) ::=
    DELETE(D) table_name_list(T) FROM comma_table_sources(S) where_clause_opt(W). {
    A = mylite_sql_parser_make_joined_delete_statement(
        state,
        D,
        T,
        S,
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
update_statement(A) ::=
    UPDATE(U) LOW_PRIORITY(LP) update_table_source(T) SET update_assignment_list(S)
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
            .low_priority_modifier =
                mylite_sql_parser_make_update_low_priority_modifier(state, LP),
        });
}
update_statement(A) ::=
    UPDATE(U) IGNORE(I) update_table_source(T) SET update_assignment_list(S)
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
            .ignore_modifier = mylite_sql_parser_make_update_ignore_modifier(state, I),
        });
}
update_statement(A) ::=
    UPDATE(U) LOW_PRIORITY(LP) IGNORE(I) update_table_source(T) SET update_assignment_list(S)
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
            .low_priority_modifier =
                mylite_sql_parser_make_update_low_priority_modifier(state, LP),
            .ignore_modifier = mylite_sql_parser_make_update_ignore_modifier(state, I),
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
    UPDATE(U) joined_update_table_source(LT) COMMA joined_update_table_source(RT)
    SET update_assignment_list(AL) where_clause_opt(W) order_clause_opt(O)
    update_limit_clause_opt(L). {
    A = mylite_sql_parser_make_joined_update_statement(
        state,
        U,
        mylite_sql_parser_make_from_join(state, U, LT, MYLITE_SQL_AST_JOIN_KIND_INNER, RT, NULL),
        AL,
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
joined_update_table_source(A) ::=
    table_name(N) table_partition_selection(P) AS identifier(AL) table_index_hints_opt(IH). {
    (void)P;
    A = mylite_sql_parser_make_table_source(state, N, AL, IH);
}
joined_update_table_source(A) ::=
    table_name(N) joined_update_bare_alias(AL) table_index_hints_opt(IH). {
    A = mylite_sql_parser_make_table_source(state, N, AL, IH);
}
joined_update_table_source(A) ::=
    table_name(N) table_partition_selection(P) joined_update_bare_alias(AL)
    table_index_hints_opt(IH). {
    (void)P;
    A = mylite_sql_parser_make_table_source(state, N, AL, IH);
}
joined_update_table_source(A) ::=
    joined_update_derived_table_source(S). {
    A = S;
}
joined_update_bare_alias(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
joined_update_bare_alias(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
joined_update_derived_table_source(A) ::=
    LPAREN(L) select_statement(S) RPAREN(R) joined_update_derived_alias(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}
joined_update_derived_alias(A) ::= joined_update_bare_alias(AL). {
    A = AL;
}
joined_update_derived_alias(A) ::= AS identifier(AL). {
    A = AL;
}

update_table_source(A) ::= table_name(N) table_index_hints_opt(IH). {
    A = mylite_sql_parser_make_table_source(state, N, NULL, IH);
}
update_table_source(A) ::= table_name(N) table_partition_selection(P) table_index_hints_opt(IH). {
    (void)P;
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
duplicate_update_value(A) ::= arithmetic_duplicate_source_column(B) PLUS(T) INTEGER(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_INTEGER));
}
duplicate_update_value(A) ::= arithmetic_duplicate_source_column(B) MINUS(T) INTEGER(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_INTEGER));
}

arithmetic_duplicate_source_column(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
arithmetic_duplicate_source_column(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
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
insert_value(A) ::= string_text_literal(V). {
    A = V;
}
insert_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
insert_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
insert_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
insert_value(A) ::= user_variable(T). {
    A = T;
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
insert_value(A) ::= sysdate_value(T). {
    A = T;
}
insert_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}
insert_value(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DEFAULT_FUNCTION, C, R);
}
insert_value(A) ::= rand_expression(B). {
    A = B;
}
insert_value(A) ::= insert_unix_timestamp_value(B). {
    A = B;
}
insert_value(A) ::= dml_constant_scalar_value(B). {
    A = B;
}
insert_value(A) ::= update_constant_arithmetic_value(B). {
    A = B;
}
insert_value(A) ::= dml_function_call(B). {
    A = B;
}
insert_value(A) ::= ROW(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_generic_function(state, T, B, R);
}
insert_value(A) ::= LPAREN(L) dml_parenthesized_scalar_value(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
insert_value(A) ::= BITWISE_NOT(T) dml_bitwise_operand(B). [BITWISE_NOT] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, B);
}
insert_value(A) ::= dml_bitwise_operand(B) BITWISE_OR(T) dml_bitwise_operand(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_OR, C);
}
insert_value(A) ::= variable_value_expression(B). {
    A = B;
}

insert_unix_timestamp_value(A) ::= insert_unix_timestamp_now(B). {
    A = B;
}
insert_unix_timestamp_value(A) ::=
    insert_unix_timestamp_now(B) PLUS(T) insert_unix_timestamp_delta(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
insert_unix_timestamp_value(A) ::=
    insert_unix_timestamp_now(B) MINUS(T) insert_unix_timestamp_delta(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}

insert_unix_timestamp_now(A) ::= UNIX_TIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION, R);
}

insert_unix_timestamp_delta(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
insert_unix_timestamp_delta(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_unix_timestamp_delta(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_unix_timestamp_delta(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
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
update_value(A) ::= string_text_literal(V). {
    A = V;
}
update_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
update_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
update_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
update_value(A) ::= user_variable(T). {
    A = T;
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
update_value(A) ::= sysdate_value(T). {
    A = T;
}
update_value(A) ::= insert_unix_timestamp_value(B). {
    A = B;
}
update_value(A) ::= rand_expression(B). {
    A = B;
}
update_value(A) ::= dml_constant_scalar_value(B). {
    A = B;
}
update_value(A) ::=
    DATE_ADD(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_DATE_ADD_FUNCTION, C, I, U, R);
}
update_value(A) ::=
    DATE_SUB(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_DATE_SUB_FUNCTION, C, I, U, R);
}
update_value(A) ::=
    ADDDATE(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_ADDDATE_FUNCTION, C, I, U, R);
}
update_value(A) ::=
    SUBDATE(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBDATE_FUNCTION, C, I, U, R);
}
update_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}
update_value(A) ::= DEFAULT(T) LPAREN qualified_identifier(C) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DEFAULT_FUNCTION, C, R);
}
update_value(A) ::= qualified_identifier(C). {
    A = C;
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
update_value(A) ::= arithmetic_update_source_column(B) PLUS(T) arithmetic_update_source_column(C). {
    A = mylite_sql_parser_make_binary_expression(state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
update_value(A) ::= arithmetic_update_source_column(B) MINUS(T) arithmetic_update_source_column(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
update_value(A) ::= arithmetic_update_source_column(B) STAR(T) arithmetic_update_source_column(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
update_value(A) ::= update_constant_arithmetic_value(B). {
    A = B;
}
update_value(A) ::= LPAREN(L) select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R);
}
update_value(A) ::= dml_function_call(B). {
    A = B;
}
update_value(A) ::= ROW(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_generic_function(state, T, B, R);
}
update_value(A) ::= LPAREN(L) dml_parenthesized_scalar_value(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
update_value(A) ::= BITWISE_NOT(T) dml_bitwise_operand(B). [BITWISE_NOT] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, B);
}
update_value(A) ::= dml_bitwise_operand(B) BITWISE_OR(T) dml_bitwise_operand(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_OR, C);
}
update_value(A) ::= variable_value_expression(B). {
    A = B;
}

dml_parenthesized_scalar_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
dml_parenthesized_scalar_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
dml_parenthesized_scalar_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
dml_parenthesized_scalar_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
dml_parenthesized_scalar_value(A) ::= PLUS(P) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
dml_parenthesized_scalar_value(A) ::= PLUS(P) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
dml_parenthesized_scalar_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
dml_parenthesized_scalar_value(A) ::= MINUS(M) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
dml_parenthesized_scalar_value(A) ::= MINUS(M) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
dml_parenthesized_scalar_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
dml_parenthesized_scalar_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
dml_parenthesized_scalar_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
dml_parenthesized_scalar_value(A) ::= string_text_literal(V). {
    A = V;
}
dml_parenthesized_scalar_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
dml_parenthesized_scalar_value(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
dml_parenthesized_scalar_value(A) ::= dml_constant_scalar_value(B). {
    A = B;
}
dml_parenthesized_scalar_value(A) ::= dml_function_call(B). {
    A = B;
}
dml_parenthesized_scalar_value(A) ::= BITWISE_NOT(T) dml_bitwise_operand(B). [BITWISE_NOT] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, B);
}

dml_function_call(A) ::= dml_function_token(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_generic_function(state, T, NULL, R);
}
dml_function_call(A) ::= dml_function_token(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_generic_function(state, T, B, R);
}

dml_bitwise_operand(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
dml_bitwise_operand(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
dml_bitwise_operand(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}

dml_function_token(A) ::= IDENTIFIER(T). {
    A = T;
}
dml_function_token(A) ::= POINT(T). {
    A = T;
}
dml_function_token(A) ::= LINESTRING(T). {
    A = T;
}
dml_function_token(A) ::= POLYGON(T). {
    A = T;
}
dml_function_token(A) ::= MULTIPOINT(T). {
    A = T;
}
dml_function_token(A) ::= MULTILINESTRING(T). {
    A = T;
}
dml_function_token(A) ::= MULTIPOLYGON(T). {
    A = T;
}
dml_function_token(A) ::= GEOMETRYCOLLECTION(T). {
    A = T;
}
dml_function_token(A) ::= GEOMCOLLECTION(T). {
    A = T;
}

keyword_function_token(A) ::= ROW(T). {
    A = T;
}
keyword_function_token(A) ::= VALUES(T). {
    A = T;
}
keyword_function_token(A) ::= GROUPING(T). {
    A = T;
}
keyword_function_token(A) ::= POINT(T). {
    A = T;
}
keyword_function_token(A) ::= LINESTRING(T). {
    A = T;
}
keyword_function_token(A) ::= POLYGON(T). {
    A = T;
}
keyword_function_token(A) ::= MULTIPOINT(T). {
    A = T;
}
keyword_function_token(A) ::= MULTILINESTRING(T). {
    A = T;
}
keyword_function_token(A) ::= MULTIPOLYGON(T). {
    A = T;
}
keyword_function_token(A) ::= GEOMETRYCOLLECTION(T). {
    A = T;
}
keyword_function_token(A) ::= GEOMCOLLECTION(T). {
    A = T;
}

collated_literal_expression(A) ::= ordinary_string_literal(V) COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_collate_expression(
        state,
        V,
        C,
        N);
}
collated_literal_expression(A) ::= HEX_LITERAL(T) COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_collate_expression(
        state,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX),
        C,
        N);
}
collated_literal_expression(A) ::= BIT_LITERAL(T) COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_collate_expression(
        state,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT),
        C,
        N);
}

dml_constant_scalar_value(A) ::= collated_literal_expression(V). {
    A = V;
}
dml_constant_scalar_value(A) ::= charset_introducer STRING(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
dml_constant_scalar_value(A) ::= charset_introducer STRING(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
dml_constant_scalar_value(A) ::= charset_introducer HEX_LITERAL(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
dml_constant_scalar_value(A) ::= charset_introducer HEX_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
dml_constant_scalar_value(A) ::= charset_introducer BIT_LITERAL(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
dml_constant_scalar_value(A) ::= charset_introducer BIT_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
dml_constant_scalar_value(A) ::= TEMPORAL_LITERAL_INTRODUCER STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
dml_constant_scalar_value(A) ::= cast_convert_expression(B). {
    A = B;
}
dml_constant_scalar_value(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= REPEAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_REPEAT_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= REPLACE(T) LPAREN expression(B) COMMA expression(C)
    COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_REPLACE_FUNCTION, B, C, D, R);
}
dml_constant_scalar_value(A) ::= REGEXP_REPLACE(T) LPAREN expression(B) COMMA expression(C)
    COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION, B, C, D, R);
}
dml_constant_scalar_value(A) ::= REGEXP_SUBSTR(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= STR_TO_DATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= SEC_TO_TIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= IF(T) LPAREN expression(B) COMMA expression(C)
    COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_IF_FUNCTION, B, C, D, R);
}
dml_constant_scalar_value(A) ::= IFNULL(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_IFNULL_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= NULLIF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= COALESCE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_COALESCE_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= CONCAT_WS(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= GREATEST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_GREATEST_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= LEAST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_LEAST_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= JSON_ARRAY(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_FUNCTION, R);
}
dml_constant_scalar_value(A) ::= JSON_ARRAY(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= JSON_OBJECT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_JSON_OBJECT_FUNCTION, R);
}
dml_constant_scalar_value(A) ::= JSON_OBJECT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_OBJECT_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= JSON_EXTRACT(T) LPAREN expression(B) COMMA
                                 expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= JSON_UNQUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= JSON_QUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= JSON_VALID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_VALID_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= HEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_HEX_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= UNHEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNHEX_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= ABS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ABS_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= FLOOR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FLOOR_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= ROUND(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ROUND_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= ACOS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ACOS_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= LOG2(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG2_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= POW(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_POW_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= POWER(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_POWER_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= BIT_COUNT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= DATE_ADD(T) LPAREN(L) STRING(B) COMMA INTERVAL expression(C)
    date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state,
        T,
        L,
        MYLITE_SQL_AST_DATE_ADD_FUNCTION,
        mylite_sql_parser_make_literal(state, B, MYLITE_SQL_AST_LITERAL_STRING),
        C,
        U,
        R);
}
dml_constant_scalar_value(A) ::= ADDTIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ADDTIME_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= TIMESTAMP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMP_FUNCTION, B, R);
}
dml_constant_scalar_value(A) ::= TIMESTAMP(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMP_FUNCTION, B, C, R);
}
dml_constant_scalar_value(A) ::= session_state_scalar_function_value(B). {
    A = B;
}

session_state_scalar_function_value(A) ::= CONNECTION_ID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CONNECTION_ID_FUNCTION, R);
}
session_state_scalar_function_value(A) ::= ROW_COUNT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_ROW_COUNT_FUNCTION, R);
}
session_state_scalar_function_value(A) ::= FOUND_ROWS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_FOUND_ROWS_FUNCTION, R);
}
session_state_scalar_function_value(A) ::= LAST_INSERT_ID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION, R);
}
session_state_scalar_function_value(A) ::= LAST_INSERT_ID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION, B, R);
}
session_state_scalar_function_value(A) ::= UUID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UUID_FUNCTION, R);
}

arithmetic_update_source_column(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
arithmetic_update_source_column(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

update_date_interval_interval(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
update_date_interval_interval(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_date_interval_interval(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_date_interval_interval(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
update_date_interval_interval(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
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
    window_clause_opt(WN) select_locking_clause_opt(K). {
    A = mylite_sql_parser_attach_select_window_clause(
        state,
        mylite_sql_parser_make_select_statement_with_modifiers(
            state, T, M, B, NULL, NULL, NULL, NULL, NULL, NULL, K),
        WN);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) INTO select_into_list(PI)
    window_clause_opt(WN) select_locking_clause_opt(K). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, NULL, NULL, NULL, NULL, NULL, NULL, K),
            WN),
        PI);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) DUAL(D)
    where_clause_opt(W) window_clause_opt(WN) select_locking_clause_opt(K) select_into_opt(AI). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D), W, NULL, NULL,
                NULL, NULL, K),
            WN),
        AI);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) INTO select_into_list(PI)
    FROM(F) DUAL(D) where_clause_opt(W) window_clause_opt(WN) select_locking_clause_opt(K). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D), W, NULL, NULL,
                NULL, NULL, K),
            WN),
        PI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) table_name(N) table_alias_opt(AL)
    table_index_hints_opt(IH) where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    window_clause_opt(WN) select_order_clause_opt(O) limit_clause_opt(L)
    select_locking_clause_opt(K) select_into_opt(AI). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL, IH), W, G, H,
                O, L, K),
            WN),
        AI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) table_name(N)
    table_partition_selection(P) table_alias_opt(AL) table_index_hints_opt(IH)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) window_clause_opt(WN)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K)
    select_into_opt(AI). {
    (void)P;
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL, IH), W, G, H,
                O, L, K),
            WN),
        AI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) INTO select_into_list(PI) FROM(F)
    table_name(N) table_alias_opt(AL) table_index_hints_opt(IH) where_clause_opt(W)
    group_clause_opt(G) having_clause_opt(H) window_clause_opt(WN) select_order_clause_opt(O)
    limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL, IH), W, G, H,
                O, L, K),
            WN),
        PI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) INTO select_into_list(PI) FROM(F)
    table_name(N) table_partition_selection(P) table_alias_opt(AL) table_index_hints_opt(IH)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) window_clause_opt(WN)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    (void)P;
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL, IH), W, G, H,
                O, L, K),
            WN),
        PI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM derived_table_source(D)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) window_clause_opt(WN)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K)
    select_into_opt(AI). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, D, W, G, H, O, L, K),
            WN),
        AI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) INTO select_into_list(PI)
    FROM derived_table_source(D) where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    window_clause_opt(WN) select_order_clause_opt(O) limit_clause_opt(L)
    select_locking_clause_opt(K). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, D, W, G, H, O, L, K),
            WN),
        PI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM joined_table_source(JT)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) window_clause_opt(WN)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K)
    select_into_opt(AI). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, JT, W, G, H, O, L, K),
            WN),
        AI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) INTO select_into_list(PI)
    FROM joined_table_source(JT) where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    window_clause_opt(WN) select_order_clause_opt(O) limit_clause_opt(L)
    select_locking_clause_opt(K). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, JT, W, G, H, O, L, K),
            WN),
        PI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) FROM comma_table_sources(CT)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H) window_clause_opt(WN)
    select_order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K)
    select_into_opt(AI). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, CT, W, G, H, O, L, K),
            WN),
        AI);
}
select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B) INTO select_into_list(PI)
    FROM comma_table_sources(CT) where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    window_clause_opt(WN) select_order_clause_opt(O) limit_clause_opt(L)
    select_locking_clause_opt(K). {
    A = mylite_sql_parser_attach_select_into_clause(
        state,
        mylite_sql_parser_attach_select_window_clause(
            state,
            mylite_sql_parser_make_select_statement_with_modifiers(
                state, T, M, B, CT, W, G, H, O, L, K),
            WN),
        PI);
}
query_expression_body(A) ::= select_statement(S). {
    A = S;
}
query_expression_body(A) ::= table_statement(S). {
    A = S;
}
query_expression_body(A) ::= values_statement(S). {
    A = S;
}
query_expression_body(A) ::= parenthesized_query_expression(S). {
    A = S;
}

compound_select_statement(A) ::= select_statement(S) union_term_list(T). {
    A = mylite_sql_parser_make_compound_select_statement(state, S, T);
}

query_compound_statement(A) ::= table_statement(S) union_term_list(T). {
    A = mylite_sql_parser_make_compound_select_statement(state, S, T);
}
query_compound_statement(A) ::= values_statement(S) union_term_list(T). {
    A = mylite_sql_parser_make_compound_select_statement(state, S, T);
}
query_compound_statement(A) ::= parenthesized_query_expression(S) union_term_list(T). {
    A = mylite_sql_parser_make_compound_select_statement(state, S, T);
}

view_query_expression(A) ::= select_statement(S). {
    A = S;
}
view_query_expression(A) ::= compound_select_statement(S). {
    A = S;
}
view_query_expression(A) ::= query_compound_statement(S). {
    A = S;
}
view_query_expression(A) ::= parenthesized_query_expression(S). {
    A = S;
}
view_query_expression(A) ::= table_statement(S). {
    A = S;
}
view_query_expression(A) ::= values_statement(S). {
    A = S;
}

parenthesized_query_expression(A) ::=
    LPAREN(L) query_expression_body(S) RPAREN(R)
    query_expression_order_clause_opt(O) query_expression_limit_clause_opt(LM). {
    A = mylite_sql_parser_make_parenthesized_query_expression(state, L, S, R, O, LM);
}
parenthesized_query_expression(A) ::=
    LPAREN(L) compound_select_statement(S) RPAREN(R)
    query_expression_order_clause_opt(O) query_expression_limit_clause_opt(LM). {
    A = mylite_sql_parser_make_parenthesized_query_expression(state, L, S, R, O, LM);
}
parenthesized_query_expression(A) ::=
    LPAREN(L) query_compound_statement(S) RPAREN(R)
    query_expression_order_clause_opt(O) query_expression_limit_clause_opt(LM). {
    A = mylite_sql_parser_make_parenthesized_query_expression(state, L, S, R, O, LM);
}

with_select_statement(A) ::=
    WITH(W) common_table_expression_list with_union_select with_select_order_clause(O). {
    A = mylite_sql_parser_make_with_select_statement(state, W, NULL, O);
}

common_table_expression_list(A) ::= common_table_expression(E). {
    A = E;
}
common_table_expression_list(A) ::= common_table_expression_list(L) COMMA
    common_table_expression(E). {
    (void)E;
    A = L;
}

common_table_expression(A) ::= identifier AS LPAREN with_columns_cte_select(S) RPAREN. {
    A = S;
}
common_table_expression(A) ::= identifier AS LPAREN with_indexes_cte_select(S) RPAREN. {
    A = S;
}

with_columns_cte_select(A) ::= SELECT(S) identifier AS identifier FROM table_name WHERE
    with_table_filter. {
    A = mylite_sql_parser_make_select_statement(
        state, S, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

with_indexes_cte_select(A) ::= SELECT(S) DISTINCT identifier AS identifier FROM table_name WHERE
    with_table_filter. {
    A = mylite_sql_parser_make_select_statement(
        state, S, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

with_table_filter(A) ::= identifier EQUAL STRING AND identifier EQUAL STRING. {
    A = NULL;
}

with_union_select(A) ::=
    SELECT CONCAT LPAREN identifier COMMA STRING RPAREN AS identifier FROM identifier UNION ALL
    SELECT CONCAT LPAREN identifier COMMA STRING RPAREN AS identifier FROM identifier(I). {
    A = I;
}

with_select_order_clause(A) ::= ORDER BY identifier(I). {
    A = I;
}

union_term_list(A) ::= union_term(T). {
    A = mylite_sql_parser_make_union_term_list(state, T);
}
union_term_list(A) ::= union_term_list(L) union_term(T). {
    A = mylite_sql_parser_append_union_term(state, L, T);
}

union_term(A) ::= UNION(U) union_modifier_opt(M) query_expression_body(S). {
    A = mylite_sql_parser_make_union_term(state, U, M, S);
}
union_term(A) ::= INTERSECT(I) union_modifier_opt(M) query_expression_body(S). {
    A = mylite_sql_parser_make_set_operation_term(
        state, I, MYLITE_SQL_AST_SET_OPERATOR_INTERSECT, M, S);
}
union_term(A) ::= EXCEPT(E) union_modifier_opt(M) query_expression_body(S). {
    A = mylite_sql_parser_make_set_operation_term(
        state, E, MYLITE_SQL_AST_SET_OPERATOR_EXCEPT, M, S);
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
table_source(A) ::=
    table_name(N) table_partition_selection(P) table_alias_opt(AL) table_index_hints_opt(IH). {
    (void)P;
    A = mylite_sql_parser_make_table_source(state, N, AL, IH);
}
table_source(A) ::= derived_table_source(D). {
    A = D;
}

derived_table_source(A) ::= LPAREN(L) select_statement(S) RPAREN(R) derived_table_alias_opt(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}
derived_table_source(A) ::=
    LPAREN(L) compound_select_statement(S) RPAREN(R) derived_table_alias_opt(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}
derived_table_source(A) ::=
    LPAREN(L) query_compound_statement(S) RPAREN(R) derived_table_alias_opt(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}
derived_table_source(A) ::= LPAREN(L) table_statement(S) RPAREN(R) derived_table_alias_opt(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}
derived_table_source(A) ::= LPAREN(L) values_statement(S) RPAREN(R) derived_table_alias_opt(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}
derived_table_source(A) ::=
    LPAREN(L) parenthesized_query_expression(S) RPAREN(R) derived_table_alias_opt(AL). {
    A = mylite_sql_parser_make_derived_table_source(state, L, S, R, AL);
}

joined_table_source(A) ::=
    table_source(LT) inner_join_operator(JO) table_source(RT) join_condition_opt(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}
joined_table_source(A) ::=
    joined_table_source(LT) inner_join_operator(JO) table_source(RT) join_condition_opt(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}
joined_table_source(A) ::=
    table_source(LT) outer_join_operator(JO) table_source(RT) ON join_condition(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}
joined_table_source(A) ::=
    joined_table_source(LT) outer_join_operator(JO) table_source(RT) ON join_condition(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}
joined_table_source(A) ::=
    table_source(LT) outer_join_operator(JO) table_source(RT) join_using_clause(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}
joined_table_source(A) ::=
    joined_table_source(LT) outer_join_operator(JO) table_source(RT) join_using_clause(J). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, J);
}
joined_table_source(A) ::=
    table_source(LT) natural_join_operator(JO) table_source(RT). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, NULL);
}
joined_table_source(A) ::=
    joined_table_source(LT) natural_join_operator(JO) table_source(RT). {
    A = mylite_sql_parser_make_join_source(state, LT, JO, RT, NULL);
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

values_statement(A) ::= VALUES(V) values_row_constructor_list(R) values_order_clause_opt(O)
    limit_clause_opt(L). {
    A = mylite_sql_parser_make_values_statement(state, V, R, O, L);
}

values_row_constructor_list(A) ::= values_row_constructor(R). {
    A = mylite_sql_parser_make_values_row_list(state, R);
}
values_row_constructor_list(A) ::= values_row_constructor_list(L) COMMA values_row_constructor(R). {
    A = mylite_sql_parser_append_values_row(state, L, R);
}

values_row_constructor(A) ::= ROW(T) LPAREN values_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_values_row(state, T, V, R);
}
values_row_constructor(A) ::= ROW(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_values_row(
        state,
        T,
        mylite_sql_parser_make_values_row_values(state, NULL),
        R
    );
}

values_value_list(A) ::= values_value(V). {
    A = mylite_sql_parser_make_values_row_values(state, V);
}
values_value_list(A) ::= values_value_list(L) COMMA values_value(V). {
    A = mylite_sql_parser_append_values_value(state, L, V);
}

values_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
values_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
values_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
values_value(A) ::= string_text_literal(V). {
    A = V;
}
values_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
values_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
values_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
values_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_dml_default_value(state, T);
}

values_order_clause_opt(A) ::= . {
    A = NULL;
}
values_order_clause_opt(A) ::=
    ORDER(O) BY values_order_key(K) order_direction_opt(D) values_order_tail_opt(T). {
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

values_order_tail_opt(A) ::= . {
    A = NULL;
}
values_order_tail_opt(A) ::= COMMA values_order_item_list(L). {
    A = L;
}

values_order_item_list(A) ::= values_order_item(I). {
    A = mylite_sql_parser_make_order_by_item_list(state, I);
}
values_order_item_list(A) ::= values_order_item_list(L) COMMA values_order_item(I). {
    A = mylite_sql_parser_append_order_by_item(state, L, I);
}

values_order_item(A) ::= values_order_key(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_item(state, K, D);
}

values_order_key(A) ::= qualified_identifier(K). {
    A = K;
}
values_order_key(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
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
join_operator(A) ::= STRAIGHT_JOIN. {
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
join_operator(A) ::= natural_join_operator(JO). {
    A = JO;
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
inner_join_operator(A) ::= STRAIGHT_JOIN. {
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

natural_join_operator(A) ::= NATURAL JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_NATURAL_INNER;
}
natural_join_operator(A) ::= NATURAL INNER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_NATURAL_INNER;
}
natural_join_operator(A) ::= NATURAL LEFT JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_NATURAL_LEFT_OUTER;
}
natural_join_operator(A) ::= NATURAL LEFT OUTER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_NATURAL_LEFT_OUTER;
}
natural_join_operator(A) ::= NATURAL RIGHT JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_NATURAL_RIGHT_OUTER;
}
natural_join_operator(A) ::= NATURAL RIGHT OUTER JOIN. {
    A = MYLITE_SQL_AST_JOIN_KIND_NATURAL_RIGHT_OUTER;
}

join_condition_opt(A) ::= . [ON] {
    A = NULL;
}
join_condition_opt(A) ::= ON join_condition(C). {
    A = C;
}
join_condition_opt(A) ::= join_using_clause(C). {
    A = C;
}

join_using_clause(A) ::= USING(U) LPAREN identifier_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_join_using_clause(state, U, L, R);
}

join_condition(A) ::= predicate(P). {
    A = P;
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
select_locking_clause_opt(A) ::= FOR(F) UPDATE(U) select_lock_wait_opt(W). {
    struct mylite_sql_token end_token = W.text != NULL ? W : U;
    A = (struct mylite_sql_select_locking_clause){
        .kind = MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        .span = {
            .text = F.text,
            .length = (end_token.offset + end_token.length) - F.offset,
            .offset = F.offset,
            .line = F.line,
            .column = F.column,
        },
    };
}
select_locking_clause_opt(A) ::= FOR(F) SHARE(S) select_lock_wait_opt(W). {
    struct mylite_sql_token end_token = W.text != NULL ? W : S;
    A = (struct mylite_sql_select_locking_clause){
        .kind = MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        .span = {
            .text = F.text,
            .length = (end_token.offset + end_token.length) - F.offset,
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

select_lock_wait_opt(A) ::= . {
    A = (struct mylite_sql_token){0};
}
select_lock_wait_opt(A) ::= NOWAIT(N). {
    A = N;
}
select_lock_wait_opt(A) ::= SKIP LOCKED(L). {
    A = L;
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

derived_table_alias_opt(A) ::= table_alias_opt(B). {
    A = B;
}
derived_table_alias_opt(A) ::= AS identifier(B) LPAREN identifier_list RPAREN. {
    A = B;
}
derived_table_alias_opt(A) ::= identifier(B) LPAREN identifier_list RPAREN. {
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
group_clause_opt(A) ::= GROUP(G) BY group_key_list(K) group_rollup_opt(R). {
    A = mylite_sql_parser_make_group_by_clause(
        state,
        G,
        R == NULL ? K : mylite_sql_parser_append_group_by_key(state, K, R));
}

group_rollup_opt(A) ::= . [WITH] {
    A = NULL;
}
group_rollup_opt(A) ::= WITH(W) ROLLUP(R). {
    A = mylite_sql_parser_make_group_by_rollup_modifier(state, W, R);
}
group_key_list(A) ::= group_key(K). {
    A = mylite_sql_parser_make_group_by_key_list(state, K);
}
group_key_list(A) ::= group_key_list(L) COMMA group_key(K). {
    A = mylite_sql_parser_append_group_by_key(state, L, K);
}

group_key(A) ::= qualified_identifier(K). {
    A = K;
}
group_key(A) ::= LPAREN(L) group_key(K) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, K, R);
}
group_key(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
group_key(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
group_key(A) ::= qualified_identifier(V) COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_collate_expression(state, V, C, N);
}
group_key(A) ::= BINARY(T) qualified_identifier(V). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(state, T, V);
}
group_key(A) ::= WEEK(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEEK_FUNCTION, B, R);
}
group_key(A) ::= WEEK(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_WEEK_FUNCTION, B, C, R);
}
group_key(A) ::= YEAR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_YEAR_FUNCTION, B, R);
}
group_key(A) ::= MONTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MONTH_FUNCTION, B, R);
}
group_key(A) ::= DAYOFMONTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYOFMONTH_FUNCTION, B, R);
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
having_predicate_atom(A) ::= having_operand(C). {
    A = C;
}
having_predicate_atom(A) ::= having_operand(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
having_predicate_atom(A) ::= having_operand(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
having_predicate_atom(A) ::=
    qualified_identifier(C) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
            .right = P,
            .escape = E,
        });
}
having_predicate_atom(A) ::=
    qualified_identifier(C) NOT(N) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_like_comparison_predicate(
            state,
            &(const struct mylite_sql_parser_like_comparison_predicate_request){
                .left = C,
                .operator_token = O,
                .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
                .right = P,
                .escape = E,
            }));
}

having_operand(A) ::= qualified_identifier(B). {
    A = B;
}
having_operand(A) ::= qualified_identifier(B) PLUS(T) having_integer_value(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
having_operand(A) ::= qualified_identifier(B) MINUS(T) having_integer_value(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
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
selected_grouped_aggregate_expression(A) ::= SUM(T) LPAREN(L) sum_aggregate_argument(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, B, R);
}
selected_grouped_aggregate_expression(A) ::= AVG(T) LPAREN qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, B, R);
}

sum_aggregate_argument(A) ::= qualified_identifier(B). {
    A = B;
}
sum_aggregate_argument(A) ::= string_length_expression(B). {
    A = B;
}
sum_aggregate_argument(A) ::= aggregate_literal(B). {
    A = B;
}
sum_aggregate_argument(A) ::= cast_convert_expression(B). {
    A = B;
}
sum_aggregate_argument(A) ::= aggregate_nested_function(B). {
    A = B;
}
sum_aggregate_argument(A) ::= qualified_identifier(B) PLUS(T) qualified_identifier(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
sum_aggregate_argument(A) ::= qualified_identifier(B) PLUS(T) qualified_identifier(C)
                              PLUS(U) qualified_identifier(D). {
    A = mylite_sql_parser_make_binary_expression(
        state,
        mylite_sql_parser_make_binary_expression(
            state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C),
        U,
        MYLITE_SQL_AST_OPERATOR_ADD,
        D);
}
sum_aggregate_argument(A) ::= qualified_identifier(B) PLUS(T) aggregate_literal(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
sum_aggregate_argument(A) ::= qualified_identifier(B) MINUS(T) aggregate_literal(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
sum_aggregate_argument(A) ::= qualified_identifier(B) STAR(T) qualified_identifier(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
sum_aggregate_argument(A) ::= qualified_identifier(B) SLASH(T) qualified_identifier(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_DIVIDE, C);
}

aggregate_nested_function(A) ::= MIN(T) LPAREN qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, B, R);
}
aggregate_nested_function(A) ::= MAX(T) LPAREN qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, B, R);
}
aggregate_nested_function(A) ::= SUM(T) LPAREN qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, B, R);
}
aggregate_nested_function(A) ::= AVG(T) LPAREN qualified_identifier(B) RPAREN(R). {
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
predicate_atom(A) ::= EXISTS(E) LPAREN values_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_exists_predicate(state, E, S, R);
}
predicate_atom(A) ::= LPAREN(L) select_statement(S) RPAREN(R) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state,
        mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R),
        I,
        MYLITE_SQL_AST_OPERATOR_IS_NULL,
        N);
}
predicate_atom(A) ::= LPAREN(L) select_statement(S) RPAREN(R) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state,
        mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R),
        I,
        MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
        N);
}
predicate_atom(A) ::= LPAREN(L) select_statement(S) RPAREN(R)
        predicate_comparison_operator(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state,
        mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R),
        O.token,
        O.operator_kind,
        V);
}
predicate_atom(A) ::= LPAREN(L) select_statement(S) RPAREN(R) BETWEEN(B)
        predicate_range_value(V) AND predicate_range_value(E). {
    A = mylite_sql_parser_make_between_predicate(
        state,
        mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R),
        B,
        V,
        E);
}
predicate_atom(A) ::= LPAREN(L) select_statement(S) RPAREN(R) NOT(N) BETWEEN(B)
        predicate_range_value(V) AND predicate_range_value(E). {
    A = mylite_sql_parser_make_not_predicate(
        state,
        N,
        mylite_sql_parser_make_between_predicate(
            state,
            mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R),
            B,
            V,
            E));
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
predicate_atom(A) ::= match_against_expression(C). {
    A = C;
}
predicate_atom(A) ::= match_against_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= string_length_expression(C). {
    A = C;
}
predicate_atom(A) ::= string_length_expression(C) predicate_comparison_operator(O)
        string_length_expression(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= string_length_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= string_length_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= string_length_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= rand_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= substring_expression(C) predicate_comparison_operator(O)
        substring_expression(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= substring_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= substring_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= substring_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= date_format_numeric_predicate_expression(C). {
    A = C;
}
predicate_atom(A) ::= temporal_extract_predicate_expression(C). {
    A = C;
}
predicate_atom(A) ::= temporal_extract_predicate_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= temporal_extract_arithmetic_predicate_expression(C)
        predicate_comparison_operator(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= temporal_extract_predicate_expression(C) BETWEEN(B)
        predicate_range_value(L) AND predicate_range_value(U). {
    A = mylite_sql_parser_make_between_predicate(state, C, B, L, U);
}
predicate_atom(A) ::= temporal_extract_predicate_expression(C) NOT(N) BETWEEN(B)
        predicate_range_value(L) AND predicate_range_value(U). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, C, B, L, U));
}
predicate_atom(A) ::= temporal_extract_predicate_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= temporal_extract_predicate_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= cast_convert_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= predicate_collate_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= predicate_row_scalar_expression(C) predicate_comparison_operator(O)
        predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= predicate_row_scalar_expression(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= predicate_row_scalar_expression(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}
predicate_atom(A) ::= predicate_row_scalar_expression(C) BETWEEN(B)
        predicate_range_value(L) AND predicate_range_value(U). {
    A = mylite_sql_parser_make_between_predicate(state, C, B, L, U);
}
predicate_atom(A) ::= predicate_row_scalar_expression(C) NOT(N) BETWEEN(B)
        predicate_range_value(L) AND predicate_range_value(U). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, C, B, L, U));
}
predicate_atom(A) ::= predicate_row_scalar_expression(C) IN(I) LPAREN predicate_in_value_list(V)
        RPAREN(R). {
    A = mylite_sql_parser_make_in_predicate(state, C, I, V, R);
}
predicate_atom(A) ::= predicate_row_scalar_expression(C) NOT(N) IN(I) LPAREN
        predicate_in_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_in_predicate(state, C, I, V, R));
}
predicate_atom(A) ::=
    cast_convert_expression(C) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
            .right = P,
            .escape = E,
        });
}
predicate_atom(A) ::=
    cast_convert_expression(C) NOT(N) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_like_comparison_predicate(
            state,
            &(const struct mylite_sql_parser_like_comparison_predicate_request){
                .left = C,
                .operator_token = O,
                .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
                .right = P,
                .escape = E,
            }));
}
predicate_atom(A) ::=
    predicate_collate_expression(C) LIKE(O) predicate_like_pattern(P)
    predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
            .right = P,
            .escape = E,
        });
}
predicate_atom(A) ::=
    predicate_collate_expression(C) NOT(N) LIKE(O) predicate_like_pattern(P)
    predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_like_comparison_predicate(
            state,
            &(const struct mylite_sql_parser_like_comparison_predicate_request){
                .left = C,
                .operator_token = O,
                .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
                .right = P,
                .escape = E,
            }));
}
predicate_atom(A) ::= cast_convert_expression(C) BETWEEN(B)
        predicate_range_value(L) AND predicate_range_value(U). {
    A = mylite_sql_parser_make_between_predicate(state, C, B, L, U);
}
predicate_atom(A) ::= cast_convert_expression(C) NOT(N) BETWEEN(B)
        predicate_range_value(L) AND predicate_range_value(U). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, C, B, L, U));
}
predicate_atom(A) ::= cast_convert_expression(C) REGEXP(O) STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
predicate_atom(A) ::= cast_convert_expression(C) RLIKE(O) STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
predicate_atom(A) ::= cast_convert_expression(C) REGEXP(O) BINARY STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
predicate_atom(A) ::= cast_convert_expression(C) RLIKE(O) BINARY STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
predicate_atom(A) ::= cast_convert_expression(C) NOT(N) REGEXP(O) STRING(T). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP,
            mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING)));
}
predicate_atom(A) ::= cast_convert_expression(C) NOT(N) RLIKE(O) STRING(T). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE,
            mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING)));
}
predicate_atom(A) ::= cast_convert_expression(C) NOT(N) REGEXP(O) BINARY STRING(T). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP,
            mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING)));
}
predicate_atom(A) ::= cast_convert_expression(C) NOT(N) RLIKE(O) BINARY STRING(T). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE,
            mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING)));
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
predicate_atom(A) ::= qualified_identifier(C) predicate_comparison_operator(O)
        introduced_predicate_literal(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::= qualified_identifier(C) predicate_comparison_operator(O)
        binary_predicate_literal(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O.token, O.operator_kind, V);
}
predicate_atom(A) ::=
    qualified_identifier(C) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
            .right = P,
            .escape = E,
        });
}
predicate_atom(A) ::=
    qualified_identifier(C) LIKE(O) BINARY predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE_BINARY,
            .right = P,
            .escape = E,
        });
}
predicate_atom(A) ::=
    qualified_identifier(C) NOT(N) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_like_comparison_predicate(
            state,
            &(const struct mylite_sql_parser_like_comparison_predicate_request){
                .left = C,
                .operator_token = O,
                .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
                .right = P,
                .escape = E,
            }));
}
predicate_atom(A) ::=
    qualified_identifier(C) NOT(N) LIKE(O) BINARY predicate_like_pattern(P)
    predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_like_comparison_predicate(
            state,
            &(const struct mylite_sql_parser_like_comparison_predicate_request){
                .left = C,
                .operator_token = O,
                .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE_BINARY,
                .right = P,
                .escape = E,
            }));
}
predicate_atom(A) ::= qualified_identifier(C) LIKE(O) introduced_predicate_literal(P). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
            .right = P,
            .escape = NULL,
        });
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) LIKE(O) introduced_predicate_literal(P). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_like_comparison_predicate(
            state,
            &(const struct mylite_sql_parser_like_comparison_predicate_request){
                .left = C,
                .operator_token = O,
                .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
                .right = P,
                .escape = NULL,
            }));
}

introduced_predicate_literal(A) ::= charset_introducer STRING(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
introduced_predicate_literal(A) ::= charset_introducer STRING(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
introduced_predicate_literal(A) ::= charset_introducer HEX_LITERAL(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
introduced_predicate_literal(A) ::= charset_introducer HEX_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
introduced_predicate_literal(A) ::= charset_introducer BIT_LITERAL(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
introduced_predicate_literal(A) ::= charset_introducer BIT_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
binary_predicate_literal(A) ::= BINARY(T) STRING(V). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}
binary_predicate_literal(A) ::= BINARY(T) HEX_LITERAL(V). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_HEX));
}
binary_predicate_literal(A) ::= BINARY(T) BIT_LITERAL(V). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_BIT));
}
binary_predicate_literal(A) ::= BINARY(T) introduced_predicate_literal(V). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(state, T, V);
}
predicate_like_pattern(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
predicate_like_pattern(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
predicate_like_pattern(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
predicate_like_pattern(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
predicate_like_pattern(A) ::= predicate_collate_expression(V). {
    A = V;
}
predicate_like_pattern(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
}
predicate_like_pattern(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
predicate_like_pattern(A) ::= user_variable(T). {
    A = T;
}
predicate_like_escape_opt(A) ::= . {
    A = NULL;
}
predicate_like_escape_opt(A) ::= ESCAPE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
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
predicate_atom(A) ::= qualified_identifier(C) BETWEEN(B) introduced_predicate_literal(L) AND
        introduced_predicate_literal(U). {
    A = mylite_sql_parser_make_between_predicate(state, C, B, L, U);
}
predicate_atom(A) ::= qualified_identifier(C) BETWEEN(B) binary_predicate_literal(L) AND
        binary_predicate_literal(U). {
    A = mylite_sql_parser_make_between_predicate(state, C, B, L, U);
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) BETWEEN(B) predicate_range_value(L) AND
        predicate_range_value(U). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, C, B, L, U));
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) BETWEEN(B) introduced_predicate_literal(L)
        AND introduced_predicate_literal(U). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, C, B, L, U));
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) BETWEEN(B) binary_predicate_literal(L)
        AND binary_predicate_literal(U). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, C, B, L, U));
}
predicate_atom(A) ::= qualified_identifier(C) IN(I) LPAREN predicate_in_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_in_predicate(state, C, I, V, R);
}
predicate_atom(A) ::= qualified_identifier(C) IN(I) LPAREN introduced_predicate_literal_list(V)
        RPAREN(R). {
    A = mylite_sql_parser_make_in_predicate(state, C, I, V, R);
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) IN(I) LPAREN predicate_in_value_list(V)
        RPAREN(R). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_in_predicate(state, C, I, V, R));
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) IN(I) LPAREN
        introduced_predicate_literal_list(V) RPAREN(R). {
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
predicate_atom(A) ::= qualified_identifier(C) IN(I) LPAREN values_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_in_predicate(state, C, I, S, R);
}
predicate_atom(A) ::= qualified_identifier(C) NOT(N) IN(I) LPAREN values_statement(S)
        RPAREN(R). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_in_predicate(state, C, I, S, R));
}
predicate_atom(A) ::= LPAREN(L) select_statement(S) RPAREN(R) IN(I) LPAREN
        predicate_in_value_list(V) RPAREN(E). {
    A = mylite_sql_parser_make_in_predicate(
        state, mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R), I, V, E);
}
predicate_atom(A) ::= LPAREN(L) select_statement(S) RPAREN(R) NOT(N) IN(I) LPAREN
        predicate_in_value_list(V) RPAREN(E). {
    A = mylite_sql_parser_make_not_predicate(
        state,
        N,
        mylite_sql_parser_make_in_predicate(
            state,
            mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R),
            I,
            V,
            E));
}

predicate_in_value_list(A) ::= predicate_in_value(V). {
    A = mylite_sql_parser_make_predicate_value_list(state, V);
}
predicate_in_value_list(A) ::= predicate_in_value_list(L) COMMA predicate_in_value(V). {
    A = mylite_sql_parser_append_predicate_value(state, L, V);
}

introduced_predicate_literal_list(A) ::= introduced_predicate_literal(V). {
    A = mylite_sql_parser_make_predicate_value_list(state, V);
}
introduced_predicate_literal_list(A) ::= introduced_predicate_literal_list(L) COMMA
        introduced_predicate_literal(V). {
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
predicate_in_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
predicate_in_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
predicate_in_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
predicate_in_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
predicate_in_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
predicate_in_value(A) ::= user_variable(T). {
    A = T;
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
predicate_range_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
predicate_range_value(A) ::= LPAREN(L) select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R);
}
predicate_range_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
predicate_range_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
predicate_range_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
predicate_range_value(A) ::= user_variable(T). {
    A = T;
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
predicate_comparison_value(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
predicate_comparison_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
predicate_comparison_value(A) ::= predicate_collate_expression(V). {
    A = V;
}
predicate_comparison_value(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
}
predicate_comparison_value(A) ::= UNIX_TIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION, R);
}
predicate_comparison_value(A) ::= current_timestamp_value(T). {
    A = T;
}
predicate_comparison_value(A) ::= DATABASE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_DATABASE_FUNCTION, R);
}
predicate_comparison_value(A) ::= SCHEMA(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_SCHEMA_FUNCTION, R);
}
predicate_comparison_value(A) ::= LPAREN(L) select_statement(S) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, S, R);
}
predicate_comparison_value(A) ::= row_scalar_string_predicate_expression(V). {
    A = V;
}
predicate_comparison_value(A) ::= row_scalar_json_predicate_expression(V). {
    A = V;
}
predicate_comparison_value(A) ::= row_scalar_temporal_predicate_expression(V). {
    A = V;
}
predicate_comparison_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
predicate_comparison_value(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
predicate_comparison_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
predicate_comparison_value(A) ::= user_variable(T). {
    A = T;
}

predicate_row_scalar_expression(A) ::= IF(T) LPAREN expression(B) COMMA expression(C)
        COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_IF_FUNCTION, B, C, D, R);
}
predicate_row_scalar_expression(A) ::= IFNULL(T) LPAREN expression(B) COMMA expression(C)
        RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_IFNULL_FUNCTION, B, C, R);
}
predicate_row_scalar_expression(A) ::= NULLIF(T) LPAREN expression(B) COMMA expression(C)
        RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_FUNCTION, B, C, R);
}
predicate_row_scalar_expression(A) ::= ISNULL(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ISNULL_FUNCTION, B, R);
}
predicate_row_scalar_expression(A) ::= COALESCE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_COALESCE_FUNCTION, B, R);
}
predicate_row_scalar_expression(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
}
predicate_row_scalar_expression(A) ::= CONCAT_WS(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, B, R);
}
predicate_row_scalar_expression(A) ::= FIELD(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_FIELD_FUNCTION, B, R);
}
predicate_row_scalar_expression(A) ::= GREATEST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_GREATEST_FUNCTION, B, R);
}
predicate_row_scalar_expression(A) ::= LEAST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_LEAST_FUNCTION, B, R);
}
predicate_row_scalar_expression(A) ::= row_scalar_string_predicate_expression(B). {
    A = B;
}
predicate_row_scalar_expression(A) ::= row_scalar_json_predicate_expression(B). {
    A = B;
}
predicate_row_scalar_expression(A) ::= row_scalar_temporal_predicate_expression(B). {
    A = B;
}

row_scalar_string_predicate_expression(A) ::= HEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_HEX_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= UNHEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNHEX_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= LOWER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOWER_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= LCASE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LCASE_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= UPPER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UPPER_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= UCASE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UCASE_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= LTRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LTRIM_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= RTRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RTRIM_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= TRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(
        state, T, MYLITE_SQL_AST_TRIM_FUNCTION, NULL, B, R);
}
row_scalar_string_predicate_expression(A) ::= TRIM(T) LPAREN expression(B) FROM expression(C)
        RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(
        state, T, MYLITE_SQL_AST_TRIM_FUNCTION, B, C, R);
}
row_scalar_string_predicate_expression(A) ::= TRIM(T) LPAREN trim_direction(D) expression(B)
        FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(state, T, D, B, C, R);
}
row_scalar_string_predicate_expression(A) ::= TRIM(T) LPAREN trim_direction(D) FROM expression(C)
        RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(state, T, D, NULL, C, R);
}
row_scalar_string_predicate_expression(A) ::= REVERSE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_REVERSE_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= SOUNDEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SOUNDEX_FUNCTION, B, R);
}
row_scalar_string_predicate_expression(A) ::= QUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_QUOTE_FUNCTION, B, R);
}

row_scalar_json_predicate_expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) COMMA
        expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION, B, C, R);
}
row_scalar_json_predicate_expression(A) ::= JSON_UNQUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION, B, R);
}
row_scalar_json_predicate_expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_FUNCTION, B, R);
}
row_scalar_json_predicate_expression(A) ::= JSON_LENGTH(T) LPAREN expression(B) COMMA
        expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_LENGTH_FUNCTION, B, C, R);
}
row_scalar_json_predicate_expression(A) ::= JSON_TYPE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_TYPE_FUNCTION, B, R);
}
row_scalar_json_predicate_expression(A) ::= JSON_QUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_FUNCTION, B, R);
}

row_scalar_temporal_predicate_expression(A) ::= DATEDIFF(T) LPAREN expression(B) COMMA
        expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_DATEDIFF_FUNCTION, B, C, R);
}
row_scalar_temporal_predicate_expression(A) ::= TIMEDIFF(T) LPAREN expression(B) COMMA
        expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TIMEDIFF_FUNCTION, B, C, R);
}
row_scalar_temporal_predicate_expression(A) ::= TIMESTAMPDIFF(T) LPAREN timestampdiff_unit(U)
        COMMA expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION, U, B, C, R);
}
row_scalar_temporal_predicate_expression(A) ::= TIMESTAMP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMP_FUNCTION, B, R);
}
row_scalar_temporal_predicate_expression(A) ::= TIMESTAMP(T) LPAREN expression(B) COMMA
        expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMP_FUNCTION, B, C, R);
}
row_scalar_temporal_predicate_expression(A) ::= UNIX_TIMESTAMP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION, B, R);
}
row_scalar_temporal_predicate_expression(A) ::= SEC_TO_TIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION, B, R);
}
row_scalar_temporal_predicate_expression(A) ::= FROM_UNIXTIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION, B, R);
}
row_scalar_temporal_predicate_expression(A) ::= MAKEDATE(T) LPAREN expression(B) COMMA
        expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_MAKEDATE_FUNCTION, B, C, R);
}
row_scalar_temporal_predicate_expression(A) ::= MAKETIME(T) LPAREN expression(B) COMMA
        expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_MAKETIME_FUNCTION, B, C, D, R);
}

predicate_collate_expression(A) ::= predicate_collatable_expression(V) COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_collate_expression(state, V, C, N);
}

predicate_collatable_expression(A) ::= qualified_identifier(V). {
    A = V;
}
predicate_collatable_expression(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
predicate_collatable_expression(A) ::= cast_convert_expression(V). {
    A = V;
}
predicate_collatable_expression(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
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
predicate_scalar_literal(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
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
predicate_scalar_literal(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
predicate_scalar_literal(A) ::= user_variable(T). {
    A = T;
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
order_clause_opt(A) ::= ORDER(O) BY expression(K) order_direction_opt(D). {
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

query_expression_order_clause_opt(A) ::= . {
    A = NULL;
}
query_expression_order_clause_opt(A) ::= query_expression_order_clause(O). {
    A = O;
}
query_expression_order_clause(A) ::=
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
select_order_key(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
select_order_key(A) ::= qualified_identifier(B) PLUS(T) INTEGER(C). {
    A = mylite_sql_parser_make_binary_expression(
        state,
        B,
        T,
        MYLITE_SQL_AST_OPERATOR_ADD,
        mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_INTEGER)
    );
}
select_order_key(A) ::= selected_grouped_aggregate_expression(K). {
    A = K;
}
select_order_key(A) ::= cast_convert_expression(K). {
    A = K;
}
select_order_key(A) ::= BINARY(T) qualified_identifier(K). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(state, T, K);
}
select_order_key(A) ::= BINARY(T) STRING(V). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(
        state, T, mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}
select_order_key(A) ::= predicate_collate_expression(K). {
    A = K;
}
select_order_key(A) ::= window_function_expression(K). {
    A = K;
}
select_order_key(A) ::=
    qualified_identifier(C) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
            .right = P,
            .escape = E,
        });
}
select_order_key(A) ::= FIELD(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_FIELD_FUNCTION, B, R);
}
select_order_key(A) ::= FIELD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR, NULL, R);
}
select_order_key(A) ::= RAND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_RAND_FUNCTION, R);
}
select_order_key(A) ::= RAND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RAND_SEED_FUNCTION, B, R);
}
select_order_key(A) ::= RAND(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR, C, R);
}
select_order_key(A) ::= CASE(T) searched_case_when_list(W) case_else_opt(E) END(R). {
    A = mylite_sql_parser_make_searched_case_expression(state, T, W, E, R);
}
select_order_key(A) ::= LPAREN(L) select_order_key(B) RPAREN(R). {
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

query_expression_limit_clause_opt(A) ::= . {
    A = NULL;
}
query_expression_limit_clause_opt(A) ::= query_expression_limit_clause(L). {
    A = L;
}
query_expression_limit_clause(A) ::= LIMIT(L) limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, NULL);
}
query_expression_limit_clause(A) ::= LIMIT(L) limit_integer(C) OFFSET limit_integer(O). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, O);
}
query_expression_limit_clause(A) ::= LIMIT(L) limit_integer(O) COMMA limit_integer(C). {
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

select_item_list(A) ::= first_select_item(B). {
    A = mylite_sql_parser_make_select_list(state, B);
}
select_item_list(A) ::= select_item_list(B) COMMA trailing_select_item(C). {
    A = mylite_sql_parser_append_select_item(state, B, C);
}

first_select_item(A) ::= STAR(S). {
    A = mylite_sql_parser_make_select_item(
        state,
        mylite_sql_parser_make_wildcard(state, S),
        NULL);
}
first_select_item(A) ::= trailing_select_item(B). {
    A = B;
}

trailing_select_item(A) ::= expression(B). {
    A = mylite_sql_parser_make_select_item(state, B, NULL);
}
trailing_select_item(A) ::= expression(B) AS select_alias(C). {
    A = mylite_sql_parser_make_select_item(state, B, C);
}
trailing_select_item(A) ::= expression(B) select_alias(C). {
    A = mylite_sql_parser_make_select_item(state, B, C);
}
trailing_select_item(A) ::= qualified_wildcard(B). {
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
expression(A) ::= user_variable(T). {
    A = T;
}
expression(A) ::= user_variable(T) ASSIGN(O) expression(V). [ASSIGN] {
    A = mylite_sql_parser_make_user_variable_assignment_expression(state, T, O, V);
}
expression(A) ::= charset_introducer STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
expression(A) ::= charset_introducer HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
expression(A) ::= charset_introducer BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
expression(A) ::= TEMPORAL_LITERAL_INTRODUCER STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
expression(A) ::= qualified_identifier(B). {
    A = B;
}
expression(A) ::= IDENTIFIER(T) LPAREN RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_make_generic_function_with_window_clause(state, T, NULL, R, W);
}
expression(A) ::= IDENTIFIER(T) LPAREN function_argument_list(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_make_generic_function_with_window_clause(state, T, B, R, W);
}
expression(A) ::= keyword_function_token(T) LPAREN RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_make_generic_function_with_window_clause(state, T, NULL, R, W);
}
expression(A) ::= keyword_function_token(T) LPAREN function_argument_list(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_make_generic_function_with_window_clause(state, T, B, R, W);
}
expression(A) ::= match_against_expression(B). {
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
expression(A) ::= LPAREN(L) values_statement(B) RPAREN(R). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, L, B, R);
}
expression(A) ::= EXISTS(E) LPAREN select_statement(B) RPAREN(R). {
    A = mylite_sql_parser_make_exists_predicate(state, E, B, R);
}
expression(A) ::= EXISTS(E) LPAREN values_statement(B) RPAREN(R). {
    A = mylite_sql_parser_make_exists_predicate(state, E, B, R);
}
expression(A) ::= expression(B) BETWEEN(T) scalar_predicate_value(L)
        AND scalar_predicate_value(U). [BETWEEN] {
    A = mylite_sql_parser_make_between_predicate(state, B, T, L, U);
}
expression(A) ::= expression(B) NOT(N) BETWEEN(T) scalar_predicate_value(L)
        AND scalar_predicate_value(U). [BETWEEN] {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_between_predicate(state, B, T, L, U));
}
expression(A) ::= cast_convert_expression(B). {
    A = B;
}

scalar_predicate_value(A) ::= literal(B). {
    A = B;
}
scalar_predicate_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
scalar_predicate_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
scalar_predicate_value(A) ::= qualified_identifier(B). {
    A = B;
}
scalar_predicate_value(A) ::= cast_convert_expression(B). {
    A = B;
}

scalar_predicate_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
scalar_predicate_value(A) ::= user_variable(T). {
    A = T;
}

match_against_expression(A) ::=
    MATCH(T) LPAREN match_column_list(C) RPAREN AGAINST LPAREN expression(S)
    fulltext_search_modifier_opt RPAREN(R). {
    A = mylite_sql_parser_make_generic_function(
        state,
        T,
        mylite_sql_parser_append_function_argument(state, C, S),
        R);
}

match_column_list(A) ::= qualified_identifier(C). {
    A = mylite_sql_parser_make_function_argument_list(state, C);
}
match_column_list(A) ::= match_column_list(L) COMMA qualified_identifier(C). {
    A = mylite_sql_parser_append_function_argument(state, L, C);
}

fulltext_search_modifier_opt ::= .
fulltext_search_modifier_opt ::= IN NATURAL LANGUAGE MODE.
fulltext_search_modifier_opt ::= IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION.
fulltext_search_modifier_opt ::= IN BOOLEAN MODE.
fulltext_search_modifier_opt ::= WITH QUERY EXPANSION.

cast_convert_expression(A) ::= CAST(T) LPAREN expression(V) AS BINARY cast_length_opt RPAREN(R). {
    A = mylite_sql_parser_make_cast_binary_expression(state, T, V, R);
}
cast_convert_expression(A) ::= CAST(T) LPAREN expression(V) AS cast_basic_target(K) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(state, T, K, V, R);
}
cast_convert_expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA BINARY cast_length_opt RPAREN(R). {
    A = mylite_sql_parser_make_convert_binary_type_expression(state, T, V, R);
}
cast_convert_expression(A) ::= CONVERT(T) LPAREN expression(V) COMMA cast_basic_target(K) RPAREN(R). {
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
cast_convert_expression(A) ::= CONVERT(T) LPAREN expression(V) USING BINARY RPAREN(R). {
    A = mylite_sql_parser_make_convert_using_binary_expression(state, T, V, R);
}
cast_convert_expression(A) ::= CONVERT(T) LPAREN expression(V) USING option_name(C) RPAREN(R). {
    A = mylite_sql_parser_make_convert_using_charset_expression(state, T, V, C, R);
}
expression(A) ::= BINARY(T) expression(V). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(state, T, V);
}
expression(A) ::= expression(V) COLLATE(C) option_name(N). {
    A = mylite_sql_parser_make_collate_expression(state, V, C, N);
}
expression(A) ::= window_function_expression(B). {
    A = B;
}

window_function_expression(A) ::= ROW_NUMBER(T) LPAREN RPAREN over_clause(W). {
    A = mylite_sql_parser_make_row_number_window_function_with_clause(state, T, W);
}
window_function_expression(A) ::= RANK(T) LPAREN RPAREN over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause(
        state,
        T,
        MYLITE_SQL_AST_RANK_FUNCTION,
        (struct mylite_sql_window_function_arguments){0},
        W);
}
window_function_expression(A) ::= DENSE_RANK(T) LPAREN RPAREN over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause(
        state,
        T,
        MYLITE_SQL_AST_DENSE_RANK_FUNCTION,
        (struct mylite_sql_window_function_arguments){0},
        W);
}
window_function_expression(A) ::= PERCENT_RANK(T) LPAREN RPAREN over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause(
        state,
        T,
        MYLITE_SQL_AST_PERCENT_RANK_FUNCTION,
        (struct mylite_sql_window_function_arguments){0},
        W);
}
window_function_expression(A) ::= CUME_DIST(T) LPAREN RPAREN over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause(
        state,
        T,
        MYLITE_SQL_AST_CUME_DIST_FUNCTION,
        (struct mylite_sql_window_function_arguments){0},
        W);
}
window_function_expression(A) ::= NTILE(T) LPAREN expression(B) RPAREN over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause(
        state,
        T,
        MYLITE_SQL_AST_NTILE_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 1U, .items = {B}},
        W);
}
window_function_expression(A) ::= LAG(T) LPAREN expression(B) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_LAG_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 1U, .items = {B}},
        NT,
        W);
}
window_function_expression(A) ::= LAG(T) LPAREN expression(B) COMMA expression(C) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_LAG_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 2U, .items = {B, C}},
        NT,
        W);
}
window_function_expression(A) ::= LAG(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_LAG_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 3U, .items = {B, C, D}},
        NT,
        W);
}
window_function_expression(A) ::= LEAD(T) LPAREN expression(B) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_LEAD_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 1U, .items = {B}},
        NT,
        W);
}
window_function_expression(A) ::= LEAD(T) LPAREN expression(B) COMMA expression(C) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_LEAD_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 2U, .items = {B, C}},
        NT,
        W);
}
window_function_expression(A) ::= LEAD(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_LEAD_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 3U, .items = {B, C, D}},
        NT,
        W);
}
window_function_expression(A) ::= FIRST_VALUE(T) LPAREN expression(B) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_FIRST_VALUE_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 1U, .items = {B}},
        NT,
        W);
}
window_function_expression(A) ::= LAST_VALUE(T) LPAREN expression(B) RPAREN
                  window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_LAST_VALUE_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 1U, .items = {B}},
        NT,
        W);
}
window_function_expression(A) ::= NTH_VALUE(T) LPAREN expression(B) COMMA expression(C) RPAREN
                  window_from_first_opt window_null_treatment_opt(NT) over_clause(W). {
    A = mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        T,
        MYLITE_SQL_AST_NTH_VALUE_FUNCTION,
        (struct mylite_sql_window_function_arguments){.count = 2U, .items = {B, C}},
        NT,
        W);
}

window_from_first_opt ::= .
window_from_first_opt ::= FROM FIRST.

window_null_treatment_opt(A) ::= . {
    A = NULL;
}
window_null_treatment_opt(A) ::= RESPECT(T) NULLS(N). {
    A = mylite_sql_parser_make_window_null_treatment(
        state, T, MYLITE_SQL_AST_WINDOW_RESPECT_NULLS, N);
}
window_null_treatment_opt(A) ::= IGNORE(T) NULLS(N). {
    A = mylite_sql_parser_make_window_null_treatment(
        state, T, MYLITE_SQL_AST_WINDOW_IGNORE_NULLS, N);
}

over_clause(A) ::= OVER LPAREN window_spec_opt(W) RPAREN. {
    A = W;
}
over_clause(A) ::= OVER identifier(N). {
    A = mylite_sql_parser_make_window_reference(state, N);
}

aggregate_window_opt(A) ::= . {
    A = NULL;
}
aggregate_window_opt(A) ::= OVER(T) LPAREN window_spec_opt(W) RPAREN(R). {
    A = W == NULL ? mylite_sql_parser_make_empty_window_spec(state, T, R) : W;
}
aggregate_window_opt(A) ::= OVER identifier(N). {
    A = mylite_sql_parser_make_window_reference(state, N);
}

window_clause_opt(A) ::= . {
    A = NULL;
}
window_clause_opt(A) ::= WINDOW named_window_definition_list(L). {
    A = L;
}

named_window_definition_list(A) ::= named_window_definition(D). {
    A = mylite_sql_parser_make_window_definition_list(state, D);
}
named_window_definition_list(A) ::= named_window_definition_list(L) COMMA
    named_window_definition(D). {
    A = mylite_sql_parser_append_window_definition(state, L, D);
}

named_window_definition(A) ::= identifier(N) AS LPAREN window_spec_opt(S) RPAREN. {
    A = mylite_sql_parser_make_window_definition(state, N, S);
}

window_spec_opt(A) ::= . {
    A = NULL;
}
window_spec_opt(A) ::= window_partition_clause(P). {
    A = mylite_sql_parser_make_window_spec(state, NULL, P, NULL, NULL);
}
window_spec_opt(A) ::= window_order_clause(O). {
    A = mylite_sql_parser_make_window_spec(state, NULL, NULL, O, NULL);
}
window_spec_opt(A) ::= window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(state, NULL, NULL, NULL, F);
}
window_spec_opt(A) ::= window_partition_clause(P) window_order_clause(O). {
    A = mylite_sql_parser_make_window_spec(state, NULL, P, O, NULL);
}
window_spec_opt(A) ::= window_partition_clause(P) window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(state, NULL, P, NULL, F);
}
window_spec_opt(A) ::= window_order_clause(O) window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(state, NULL, NULL, O, F);
}
window_spec_opt(A) ::= window_partition_clause(P) window_order_clause(O) window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(state, NULL, P, O, F);
}
window_spec_opt(A) ::= identifier(N). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        NULL,
        NULL,
        NULL);
}
window_spec_opt(A) ::= identifier(N) window_partition_clause(P). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        P,
        NULL,
        NULL);
}
window_spec_opt(A) ::= identifier(N) window_order_clause(O). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        NULL,
        O,
        NULL);
}
window_spec_opt(A) ::= identifier(N) window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        NULL,
        NULL,
        F);
}
window_spec_opt(A) ::= identifier(N) window_partition_clause(P) window_order_clause(O). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        P,
        O,
        NULL);
}
window_spec_opt(A) ::= identifier(N) window_partition_clause(P) window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        P,
        NULL,
        F);
}
window_spec_opt(A) ::= identifier(N) window_order_clause(O) window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        NULL,
        O,
        F);
}
window_spec_opt(A) ::= identifier(N) window_partition_clause(P) window_order_clause(O)
    window_frame_clause(F). {
    A = mylite_sql_parser_make_window_spec(
        state,
        mylite_sql_parser_make_window_reference(state, N),
        P,
        O,
        F);
}

window_partition_clause(A) ::= PARTITION(T) BY function_argument_list(L). {
    A = mylite_sql_parser_make_window_partition_clause(state, T, L);
}

window_order_clause(A) ::= ORDER(T) BY window_order_item_list(L). {
    A = mylite_sql_parser_make_window_order_clause(state, T, L);
}

window_order_item_list(A) ::= window_order_item(I). {
    A = mylite_sql_parser_make_order_by_item_list(state, I);
}
window_order_item_list(A) ::= window_order_item_list(L) COMMA window_order_item(I). {
    A = mylite_sql_parser_append_order_by_item(state, L, I);
}

window_order_item(A) ::= expression(E) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_item(state, E, D);
}

window_frame_clause(A) ::= window_frame_unit(U) window_frame_bound(B). {
    A = mylite_sql_parser_make_window_frame_clause(state, U, B, NULL);
}
window_frame_clause(A) ::= window_frame_unit(U) BETWEEN window_frame_bound(B) AND
    window_frame_bound(C). {
    A = mylite_sql_parser_make_window_frame_clause(state, U, B, C);
}

window_frame_unit(A) ::= ROWS(T). {
    A = T;
}
window_frame_unit(A) ::= RANGE(T). {
    A = T;
}

window_frame_bound(A) ::= UNBOUNDED(U) PRECEDING. {
    A = mylite_sql_parser_make_window_frame_bound(state, U, NULL);
}
window_frame_bound(A) ::= UNBOUNDED(U) FOLLOWING. {
    A = mylite_sql_parser_make_window_frame_bound(state, U, NULL);
}
window_frame_bound(A) ::= CURRENT(T) ROW. {
    A = mylite_sql_parser_make_window_frame_bound(state, T, NULL);
}
window_frame_bound(A) ::= expression(E) PRECEDING(P). {
    A = mylite_sql_parser_make_window_frame_bound(state, P, E);
}
window_frame_bound(A) ::= expression(E) FOLLOWING(F). {
    A = mylite_sql_parser_make_window_frame_bound(state, F, E);
}
window_frame_bound(A) ::= INTERVAL(T) expression(E) date_interval_unit(U) PRECEDING(P). {
    struct mylite_sql_ast_node *arguments =
        mylite_sql_parser_make_function_argument_list(state, E);
    arguments = mylite_sql_parser_append_function_argument(state, arguments, U);
    A = mylite_sql_parser_make_window_frame_bound(
        state, P, mylite_sql_parser_make_generic_function(state, T, arguments, P));
}
window_frame_bound(A) ::= INTERVAL(T) expression(E) date_interval_unit(U) FOLLOWING(F). {
    struct mylite_sql_ast_node *arguments =
        mylite_sql_parser_make_function_argument_list(state, E);
    arguments = mylite_sql_parser_append_function_argument(state, arguments, U);
    A = mylite_sql_parser_make_window_frame_bound(
        state, F, mylite_sql_parser_make_generic_function(state, T, arguments, F));
}

cast_basic_target(A) ::= CHAR cast_length_opt cast_character_set_opt cast_binary_attribute_opt. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= DATE. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= TIME. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= TIME LPAREN INTEGER RPAREN. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= DATETIME. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= DATETIME LPAREN INTEGER RPAREN. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= TIMESTAMP. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= TIMESTAMP LPAREN INTEGER RPAREN. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= YEAR. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= NCHAR cast_length_opt. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= NATIONAL CHAR cast_length_opt. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= NATIONAL CHARACTER cast_length_opt. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= decimal_type_name cast_decimal_precision_opt. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= FLOAT_TYPE cast_float_precision_opt. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= REAL. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= DOUBLE. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= DOUBLE PRECISION. {
    A = MYLITE_SQL_AST_CAST_CHAR_EXPRESSION;
}
cast_basic_target(A) ::= JSON. {
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
cast_length_opt(A) ::= . {
    A = 0;
}
cast_length_opt(A) ::= LPAREN INTEGER RPAREN. {
    A = 0;
}
cast_character_set_opt(A) ::= . {
    A = 0;
}
cast_character_set_opt(A) ::= CHARACTER SET option_name. {
    A = 0;
}
cast_character_set_opt(A) ::= CHARACTER SET BINARY. {
    A = 0;
}
cast_character_set_opt(A) ::= CHARSET option_name. {
    A = 0;
}
cast_character_set_opt(A) ::= CHARSET BINARY. {
    A = 0;
}
cast_binary_attribute_opt(A) ::= . {
    A = 0;
}
cast_binary_attribute_opt(A) ::= BINARY. {
    A = 0;
}

charset_introducer(A) ::= CHARSET_INTRODUCER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
cast_decimal_precision_opt(A) ::= . {
    A = 0;
}
cast_decimal_precision_opt(A) ::= LPAREN INTEGER RPAREN. {
    A = 0;
}
cast_decimal_precision_opt(A) ::= LPAREN INTEGER COMMA INTEGER RPAREN. {
    A = 0;
}
cast_float_precision_opt(A) ::= . {
    A = 0;
}
cast_float_precision_opt(A) ::= LPAREN INTEGER RPAREN. {
    A = 0;
}
cast_float_precision_opt(A) ::= LPAREN INTEGER COMMA INTEGER RPAREN. {
    A = 0;
}
date_interval_unit(A) ::= YEAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= QUARTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= MONTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= WEEK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= DAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= HOUR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= MINUTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= SECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_unit(A) ::= MICROSECOND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
expression(A) ::=
    DATE_ADD(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_DATE_ADD_FUNCTION, V, I, U, R);
}
expression(A) ::=
    DATE_SUB(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_DATE_SUB_FUNCTION, V, I, U, R);
}
expression(A) ::=
    ADDDATE(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_ADDDATE_FUNCTION, V, I, U, R);
}
expression(A) ::=
    SUBDATE(T) LPAREN(L) expression(V) COMMA INTERVAL expression(I)
    date_interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBDATE_FUNCTION, V, I, U, R);
}
expression(A) ::= ADDTIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ADDTIME_FUNCTION, B, C, R);
}
expression(A) ::= SUBTIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_SUBTIME_FUNCTION, B, C, R);
}
expression(A) ::= TIMEDIFF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TIMEDIFF_FUNCTION, B, C, R);
}
expression(A) ::= DATE_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_DATE_FORMAT_FUNCTION, B, C, R);
}
get_format_class(A) ::= DATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
get_format_class(A) ::= TIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
get_format_class(A) ::= DATETIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
get_format_class(A) ::= TIMESTAMP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
expression(A) ::= GET_FORMAT(T) LPAREN get_format_class(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_GET_FORMAT_FUNCTION, B, C, R);
}
expression(A) ::= TIME_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TIME_FORMAT_FUNCTION, B, C, R);
}
expression(A) ::= STR_TO_DATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_FUNCTION, B, C, R);
}
expression(A) ::= DATEDIFF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_DATEDIFF_FUNCTION, B, C, R);
}
expression(A) ::=
    CONVERT_TZ(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_CONVERT_TZ_FUNCTION, B, C, D, R);
}
expression(A) ::= PERIOD_ADD(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_PERIOD_ADD_FUNCTION, B, C, R);
}
expression(A) ::= PERIOD_DIFF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION, B, C, R);
}
expression(A) ::=
    TIMESTAMPDIFF(T) LPAREN timestampdiff_unit(U) COMMA expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION, U, B, C, R);
}
expression(A) ::=
    TIMESTAMPADD(T) LPAREN timestampdiff_unit(U) COMMA expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION, U, B, C, R);
}
expression(A) ::= TIMESTAMP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMP_FUNCTION, B, R);
}
expression(A) ::= TIMESTAMP(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_TIMESTAMP_FUNCTION, B, C, R);
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
expression(A) ::= FROM_DAYS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FROM_DAYS_FUNCTION, B, R);
}
expression(A) ::= MAKEDATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_MAKEDATE_FUNCTION, B, C, R);
}
expression(A) ::=
    MAKETIME(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_MAKETIME_FUNCTION, B, C, D, R);
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
expression(A) ::= TO_DAYS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TO_DAYS_FUNCTION, B, R);
}
expression(A) ::= TO_SECONDS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TO_SECONDS_FUNCTION, B, R);
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
expression(A) ::= DAYNAME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYNAME_FUNCTION, B, R);
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
expression(A) ::= MONTHNAME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MONTHNAME_FUNCTION, B, R);
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
expression(A) ::= MICROSECOND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MICROSECOND_FUNCTION, B, R);
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
expression(A) ::= sysdate_value(T). {
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
current_time_value(A) ::= CURRENT_TIME(T) LPAREN INTEGER(P) RPAREN(R). {
    A = mylite_sql_parser_make_temporal_value_with_precision(
        state,
        T,
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        (struct mylite_sql_temporal_fractional_precision_tokens){
            .precision_token = P,
            .end_token = R,
            .has_precision = 1,
        });
}
current_time_value(A) ::= CURTIME(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_CURRENT_TIME_VALUE, R);
}
current_time_value(A) ::= CURTIME(T) LPAREN(L) INTEGER(P) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_temporal_value_with_precision(
        state,
        T,
        L,
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        (struct mylite_sql_temporal_fractional_precision_tokens){
            .precision_token = P,
            .end_token = R,
            .has_precision = 1,
        });
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
utc_time_value(A) ::= UTC_TIME(T) LPAREN INTEGER(P) RPAREN(R). {
    A = mylite_sql_parser_make_temporal_value_with_precision(
        state,
        T,
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        (struct mylite_sql_temporal_fractional_precision_tokens){
            .precision_token = P,
            .end_token = R,
            .has_precision = 1,
        });
}
utc_timestamp_value(A) ::= UTC_TIMESTAMP(T). {
    A = mylite_sql_parser_make_utc_timestamp_keyword(state, T);
}
utc_timestamp_value(A) ::= UTC_TIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE, R);
}
utc_timestamp_value(A) ::= UTC_TIMESTAMP(T) LPAREN INTEGER(P) RPAREN(R). {
    A = mylite_sql_parser_make_temporal_value_with_precision(
        state,
        T,
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        (struct mylite_sql_temporal_fractional_precision_tokens){
            .precision_token = P,
            .end_token = R,
            .has_precision = 1,
        });
}
sysdate_value(A) ::= SYSDATE(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_SYSDATE_FUNCTION, R);
}
sysdate_value(A) ::= SYSDATE(T) LPAREN(L) function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_function_argument_count_error(
        state, T, L, MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR, B, R);
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
expression(A) ::= CHAR(T) LPAREN function_argument_list(B) USING option_name(C) RPAREN(R). {
    A = mylite_sql_parser_make_generic_function(
        state,
        T,
        mylite_sql_parser_append_function_argument(state, B, C),
        R);
}
expression(A) ::= REPLACE(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_REPLACE_FUNCTION, B, C, D, R);
}
expression(A) ::= INSERT(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) COMMA expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_four_argument_function(
        state, T, MYLITE_SQL_AST_INSERT_STRING_FUNCTION, B, C, D, E, R);
}
expression(A) ::= REVERSE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_REVERSE_FUNCTION, B, R);
}
expression(A) ::= SOUNDEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SOUNDEX_FUNCTION, B, R);
}
expression(A) ::= QUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_QUOTE_FUNCTION, B, R);
}
expression(A) ::= ELT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_ELT_FUNCTION, B, R);
}
expression(A) ::= EXPORT_SET(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_EXPORT_SET_FUNCTION, B, R);
}
expression(A) ::= FIELD(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_FIELD_FUNCTION, B, R);
}
expression(A) ::= MAKE_SET(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_MAKE_SET_FUNCTION, B, R);
}
expression(A) ::= GREATEST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_GREATEST_FUNCTION, B, R);
}
expression(A) ::= LEAST(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_LEAST_FUNCTION, B, R);
}
expression(A) ::= INTERVAL(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_INTERVAL_FUNCTION, B, C, R);
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
expression(A) ::= JSON_INSERT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_INSERT_FUNCTION, B, R);
}
expression(A) ::= JSON_SET(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_SET_FUNCTION, B, R);
}
expression(A) ::= JSON_REPLACE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_REPLACE_FUNCTION, B, R);
}
expression(A) ::= JSON_REMOVE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_REMOVE_FUNCTION, B, R);
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
expression(A) ::= JSON_KEYS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_KEYS_FUNCTION, B, R);
}
expression(A) ::= JSON_KEYS(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_KEYS_FUNCTION, B, C, R);
}
expression(A) ::= JSON_TYPE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_TYPE_FUNCTION, B, R);
}
expression(A) ::= JSON_QUOTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_FUNCTION, B, R);
}
expression(A) ::= JSON_EXTRACT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION, B, C, R);
}
expression(A) ::= JSON_VALUE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_JSON_VALUE_FUNCTION, B, C, R);
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
expression(A) ::= REGEXP_INSTR(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION, B, R);
}
expression(A) ::= REGEXP_SUBSTR(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION, B, R);
}
expression(A) ::= REGEXP_REPLACE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION, B, R);
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
expression(A) ::= WEIGHT_STRING(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION, B, R);
}
expression(A) ::= WEIGHT_STRING(T) LPAREN expression(B) AS BINARY LPAREN INTEGER(N) RPAREN RPAREN(R). {
    struct mylite_sql_ast_node *length =
        mylite_sql_parser_make_literal(state, N, MYLITE_SQL_AST_LITERAL_INTEGER);
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION, B, length, R);
}
expression(A) ::= TO_BASE64(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TO_BASE64_FUNCTION, B, R);
}
expression(A) ::= FROM_BASE64(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FROM_BASE64_FUNCTION, B, R);
}
expression(A) ::= UNHEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNHEX_FUNCTION, B, R);
}
expression(A) ::= UUID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UUID_FUNCTION, R);
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
expression(A) ::= rand_expression(B). {
    A = B;
}

rand_expression(A) ::= RAND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(state, T, MYLITE_SQL_AST_RAND_FUNCTION, R);
}
rand_expression(A) ::= RAND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RAND_SEED_FUNCTION, B, R);
}
rand_expression(A) ::= RAND(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
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
expression(A) ::= string_length_expression(B). {
    A = B;
}

string_length_expression(A) ::= LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LENGTH_FUNCTION, B, R);
}
string_length_expression(A) ::= OCTET_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION, B, R);
}
string_length_expression(A) ::= BIT_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIT_LENGTH_FUNCTION, B, R);
}
string_length_expression(A) ::= CHAR_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION, B, R);
}
string_length_expression(A) ::= CHARACTER_LENGTH(T) LPAREN expression(B) RPAREN(R). {
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
expression(A) ::= substring_expression(B). {
    A = B;
}

substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, R);
}
substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, D, R);
}
substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, R);
}
substring_expression(A) ::= SUBSTRING(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTRING_FUNCTION, B, C, D, R);
}
substring_expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, R);
}
substring_expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, D, R);
}
substring_expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, R);
}
substring_expression(A) ::= SUBSTR(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_SUBSTR_FUNCTION, B, C, D, R);
}
substring_expression(A) ::= MID(T) LPAREN(L) expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, R);
}
substring_expression(A) ::= MID(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, D, R);
}
substring_expression(A) ::= MID(T) LPAREN(L) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_two_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, R);
}
substring_expression(A) ::= MID(T) LPAREN(L) expression(B) FROM expression(C) FOR expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_three_argument_function(
        state, T, L, MYLITE_SQL_AST_MID_FUNCTION, B, C, D, R);
}
date_format_numeric_predicate_expression(A) ::=
    DATE_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R) predicate_comparison_operator(O)
        date_format_numeric_predicate_value(V). {
    struct mylite_sql_ast_node *date_format =
        mylite_sql_parser_make_two_argument_function(
            state, T, MYLITE_SQL_AST_DATE_FORMAT_FUNCTION, B, C, R);
    A = mylite_sql_parser_make_binary_expression(
        state, date_format, O.token, O.operator_kind, V);
}
date_format_numeric_predicate_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
date_format_numeric_predicate_value(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
date_format_numeric_predicate_value(A) ::= PLUS(P) date_format_numeric_predicate_value(V). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE, V);
}
date_format_numeric_predicate_value(A) ::= MINUS(M) date_format_numeric_predicate_value(V). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE, V);
}
temporal_extract_predicate_expression(A) ::= DATE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DATE_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= TIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TIME_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= TIME_TO_SEC(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= TO_DAYS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TO_DAYS_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= TO_SECONDS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TO_SECONDS_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::=
    EXTRACT(T) LPAREN extract_unit(U) FROM expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_EXTRACT_FUNCTION, U, B, R);
}
temporal_extract_predicate_expression(A) ::= WEEK(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEEK_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= WEEK(T) LPAREN expression(B) COMMA expression(C)
        RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_WEEK_FUNCTION, B, C, R);
}
temporal_extract_predicate_expression(A) ::= WEEKDAY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEEKDAY_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= DAYNAME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYNAME_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= WEEKOFYEAR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= YEARWEEK(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_YEARWEEK_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= YEARWEEK(T) LPAREN expression(B) COMMA
        expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_YEARWEEK_FUNCTION, B, C, R);
}
temporal_extract_predicate_expression(A) ::= QUARTER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_QUARTER_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= YEAR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_YEAR_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= MONTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MONTH_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= MONTHNAME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MONTHNAME_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= DAY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAY_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= DAYOFMONTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYOFMONTH_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= DAYOFWEEK(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYOFWEEK_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= DAYOFYEAR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_DAYOFYEAR_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= LAST_DAY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LAST_DAY_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= HOUR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_HOUR_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= MINUTE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MINUTE_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= SECOND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SECOND_FUNCTION, B, R);
}
temporal_extract_predicate_expression(A) ::= MICROSECOND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_MICROSECOND_FUNCTION, B, R);
}
temporal_extract_arithmetic_predicate_expression(A) ::=
    temporal_extract_predicate_expression(B) PLUS(T) predicate_integer_value(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
temporal_extract_arithmetic_predicate_expression(A) ::=
    temporal_extract_predicate_expression(B) MINUS(T) predicate_integer_value(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
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
expression(A) ::= REGEXP_INSTR(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= REGEXP_SUBSTR(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= REGEXP_REPLACE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR, NULL, R);
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
expression(A) ::= TO_BASE64(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= TO_BASE64(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= FROM_BASE64(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FROM_BASE64(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= UUID(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR, B, R);
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
expression(A) ::= SOUNDEX(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SOUNDEX(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= ELT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= EXPORT_SET(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FIELD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= MAKE_SET(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR, NULL, R);
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
expression(A) ::= JSON_KEYS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_KEYS(T) LPAREN expression(B) COMMA expression(C) COMMA
    function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= JSON_QUOTE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_QUOTE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= JSON_INSERT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_SET(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_REPLACE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= JSON_REMOVE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR, NULL, R);
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
expression(A) ::= STR_TO_DATE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= STR_TO_DATE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    STR_TO_DATE(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= CONVERT_TZ(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CONVERT_TZ(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= CONVERT_TZ(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::=
    CONVERT_TZ(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR, E, R);
}
expression(A) ::= PERIOD_ADD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= PERIOD_ADD(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    PERIOD_ADD(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= PERIOD_DIFF(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= PERIOD_DIFF(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    PERIOD_DIFF(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= TIMEDIFF(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= TIMEDIFF(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    TIMEDIFF(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D)
    RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::=
    UNIX_TIMESTAMP(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= TIMESTAMP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    TIMESTAMP(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D)
    RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR, D, R);
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
expression(A) ::= FROM_DAYS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    FROM_DAYS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= MAKEDATE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= MAKEDATE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    MAKEDATE(T) LPAREN expression(B) COMMA expression(C) COMMA
    function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR, D, R);
}
expression(A) ::= MAKETIME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= MAKETIME(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= MAKETIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    MAKETIME(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA
    function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR, E, R);
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
expression(A) ::= TO_DAYS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    TO_DAYS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= TO_SECONDS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    TO_SECONDS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= DAYNAME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= DAYNAME(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= MONTHNAME(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= MONTHNAME(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR, C, R);
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
expression(A) ::= COUNT(T) LPAREN(L) STAR RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_zero_argument_function(
            state, T, L, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, R),
        W);
}
expression(A) ::= COUNT(T) LPAREN(L) DISTINCT qualified_identifier(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION, B, R),
        W);
}
expression(A) ::= COUNT(T) LPAREN(L) DISTINCT LPAREN qualified_identifier(B) RPAREN RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION, B, R),
        W);
}
expression(A) ::= COUNT(T) LPAREN DISTINCT qualified_identifier(B) COMMA function_argument_list(C)
                  RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_prepend_function_argument(state, B, C), R),
        W);
}
expression(A) ::= COUNT(T) LPAREN DISTINCT count_distinct_placeholder_argument(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_make_function_argument_list(state, B), R),
        W);
}
expression(A) ::= COUNT(T) LPAREN DISTINCT count_distinct_placeholder_argument(B) COMMA
                  function_argument_list(C) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_prepend_function_argument(state, B, C), R),
        W);
}
expression(A) ::= COUNT(T) LPAREN(L) qualified_identifier(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION, B, R),
        W);
}
count_distinct_placeholder_argument(A) ::= aggregate_literal(B). {
    A = B;
}
count_distinct_placeholder_argument(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(state, T, MYLITE_SQL_AST_CONCAT_FUNCTION, B, R);
}
count_distinct_placeholder_argument(A) ::= BINARY(T) expression(B). [BINARY] {
    A = mylite_sql_parser_make_unary_binary_expression(state, T, B);
}
expression(A) ::= COUNT(T) LPAREN(L) count_nullif_predicate_expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION, B, R);
}
expression(A) ::= COUNT(T) LPAREN(L) count_literal(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION, B, R);
}
count_nullif_predicate_expression(A) ::= NULLIF(T) LPAREN count_nullif_predicate(B)
        COMMA FALSE(F) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state,
        T,
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        B,
        mylite_sql_parser_make_literal(state, F, MYLITE_SQL_AST_LITERAL_FALSE),
        R);
}
count_nullif_predicate(A) ::= qualified_identifier(C) EQUAL(O) predicate_comparison_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
count_nullif_predicate(A) ::=
    qualified_identifier(C) LIKE(O) predicate_like_pattern(P) predicate_like_escape_opt(E). {
    A = mylite_sql_parser_make_like_comparison_predicate(
        state,
        &(const struct mylite_sql_parser_like_comparison_predicate_request){
            .left = C,
            .operator_token = O,
            .operator_kind = MYLITE_SQL_AST_OPERATOR_LIKE,
            .right = P,
            .escape = E,
        });
}
expression(A) ::= MIN(T) LPAREN(L) qualified_identifier(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= MIN(T) LPAREN(L) aggregate_literal(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= MAX(T) LPAREN(L) qualified_identifier(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= MAX(T) LPAREN(L) aggregate_literal(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= SUM(T) LPAREN(L) sum_aggregate_argument(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= SUM(T) LPAREN DISTINCT expression(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_make_function_argument_list(state, B), R),
        W);
}
expression(A) ::= AVG(T) LPAREN qualified_identifier(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_one_argument_function(
            state, T, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= AVG(T) LPAREN aggregate_literal(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_one_argument_function(
            state, T, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= AVG(T) LPAREN DISTINCT expression(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_make_function_argument_list(state, B), R),
        W);
}
expression(A) ::= BIT_AND(T) LPAREN(L) qualified_identifier(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= BIT_AND(T) LPAREN(L) aggregate_literal(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= BIT_OR(T) LPAREN(L) qualified_identifier(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= BIT_OR(T) LPAREN(L) aggregate_literal(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= BIT_XOR(T) LPAREN(L) qualified_identifier(B) RPAREN(R)
                  aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= BIT_XOR(T) LPAREN(L) aggregate_literal(B) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_no_space_one_argument_function(
            state, T, L, MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION, B, R),
        W);
}
expression(A) ::= GROUP_CONCAT(T) LPAREN(L) expression(B)
    group_concat_order_opt(O) group_concat_separator_opt(S) RPAREN(R) aggregate_window_opt(W). {
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_group_concat_function(state, T, L, B, O, S, R),
        W);
}
expression(A) ::= GROUP_CONCAT(T) LPAREN expression(B) COMMA function_argument_list(C)
    group_concat_order_opt(O) group_concat_separator_opt(S) RPAREN(R) aggregate_window_opt(W). {
    (void)O;
    (void)S;
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_prepend_function_argument(state, B, C), R),
        W);
}
expression(A) ::= GROUP_CONCAT(T) LPAREN DISTINCT expression(B)
    group_concat_order_opt(O) group_concat_separator_opt(S) RPAREN(R) aggregate_window_opt(W). {
    (void)O;
    (void)S;
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_make_function_argument_list(state, B), R),
        W);
}
expression(A) ::= GROUP_CONCAT(T) LPAREN DISTINCT expression(B) COMMA function_argument_list(C)
    group_concat_order_opt(O) group_concat_separator_opt(S) RPAREN(R) aggregate_window_opt(W). {
    (void)O;
    (void)S;
    A = mylite_sql_parser_attach_function_window_clause(
        mylite_sql_parser_make_generic_function(
            state, T, mylite_sql_parser_prepend_function_argument(state, B, C), R),
        W);
}
expression(A) ::= ANY_VALUE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ANY_VALUE_FUNCTION, B, R);
}
expression(A) ::= ANY_VALUE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ANY_VALUE(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR, C, R);
}

group_concat_order_opt(A) ::= . {
    A = NULL;
}
group_concat_order_opt(A) ::= ORDER(O) BY window_order_item_list(L). {
    A = mylite_sql_parser_make_order_by_clause_from_item_list(state, O, L);
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
expression(A) ::= LOGICAL_NOT(T) expression(B). [LOGICAL_NOT] {
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
expression(A) ::= expression(B) LIKE(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LIKE, C);
}
expression(A) ::= expression(B) REGEXP(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_REGEXP, C);
}
expression(A) ::= expression(B) RLIKE(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_RLIKE, C);
}
expression(A) ::= expression(B) NOT(N) REGEXP(T) expression(C). [NOT] {
    A = mylite_sql_parser_make_not_predicate(
        state,
        N,
        mylite_sql_parser_make_binary_expression(
            state, B, T, MYLITE_SQL_AST_OPERATOR_REGEXP, C));
}
expression(A) ::= expression(B) NOT(N) RLIKE(T) expression(C). [NOT] {
    A = mylite_sql_parser_make_not_predicate(
        state,
        N,
        mylite_sql_parser_make_binary_expression(state, B, T, MYLITE_SQL_AST_OPERATOR_RLIKE, C));
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
literal(A) ::= string_text_literal(V). {
    A = V;
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

string_text_literal(A) ::= ordinary_string_literal(V). [STRING_LITERAL_REDUCE] {
    A = V;
}
string_text_literal(A) ::= NATIONAL_STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NATIONAL_STRING);
}

ordinary_string_literal(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
ordinary_string_literal(A) ::= ordinary_string_literal(B) STRING(T). {
    A = mylite_sql_parser_append_string_literal_segment(state, B, T);
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

aggregate_literal(A) ::= literal(B). {
    A = B;
}
aggregate_literal(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
aggregate_literal(A) ::= PLUS(P) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
aggregate_literal(A) ::= PLUS(P) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
}
aggregate_literal(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
aggregate_literal(A) ::= MINUS(M) DECIMAL(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL));
}
aggregate_literal(A) ::= MINUS(M) FLOAT(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT));
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
identifier(A) ::= ARRAY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATA(T). {
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
identifier(A) ::= SOUNDEX(T). {
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
identifier(A) ::= ELT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= EXPORT_SET(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIELD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MAKE_SET(T). {
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
identifier(A) ::= JSON_VALUE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_KEYS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_TYPE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_QUOTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_INSERT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_SET(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_REPLACE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_REMOVE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= JSON_UNQUOTE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATE_FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= GET_FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIME_FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STR_TO_DATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DATEDIFF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CONVERT_TZ(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ESCAPE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= PERIOD_ADD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= PERIOD_DIFF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIMESTAMPDIFF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIMESTAMPADD(T). {
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
identifier(A) ::= FROM_DAYS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MAKEDATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MAKETIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TIME_TO_SEC(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TO_DAYS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TO_SECONDS(T). {
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
identifier(A) ::= TIMEDIFF(T). {
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
identifier(A) ::= WEIGHT_STRING(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CURRENT_ROLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CURRENT(T). {
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
identifier(A) ::= TO_BASE64(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FROM_BASE64(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UNHEX(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UUID(T). {
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
identifier(A) ::= SYSDATE(T). {
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
identifier(A) ::= LOGS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= REPLICA(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= REPLICAS(T). {
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
identifier(A) ::= ANY_VALUE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BIT_COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ASCII(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= UNICODE(T). {
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
identifier(A) ::= LAST(T). {
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
identifier(A) ::= GEOMCOLLECTION(T). {
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
identifier(A) ::= MIN_ROWS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MAX_ROWS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= AVG_ROW_LENGTH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DELAY_KEY_WRITE(T). {
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
identifier(A) ::= PLUGINS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= PRIVILEGES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STATUS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STORAGE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= DISK(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MEMORY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TABLESPACE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= INSERT_METHOD(T). {
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
identifier(A) ::= AGAINST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= EXPANSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LANGUAGE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUERY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ROLLUP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SHARE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SKIP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LOCKED(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= NOWAIT(T). {
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
identifier(A) ::= SRID(T). {
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
identifier(A) ::= DAYNAME(T). {
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
identifier(A) ::= MONTHNAME(T). {
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
create_table_item(A) ::= named_primary_key_definition(B). {
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
    PRIMARY(P) KEY primary_key_index_name_opt(N) index_type_opt(Y) LPAREN primary_key_part_list(L)
    RPAREN(R)
    index_option_list_opt(O). {
    A = mylite_sql_parser_make_primary_key_definition(state, P, N, Y, L, R, O);
}

named_primary_key_definition(A) ::= CONSTRAINT identifier primary_key_definition(P). {
    A = P;
}
named_primary_key_definition(A) ::= CONSTRAINT primary_key_definition(P). {
    A = P;
}

primary_key_index_name_opt(A) ::= . {
    A = NULL;
}
primary_key_index_name_opt(A) ::= identifier(B). {
    A = B;
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
primary_key_part(A) ::= identifier(B) LPAREN INTEGER(L) RPAREN index_key_direction_opt(D). {
    A = mylite_sql_parser_make_secondary_index_part(
        state,
        B,
        mylite_sql_parser_make_literal(state, L, MYLITE_SQL_AST_LITERAL_INTEGER),
        D);
}
primary_key_part(A) ::= functional_index_part(B). {
    A = B;
}
primary_key_part(A) ::= multi_valued_index_part(B). {
    A = B;
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
index_type_option(A) ::= TYPE(T) identifier(V). {
    A = mylite_sql_parser_make_index_type_option(state, T, V);
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
index_option(A) ::= ALGORITHM(T) equal_opt alter_algorithm_value. {
    A = mylite_sql_parser_make_identifier(state, T);
}
index_option(A) ::= LOCK(T) equal_opt alter_lock_value. {
    A = mylite_sql_parser_make_identifier(state, T);
}
index_option(A) ::= KEY_BLOCK_SIZE(T) equal_opt INTEGER. {
    A = mylite_sql_parser_make_identifier(state, T);
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
constraint_name_opt(A) ::= CONSTRAINT. {
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
secondary_index_part(A) ::= functional_index_part(B). {
    A = B;
}
secondary_index_part(A) ::= multi_valued_index_part(B). {
    A = B;
}

functional_index_part(A) ::= LPAREN(L) expression(B) RPAREN(R) index_key_direction_opt(D). {
    A = mylite_sql_parser_make_functional_index_part(state, L, B, R, D);
}

multi_valued_index_part(A) ::=
    LPAREN(L) CAST(T) LPAREN expression(B) AS cast_basic_target(K) ARRAY RPAREN(C) RPAREN(R)
    index_key_direction_opt(D). {
    A = mylite_sql_parser_make_multi_valued_index_part(
        state,
        (struct mylite_sql_multi_valued_index_part_tokens){
            .left_paren = L,
            .cast_token = T,
            .right_cast_paren = C,
            .right_part_paren = R,
        },
        B,
        K,
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
column_definition(A) ::= identifier(N) column_type(T) column_attribute_list_opt(L)
    inline_foreign_key_reference. {
    A = mylite_sql_parser_make_column_definition_with_attributes(state, N, T, L);
}

inline_foreign_key_reference ::= REFERENCES table_name LPAREN foreign_key_part_list RPAREN
    foreign_key_action_clause_list_opt.

column_attribute_list_opt(A) ::= . {
    A = NULL;
}
column_attribute_list_opt(A) ::= column_attribute_list(B). {
    A = B;
}

column_attribute_list(A) ::= column_attribute(B). {
    A = mylite_sql_parser_make_column_attribute_list(state, B);
}
column_attribute_list(A) ::= BINARY(B) column_charset_shorthand_attribute(C). {
    A = mylite_sql_parser_append_column_attribute(
        state,
        mylite_sql_parser_make_column_attribute_list(
            state,
            mylite_sql_parser_make_column_binary_collation_attribute(state, B)),
        C);
}
column_attribute_list(A) ::= column_attribute_list(B) column_attribute(C). {
    A = mylite_sql_parser_append_column_attribute(state, B, C);
}
column_attribute_list(A) ::= column_attribute_list(L) BINARY(B)
    column_charset_shorthand_attribute(C). {
    A = mylite_sql_parser_append_column_attribute(
        state,
        mylite_sql_parser_append_column_attribute(
            state,
            L,
            mylite_sql_parser_make_column_binary_collation_attribute(state, B)),
        C);
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
column_attribute(A) ::= column_charset_shorthand_attribute(B). {
    A = B;
}
column_charset_shorthand_attribute(A) ::= ASCII(T). {
    A = mylite_sql_parser_make_column_charset_attribute(
        state,
        T,
        mylite_sql_parser_make_identifier(state, T));
}
column_charset_shorthand_attribute(A) ::= UNICODE(T). {
    A = mylite_sql_parser_make_column_charset_attribute(
        state,
        T,
        mylite_sql_parser_make_identifier(state, T));
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
column_attribute(A) ::= BINARY(T). [COLUMN_BINARY_ATTRIBUTE] {
    A = mylite_sql_parser_make_column_binary_collation_attribute(state, T);
}
column_attribute(A) ::= COMMENT(C) STRING(V). {
    A = mylite_sql_parser_make_column_comment_attribute(
        state,
        C,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}
column_attribute(A) ::= VISIBLE(T). {
    A = mylite_sql_parser_make_column_visibility_attribute(
        state,
        T,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE);
}
column_attribute(A) ::= INVISIBLE(T). {
    A = mylite_sql_parser_make_column_visibility_attribute(
        state,
        T,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE);
}
column_attribute(A) ::= SRID(T) INTEGER(V). {
    A = mylite_sql_parser_make_column_srid_attribute(
        state,
        T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
column_attribute(A) ::= generated_column_clause(B). {
    A = B;
}
column_attribute(A) ::= PRIMARY(P) KEY(K). {
    A = mylite_sql_parser_make_inline_primary_key(state, P, K);
}
column_attribute(A) ::= KEY(K). {
    A = mylite_sql_parser_make_inline_primary_key(state, K, K);
}
column_attribute(A) ::= UNIQUE(U). [KEY] {
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
spatial_type(A) ::= GEOMCOLLECTION(T). {
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

enum_label(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
enum_label(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
enum_label(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
enum_label_list(A) ::= enum_label(V). {
    A = mylite_sql_parser_make_enum_label_list(state, V);
}
enum_label_list(A) ::= enum_label_list(L) COMMA enum_label(V). {
    A = mylite_sql_parser_append_enum_label(state, L, V);
}

set_type(A) ::= SET(T) LPAREN set_member_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_set_type(state, T, L, R);
}

set_member(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
set_member(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
set_member(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
set_member_list(A) ::= set_member(V). {
    A = mylite_sql_parser_make_set_member_list(state, V);
}
set_member_list(A) ::= set_member_list(L) COMMA set_member(V). {
    A = mylite_sql_parser_append_set_member(state, L, V);
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
binary_string_type(A) ::= VARCHAR(T) LPAREN INTEGER(L) RPAREN BYTE(B). {
    A = mylite_sql_parser_make_binary_string_type(
        state,
        (struct mylite_sql_binary_string_type_tokens){
            .type_token = T,
            .length_token = L,
            .end_token = B,
            .binary_string_type = MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY,
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
binary_string_type_name(A) ::= LONG(T) BYTE(B). {
    A = (struct mylite_sql_binary_string_type_tokens){
        .type_token = T,
        .end_token = B,
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
            .scale_token = P.scale_token,
            .end_token = U.attribute_token.text == NULL
                ? (P.end_token.text == NULL ? T : P.end_token)
                : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
            .has_precision = P.has_precision,
            .has_scale = P.has_scale,
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
approximate_type(A) ::= DOUBLE(T) LPAREN INTEGER(P) COMMA INTEGER(S) RPAREN(R)
    approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = T,
            .precision_token = P,
            .scale_token = S,
            .end_token = U.attribute_token.text == NULL ? R : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE,
            .has_precision = 1,
            .has_scale = 1,
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
approximate_type(A) ::= DOUBLE(D) PRECISION LPAREN INTEGER(P) COMMA INTEGER(S) RPAREN(R)
    approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = D,
            .precision_token = P,
            .scale_token = S,
            .end_token = U.attribute_token.text == NULL ? R : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE,
            .has_precision = 1,
            .has_scale = 1,
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
approximate_type(A) ::= REAL(T) LPAREN INTEGER(P) COMMA INTEGER(S) RPAREN(R)
    approximate_unsigned_opt(U). {
    A = mylite_sql_parser_make_approximate_type(
        state,
        (struct mylite_sql_approximate_type_tokens){
            .type_token = T,
            .precision_token = P,
            .scale_token = S,
            .end_token = U.attribute_token.text == NULL ? R : U.attribute_token,
            .attribute_token = U.attribute_token,
            .approximate_type = MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL,
            .has_precision = 1,
            .has_scale = 1,
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
approximate_precision_opt(A) ::= LPAREN INTEGER(P) COMMA INTEGER(S) RPAREN(R). {
    A = (struct mylite_sql_approximate_type_tokens){
        .precision_token = P,
        .scale_token = S,
        .end_token = R,
        .has_precision = 1,
        .has_scale = 1,
    };
}
approximate_precision_opt(A) ::= LPAREN DECIMAL RPAREN(R). {
    A = (struct mylite_sql_approximate_type_tokens){
        .end_token = R,
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
approximate_unsigned_opt(A) ::= SIGNED(S). {
    A = (struct mylite_sql_approximate_type_tokens){
        .attribute_token = S,
        .is_unsigned = 0,
    };
}

date_type(A) ::= DATE(T). {
    A = mylite_sql_parser_make_date_type(state, T);
}

datetime_type(A) ::= DATETIME(T). {
    A = mylite_sql_parser_make_datetime_type(
        state,
        (struct mylite_sql_temporal_type_tokens){
            .type_token = T,
            .end_token = T,
        });
}
datetime_type(A) ::= DATETIME(T) LPAREN INTEGER(P) RPAREN(R). {
    A = mylite_sql_parser_make_datetime_type(
        state,
        (struct mylite_sql_temporal_type_tokens){
            .type_token = T,
            .precision_token = P,
            .end_token = R,
            .has_precision = 1,
        });
}

time_type(A) ::= TIME(T). {
    A = mylite_sql_parser_make_time_type(
        state,
        (struct mylite_sql_temporal_type_tokens){
            .type_token = T,
            .end_token = T,
        });
}
time_type(A) ::= TIME(T) LPAREN INTEGER(P) RPAREN(R). {
    A = mylite_sql_parser_make_time_type(
        state,
        (struct mylite_sql_temporal_type_tokens){
            .type_token = T,
            .precision_token = P,
            .end_token = R,
            .has_precision = 1,
        });
}

timestamp_type(A) ::= TIMESTAMP(T). {
    A = mylite_sql_parser_make_timestamp_type(
        state,
        (struct mylite_sql_temporal_type_tokens){
            .type_token = T,
            .end_token = T,
        });
}
timestamp_type(A) ::= TIMESTAMP(T) LPAREN INTEGER(P) RPAREN(R). {
    A = mylite_sql_parser_make_timestamp_type(
        state,
        (struct mylite_sql_temporal_type_tokens){
            .type_token = T,
            .precision_token = P,
            .end_token = R,
            .has_precision = 1,
        });
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
year_type(A) ::= YEAR(T) UNSIGNED(U). {
    A = mylite_sql_parser_make_year_type(
        state,
        (struct mylite_sql_year_type_tokens){
            .type_token = T,
            .end_token = U,
            .has_width = 0,
        });
}
year_type(A) ::= YEAR(T) LPAREN INTEGER(W) RPAREN UNSIGNED(U). {
    A = mylite_sql_parser_make_year_type(
        state,
        (struct mylite_sql_year_type_tokens){
            .type_token = T,
            .width_token = W,
            .end_token = U,
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
column_default(A) ::= DEFAULT NULL DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_column_default_null(state, D, N);
}
column_default(A) ::= DEFAULT NULL DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_column_default_value(state, D, V);
}
column_default(A) ::= DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_column_default_value(state, D, V);
}
column_default(A) ::= DEFAULT column_default_value DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_column_default_null(state, D, N);
}
column_default(A) ::= DEFAULT column_default_value DEFAULT(D) column_default_value(V). {
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
column_default_value(A) ::= charset_introducer STRING(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
column_default_value(A) ::= charset_introducer STRING(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
column_default_value(A) ::= charset_introducer HEX_LITERAL(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
column_default_value(A) ::= charset_introducer HEX_LITERAL(T) COLLATE option_name. {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
column_default_value(A) ::= charset_introducer BIT_LITERAL(T). [INTRODUCED_LITERAL_VALUE] {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
column_default_value(A) ::= charset_introducer BIT_LITERAL(T) COLLATE option_name. {
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

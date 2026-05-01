%name MyLiteLemon
%token_prefix ML_
%token_type {MyliteToken}
%default_type {MyliteToken}
%fallback ATOM ACTIVE ADD AFTER ASC AS AT AUTO_INCREMENT AVG_ROW_LENGTH BACKUP BEFORE BLOCK BUCKETS CATALOG_NAME CHANGED CHANNEL CLASS_ORIGIN COALESCE CODE COLLATE COLUMN_NAME COMMENT COMPLETION COMPRESSION CONSISTENT CONSTRAINT_CATALOG CONSTRAINT_NAME CONSTRAINT_SCHEMA CONTAINS CONTEXT CONVERT CPU CURSOR_NAME CURRENT_USER DATAFILE DECIMAL DEFINITION DELAY_KEY_WRITE DESCRIPTION DETERMINISTIC DIRECTORY DISABLE DISCARD DUPLICATE EACH ENABLE ENCRYPTION ENGINE_ATTRIBUTE EVERY EXCHANGE EXISTS EXPORT FAST FAULTS FILE_BLOCK_SIZE FILTER FOLLOWS FORCE FOREIGN FOUND GENERAL GROUP GTIDS HISTOGRAM IDENTIFIED INACTIVE INFILE INNODB INSERT_METHOD INT INTEGER INVOKER IO IPC JOIN JSON KEYRING KEY_BLOCK_SIZE LANGUAGE LEAVES LOG MAX_ROWS MEDIUM MEMORY MERGE MESSAGE_TEXT MIGRATE MIN_ROWS MODIFIES MODIFY MUTEX MYSQL_ERRNO NAME NOT NUMBER ONE ONLY OPTIONS ORGANIZATION PACK_KEYS PAGE PARSE_TREE PARTITION PHASE PRECEDES PRESERVE READS REAL REBUILD REDO_LOG REFERENCE RELAY_LOG_FILE RELAY_LOG_POS RELOAD REMOVE REORGANIZE REPLICATE_DO_DB REPLICATE_DO_TABLE REPLICATE_IGNORE_DB REPLICATE_IGNORE_TABLE REPLICATE_REWRITE_DB REPLICATE_WILD_DO_TABLE REPLICATE_WILD_IGNORE_TABLE REQUIRE RESUME RETURNED_SQLSTATE RETURNS ROTATE ROW_COUNT ROW_FORMAT SCHEDULE SCHEMA_NAME SECONDARY_ENGINE SECONDARY_ENGINE_ATTRIBUTE SLOW SNAPSHOT SONAME SOURCE SOURCE_LOG_FILE SOURCE_LOG_POS SQL_AFTER_GTIDS SQL_AFTER_MTS_GAPS SQL_BEFORE_GTIDS SSL STATS_AUTO_RECALC STATS_PERSISTENT STATS_SAMPLE_PAGES STRING SUBCLASS_ORIGIN SUSPEND SWAPS SWITCHES SYSTEM TABLE_NAME TEMPTABLE THREAD_PRIORITY TLS TRADITIONAL TREE TYPE UNDEFINED UNDOFILE UPGRADE USE_FRM VALUE VCPU WRAPPER XID ASSIGN COLON DOT DOUBLE_QUOTED_STRING EQUALS MINUS QUOTED_ID STAR AT_SIGN AT_EMPTY AT_HOST.
%type labeled_statement_start {MyliteStatementKind}
%type permissive_start {MyliteStatementKind}
%type alter_instance_reload_tls_tail {int}
%type alter_instance_reload_channel_tail {int}
%type alter_instance_reload_rollback_tail {int}
%extra_argument {MyliteParseContext *ctx}
%token_destructor { (void)ctx; (void)yypminor; }
%default_destructor { (void)ctx; (void)yypminor; }

%include {
#include "mylite_parser_internal.h"
}

%syntax_error {
  mylite_parser_syntax_error(ctx, yymajor, TOKEN);
}

%parse_failure {
  mylite_parser_failure(ctx);
}

%parse_accept {
  mylite_parser_accept(ctx);
}

input ::= . { (void)ctx; }
input ::= statement_chunks. { (void)ctx; }

statement_chunks ::= statement_chunk.
statement_chunks ::= statement_chunks statement_chunk.

statement_chunk ::= SEMI. { mylite_parser_record_empty_statement(ctx); }
statement_chunk ::= statement SEMI.

statement ::= select_statement.
statement ::= create_statement.
statement ::= drop_statement.
statement ::= alter_statement.
statement ::= rename_statement.
statement ::= truncate_statement.
statement ::= load_statement.
statement ::= start_statement.
statement ::= stop_statement.
statement ::= savepoint_statement.
statement ::= release_statement.
statement ::= lock_statement.
statement ::= unlock_statement.
statement ::= table_admin_statement.
statement ::= plugin_admin_statement.
statement ::= import_statement.
statement ::= cache_statement.
statement ::= kill_statement.
statement ::= deallocate_statement.
statement ::= reset_statement.
statement ::= purge_statement.
statement ::= change_statement.
statement ::= xa_statement.
statement ::= show_statement.
statement ::= describe_statement.
statement ::= explain_statement.
statement ::= use_statement.
statement ::= handler_statement.
statement ::= call_statement.
statement ::= binlog_statement.
statement ::= clone_statement.
statement ::= flush_statement.
statement ::= restart_statement.
statement ::= shutdown_statement.
statement ::= insert_statement.
statement ::= replace_statement.
statement ::= update_statement.
statement ::= delete_statement.
statement ::= with_statement.
statement ::= table_statement.
statement ::= values_statement.
statement ::= prepare_statement.
statement ::= execute_statement.
statement ::= get_statement.
statement ::= signal_statement.
statement ::= begin_statement.
statement ::= commit_statement.
statement ::= rollback_statement.
statement ::= set_statement.
statement ::= grant_statement.
statement ::= revoke_statement.
statement ::= leave_statement.
statement ::= iterate_statement.
statement ::= help_statement.
statement ::= do_statement.
statement ::= if_statement.
statement ::= elseif_statement.
statement ::= return_statement.
statement ::= resignal_statement.
statement ::= while_statement.
statement ::= until_statement.
statement ::= when_statement.
statement ::= open_statement.
statement ::= fetch_statement.
statement ::= close_statement.
statement ::= else_statement.
statement ::= loop_statement.
statement ::= repeat_statement.
statement ::= case_statement.
statement ::= declare_statement.
statement ::= end_statement.
statement ::= parenthesized_statement.
statement ::= LABEL labeled_statement_start(A) statement_tail. {
  mylite_parser_record_statement(ctx, A);
}
statement ::= permissive_start(A) statement_tail. {
  mylite_parser_record_statement(ctx, A);
}

labeled_statement_start(A) ::= BEGIN. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
labeled_statement_start(A) ::= LOOP. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
labeled_statement_start(A) ::= REPEAT. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
labeled_statement_start(A) ::= WHILE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }

select_statement ::= SELECT select_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

select_tail ::= select_expression_start statement_tail.

select_expression_start ::= expression_start.
select_expression_start ::= select_modifiers expression_start.

select_modifiers ::= select_modifier.
select_modifiers ::= select_modifiers select_modifier.

select_modifier ::= ALL.
select_modifier ::= DISTINCT.
select_modifier ::= DISTINCTROW.
select_modifier ::= HIGH_PRIORITY.
select_modifier ::= SQL_BIG_RESULT.
select_modifier ::= SQL_BUFFER_RESULT.
select_modifier ::= SQL_CALC_FOUND_ROWS.
select_modifier ::= SQL_SMALL_RESULT.
select_modifier ::= STRAIGHT_JOIN.

create_statement ::= CREATE create_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

create_tail ::= create_table_prefix create_if_not_exists_tail cache_table_ref create_table_tail.
create_tail ::= AGGREGATE FUNCTION cache_table_ref create_udf_tail.
create_tail ::= INDEX create_index_name create_index_using_tail ON cache_table_ref create_index_tail.
create_tail ::= create_index_kind INDEX create_index_name create_index_using_tail ON cache_table_ref create_index_tail.
create_tail ::= LOGFILE create_logfile_group cache_name_part create_logfile_group_tail.
create_tail ::= RESOURCE create_resource_group cache_name_part create_resource_group_tail.
create_tail ::= SPATIAL create_reference create_system create_if_not_exists_tail cache_name_part create_srs_attribute_start statement_token create_options_tail.
create_tail ::= OR REPLACE SPATIAL create_reference create_system cache_name_part create_srs_attribute_start statement_token create_options_tail.
create_tail ::= SERVER cache_name_part create_server_tail.
create_tail ::= TABLESPACE cache_name_part create_tablespace_tail.
create_tail ::= UNDO TABLESPACE cache_name_part create_undo_tablespace_tail.
create_tail ::= create_database_kind create_if_not_exists_tail cache_name_part create_database_tail.
create_tail ::= ROLE create_if_not_exists_tail drop_account_list.
create_tail ::= USER create_if_not_exists_tail drop_account_name create_user_tail.
create_tail ::= VIEW cache_table_ref view_column_tail view_body.
create_tail ::= create_prefixed_view_tail.
create_tail ::= create_definer_clause create_definer_object_tail.
create_tail ::= EVENT create_if_not_exists_tail cache_table_ref create_event_body.
create_tail ::= TRIGGER create_if_not_exists_tail cache_table_ref create_trigger_body.
create_tail ::= FUNCTION create_if_not_exists_tail cache_table_ref create_function_tail.
create_tail ::= PROCEDURE create_if_not_exists_tail cache_table_ref create_procedure_tail.

create_index_kind ::= UNIQUE.
create_index_kind ::= FULLTEXT.
create_index_kind ::= SPATIAL.

create_index_name ::= cache_name_part.

create_index_using_tail ::= .
create_index_using_tail ::= USING cache_name_part.
create_index_using_tail ::= create_index_type cache_name_part.

create_index_type ::= TYPE.

create_index_tail ::= LP create_table_definition_tokens RP create_options_tail.

create_table_prefix ::= TABLE.
create_table_prefix ::= TEMPORARY TABLE.

create_database_kind ::= DATABASE.
create_database_kind ::= SCHEMA.

create_database_tail ::= .
create_database_tail ::= create_database_option_start create_options_tail.

create_table_tail ::= LP create_table_definition_tokens RP create_options_tail.
create_table_tail ::= LIKE cache_table_ref.
create_table_tail ::= SELECT select_tail.
create_table_tail ::= AS SELECT select_tail.
create_table_tail ::= TABLE table_statement_target table_query_tail.
create_table_tail ::= AS TABLE table_statement_target table_query_tail.
create_table_tail ::= VALUES values_row_list values_query_tail.
create_table_tail ::= AS VALUES values_row_list values_query_tail.
create_table_tail ::= WITH with_recursive_tail with_cte_list with_query_body.
create_table_tail ::= AS WITH with_recursive_tail with_cte_list with_query_body.
create_table_tail ::= AS query_parenthesized_body.
create_table_tail ::= AS LP LP dml_write_query_start required_statement_tail.
create_table_tail ::= create_table_tail_option_start required_statement_tail.

create_table_definition_tokens ::= .
create_table_definition_tokens ::= create_table_definition_tokens create_table_definition_token.

create_table_definition_token ::= ATOM.
create_table_definition_token ::= LABEL.
create_table_definition_token ::= keyword.
create_table_definition_token ::= DOT.
create_table_definition_token ::= COMMA.
create_table_definition_token ::= LP create_table_definition_tokens RP.
create_table_definition_token ::= LB.
create_table_definition_token ::= RB.
create_table_definition_token ::= LC.
create_table_definition_token ::= RC.

create_table_tail_option_start ::= AUTO_INCREMENT.
create_table_tail_option_start ::= AVG_ROW_LENGTH.
create_table_tail_option_start ::= CHARACTER.
create_table_tail_option_start ::= CHARSET.
create_table_tail_option_start ::= CHECKSUM.
create_table_tail_option_start ::= COLLATE.
create_table_tail_option_start ::= COMMENT.
create_table_tail_option_start ::= COMPRESSION.
create_table_tail_option_start ::= CONNECTION.
create_table_tail_option_start ::= DATA.
create_table_tail_option_start ::= DEFAULT.
create_table_tail_option_start ::= DELAY_KEY_WRITE.
create_table_tail_option_start ::= ENCRYPTION.
create_table_tail_option_start ::= ENGINE.
create_table_tail_option_start ::= ENGINE_ATTRIBUTE.
create_table_tail_option_start ::= INDEX.
create_table_tail_option_start ::= INSERT_METHOD.
create_table_tail_option_start ::= KEY_BLOCK_SIZE.
create_table_tail_option_start ::= MAX_ROWS.
create_table_tail_option_start ::= MIN_ROWS.
create_table_tail_option_start ::= PACK_KEYS.
create_table_tail_option_start ::= PASSWORD.
create_table_tail_option_start ::= PARTITION.
create_table_tail_option_start ::= ROW_FORMAT.
create_table_tail_option_start ::= SECONDARY_ENGINE_ATTRIBUTE.
create_table_tail_option_start ::= STATS_AUTO_RECALC.
create_table_tail_option_start ::= STATS_PERSISTENT.
create_table_tail_option_start ::= STATS_SAMPLE_PAGES.
create_table_tail_option_start ::= STORAGE.
create_table_tail_option_start ::= TABLESPACE.
create_table_tail_option_start ::= UNION.

create_database_option_start ::= DEFAULT.
create_database_option_start ::= CHARACTER.
create_database_option_start ::= CHARSET.
create_database_option_start ::= COLLATE.
create_database_option_start ::= ENCRYPTION.

create_udf_tail ::= create_returns create_udf_return_type create_soname ATOM.

create_returns ::= RETURNS.

create_udf_return_type ::= DECIMAL.
create_udf_return_type ::= INT.
create_udf_return_type ::= INTEGER.
create_udf_return_type ::= REAL.
create_udf_return_type ::= STRING.

create_soname ::= SONAME.

create_prefixed_view_tail ::= create_view_prefix VIEW cache_table_ref view_column_tail view_body.

create_view_prefix ::= OR REPLACE create_view_optional_options.
create_view_prefix ::= create_view_options.

create_view_optional_options ::= .
create_view_optional_options ::= create_view_options.

create_view_options ::= create_view_algorithm create_view_definer_tail create_view_sql_security_tail.
create_view_options ::= create_definer_clause create_view_sql_security_tail.
create_view_options ::= create_view_sql_security.

create_view_algorithm ::= ALGORITHM diagnostics_equals create_view_algorithm_name.

create_view_algorithm_name ::= MERGE.
create_view_algorithm_name ::= TEMPTABLE.
create_view_algorithm_name ::= UNDEFINED.

create_view_definer_tail ::= .
create_view_definer_tail ::= create_definer_clause.

create_view_sql_security_tail ::= .
create_view_sql_security_tail ::= create_view_sql_security.

create_view_sql_security ::= SQL SECURITY create_view_security_kind.

create_view_security_kind ::= DEFINER.
create_view_security_kind ::= INVOKER.

view_body ::= view_as SELECT select_tail.
view_body ::= view_as TABLE table_statement_target table_query_tail.
view_body ::= view_as VALUES values_row_list values_query_tail.
view_body ::= view_as WITH with_recursive_tail with_cte_list with_query_body.
view_body ::= view_as query_parenthesized_body.

view_as ::= AS.

view_column_tail ::= .
view_column_tail ::= LP view_column_list RP.

view_column_list ::= cache_name_part.
view_column_list ::= view_column_list import_comma cache_name_part.

create_definer_clause ::= DEFINER diagnostics_equals create_definer_account.

create_definer_account ::= drop_account_name.
create_definer_account ::= current_user_ref.

create_definer_object_tail ::= EVENT create_if_not_exists_tail cache_table_ref create_event_body.
create_definer_object_tail ::= TRIGGER create_if_not_exists_tail cache_table_ref create_trigger_body.
create_definer_object_tail ::= FUNCTION create_if_not_exists_tail cache_table_ref create_function_tail.
create_definer_object_tail ::= PROCEDURE create_if_not_exists_tail cache_table_ref create_procedure_tail.

create_event_body ::= ON create_schedule event_schedule_start create_event_before_do create_event_do event_statement_start statement_tail.

create_schedule ::= SCHEDULE.

event_schedule_start ::= AT.
event_schedule_start ::= EVERY.

create_event_before_do ::= .
create_event_before_do ::= create_event_before_do create_event_before_do_token.

create_event_before_do_nested ::= .
create_event_before_do_nested ::= create_event_before_do_nested create_event_before_do_token.

create_event_before_do_token ::= ATOM.
create_event_before_do_token ::= LABEL.
create_event_before_do_token ::= ON.
create_event_before_do_token ::= COMMA.
create_event_before_do_token ::= LP create_event_before_do_nested RP.

create_trigger_body ::= create_trigger_time create_trigger_event ON cache_table_ref FOR create_each ROW create_trigger_statement_tail.

create_trigger_time ::= BEFORE.
create_trigger_time ::= AFTER.

create_trigger_event ::= INSERT.
create_trigger_event ::= UPDATE.
create_trigger_event ::= DELETE.

create_each ::= EACH.

create_trigger_statement_tail ::= create_trigger_statement_start statement_tail.
create_trigger_statement_tail ::= create_trigger_order create_trigger_statement_start statement_tail.

create_trigger_order ::= FOLLOWS cache_name_part.
create_trigger_order ::= PRECEDES cache_name_part.

create_trigger_statement_start ::= BEGIN.
create_trigger_statement_start ::= DELETE.
create_trigger_statement_start ::= INSERT.
create_trigger_statement_start ::= LABEL.
create_trigger_statement_start ::= SELECT.
create_trigger_statement_start ::= UPDATE.

create_function_tail ::= create_udf_tail.
create_function_tail ::= routine_signature create_returns create_function_return_tail create_function_body_start statement_tail.

create_function_return_tail ::= create_function_return_token.
create_function_return_tail ::= create_function_return_tail create_function_return_token.

create_function_return_nested ::= .
create_function_return_nested ::= create_function_return_nested create_function_return_token.

create_function_return_token ::= ATOM.
create_function_return_token ::= LABEL.
create_function_return_token ::= BINARY.
create_function_return_token ::= CHARACTER.
create_function_return_token ::= CHARSET.
create_function_return_token ::= COLLATION.
create_function_return_token ::= COMMA.
create_function_return_token ::= DATA.
create_function_return_token ::= DEFAULT.
create_function_return_token ::= DEFINER.
create_function_return_token ::= DOT.
create_function_return_token ::= NO.
create_function_return_token ::= READ.
create_function_return_token ::= SECURITY.
create_function_return_token ::= SET.
create_function_return_token ::= SQL.
create_function_return_token ::= USER.
create_function_return_token ::= LP create_function_return_nested RP.

create_function_body_start ::= BEGIN.
create_function_body_start ::= RETURN.

create_procedure_tail ::= routine_signature create_procedure_tail_start statement_tail.

create_procedure_tail_start ::= keyword_not_select_clause.
create_procedure_tail_start ::= LABEL.
create_procedure_tail_start ::= COMMENT.
create_procedure_tail_start ::= CONTAINS.
create_procedure_tail_start ::= DETERMINISTIC.
create_procedure_tail_start ::= LANGUAGE.
create_procedure_tail_start ::= MODIFIES.
create_procedure_tail_start ::= NOT.
create_procedure_tail_start ::= QUOTED_ID.
create_procedure_tail_start ::= READS.

routine_signature ::= LP routine_signature_tokens RP.

routine_signature_tokens ::= .
routine_signature_tokens ::= routine_signature_tokens routine_signature_token.

routine_signature_token ::= ATOM.
routine_signature_token ::= LABEL.
routine_signature_token ::= keyword.
routine_signature_token ::= COMMA.
routine_signature_token ::= LP routine_signature_tokens RP.
routine_signature_token ::= LB.
routine_signature_token ::= RB.
routine_signature_token ::= LC.
routine_signature_token ::= RC.

create_if_not_exists_tail ::= .
create_if_not_exists_tail ::= IF create_not reset_exists.

create_not ::= NOT.

create_options_tail ::= .
create_options_tail ::= create_options_tail statement_token.

create_options_required_tail ::= statement_token.
create_options_required_tail ::= create_options_required_tail statement_token.

create_user_tail ::= .
create_user_tail ::= create_user_tail statement_token.

create_resource_group ::= GROUP.

create_resource_group_tail ::= create_resource_type create_resource_group_options_tail.

create_resource_group_options_tail ::= resource_group_vcpu_tail resource_group_thread_priority_tail resource_group_state_tail.

resource_group_vcpu_tail ::= .
resource_group_vcpu_tail ::= resource_group_vcpu_clause.

resource_group_vcpu_clause ::= VCPU resource_group_optional_equals resource_group_vcpu_list.

resource_group_vcpu_list ::= resource_group_vcpu_spec.
resource_group_vcpu_list ::= resource_group_vcpu_list import_comma resource_group_vcpu_spec.

resource_group_vcpu_spec ::= ATOM.
resource_group_vcpu_spec ::= ATOM MINUS ATOM.

resource_group_thread_priority_tail ::= .
resource_group_thread_priority_tail ::= resource_group_thread_priority_clause.

resource_group_thread_priority_clause ::= THREAD_PRIORITY resource_group_optional_equals resource_group_signed_atom.

resource_group_state_tail ::= .
resource_group_state_tail ::= ENABLE.
resource_group_state_tail ::= DISABLE.

resource_group_optional_equals ::= .
resource_group_optional_equals ::= diagnostics_equals.

resource_group_signed_atom ::= ATOM.

create_resource_type ::= create_type_marker diagnostics_equals create_resource_type_value.

create_type_marker ::= TYPE.

create_resource_type_value ::= USER.
create_resource_type_value ::= SYSTEM.

create_logfile_group ::= GROUP.

create_logfile_group_tail ::= create_add create_undofile ATOM create_options_tail.

create_add ::= ADD.

create_datafile ::= DATAFILE.

create_undofile ::= UNDOFILE.

create_tablespace_tail ::= .
create_tablespace_tail ::= ADD create_options_tail.
create_tablespace_tail ::= ENGINE create_options_tail.
create_tablespace_tail ::= ENCRYPTION create_options_tail.
create_tablespace_tail ::= ENGINE_ATTRIBUTE create_options_tail.
create_tablespace_tail ::= FILE_BLOCK_SIZE create_options_tail.

create_undo_tablespace_tail ::= create_add create_datafile ATOM create_options_tail.

create_server_tail ::= create_foreign DATA create_wrapper cache_name_part create_server_options.

create_foreign ::= FOREIGN.

create_wrapper ::= WRAPPER.

create_server_options ::= create_options_marker LP create_server_option_tokens RP.

create_options_marker ::= OPTIONS.

create_server_option_tokens ::= .
create_server_option_tokens ::= create_server_option_tokens create_server_option_token.

create_server_option_token ::= ATOM.
create_server_option_token ::= LABEL.
create_server_option_token ::= keyword.
create_server_option_token ::= COMMA.
create_server_option_token ::= LP.
create_server_option_token ::= LB.
create_server_option_token ::= RB.
create_server_option_token ::= LC.
create_server_option_token ::= RC.

create_reference ::= REFERENCE.

create_system ::= SYSTEM.

create_srs_attribute_start ::= DEFINITION.
create_srs_attribute_start ::= DESCRIPTION.
create_srs_attribute_start ::= NAME.
create_srs_attribute_start ::= ORGANIZATION.

drop_statement ::= DROP drop_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

drop_tail ::= USER drop_if_exists_tail drop_user_ref_list drop_account_trailing_tail.
drop_tail ::= ROLE drop_if_exists_tail drop_account_list drop_account_trailing_tail.
drop_tail ::= drop_table_prefix drop_if_exists_tail drop_name_list drop_restrict_tail.
drop_tail ::= LOGFILE drop_logfile_group cache_name_part drop_tablespace_engine_tail.
drop_tail ::= RESOURCE drop_resource_group cache_name_part drop_resource_force_tail.
drop_tail ::= SPATIAL drop_reference drop_system drop_if_exists_tail cache_name_part.
drop_tail ::= UNDO TABLESPACE cache_name_part drop_tablespace_engine_tail.
drop_tail ::= PREPARE prepared_statement_name.
drop_tail ::= INDEX drop_index_name ON cache_table_ref drop_index_options_tail.
drop_tail ::= drop_database_kind drop_if_exists_tail cache_name_part.
drop_tail ::= drop_routine_kind drop_if_exists_tail cache_table_ref.
drop_tail ::= EVENT drop_if_exists_tail cache_table_ref.
drop_tail ::= TRIGGER drop_if_exists_tail cache_table_ref.
drop_tail ::= VIEW drop_if_exists_tail drop_name_list drop_restrict_tail.
drop_tail ::= SERVER drop_if_exists_tail cache_name_part.
drop_tail ::= TABLESPACE cache_name_part drop_tablespace_engine_tail.

drop_table_kind ::= TABLE.
drop_table_kind ::= TABLES.

drop_table_prefix ::= drop_table_kind.
drop_table_prefix ::= TEMPORARY drop_table_kind.

drop_routine_kind ::= FUNCTION.
drop_routine_kind ::= PROCEDURE.

drop_account_list ::= drop_account_name.
drop_account_list ::= drop_account_list COMMA drop_account_name.

drop_user_ref_list ::= drop_user_ref.
drop_user_ref_list ::= drop_user_ref_list COMMA drop_user_ref.

drop_user_ref ::= drop_account_name.
drop_user_ref ::= current_user_ref.

drop_account_name ::= drop_account_principal.
drop_account_name ::= drop_account_principal drop_account_host.

drop_account_trailing_tail ::= .
drop_account_trailing_tail ::= DOUBLE_QUOTED_STRING.

drop_account_principal ::= drop_account_ident.

drop_account_host ::= AT_HOST drop_host_dot_tail.
drop_account_host ::= AT_SIGN drop_host_name.
drop_account_host ::= AT_EMPTY.

drop_host_name ::= drop_account_ident drop_host_dot_tail.

drop_host_dot_tail ::= .
drop_host_dot_tail ::= drop_host_dot_tail DOT drop_account_ident.

drop_account_ident ::= ATOM.
drop_account_ident ::= LABEL.
drop_account_ident ::= MASTER.
drop_account_ident ::= ROLE.
drop_account_ident ::= USER.

drop_name_list ::= cache_table_ref.
drop_name_list ::= drop_name_list COMMA cache_table_ref.

drop_restrict_tail ::= .
drop_restrict_tail ::= RESTRICT.
drop_restrict_tail ::= CASCADE.

drop_resource_group ::= GROUP.

drop_logfile_group ::= GROUP.

drop_resource_force_tail ::= .
drop_resource_force_tail ::= FORCE.

drop_reference ::= REFERENCE.

drop_system ::= SYSTEM.

drop_tablespace_engine_tail ::= .
drop_tablespace_engine_tail ::= ENGINE cache_name_part.

drop_index_name ::= cache_name_part.

drop_index_options_tail ::= .
drop_index_options_tail ::= drop_index_options_tail statement_token.

drop_database_kind ::= DATABASE.
drop_database_kind ::= SCHEMA.

drop_if_exists_tail ::= .
drop_if_exists_tail ::= IF reset_exists.

alter_statement ::= ALTER alter_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

alter_tail ::= TABLE cache_table_ref.
alter_tail ::= TABLE cache_table_ref alter_table_tail.
alter_tail ::= LOGFILE create_logfile_group cache_name_part create_logfile_group_tail.
alter_tail ::= RESOURCE create_resource_group cache_name_part alter_resource_group_actions.
alter_tail ::= SERVER cache_name_part create_server_options.
alter_tail ::= TABLESPACE cache_name_part alter_tablespace_action.
alter_tail ::= UNDO TABLESPACE cache_name_part alter_undo_tablespace_action.
alter_tail ::= USER drop_if_exists_tail alter_user_target create_user_tail.
alter_tail ::= EVENT cache_table_ref alter_event_action.
alter_tail ::= alter_routine_kind cache_table_ref create_options_tail.
alter_tail ::= alter_database_kind cache_name_part alter_database_tail.
alter_tail ::= alter_database_kind CHARACTER create_options_required_tail.
alter_tail ::= alter_database_kind CHARSET create_options_required_tail.
alter_tail ::= alter_database_kind alter_database_read_tail.
alter_tail ::= VIEW cache_table_ref view_column_tail view_body.
alter_tail ::= alter_prefixed_view_tail.
alter_tail ::= create_definer_clause alter_definer_object_tail.
alter_tail ::= INSTANCE alter_instance_action.

alter_database_kind ::= DATABASE.
alter_database_kind ::= SCHEMA.

alter_database_tail ::= alter_database_option_start create_options_required_tail.

alter_database_option_start ::= DEFAULT.
alter_database_option_start ::= CHARACTER.
alter_database_option_start ::= CHARSET.
alter_database_option_start ::= COLLATE.
alter_database_option_start ::= ENCRYPTION.

alter_routine_kind ::= FUNCTION.
alter_routine_kind ::= PROCEDURE.

alter_table_tail ::= alter_table_action_start create_options_tail.

alter_table_action_start ::= ADD.
alter_table_action_start ::= ALGORITHM.
alter_table_action_start ::= ALTER.
alter_table_action_start ::= ANALYZE.
alter_table_action_start ::= AUTO_INCREMENT.
alter_table_action_start ::= AVG_ROW_LENGTH.
alter_table_action_start ::= CHANGE.
alter_table_action_start ::= CHARACTER.
alter_table_action_start ::= CHECK.
alter_table_action_start ::= CHECKSUM.
alter_table_action_start ::= COALESCE.
alter_table_action_start ::= COMMENT.
alter_table_action_start ::= CONVERT.
alter_table_action_start ::= DEFAULT.
alter_table_action_start ::= DISABLE.
alter_table_action_start ::= DISCARD.
alter_table_action_start ::= DROP.
alter_table_action_start ::= ENABLE.
alter_table_action_start ::= ENCRYPTION.
alter_table_action_start ::= ENGINE.
alter_table_action_start ::= EXCHANGE.
alter_table_action_start ::= FORCE.
alter_table_action_start ::= IMPORT.
alter_table_action_start ::= INSERT_METHOD.
alter_table_action_start ::= KEY_BLOCK_SIZE.
alter_table_action_start ::= LOCK.
alter_table_action_start ::= MAX_ROWS.
alter_table_action_start ::= MIN_ROWS.
alter_table_action_start ::= MODIFY.
alter_table_action_start ::= OPTIMIZE.
alter_table_action_start ::= ORDER.
alter_table_action_start ::= PACK_KEYS.
alter_table_action_start ::= PARTITION.
alter_table_action_start ::= REBUILD.
alter_table_action_start ::= REMOVE.
alter_table_action_start ::= REORGANIZE.
alter_table_action_start ::= RENAME.
alter_table_action_start ::= REPAIR.
alter_table_action_start ::= ROW_FORMAT.
alter_table_action_start ::= SECONDARY_ENGINE.
alter_table_action_start ::= STORAGE.
alter_table_action_start ::= TABLESPACE.
alter_table_action_start ::= TRUNCATE.
alter_table_action_start ::= UNION.

alter_prefixed_view_tail ::= alter_view_prefix VIEW cache_table_ref view_column_tail view_body.

alter_view_prefix ::= create_view_algorithm create_view_definer_tail create_view_sql_security_tail.
alter_view_prefix ::= create_definer_clause create_view_sql_security_tail.
alter_view_prefix ::= create_view_sql_security.

alter_definer_object_tail ::= EVENT cache_table_ref alter_event_action.

alter_event_action ::= ON alter_event_on_tail.
alter_event_action ::= RENAME TO cache_table_ref create_options_tail.
alter_event_action ::= COMMENT create_options_tail.
alter_event_action ::= DISABLE create_options_tail.
alter_event_action ::= ENABLE create_options_tail.
alter_event_action ::= DO event_statement_start statement_tail.

create_event_do ::= DO.

alter_event_on_tail ::= SCHEDULE event_schedule_start statement_tail.
alter_event_on_tail ::= COMPLETION alter_event_completion_start statement_tail.

alter_event_completion_start ::= NOT.
alter_event_completion_start ::= PRESERVE.

event_statement_start ::= keyword_not_select_clause.
event_statement_start ::= LABEL.
event_statement_start ::= COMMENT.
event_statement_start ::= DETERMINISTIC.
event_statement_start ::= LANGUAGE.
event_statement_start ::= MODIFIES.
event_statement_start ::= NOT.
event_statement_start ::= QUOTED_ID.
event_statement_start ::= READS.

alter_resource_group_actions ::= resource_group_vcpu_clause resource_group_thread_priority_tail alter_resource_group_state_tail.
alter_resource_group_actions ::= resource_group_thread_priority_clause alter_resource_group_state_tail.
alter_resource_group_actions ::= alter_resource_group_state_clause.

alter_resource_group_state_tail ::= .
alter_resource_group_state_tail ::= alter_resource_group_state_clause.

alter_resource_group_state_clause ::= ENABLE.
alter_resource_group_state_clause ::= DISABLE alter_resource_group_force_tail.

alter_resource_group_force_tail ::= .
alter_resource_group_force_tail ::= FORCE.

alter_tablespace_action ::= RENAME TO cache_name_part.
alter_tablespace_action ::= ADD create_datafile ATOM create_options_tail.
alter_tablespace_action ::= DROP create_datafile ATOM create_options_tail.
alter_tablespace_action ::= alter_tablespace_option_name diagnostics_equals alter_tablespace_option_value.

alter_tablespace_option_name ::= ATOM.

alter_tablespace_option_value ::= statement_token.
alter_tablespace_option_value ::= alter_tablespace_option_value statement_token.

alter_undo_tablespace_action ::= SET alter_undo_tablespace_state drop_tablespace_engine_tail.
create_trigger_statement_start ::= SET.

alter_undo_tablespace_state ::= ACTIVE.
alter_undo_tablespace_state ::= INACTIVE.

alter_instance_action ::= ENABLE alter_instance_innodb alter_instance_redo_log.
alter_instance_action ::= DISABLE alter_instance_innodb alter_instance_redo_log.
alter_instance_action ::= ROTATE alter_instance_master_key_kind MASTER KEY.
alter_instance_action ::= RELOAD alter_instance_reload_target.

alter_instance_innodb ::= INNODB.

alter_instance_redo_log ::= REDO_LOG.

alter_instance_master_key_kind ::= alter_instance_innodb.
alter_instance_master_key_kind ::= BINLOG.

alter_instance_reload_target ::= TLS alter_instance_reload_tls_tail.
alter_instance_reload_target ::= KEYRING.

alter_instance_reload_tls_tail(A) ::= alter_instance_reload_channel_tail(B) alter_instance_reload_rollback_tail(C). {
  A = B || C;
}

alter_instance_reload_channel_tail(A) ::= . {
  A = 0;
}
alter_instance_reload_channel_tail(A) ::= reset_channel_tail. {
  A = 1;
}

alter_instance_reload_rollback_tail(A) ::= . {
  A = 0;
}
alter_instance_reload_rollback_tail(A) ::= NO ROLLBACK ON ERROR. {
  A = 1;
}
create_trigger_statement_start ::= ROLLBACK.

alter_user_target ::= drop_account_name.

rename_statement ::= RENAME rename_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

rename_tail ::= rename_table_kind rename_table_pairs.
rename_tail ::= USER rename_user_pairs.

rename_table_kind ::= TABLE.
rename_table_kind ::= TABLES.

rename_table_pairs ::= rename_table_pair.
rename_table_pairs ::= rename_table_pairs import_comma rename_table_pair.

rename_table_pair ::= cache_table_ref TO cache_table_ref.

rename_user_pairs ::= rename_user_pair.
rename_user_pairs ::= rename_user_pairs import_comma rename_user_pair.

rename_user_pair ::= rename_user_account TO rename_user_account.

rename_user_account ::= rename_user_token.
rename_user_account ::= rename_user_account rename_user_token.
rename_user_account ::= current_user_ref.

rename_user_token ::= ATOM.
rename_user_token ::= LABEL.
rename_user_token ::= MASTER.
rename_user_token ::= ROLE.
rename_user_token ::= USER.

current_user_ref ::= CURRENT_USER.
current_user_ref ::= CURRENT_USER LP RP.

truncate_statement ::= TRUNCATE truncate_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

truncate_tail ::= TABLE truncate_table_ref.
truncate_tail ::= truncate_table_ref.

truncate_table_ref ::= truncate_table_part.
truncate_table_ref ::= truncate_table_part ATOM truncate_table_part.

truncate_table_part ::= ATOM.
truncate_table_part ::= LABEL.
truncate_table_part ::= USER.

load_statement ::= LOAD load_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

load_tail ::= DATA load_file_tail.
load_tail ::= XML load_file_tail.
load_tail ::= INDEX INTO CACHE cache_table_list load_index_tail.

load_file_tail ::= load_infile ATOM load_file_table_tail.
load_file_tail ::= load_file_modifier load_file_tail.

load_file_modifier ::= LOW_PRIORITY.
load_file_modifier ::= LOCAL.
load_file_modifier ::= CONCURRENT.

load_infile ::= INFILE.

load_file_table_tail ::= INTO TABLE cache_table_ref load_file_options_tail.
load_file_table_tail ::= load_duplicate_handling INTO TABLE cache_table_ref load_file_options_tail.

load_file_options_tail ::= .
load_file_options_tail ::= load_file_options_tail statement_token.

load_duplicate_handling ::= IGNORE.
load_duplicate_handling ::= REPLACE.

load_index_tail ::= .
load_index_tail ::= load_index_partition.
load_index_tail ::= IGNORE load_leaves.
load_index_tail ::= load_index_partition IGNORE load_leaves.

load_index_partition ::= load_partition LP load_partition_names RP.

load_partition ::= PARTITION.

load_partition_names ::= load_partition_name.
load_partition_names ::= load_partition_names import_comma load_partition_name.

load_partition_name ::= ATOM.
load_partition_name ::= LABEL.
load_partition_name ::= ALL.

load_leaves ::= LEAVES.

start_statement ::= START start_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

start_tail ::= TRANSACTION start_transaction_tail.
start_tail ::= REPLICA start_replica_tail.
start_tail ::= SLAVE start_replica_tail.
start_tail ::= GROUP_REPLICATION start_group_replication_tail.

start_transaction_tail ::= .
start_transaction_tail ::= transaction_characteristics.

transaction_characteristics ::= transaction_characteristic.
transaction_characteristics ::= transaction_characteristics import_comma transaction_characteristic.

transaction_characteristic ::= READ transaction_access_mode.
transaction_characteristic ::= WITH transaction_consistent transaction_snapshot.

alter_database_read_tail ::= READ create_options_required_tail.
alter_database_option_start ::= READ.

transaction_access_mode ::= ONLY.
transaction_access_mode ::= WRITE.

transaction_consistent ::= CONSISTENT.
transaction_snapshot ::= SNAPSHOT.

start_replica_tail ::= start_thread_tail start_until_tail start_connection_tail show_channel_tail.

start_thread_tail ::= .
start_thread_tail ::= start_thread_list.

start_thread_list ::= start_thread_type.
start_thread_list ::= start_thread_list import_comma start_thread_type.

start_thread_type ::= IO_THREAD.
start_thread_type ::= SQL_THREAD.

start_until_tail ::= .
start_until_tail ::= UNTIL start_until_spec.

start_until_spec ::= SQL_BEFORE_GTIDS start_option_equals ATOM.
start_until_spec ::= SQL_AFTER_GTIDS start_option_equals ATOM.
start_until_spec ::= SOURCE_LOG_FILE start_option_equals ATOM import_comma SOURCE_LOG_POS start_option_equals ATOM.
start_until_spec ::= RELAY_LOG_FILE start_option_equals ATOM import_comma RELAY_LOG_POS start_option_equals ATOM.
start_until_spec ::= SQL_AFTER_MTS_GAPS.

start_connection_tail ::= start_user_option start_password_option start_default_auth_option start_plugin_dir_option.

start_user_option ::= .
start_user_option ::= USER start_option_equals ATOM.

start_password_option ::= .
start_password_option ::= PASSWORD start_option_equals ATOM.

start_default_auth_option ::= .
start_default_auth_option ::= DEFAULT_AUTH start_option_equals ATOM.

start_plugin_dir_option ::= .
start_plugin_dir_option ::= PLUGIN_DIR start_option_equals ATOM.

start_group_replication_tail ::= .
start_group_replication_tail ::= start_group_replication_options.

start_group_replication_options ::= start_group_replication_option.
start_group_replication_options ::= start_group_replication_options import_comma start_group_replication_option.

start_group_replication_option ::= USER start_option_equals ATOM.
start_group_replication_option ::= PASSWORD start_option_equals ATOM.
start_group_replication_option ::= DEFAULT_AUTH start_option_equals ATOM.

start_option_equals ::= EQUALS.

stop_statement ::= STOP stop_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

stop_tail ::= REPLICA stop_replica_tail.
stop_tail ::= SLAVE stop_replica_tail.
stop_tail ::= GROUP_REPLICATION.

stop_replica_tail ::= start_thread_tail show_channel_tail.

savepoint_statement ::= SAVEPOINT savepoint_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

savepoint_name ::= ATOM.
savepoint_name ::= LABEL.

release_statement ::= RELEASE SAVEPOINT savepoint_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}
create_trigger_statement_start ::= RELEASE.

lock_statement ::= LOCK lock_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

lock_tail ::= lock_table_kind lock_table_list.
lock_tail ::= INSTANCE FOR lock_backup.

lock_backup ::= BACKUP.

lock_table_kind ::= TABLE.
lock_table_kind ::= TABLES.

lock_table_list ::= lock_table_spec.
lock_table_list ::= lock_table_list import_comma lock_table_spec.

lock_table_spec ::= cache_table_ref lock_table_alias lock_type.
lock_table_spec ::= cache_table_ref lock_type.

lock_table_alias ::= handler_as lock_alias.
lock_table_alias ::= lock_alias.

lock_alias ::= ATOM.
lock_alias ::= LABEL.

lock_type ::= READ.
lock_type ::= READ LOCAL.
lock_type ::= WRITE.
lock_type ::= LOW_PRIORITY WRITE.

unlock_statement ::= UNLOCK unlock_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

unlock_tail ::= unlock_table_kind.
unlock_tail ::= INSTANCE.

unlock_table_kind ::= TABLE.
unlock_table_kind ::= TABLES.

table_admin_statement ::= ANALYZE analyze_table_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= CHECK check_table_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= CHECKSUM checksum_table_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= OPTIMIZE optimize_table_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= REPAIR repair_table_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

analyze_table_tail ::= table_admin_optional_binlog table_admin_table_keyword table_admin_table_list analyze_table_options.

analyze_table_options ::= .
analyze_table_options ::= UPDATE analyze_histogram_marker ON table_admin_column_list analyze_histogram_value_tail.
analyze_table_options ::= DROP analyze_histogram_marker ON table_admin_column_list.

analyze_histogram_marker ::= HISTOGRAM.

analyze_histogram_value_tail ::= .
analyze_histogram_value_tail ::= WITH ATOM analyze_buckets_marker.
analyze_histogram_value_tail ::= USING DATA ATOM.

analyze_buckets_marker ::= BUCKETS.

table_admin_column_list ::= table_admin_column.
table_admin_column_list ::= table_admin_column_list import_comma table_admin_column.

table_admin_column ::= ATOM.
table_admin_column ::= LABEL.

check_table_tail ::= table_admin_table_keyword table_admin_table_list check_table_options.

check_table_options ::= .
check_table_options ::= check_table_option_list.

check_table_option_list ::= check_table_option.
check_table_option_list ::= check_table_option_list check_table_option.

check_table_option ::= FOR check_upgrade_marker.
check_table_option ::= EXTENDED.
check_table_option ::= QUICK.
check_table_option ::= CHANGED.
check_table_option ::= FAST.
check_table_option ::= MEDIUM.

check_upgrade_marker ::= UPGRADE.

checksum_table_tail ::= TABLE table_admin_table_list checksum_table_option.

checksum_table_option ::= .
checksum_table_option ::= QUICK.
checksum_table_option ::= EXTENDED.

optimize_table_tail ::= table_admin_optional_binlog table_admin_table_keyword table_admin_table_list.

repair_table_tail ::= table_admin_optional_binlog table_admin_table_keyword table_admin_table_list repair_table_options.

repair_table_options ::= .
repair_table_options ::= repair_table_option_list.

repair_table_option_list ::= repair_table_option.
repair_table_option_list ::= repair_table_option_list repair_table_option.

repair_table_option ::= QUICK.
repair_table_option ::= EXTENDED.
repair_table_option ::= USE_FRM.

table_admin_optional_binlog ::= .
table_admin_optional_binlog ::= LOCAL.
table_admin_optional_binlog ::= NO_WRITE_TO_BINLOG.

table_admin_table_keyword ::= TABLE.
table_admin_table_keyword ::= TABLES.

table_admin_table_list ::= cache_table_ref.
table_admin_table_list ::= table_admin_table_list import_comma cache_table_ref.

plugin_admin_statement ::= INSTALL plugin_admin_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
plugin_admin_statement ::= UNINSTALL plugin_uninstall_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

plugin_admin_tail ::= PLUGIN plugin_name plugin_soname ATOM.
plugin_admin_tail ::= COMPONENT component_file_list component_install_tail.

plugin_uninstall_tail ::= PLUGIN plugin_name.
plugin_uninstall_tail ::= COMPONENT component_file_list.

component_install_tail ::= .
component_install_tail ::= SET component_install_assignments.

component_install_assignments ::= component_install_assignment.
component_install_assignments ::= component_install_assignments import_comma component_install_assignment.

component_install_assignment ::= component_install_assignment_scope component_install_name diagnostics_equals component_install_value.

component_install_assignment_scope ::= .
component_install_assignment_scope ::= GLOBAL.
component_install_assignment_scope ::= PERSIST.

component_install_name ::= cache_name_part.
component_install_name ::= cache_name_part DOT cache_name_part.

component_install_value ::= component_install_value_token.
component_install_value ::= component_install_value component_install_value_token.

component_install_value_token ::= ATOM.
component_install_value_token ::= LABEL.
component_install_value_token ::= keyword.
component_install_value_token ::= DOT.
component_install_value_token ::= LP component_install_value_inner RP.
component_install_value_token ::= LB component_install_value_inner RB.
component_install_value_token ::= LC component_install_value_inner RC.

component_install_value_inner ::= .
component_install_value_inner ::= component_install_value_inner component_install_value_inner_token.

component_install_value_inner_token ::= ATOM.
component_install_value_inner_token ::= LABEL.
component_install_value_inner_token ::= keyword.
component_install_value_inner_token ::= DOT.
component_install_value_inner_token ::= COMMA.
component_install_value_inner_token ::= LP component_install_value_inner RP.
component_install_value_inner_token ::= LB component_install_value_inner RB.
component_install_value_inner_token ::= LC component_install_value_inner RC.

component_file_list ::= component_file.
component_file_list ::= component_file_list import_comma component_file.

component_file ::= ATOM.

plugin_soname ::= SONAME.

plugin_name ::= ATOM.
plugin_name ::= LABEL.

import_statement ::= IMPORT TABLE FROM import_file_list. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

import_file_list ::= import_file.
import_file_list ::= import_file_list import_comma import_file.

import_comma ::= COMMA.

import_file ::= ATOM.

cache_statement ::= CACHE INDEX cache_table_list IN cache_keycache. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

cache_table_list ::= cache_table_spec.
cache_table_list ::= cache_table_list import_comma cache_table_spec.

cache_table_spec ::= cache_table_ref.
cache_table_spec ::= cache_table_ref KEY cache_key_list.

cache_table_ref ::= cache_name_part.
cache_table_ref ::= cache_name_part DOT cache_name_part.

cache_name_part ::= ATOM.
cache_name_part ::= CASCADE.
cache_name_part ::= COMPONENT.
cache_name_part ::= COUNT.
cache_name_part ::= DATABASE.
cache_name_part ::= LABEL.
cache_name_part ::= DEFAULT.
cache_name_part ::= ENGINE.
cache_name_part ::= EVENTS.
cache_name_part ::= FIRST.
cache_name_part ::= FULL.
cache_name_part ::= GRANTS.
cache_name_part ::= PLUGIN.
cache_name_part ::= PROCESSLIST.
cache_name_part ::= RESTRICT.
cache_name_part ::= TABLES.
cache_name_part ::= TABLESPACE.
cache_name_part ::= TRIGGERS.
cache_name_part ::= USER.
cache_name_part ::= VARIABLES.

cache_key_list ::= LP cache_key_names RP.

cache_key_names ::= cache_key_name.
cache_key_names ::= cache_key_names import_comma cache_key_name.

cache_key_name ::= ATOM.
cache_key_name ::= LABEL.
cache_key_name ::= DEFAULT.

cache_keycache ::= ATOM.
cache_keycache ::= LABEL.
cache_keycache ::= DEFAULT.

kill_statement ::= KILL kill_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

kill_tail ::= kill_target.
kill_tail ::= kill_mode kill_target.

kill_mode ::= CONNECTION.
kill_mode ::= QUERY.

kill_target ::= ATOM.
kill_target ::= LABEL.

deallocate_statement ::= DEALLOCATE PREPARE prepared_statement_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_PREPARED);
}

reset_statement ::= RESET reset_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

reset_tail ::= BINARY LOGS AND reset_gtids reset_binary_logs_tail.
reset_tail ::= MASTER.
reset_tail ::= PERSIST reset_persist_tail.
reset_tail ::= REPLICA reset_replica_tail.
reset_tail ::= SLAVE reset_replica_tail.

reset_binary_logs_tail ::= .
reset_binary_logs_tail ::= TO ATOM.

reset_gtids ::= GTIDS.

reset_persist_tail ::= .
reset_persist_tail ::= reset_persist_target.
reset_persist_tail ::= IF reset_exists reset_persist_target.

reset_exists ::= EXISTS.

reset_persist_target ::= reset_persist_name.
reset_persist_target ::= reset_persist_name DOT reset_persist_name.

reset_persist_name ::= ATOM.
reset_persist_name ::= LABEL.
reset_persist_name ::= DEFAULT.

reset_replica_tail ::= .
reset_replica_tail ::= ALL.
reset_replica_tail ::= reset_channel_tail.
reset_replica_tail ::= ALL reset_channel_tail.

reset_channel_tail ::= FOR reset_channel reset_channel_name.

reset_channel ::= CHANNEL.

reset_channel_name ::= ATOM.
reset_channel_name ::= LABEL.

purge_statement ::= PURGE purge_log_kind LOGS purge_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

purge_tail ::= TO purge_log_name.
purge_tail ::= purge_before expression_start statement_tail.

purge_before ::= BEFORE.

purge_log_kind ::= BINARY.
purge_log_kind ::= MASTER.

purge_log_name ::= ATOM.
purge_log_name ::= LABEL.

change_statement ::= CHANGE change_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

change_tail ::= MASTER TO change_options change_for_channel_tail.
change_tail ::= REPLICATION FILTER change_replication_filters change_for_channel_tail.
change_tail ::= REPLICATION change_replication_source TO change_options change_for_channel_tail.

change_replication_source ::= SOURCE.

change_replication_filters ::= change_replication_filter.
change_replication_filters ::= change_replication_filters import_comma change_replication_filter.

change_replication_filter ::= change_replication_filter_name diagnostics_equals LP change_replication_filter_contents RP.

change_replication_filter_name ::= REPLICATE_DO_DB.
change_replication_filter_name ::= REPLICATE_IGNORE_DB.
change_replication_filter_name ::= REPLICATE_DO_TABLE.
change_replication_filter_name ::= REPLICATE_IGNORE_TABLE.
change_replication_filter_name ::= REPLICATE_WILD_DO_TABLE.
change_replication_filter_name ::= REPLICATE_WILD_IGNORE_TABLE.
change_replication_filter_name ::= REPLICATE_REWRITE_DB.

change_replication_filter_contents ::= .
change_replication_filter_contents ::= change_replication_filter_contents change_replication_filter_token.

change_replication_filter_token ::= ATOM.
change_replication_filter_token ::= LABEL.
change_replication_filter_token ::= keyword.
change_replication_filter_token ::= DOT.
change_replication_filter_token ::= COMMA.
change_replication_filter_token ::= LP change_replication_filter_contents RP.

change_options ::= change_option.
change_options ::= change_options import_comma change_option.

change_option ::= change_option_name diagnostics_equals change_option_value.

change_option_name ::= ATOM.

change_option_value ::= change_option_value_token.
change_option_value ::= change_option_value change_option_value_token.

change_option_value_token ::= ATOM.
change_option_value_token ::= LABEL.
change_option_value_token ::= DOT.
change_option_value_token ::= LP change_option_value_contents RP.
change_option_value_token ::= LB.
change_option_value_token ::= RB.
change_option_value_token ::= LC.
change_option_value_token ::= RC.

change_option_value_contents ::= .
change_option_value_contents ::= change_option_value_contents change_option_value_content.

change_option_value_content ::= ATOM.
change_option_value_content ::= LABEL.
change_option_value_content ::= keyword.
change_option_value_content ::= DOT.
change_option_value_content ::= COMMA.
change_option_value_content ::= LP change_option_value_contents RP.
change_option_value_content ::= LB.
change_option_value_content ::= RB.
change_option_value_content ::= LC.
change_option_value_content ::= RC.

change_for_channel_tail ::= .
change_for_channel_tail ::= FOR reset_channel change_channel_name.

change_channel_name ::= ATOM.
change_channel_name ::= LABEL.

xa_statement ::= XA xa_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

xa_tail ::= RECOVER.
xa_tail ::= RECOVER xa_recover_convert xa_recover_xid.
xa_tail ::= START xa_xid xa_start_tail.
xa_tail ::= BEGIN xa_xid xa_start_tail.
xa_tail ::= END xa_xid xa_end_tail.
xa_tail ::= PREPARE xa_xid.
xa_tail ::= COMMIT xa_xid xa_commit_tail.
xa_tail ::= ROLLBACK xa_xid.

xa_recover_convert ::= CONVERT.
xa_recover_xid ::= XID.

xa_xid ::= ATOM.
xa_xid ::= ATOM import_comma ATOM.
xa_xid ::= ATOM import_comma ATOM import_comma ATOM.

xa_start_tail ::= .
xa_start_tail ::= xa_start_option.

xa_start_option ::= JOIN.
xa_start_option ::= RESUME.

xa_end_tail ::= .
xa_end_tail ::= xa_suspend.
xa_end_tail ::= xa_suspend FOR xa_migrate.

xa_suspend ::= SUSPEND.
xa_migrate ::= MIGRATE.

xa_commit_tail ::= .
xa_commit_tail ::= xa_one xa_phase.

xa_one ::= ONE.
xa_phase ::= PHASE.

show_statement ::= SHOW show_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}

show_tail ::= show_full_tail.
show_tail ::= show_scope_prefix show_scoped_kind show_filter_tail.
show_tail ::= COUNT LP show_count_star RP show_count_kind.
show_tail ::= CREATE show_create_tail.
show_tail ::= show_diagnostics_kind show_limit_tail.
show_tail ::= show_simple_kind.
show_tail ::= BINARY LOG STATUS.
show_tail ::= MASTER STATUS.
show_tail ::= SLAVE show_slave_tail.
show_tail ::= GRANTS show_grants_tail.
show_tail ::= show_scoped_kind show_filter_tail.
show_tail ::= show_schema_list_kind show_filter_tail.
show_tail ::= show_table_list_prefix TABLES show_database_tail show_filter_tail.
show_tail ::= TABLE STATUS show_database_tail show_filter_tail.
show_tail ::= OPEN TABLES show_database_tail show_filter_tail.
show_tail ::= EVENTS show_database_tail show_filter_tail.
show_tail ::= TRIGGERS show_database_tail show_filter_tail.
show_tail ::= show_table_metadata_prefix show_column_kind show_table_source show_database_tail show_filter_tail.
show_tail ::= show_table_metadata_prefix show_index_kind show_table_source show_database_tail show_filter_tail.
show_tail ::= CHARACTER SET show_filter_tail.
show_tail ::= CHARSET show_filter_tail.
show_tail ::= COLLATION show_filter_tail.
show_tail ::= ENGINE show_engine_name show_engine_kind.
show_tail ::= BINARY LOGS.
show_tail ::= MASTER LOGS.
show_tail ::= BINLOG EVENTS show_log_events_tail.
show_tail ::= RELAYLOG EVENTS show_log_events_tail.
show_tail ::= show_routine_status_kind STATUS show_filter_tail.
show_tail ::= show_routine_status_kind show_routine_code_marker cache_table_ref.
show_tail ::= STORAGE ENGINES.
show_tail ::= PARSE_TREE show_parse_tree_query.
show_tail ::= PROFILE show_profile_tail.
show_tail ::= REPLICA STATUS show_channel_tail.

show_full_tail ::= FULL show_full_kind.

show_scope_prefix ::= GLOBAL.
show_scope_prefix ::= LOCAL.
show_scope_prefix ::= SESSION.

show_scoped_kind ::= STATUS.
show_scoped_kind ::= VARIABLES.

show_filter_tail ::= .
show_filter_tail ::= LIKE ATOM.
show_filter_tail ::= WHERE expression_start statement_tail.

show_database_tail ::= .
show_database_tail ::= FROM show_database_name.
show_database_tail ::= IN show_database_name.

show_database_name ::= cache_name_part.

show_schema_list_kind ::= DATABASES.
show_schema_list_kind ::= SCHEMAS.

show_table_list_prefix ::= .
show_table_list_prefix ::= EXTENDED.
show_table_list_prefix ::= FULL.
show_table_list_prefix ::= EXTENDED FULL.

show_table_metadata_prefix ::= .
show_table_metadata_prefix ::= EXTENDED.
show_table_metadata_prefix ::= FULL.
show_table_metadata_prefix ::= EXTENDED FULL.

show_column_kind ::= COLUMNS.
show_column_kind ::= FIELDS.

show_index_kind ::= INDEX.
show_index_kind ::= INDEXES.
show_index_kind ::= KEYS.

show_table_source ::= FROM cache_table_ref.
show_table_source ::= IN cache_table_ref.

show_engine_name ::= ATOM.
show_engine_name ::= LABEL.

show_engine_kind ::= STATUS.
show_engine_kind ::= LOGS.
show_engine_kind ::= MUTEX.

show_log_events_tail ::= show_log_file_tail show_log_from_tail show_limit_tail.

show_log_file_tail ::= .
show_log_file_tail ::= IN ATOM.

show_log_from_tail ::= .
show_log_from_tail ::= FROM ATOM.

show_routine_status_kind ::= FUNCTION.
show_routine_status_kind ::= PROCEDURE.

show_routine_code_marker ::= CODE.

show_profile_tail ::= show_profile_type_tail show_profile_for_tail show_limit_tail.

show_profile_type_tail ::= .
show_profile_type_tail ::= show_profile_type_list.

show_profile_type_list ::= show_profile_type.
show_profile_type_list ::= show_profile_type_list import_comma show_profile_type.

show_profile_type ::= ALL.
show_profile_type ::= BLOCK IO.
show_profile_type ::= CONTEXT SWITCHES.
show_profile_type ::= CPU.
show_profile_type ::= IPC.
show_profile_type ::= MEMORY.
show_profile_type ::= PAGE FAULTS.
show_profile_type ::= SOURCE.
show_profile_type ::= SWAPS.

show_profile_for_tail ::= .
show_profile_for_tail ::= FOR QUERY ATOM.

show_parse_tree_query ::= SELECT select_tail.
show_parse_tree_query ::= WITH with_recursive_tail with_cte_list with_query_body.

show_count_star ::= STAR.

show_count_kind ::= ERRORS.
show_count_kind ::= WARNINGS.

show_create_tail ::= show_create_database_kind create_if_not_exists_tail cache_name_part.
show_create_tail ::= show_create_named_kind cache_table_ref.
show_create_tail ::= USER show_create_user_target.

show_create_user_target ::= rename_user_account.

show_create_database_kind ::= DATABASE.
show_create_database_kind ::= SCHEMA.

show_create_named_kind ::= EVENT.
show_create_named_kind ::= FUNCTION.
show_create_named_kind ::= PROCEDURE.
show_create_named_kind ::= TABLE.
show_create_named_kind ::= TRIGGER.
show_create_named_kind ::= VIEW.

show_diagnostics_kind ::= ERRORS.
show_diagnostics_kind ::= WARNINGS.

show_limit_tail ::= .
show_limit_tail ::= LIMIT ATOM.
show_limit_tail ::= LIMIT ATOM import_comma ATOM.
show_limit_tail ::= LIMIT ATOM OFFSET ATOM.

show_simple_kind ::= ENGINES.
show_simple_kind ::= PLUGINS.
show_simple_kind ::= PRIVILEGES.
show_simple_kind ::= PROCESSLIST.
show_simple_kind ::= PROFILES.
show_simple_kind ::= REPLICAS.

show_slave_tail ::= HOSTS.
show_slave_tail ::= STATUS show_channel_tail.

show_channel_tail ::= .
show_channel_tail ::= reset_channel_tail.

show_grants_tail ::= .
show_grants_tail ::= FOR show_grants_principal show_grants_using_tail.

show_grants_using_tail ::= .
show_grants_using_tail ::= USING show_grants_principal_list.

show_grants_principal_list ::= show_grants_principal.
show_grants_principal_list ::= show_grants_principal_list import_comma show_grants_principal.

show_grants_principal ::= rename_user_account.

show_full_kind ::= PROCESSLIST.

describe_statement ::= DESCRIBE describe_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}
describe_statement ::= DESC describe_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}
describe_statement ::= DESCRIBE describe_explain_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}
describe_statement ::= DESC describe_explain_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}

describe_tail ::= describe_table_ref.
describe_tail ::= describe_table_ref describe_column_ref.

describe_table_ref ::= describe_name_part.
describe_table_ref ::= describe_name_part DOT describe_name_part.

describe_column_ref ::= ATOM.
describe_column_ref ::= LABEL.

describe_name_part ::= ATOM.
describe_name_part ::= LABEL.

describe_explain_tail ::= describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_schema_spec describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_clause describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_clause explain_schema_spec describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_json_clause explain_into_tail describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_json_clause explain_into_tail explain_schema_spec describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_analyze_tail.
describe_explain_tail ::= FOR CONNECTION ATOM.

describe_explain_query_start ::= DELETE.
describe_explain_query_start ::= INSERT.
describe_explain_query_start ::= LP.
describe_explain_query_start ::= REPLACE.
describe_explain_query_start ::= SELECT.
describe_explain_query_start ::= TABLE.
describe_explain_query_start ::= UPDATE.
describe_explain_query_start ::= WITH.

explain_statement ::= EXPLAIN explain_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}

explain_tail ::= explain_query_start statement_tail.
explain_tail ::= explain_schema_spec explain_query_start statement_tail.
explain_tail ::= explain_format_clause explain_query_start statement_tail.
explain_tail ::= explain_format_clause explain_schema_spec explain_query_start statement_tail.
explain_tail ::= explain_format_json_clause explain_into_tail explain_query_start statement_tail.
explain_tail ::= explain_format_json_clause explain_into_tail explain_schema_spec explain_query_start statement_tail.
explain_tail ::= explain_analyze_tail.
explain_tail ::= FOR CONNECTION ATOM.

explain_format_clause ::= FORMAT diagnostics_equals explain_format_name.

explain_format_json_clause ::= FORMAT diagnostics_equals JSON.

explain_into_tail ::= INTO set_variable_name.

explain_schema_spec ::= FOR explain_schema_kind cache_name_part.

explain_schema_kind ::= SCHEMA.
explain_schema_kind ::= DATABASE.

explain_analyze_tail ::= ANALYZE explain_analyze_format_tail explain_schema_tail explain_analyze_query_start statement_tail.

explain_analyze_format_tail ::= .
explain_analyze_format_tail ::= FORMAT diagnostics_equals TREE.

explain_schema_tail ::= .
explain_schema_tail ::= explain_schema_spec.

explain_analyze_query_start ::= DELETE.
explain_analyze_query_start ::= INSERT.
explain_analyze_query_start ::= LP.
explain_analyze_query_start ::= REPLACE.
explain_analyze_query_start ::= SELECT.
explain_analyze_query_start ::= UPDATE.
explain_analyze_query_start ::= WITH.

explain_format_name ::= JSON.
explain_format_name ::= TRADITIONAL.
explain_format_name ::= TREE.

explain_query_start ::= DELETE.
explain_query_start ::= INSERT.
explain_query_start ::= LP.
explain_query_start ::= REPLACE.
explain_query_start ::= SELECT.
explain_query_start ::= UPDATE.
explain_query_start ::= WITH.
explain_query_start ::= ATOM.
explain_query_start ::= LABEL.

use_statement ::= USE use_target. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

use_target ::= ATOM.
use_target ::= LABEL.
use_target ::= FIRST.

handler_statement ::= HANDLER handler_name handler_operation. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

handler_name ::= handler_target.
handler_name ::= handler_name handler_target.

handler_target ::= ATOM.
handler_target ::= LABEL.

handler_operation ::= OPEN.
handler_operation ::= OPEN handler_alias.
handler_operation ::= OPEN handler_as handler_alias.
handler_operation ::= READ handler_read_tail.
handler_operation ::= CLOSE.

handler_as ::= AS.

handler_alias ::= ATOM.
handler_alias ::= LABEL.

handler_read_tail ::= handler_read_direction handler_read_suffix.
handler_read_tail ::= handler_read_index handler_read_direction handler_read_suffix.
handler_read_tail ::= handler_read_index handler_read_operator handler_read_tuple handler_read_suffix.

handler_read_index ::= ATOM.
handler_read_index ::= LABEL.

handler_read_direction ::= FIRST.
handler_read_direction ::= NEXT.
handler_read_direction ::= PREV.
handler_read_direction ::= LAST.

handler_read_operator ::= EQUALS.

handler_read_tuple ::= LP values_row_contents RP.

handler_read_suffix ::= .
handler_read_suffix ::= WHERE expression_start statement_tail.
handler_read_suffix ::= handler_limit_tail.

handler_limit_tail ::= LIMIT ATOM.
handler_limit_tail ::= LIMIT ATOM import_comma ATOM.
handler_limit_tail ::= LIMIT ATOM OFFSET ATOM.

call_statement ::= CALL call_name call_arguments. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}
create_trigger_statement_start ::= CALL.

call_name ::= call_target.
call_name ::= call_target ATOM call_target.

call_target ::= ATOM.
call_target ::= LABEL.

call_arguments ::= .
call_arguments ::= LP RP.
call_arguments ::= LP call_argument_contents RP.
call_arguments ::= LP(A) call_argument_contents. {
  mylite_parser_require_permissive(ctx, A);
}

call_argument_contents ::= call_argument_token.
call_argument_contents ::= call_argument_contents call_argument_token.

call_argument_token ::= ATOM.
call_argument_token ::= LABEL.
call_argument_token ::= keyword.
call_argument_token ::= DOT.
call_argument_token ::= COMMA.
call_argument_token ::= LP RP.
call_argument_token ::= LP call_argument_contents RP.
call_argument_token ::= LB.
call_argument_token ::= RB.
call_argument_token ::= LC.
call_argument_token ::= RC.

binlog_statement ::= BINLOG binlog_payload. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

binlog_payload ::= ATOM.
binlog_payload ::= LABEL.

clone_statement ::= CLONE clone_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

clone_tail ::= INSTANCE FROM clone_instance_source clone_identified BY clone_password clone_remote_tail.
clone_tail ::= LOCAL DATA clone_directory clone_directory_equals_tail clone_directory_path.

clone_instance_source ::= clone_account_name clone_colon ATOM.

clone_account_name ::= drop_account_principal clone_account_host.

clone_account_host ::= AT_HOST drop_host_dot_tail.
clone_account_host ::= AT_SIGN drop_host_name.

clone_colon ::= COLON.

clone_identified ::= IDENTIFIED.

clone_password ::= ATOM.

clone_remote_tail ::= clone_data_directory_tail clone_require_ssl_tail.

clone_data_directory_tail ::= .
clone_data_directory_tail ::= DATA clone_directory clone_directory_equals_tail clone_directory_path.

clone_directory_equals_tail ::= .
clone_directory_equals_tail ::= diagnostics_equals.

clone_require_ssl_tail ::= .
clone_require_ssl_tail ::= REQUIRE clone_ssl.

clone_ssl ::= SSL.
clone_ssl ::= NO SSL.

clone_directory ::= DIRECTORY.

clone_directory_path ::= ATOM.

flush_statement ::= FLUSH flush_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

flush_tail ::= flush_simple_list.
flush_tail ::= flush_table_kind flush_table_tail.
flush_tail ::= LOCAL flush_table_kind flush_table_tail.
flush_tail ::= NO_WRITE_TO_BINLOG flush_table_kind flush_table_tail.
flush_tail ::= BINARY LOGS.
flush_tail ::= ENGINE LOGS.
flush_tail ::= ERROR LOGS.
flush_tail ::= GENERAL LOGS.
flush_tail ::= RELAY LOGS show_channel_tail.
flush_tail ::= SLOW LOGS.

flush_simple_list ::= flush_simple_kind.
flush_simple_list ::= flush_simple_list import_comma flush_simple_kind.

flush_simple_kind ::= HOSTS.
flush_simple_kind ::= LOGS.
flush_simple_kind ::= OPTIMIZER_COSTS.
flush_simple_kind ::= PRIVILEGES.
flush_simple_kind ::= STATUS.
flush_simple_kind ::= USER_RESOURCES.

flush_table_kind ::= TABLE.
flush_table_kind ::= TABLES.

flush_table_tail ::= .
flush_table_tail ::= flush_table_list.
flush_table_tail ::= flush_table_modifier.
flush_table_tail ::= flush_table_list flush_table_modifier.

flush_table_list ::= cache_table_ref.
flush_table_list ::= flush_table_list import_comma cache_table_ref.

flush_table_modifier ::= WITH READ LOCK.
flush_table_modifier ::= FOR flush_export.

flush_export ::= EXPORT.

restart_statement ::= RESTART. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

shutdown_statement ::= SHUTDOWN. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

insert_statement ::= INSERT insert_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_INSERT);
}

insert_tail ::= dml_insert_target dml_write_body.
insert_tail ::= dml_insert_modifiers dml_insert_target dml_write_body.

dml_insert_modifiers ::= dml_insert_modifier.
dml_insert_modifiers ::= dml_insert_modifiers dml_insert_modifier.

dml_insert_modifier ::= DELAYED.
dml_insert_modifier ::= HIGH_PRIORITY.
dml_insert_modifier ::= IGNORE.
dml_insert_modifier ::= LOW_PRIORITY.

dml_insert_target ::= cache_table_ref.
dml_insert_target ::= INTO cache_table_ref.

replace_statement ::= REPLACE replace_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLACE);
}

replace_tail ::= dml_replace_target dml_write_body.
replace_tail ::= dml_replace_modifiers dml_replace_target dml_write_body.

dml_replace_modifiers ::= dml_replace_modifier.
dml_replace_modifiers ::= dml_replace_modifiers dml_replace_modifier.

dml_replace_modifier ::= DELAYED.
dml_replace_modifier ::= LOW_PRIORITY.

dml_replace_target ::= cache_table_ref.
dml_replace_target ::= INTO cache_table_ref.

dml_write_body ::= dml_write_payload.

dml_write_payload ::= LP dml_write_column_tokens RP dml_write_after_column_list.
dml_write_payload ::= LP dml_write_query_start dml_write_parenthesized_query_tail RP dml_write_after_parenthesized_query.
dml_write_payload ::= SET update_assignment_start.
dml_write_payload ::= SELECT select_tail.
dml_write_payload ::= TABLE table_statement_target table_query_tail.
dml_write_payload ::= WITH with_recursive_tail with_cte_list with_query_body.
dml_write_payload ::= dml_write_partition_clause dml_write_payload.
dml_write_payload ::= dml_write_start required_statement_tail.

dml_write_column_tokens ::= .
dml_write_column_tokens ::= dml_write_column_tokens dml_write_column_token.

dml_write_column_token ::= ATOM.
dml_write_column_token ::= LABEL.
dml_write_column_token ::= CONNECTION.
dml_write_column_token ::= COUNT.
dml_write_column_token ::= DATA.
dml_write_column_token ::= NO.
dml_write_column_token ::= PLUGIN.
dml_write_column_token ::= QUERY.
dml_write_column_token ::= START.
dml_write_column_token ::= STATUS.
dml_write_column_token ::= USER.
dml_write_column_token ::= XML.
dml_write_column_token ::= COMMA.
dml_write_column_token ::= LP dml_write_column_tokens RP.

dml_write_partition_clause ::= PARTITION LP delete_partition_list RP.

dml_write_after_column_list ::= dml_write_start required_statement_tail.
dml_write_after_column_list ::= SET update_assignment_start.
dml_write_after_column_list ::= SELECT select_tail.
dml_write_after_column_list ::= TABLE table_statement_target table_query_tail.
dml_write_after_column_list ::= WITH with_recursive_tail with_cte_list with_query_body.
dml_write_after_column_list ::= LP dml_write_query_start dml_write_parenthesized_query_tail RP dml_write_after_parenthesized_query.

dml_write_query_start ::= SELECT.
dml_write_query_start ::= TABLE.
dml_write_query_start ::= VALUES.
dml_write_query_start ::= WITH.

dml_write_parenthesized_query_tail ::= .
dml_write_parenthesized_query_tail ::= dml_write_parenthesized_query_tail dml_write_parenthesized_query_token.

dml_write_parenthesized_query_token ::= ATOM.
dml_write_parenthesized_query_token ::= LABEL.
dml_write_parenthesized_query_token ::= keyword.
dml_write_parenthesized_query_token ::= COMMA.
dml_write_parenthesized_query_token ::= LP dml_write_parenthesized_query_tail RP.
dml_write_parenthesized_query_token ::= LB.
dml_write_parenthesized_query_token ::= RB.
dml_write_parenthesized_query_token ::= LC.
dml_write_parenthesized_query_token ::= RC.

dml_write_after_parenthesized_query ::= .
dml_write_after_parenthesized_query ::= UNION dml_write_union_tail.
dml_write_after_parenthesized_query ::= ORDER BY expression_start statement_tail.
dml_write_after_parenthesized_query ::= LIMIT ATOM statement_tail.
dml_write_after_parenthesized_query ::= ON dml_write_duplicate_tail.

dml_write_union_tail ::= SELECT select_tail.
dml_write_union_tail ::= values_union_option SELECT select_tail.
dml_write_union_tail ::= TABLE table_statement_target table_query_tail.
dml_write_union_tail ::= values_union_option TABLE table_statement_target table_query_tail.
dml_write_union_tail ::= VALUES values_row_list values_query_tail.
dml_write_union_tail ::= values_union_option VALUES values_row_list values_query_tail.
dml_write_union_tail ::= WITH with_recursive_tail with_cte_list with_query_body.
dml_write_union_tail ::= values_union_option WITH with_recursive_tail with_cte_list with_query_body.
dml_write_union_tail ::= query_parenthesized_body.
dml_write_union_tail ::= values_union_option query_parenthesized_body.

query_parenthesized_body ::= LP dml_write_query_start dml_write_parenthesized_query_tail RP query_parenthesized_tail.

query_parenthesized_tail ::= .
query_parenthesized_tail ::= UNION dml_write_union_tail.
query_parenthesized_tail ::= ORDER BY expression_start statement_tail.
query_parenthesized_tail ::= LIMIT ATOM statement_tail.

dml_write_duplicate_tail ::= DUPLICATE KEY UPDATE update_assignment_start.

dml_write_start ::= VALUES.
dml_write_start ::= VALUE.

update_statement ::= UPDATE update_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UPDATE);
}

update_tail ::= dml_update_table_reference_tokens SET update_assignment_start.
update_tail ::= dml_update_modifiers dml_update_table_reference_tokens SET update_assignment_start.

update_assignment_start ::= update_assignment_target set_assignment_operator set_value_start statement_tail.

update_assignment_target ::= update_assignment_part.
update_assignment_target ::= update_assignment_target DOT update_assignment_part.

update_assignment_part ::= cache_name_part.
update_assignment_part ::= CHECKSUM.
update_assignment_part ::= CONNECTION.
update_assignment_part ::= DATA.
update_assignment_part ::= NO.
update_assignment_part ::= PASSWORD.
update_assignment_part ::= QUERY.
update_assignment_part ::= STATUS.
update_assignment_part ::= XML.

dml_update_modifiers ::= dml_update_modifier.
dml_update_modifiers ::= dml_update_modifiers dml_update_modifier.

dml_update_modifier ::= IGNORE.
dml_update_modifier ::= LOW_PRIORITY.

dml_update_table_reference_tokens ::= dml_update_table_reference_head.
dml_update_table_reference_tokens ::= dml_update_table_reference_tokens dml_update_table_reference_continuation.

dml_update_table_reference_head ::= cache_name_part.
dml_update_table_reference_head ::= LP dml_update_table_reference_nested RP.

dml_update_table_reference_continuation ::= dml_update_table_reference_head.
dml_update_table_reference_continuation ::= dml_update_table_reference_keyword.
dml_update_table_reference_continuation ::= COMMA.

dml_update_table_reference_keyword ::= AND.
dml_update_table_reference_keyword ::= BY.
dml_update_table_reference_keyword ::= FOR.
dml_update_table_reference_keyword ::= GROUP_REPLICATION.
dml_update_table_reference_keyword ::= IGNORE.
dml_update_table_reference_keyword ::= INDEX.
dml_update_table_reference_keyword ::= KEY.
dml_update_table_reference_keyword ::= ON.
dml_update_table_reference_keyword ::= ORDER.
dml_update_table_reference_keyword ::= STRAIGHT_JOIN.
dml_update_table_reference_keyword ::= USE.
dml_update_table_reference_keyword ::= USING.

dml_update_table_reference_nested ::= .
dml_update_table_reference_nested ::= dml_update_table_reference_nested dml_update_table_reference_nested_token.

dml_update_table_reference_nested_token ::= ATOM.
dml_update_table_reference_nested_token ::= LABEL.
dml_update_table_reference_nested_token ::= keyword.
dml_update_table_reference_nested_token ::= COMMA.
dml_update_table_reference_nested_token ::= LP dml_update_table_reference_nested RP.
dml_update_table_reference_nested_token ::= LB.
dml_update_table_reference_nested_token ::= RB.
dml_update_table_reference_nested_token ::= LC.
dml_update_table_reference_nested_token ::= RC.

delete_statement ::= DELETE delete_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DELETE);
}

delete_tail ::= delete_core.
delete_tail ::= dml_delete_modifiers delete_core.

dml_delete_modifiers ::= dml_delete_modifier.
dml_delete_modifiers ::= dml_delete_modifiers dml_delete_modifier.

dml_delete_modifier ::= IGNORE.
dml_delete_modifier ::= LOW_PRIORITY.
dml_delete_modifier ::= QUICK.

delete_core ::= FROM dml_delete_table_list delete_after_from_tail.
delete_core ::= dml_delete_table_list FROM dml_delete_source_start statement_tail.
delete_core ::= FROM dml_delete_table_list USING delete_using_tail.

delete_using_tail ::= dml_update_table_reference_tokens.
delete_using_tail ::= dml_update_table_reference_tokens WHERE expression_start statement_tail.

dml_delete_table_list ::= cache_table_ref.
dml_delete_table_list ::= dml_delete_table_list import_comma cache_table_ref.

dml_delete_source_start ::= cache_table_ref.
dml_delete_source_start ::= LP.

delete_after_from_tail ::= .
delete_after_from_tail ::= WHERE expression_start statement_tail.
delete_after_from_tail ::= ORDER BY expression_start statement_tail.
delete_after_from_tail ::= LIMIT ATOM.
delete_after_from_tail ::= delete_partition_clause delete_after_from_tail.

delete_partition_clause ::= PARTITION LP delete_partition_list RP.

delete_partition_list ::= cache_name_part.
delete_partition_list ::= delete_partition_list import_comma cache_name_part.

with_statement ::= WITH with_recursive_tail with_cte_list with_query_body. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

with_recursive_tail ::= .
with_recursive_tail ::= RECURSIVE.

with_cte_list ::= with_cte.
with_cte_list ::= with_cte_list import_comma with_cte.

with_cte ::= with_cte_name with_cte_column_tail with_cte_as LP with_cte_body RP.

with_cte_name ::= cache_name_part.

with_cte_column_tail ::= .
with_cte_column_tail ::= LP with_cte_column_list RP.

with_cte_column_list ::= cache_name_part.
with_cte_column_list ::= with_cte_column_list import_comma cache_name_part.

with_cte_as ::= AS.

with_cte_body ::= with_cte_body_token.
with_cte_body ::= with_cte_body with_cte_body_token.

with_cte_body_nested ::= .
with_cte_body_nested ::= with_cte_body_nested with_cte_body_token.

with_cte_body_token ::= ATOM.
with_cte_body_token ::= LABEL.
with_cte_body_token ::= keyword.
with_cte_body_token ::= COMMA.
with_cte_body_token ::= LP with_cte_body_nested RP.
with_cte_body_token ::= LB.
with_cte_body_token ::= RB.
with_cte_body_token ::= LC.
with_cte_body_token ::= RC.

with_query_body ::= SELECT select_tail.
with_query_body ::= TABLE table_statement_target table_query_tail.
with_query_body ::= VALUES values_row_list values_query_tail.
with_query_body ::= DELETE delete_tail.
with_query_body ::= INSERT insert_tail.
with_query_body ::= REPLACE replace_tail.
with_query_body ::= UPDATE update_tail.
with_query_body ::= query_parenthesized_body.

table_statement ::= TABLE table_statement_target table_query_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

table_statement_target ::= cache_table_ref.

table_query_tail ::= .
table_query_tail ::= table_limit.

table_limit ::= LIMIT ATOM table_offset_tail.

table_offset_tail ::= .
table_offset_tail ::= OFFSET ATOM.

values_statement ::= VALUES values_row_list values_query_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

values_row_list ::= values_row.
values_row_list ::= values_row_list import_comma values_row.

values_row ::= ROW LP values_row_contents RP.

values_row_contents ::= .
values_row_contents ::= values_row_contents values_row_token.

values_row_token ::= ATOM.
values_row_token ::= LABEL.
values_row_token ::= keyword.
values_row_token ::= COMMA.
values_row_token ::= LP values_row_contents RP.
values_row_token ::= LB.
values_row_token ::= RB.
values_row_token ::= LC.
values_row_token ::= RC.

values_query_tail ::= .
values_query_tail ::= UNION values_union_tail.
values_query_tail ::= ORDER BY values_order_list.
values_query_tail ::= values_limit_tail.

values_union_tail ::= SELECT select_tail.
values_union_tail ::= values_union_option SELECT select_tail.
values_union_tail ::= TABLE table_statement_target table_query_tail.
values_union_tail ::= values_union_option TABLE table_statement_target table_query_tail.
values_union_tail ::= VALUES values_row_list values_query_tail.
values_union_tail ::= values_union_option VALUES values_row_list values_query_tail.
values_union_tail ::= WITH with_recursive_tail with_cte_list with_query_body.
values_union_tail ::= values_union_option WITH with_recursive_tail with_cte_list with_query_body.
values_union_tail ::= query_parenthesized_body.
values_union_tail ::= values_union_option query_parenthesized_body.

values_union_option ::= ALL.
values_union_option ::= DISTINCT.

values_order_list ::= values_order_item.
values_order_list ::= values_order_list import_comma values_order_item.

values_order_item ::= values_order_expression values_order_direction.

values_order_expression ::= ATOM.
values_order_expression ::= LABEL.
values_order_expression ::= LP values_row_contents RP.

values_order_direction ::= .
values_order_direction ::= DESC.
values_order_direction ::= ASC.

values_limit_tail ::= LIMIT ATOM.
values_limit_tail ::= LIMIT ATOM import_comma ATOM.
values_limit_tail ::= LIMIT ATOM OFFSET ATOM.

prepare_statement ::= PREPARE prepared_statement_name FROM prepare_source. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_PREPARED);
}

prepare_source ::= ATOM.

execute_statement ::= EXECUTE prepared_statement_name execute_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_PREPARED);
}

execute_tail ::= .
execute_tail ::= USING execute_using_list.

execute_using_list ::= execute_using_arg.
execute_using_list ::= execute_using_list import_comma execute_using_arg.

execute_using_arg ::= ATOM.

prepared_statement_name ::= ATOM.
prepared_statement_name ::= LABEL.

get_statement ::= GET DIAGNOSTICS diagnostics_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}
dml_write_column_token ::= DIAGNOSTICS.

diagnostics_tail ::= diagnostics_statement_items.
diagnostics_tail ::= CONDITION diagnostics_condition_value diagnostics_condition_items.

diagnostics_statement_items ::= diagnostics_statement_item.
diagnostics_statement_items ::= diagnostics_statement_items import_comma diagnostics_statement_item.

diagnostics_statement_item ::= diagnostics_target diagnostics_equals diagnostics_statement_item_name.

diagnostics_condition_items ::= diagnostics_condition_item.
diagnostics_condition_items ::= diagnostics_condition_items import_comma diagnostics_condition_item.

diagnostics_condition_item ::= diagnostics_target diagnostics_equals diagnostics_condition_item_name.

diagnostics_target ::= ATOM.
diagnostics_target ::= LABEL.

diagnostics_condition_value ::= ATOM.
diagnostics_condition_value ::= LABEL.

diagnostics_equals ::= EQUALS.

diagnostics_statement_item_name ::= NUMBER.
diagnostics_statement_item_name ::= ROW_COUNT.

diagnostics_condition_item_name ::= CATALOG_NAME.
diagnostics_condition_item_name ::= CLASS_ORIGIN.
diagnostics_condition_item_name ::= COLUMN_NAME.
diagnostics_condition_item_name ::= CONSTRAINT_CATALOG.
diagnostics_condition_item_name ::= CONSTRAINT_NAME.
diagnostics_condition_item_name ::= CONSTRAINT_SCHEMA.
diagnostics_condition_item_name ::= CURSOR_NAME.
diagnostics_condition_item_name ::= MESSAGE_TEXT.
diagnostics_condition_item_name ::= MYSQL_ERRNO.
diagnostics_condition_item_name ::= RETURNED_SQLSTATE.
diagnostics_condition_item_name ::= SCHEMA_NAME.
diagnostics_condition_item_name ::= SUBCLASS_ORIGIN.
diagnostics_condition_item_name ::= TABLE_NAME.

signal_statement ::= SIGNAL signal_condition_value signal_set_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

signal_condition_value ::= signal_named_condition.
signal_condition_value ::= SQLSTATE ATOM.

signal_named_condition ::= ATOM.
signal_named_condition ::= LABEL.

signal_set_tail ::= .
signal_set_tail ::= SET signal_information_items.

signal_information_items ::= signal_information_item.
signal_information_items ::= signal_information_items import_comma signal_information_item.

signal_information_item ::= signal_condition_item_name diagnostics_equals signal_information_value.

signal_condition_item_name ::= CATALOG_NAME.
signal_condition_item_name ::= CLASS_ORIGIN.
signal_condition_item_name ::= COLUMN_NAME.
signal_condition_item_name ::= CONSTRAINT_CATALOG.
signal_condition_item_name ::= CONSTRAINT_NAME.
signal_condition_item_name ::= CONSTRAINT_SCHEMA.
signal_condition_item_name ::= CURSOR_NAME.
signal_condition_item_name ::= MESSAGE_TEXT.
signal_condition_item_name ::= MYSQL_ERRNO.
signal_condition_item_name ::= SCHEMA_NAME.
signal_condition_item_name ::= SUBCLASS_ORIGIN.
signal_condition_item_name ::= TABLE_NAME.

signal_information_value ::= ATOM.
signal_information_value ::= LABEL.
signal_information_value ::= DEFAULT.

begin_statement ::= BEGIN. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}
begin_statement ::= BEGIN WORK. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

commit_statement ::= COMMIT transaction_end_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

rollback_statement ::= ROLLBACK transaction_end_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}
rollback_statement ::= ROLLBACK rollback_to_savepoint_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

transaction_end_tail ::= .
transaction_end_tail ::= WORK.
transaction_end_tail ::= RELEASE.
transaction_end_tail ::= NO RELEASE.
transaction_end_tail ::= AND CHAIN.
transaction_end_tail ::= AND NO CHAIN.
transaction_end_tail ::= WORK RELEASE.
transaction_end_tail ::= WORK NO RELEASE.
transaction_end_tail ::= WORK AND CHAIN.
transaction_end_tail ::= WORK AND NO CHAIN.
transaction_end_tail ::= AND CHAIN RELEASE.
transaction_end_tail ::= AND CHAIN NO RELEASE.
transaction_end_tail ::= AND NO CHAIN RELEASE.
transaction_end_tail ::= AND NO CHAIN NO RELEASE.
transaction_end_tail ::= WORK AND CHAIN RELEASE.
transaction_end_tail ::= WORK AND CHAIN NO RELEASE.
transaction_end_tail ::= WORK AND NO CHAIN RELEASE.
transaction_end_tail ::= WORK AND NO CHAIN NO RELEASE.

rollback_to_savepoint_tail ::= TO savepoint_reference.
rollback_to_savepoint_tail ::= TO SAVEPOINT savepoint_reference.
rollback_to_savepoint_tail ::= WORK TO savepoint_reference.
rollback_to_savepoint_tail ::= WORK TO SAVEPOINT savepoint_reference.

savepoint_reference ::= ATOM.
savepoint_reference ::= LABEL.

set_statement ::= SET set_names_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET CHARACTER SET set_charset_value statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET CHARSET set_charset_value statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET PASSWORD set_password_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET ROLE set_role_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET DEFAULT ROLE set_role_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET RESOURCE create_resource_group cache_name_part set_resource_group_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET TRANSACTION set_transaction_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET set_assignment. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET set_assignment_scope set_assignment. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET set_assignment_scope TRANSACTION set_transaction_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

set_names_tail ::= NAMES set_charset_value statement_tail.

set_charset_value ::= ATOM.
set_charset_value ::= BINARY.
set_charset_value ::= DEFAULT.
set_charset_value ::= LABEL.

set_resource_group_tail ::= .
set_resource_group_tail ::= FOR set_resource_group_thread_list.

set_resource_group_thread_list ::= set_resource_group_thread.
set_resource_group_thread_list ::= set_resource_group_thread_list import_comma set_resource_group_thread.

set_resource_group_thread ::= ATOM.

set_password_tail ::= set_password_operator set_value_start statement_tail.
set_password_tail ::= FOR set_password_target set_password_operator set_value_start statement_tail.

set_password_operator ::= set_assignment_operator.
set_password_operator ::= TO.

set_password_target ::= drop_account_name.
set_password_target ::= current_user_ref.

set_role_tail ::= set_role_start statement_tail.

set_role_start ::= ALL.
set_role_start ::= ATOM.
set_role_start ::= DEFAULT.
set_role_start ::= LABEL.

set_transaction_tail ::= set_transaction_token statement_tail.

set_transaction_token ::= ATOM.
set_transaction_token ::= READ.
set_transaction_token ::= WITH.
set_transaction_token ::= WRITE.

set_assignment_scope ::= GLOBAL.
set_assignment_scope ::= LOCAL.
set_assignment_scope ::= PERSIST.
set_assignment_scope ::= PERSIST_ONLY.
set_assignment_scope ::= SESSION.

set_assignment ::= set_variable_name set_assignment_operator set_value_start statement_tail.

set_assignment_operator ::= EQUALS.
set_assignment_operator ::= ASSIGN.

set_value_start ::= expression_start.
set_value_start ::= ALL.
set_value_start ::= GLOBAL.
set_value_start ::= LOCAL.
set_value_start ::= NO.
set_value_start ::= ON.
set_value_start ::= PERSIST.
set_value_start ::= PERSIST_ONLY.
set_value_start ::= READ.
set_value_start ::= RESET.
set_value_start ::= SESSION.
set_value_start ::= WRITE.

set_variable_name ::= set_variable_part set_variable_dot_tail.
set_variable_name ::= AT_EMPTY set_variable_part set_variable_dot_tail.
set_variable_name ::= AT_SIGN set_variable_part set_variable_dot_tail.

set_variable_dot_tail ::= .
set_variable_dot_tail ::= set_variable_dot_tail DOT set_variable_part.

set_variable_part ::= ATOM.
set_variable_part ::= AT_HOST.
set_variable_part ::= DEFAULT.
set_variable_part ::= FLUSH.
set_variable_part ::= LABEL.
set_variable_part ::= SQL_BUFFER_RESULT.
set_variable_part ::= USER.

grant_statement ::= GRANT grant_subject_list grant_destination_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

revoke_statement ::= REVOKE revoke_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

grant_destination_tail ::= ON grant_object TO grant_recipient_list.
grant_destination_tail ::= TO grant_recipient_list.

revoke_tail ::= grant_subject_list revoke_destination_tail.
revoke_tail ::= IF reset_exists grant_subject_list revoke_destination_tail.

revoke_destination_tail ::= ON grant_object FROM grant_recipient_list.
revoke_destination_tail ::= FROM grant_recipient_list.

grant_subject_list ::= grant_subject_item.
grant_subject_list ::= grant_subject_list COMMA grant_subject_item.

grant_subject_item ::= grant_subject_part.
grant_subject_item ::= grant_subject_item grant_subject_part.

grant_subject_part ::= grant_subject_token.
grant_subject_part ::= LP grant_subject_parenthesized RP.

grant_subject_parenthesized ::= .
grant_subject_parenthesized ::= grant_subject_parenthesized grant_subject_parenthesized_token.

grant_subject_parenthesized_token ::= ATOM.
grant_subject_parenthesized_token ::= LABEL.
grant_subject_parenthesized_token ::= keyword.
grant_subject_parenthesized_token ::= DOT.
grant_subject_parenthesized_token ::= COMMA.
grant_subject_parenthesized_token ::= LP grant_subject_parenthesized RP.
grant_subject_parenthesized_token ::= LB.
grant_subject_parenthesized_token ::= RB.
grant_subject_parenthesized_token ::= LC.
grant_subject_parenthesized_token ::= RC.

grant_subject_token ::= ALL.
grant_subject_token ::= ALTER.
grant_subject_token ::= ATOM.
grant_subject_token ::= CONNECTION.
grant_subject_token ::= CREATE.
grant_subject_token ::= DATA.
grant_subject_token ::= DATABASE.
grant_subject_token ::= DATABASES.
grant_subject_token ::= DELETE.
grant_subject_token ::= DROP.
grant_subject_token ::= EVENT.
grant_subject_token ::= EXECUTE.
grant_subject_token ::= FUNCTION.
grant_subject_token ::= GRANT.
grant_subject_token ::= GROUP_REPLICATION.
grant_subject_token ::= INDEX.
grant_subject_token ::= INSERT.
grant_subject_token ::= LABEL.
grant_subject_token ::= LOCK.
grant_subject_token ::= NO.
grant_subject_token ::= PRIVILEGES.
grant_subject_token ::= PROCEDURE.
grant_subject_token ::= REPLICATION.
grant_subject_token ::= ROLE.
grant_subject_token ::= SELECT.
grant_subject_token ::= SHOW.
grant_subject_token ::= SHUTDOWN.
grant_subject_token ::= SLAVE.
grant_subject_token ::= TABLE.
grant_subject_token ::= TABLES.
grant_subject_token ::= TABLESPACE.
grant_subject_token ::= TEMPORARY.
grant_subject_token ::= TRIGGER.
grant_subject_token ::= UPDATE.
grant_subject_token ::= USER.
grant_subject_token ::= VIEW.

grant_object ::= grant_object_token.
grant_object ::= grant_object grant_object_token.

grant_object_token ::= ATOM.
grant_object_token ::= LABEL.
grant_object_token ::= DOT.
grant_object_token ::= COMPONENT.
grant_object_token ::= DATABASE.
grant_object_token ::= DATABASES.
grant_object_token ::= DEFAULT.
grant_object_token ::= FUNCTION.
grant_object_token ::= PLUGIN.
grant_object_token ::= PROCEDURE.
grant_object_token ::= TABLE.
grant_object_token ::= TABLES.
grant_object_token ::= TABLESPACE.
grant_object_token ::= USER.

grant_recipient_list ::= grant_recipient.
grant_recipient_list ::= grant_recipient_list COMMA grant_recipient.

grant_recipient ::= drop_account_name grant_recipient_option_tail.

grant_recipient_option_tail ::= .
grant_recipient_option_tail ::= grant_recipient_option_tail grant_recipient_option_token.

grant_recipient_option_nested ::= .
grant_recipient_option_nested ::= grant_recipient_option_nested grant_recipient_option_token.

grant_recipient_option_token ::= ATOM.
grant_recipient_option_token ::= LABEL.
grant_recipient_option_token ::= keyword.
grant_recipient_option_token ::= LP grant_recipient_option_nested RP.
grant_recipient_option_token ::= LB.
grant_recipient_option_token ::= RB.
grant_recipient_option_token ::= LC.
grant_recipient_option_token ::= RC.

leave_statement ::= LEAVE stored_program_label_ref. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

iterate_statement ::= ITERATE stored_program_label_ref. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

stored_program_label_ref ::= ATOM.
stored_program_label_ref ::= LABEL.

help_statement ::= HELP help_topic. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

help_topic ::= ATOM.
help_topic ::= LABEL.
help_topic ::= keyword_not_select_clause.

do_statement ::= DO expression_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

expression_start ::= ATOM.
expression_start ::= LABEL.
expression_start ::= expression_start_keyword.
expression_start ::= LP.
expression_start ::= LB.
expression_start ::= LC.

expression_start_keyword ::= BINARY.
expression_start_keyword ::= CASE.
expression_start_keyword ::= CHARSET.
expression_start_keyword ::= COLLATION.
expression_start_keyword ::= COUNT.
expression_start_keyword ::= CURRENT_USER.
expression_start_keyword ::= DATA.
expression_start_keyword ::= DATABASE.
expression_start_keyword ::= DEFAULT.
expression_start_keyword ::= DEFINER.
expression_start_keyword ::= ENGINE.
expression_start_keyword ::= EVENTS.
expression_start_keyword ::= FIRST.
expression_start_keyword ::= FORMAT.
expression_start_keyword ::= IF.
expression_start_keyword ::= INSERT.
expression_start_keyword ::= LAST.
expression_start_keyword ::= NEXT.
expression_start_keyword ::= PLUGIN.
expression_start_keyword ::= PREV.
expression_start_keyword ::= PRIVILEGES.
expression_start_keyword ::= PROFILE.
expression_start_keyword ::= REPEAT.
expression_start_keyword ::= REPLACE.
expression_start_keyword ::= ROW.
expression_start_keyword ::= STATUS.
expression_start_keyword ::= TRUNCATE.
expression_start_keyword ::= USER.
expression_start_keyword ::= VALUES.

if_statement ::= IF if_condition_start while_condition_tail THEN statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}
if_statement ::= IF LP statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_PERMISSIVE);
}

if_condition_start ::= ATOM.
if_condition_start ::= LABEL.
if_condition_start ::= expression_start_keyword.
if_condition_start ::= LB.
if_condition_start ::= LC.

elseif_statement ::= ELSEIF expression_start while_condition_tail THEN statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

return_statement ::= RETURN expression_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

resignal_statement ::= RESIGNAL resignal_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

resignal_tail ::= .
resignal_tail ::= SET signal_information_items.
resignal_tail ::= signal_condition_value signal_set_tail.

while_statement ::= WHILE expression_start while_condition_tail DO statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

while_condition_tail ::= .
while_condition_tail ::= while_condition_tail while_condition_token.

while_condition_nested ::= .
while_condition_nested ::= while_condition_nested while_condition_nested_token.

while_condition_token ::= ATOM.
while_condition_token ::= LABEL.
while_condition_token ::= while_condition_keyword.
while_condition_token ::= COMMA.
while_condition_token ::= LP while_condition_nested RP.
while_condition_token ::= LB.
while_condition_token ::= RB.
while_condition_token ::= LC.
while_condition_token ::= RC.

while_condition_nested_token ::= ATOM.
while_condition_nested_token ::= LABEL.
while_condition_nested_token ::= keyword.
while_condition_nested_token ::= COMMA.
while_condition_nested_token ::= LP while_condition_nested RP.
while_condition_nested_token ::= LB.
while_condition_nested_token ::= RB.
while_condition_nested_token ::= LC.
while_condition_nested_token ::= RC.

while_condition_keyword ::= ALL.
while_condition_keyword ::= AND.
while_condition_keyword ::= BINARY.
while_condition_keyword ::= CASE.
while_condition_keyword ::= DEFAULT.
while_condition_keyword ::= IN.
while_condition_keyword ::= LIKE.
while_condition_keyword ::= NO.
while_condition_keyword ::= ON.
while_condition_keyword ::= OR.
while_condition_keyword ::= SELECT.
while_condition_keyword ::= VALUES.

until_statement ::= UNTIL expression_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

when_statement ::= WHEN expression_start while_condition_tail THEN statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

open_statement ::= OPEN stored_program_label_ref. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

fetch_statement ::= FETCH stored_program_label_ref INTO fetch_target_list. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

fetch_target_list ::= fetch_target.
fetch_target_list ::= fetch_target_list import_comma fetch_target.

fetch_target ::= ATOM.
fetch_target ::= LABEL.

close_statement ::= CLOSE stored_program_label_ref. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

else_statement ::= ELSE statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

loop_statement ::= LOOP statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

repeat_statement ::= REPEAT statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

case_statement ::= CASE statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

declare_statement ::= DECLARE declare_identifier_list declare_type_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}
declare_statement ::= DECLARE declare_name CONDITION FOR declare_condition_value. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}
declare_statement ::= DECLARE declare_name CURSOR FOR declare_cursor_query_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}
declare_statement ::= DECLARE declare_handler_action declare_handler_keyword FOR declare_handler_conditions declare_handler_statement_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

declare_identifier_list ::= declare_name.
declare_identifier_list ::= declare_identifier_list COMMA declare_name.

declare_name ::= ATOM.
declare_name ::= LABEL.

declare_type_start ::= ATOM.
declare_type_start ::= BINARY.
declare_type_start ::= CHARACTER.
declare_type_start ::= CHARSET.
declare_type_start ::= DATA.
declare_type_start ::= DEFAULT.
declare_type_start ::= LABEL.

declare_condition_value ::= ATOM.
declare_condition_value ::= LABEL.
declare_condition_value ::= SQLSTATE ATOM.

declare_cursor_query_start ::= LP.
declare_cursor_query_start ::= SELECT.
declare_cursor_query_start ::= TABLE.
declare_cursor_query_start ::= VALUES.
declare_cursor_query_start ::= WITH.

declare_handler_action ::= CONTINUE.
declare_handler_action ::= EXIT.
declare_handler_action ::= UNDO.

declare_handler_keyword ::= HANDLER.

declare_handler_conditions ::= declare_handler_condition.
declare_handler_conditions ::= declare_handler_conditions COMMA declare_handler_condition.

declare_handler_condition ::= declare_condition_value.
declare_handler_condition ::= declare_not declare_found.

declare_not ::= NOT.

declare_found ::= FOUND.

declare_handler_statement_start ::= BEGIN.
declare_handler_statement_start ::= CALL.
declare_handler_statement_start ::= DELETE.
declare_handler_statement_start ::= DO.
declare_handler_statement_start ::= GET.
declare_handler_statement_start ::= INSERT.
declare_handler_statement_start ::= REPLACE.
declare_handler_statement_start ::= RESIGNAL.
declare_handler_statement_start ::= SELECT.
declare_handler_statement_start ::= SET.
declare_handler_statement_start ::= SIGNAL.
declare_handler_statement_start ::= UPDATE.

end_statement ::= END end_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}
dml_write_column_token ::= END.

end_tail ::= .
end_tail ::= stored_program_label_ref.
end_tail ::= IF.
end_tail ::= LOOP end_label_tail.
end_tail ::= REPEAT end_label_tail.
end_tail ::= WHILE end_label_tail.
end_tail ::= CASE.

end_label_tail ::= .
end_label_tail ::= stored_program_label_ref.

parenthesized_statement ::= LP statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

permissive_start(A) ::= ATOM(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}
permissive_start(A) ::= FROM(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}
permissive_start(A) ::= HAVING(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}
permissive_start(A) ::= RP(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}
permissive_start(A) ::= LB(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}
permissive_start(A) ::= RB(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}
permissive_start(A) ::= LC(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}
permissive_start(A) ::= RC(B). {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_PERMISSIVE;
}

statement_tail ::= .
statement_tail ::= statement_tail statement_token.

required_statement_tail ::= statement_token.
required_statement_tail ::= required_statement_tail statement_token.

statement_token ::= ATOM.
statement_token ::= LABEL.
statement_token ::= keyword.
statement_token ::= COMMA.
statement_token ::= LP.
statement_token ::= RP.
statement_token ::= LB.
statement_token ::= RB.
statement_token ::= LC.
statement_token ::= RC.

keyword ::= SELECT.
keyword ::= WITH.
keyword ::= INSERT.
keyword ::= REPLACE.
keyword ::= UPDATE.
keyword ::= DELETE.
keyword ::= CREATE.
keyword ::= CURRENT_USER.
keyword ::= DATA.
keyword ::= DATABASE.
keyword ::= ALTER.
keyword ::= DROP.
keyword ::= TRUNCATE.
keyword ::= RENAME.
keyword ::= CALL.
keyword ::= DO.
keyword ::= LOAD.
keyword ::= LOCAL.
keyword ::= MASTER.
keyword ::= TABLE.
keyword ::= TABLES.
keyword ::= TABLESPACE.
keyword ::= VALUES.
keyword ::= HANDLER.
keyword ::= IMPORT.
keyword ::= IN.
keyword ::= START.
keyword ::= BEGIN.
keyword ::= COMMIT.
keyword ::= COMPONENT.
keyword ::= CONCURRENT.
keyword ::= CONNECTION.
keyword ::= ROLLBACK.
keyword ::= SAVEPOINT.
keyword ::= RELEASE.
keyword ::= REPLICA.
keyword ::= SET.
keyword ::= SLAVE.
keyword ::= SLOW.
keyword ::= LOCK.
keyword ::= UNLOCK.
keyword ::= XA.
keyword ::= BINARY.
keyword ::= BINLOG.
keyword ::= BY.
keyword ::= CASCADE.
keyword ::= PURGE.
keyword ::= RESET.
keyword ::= RESTRICT.
keyword ::= CHANGE.
keyword ::= PREPARE.
keyword ::= PROCEDURE.
keyword ::= EXECUTE.
keyword ::= DEALLOCATE.
keyword ::= GRANT.
keyword ::= GROUP_REPLICATION.
keyword ::= REVOKE.
keyword ::= ROLE.
keyword ::= SHOW.
keyword ::= SCHEMA.
keyword ::= SERVER.
keyword ::= DESCRIBE.
keyword ::= DESC.
keyword ::= EXPLAIN.
keyword ::= EVENT.
keyword ::= FUNCTION.
keyword ::= HELP.
keyword ::= USE.
keyword ::= USING.
keyword ::= ANALYZE.
keyword ::= CHECK.
keyword ::= CHECKSUM.
keyword ::= OPTIMIZE.
keyword ::= REPAIR.
keyword ::= REQUIRE.
keyword ::= INSTALL.
keyword ::= INDEX.
keyword ::= INSTANCE.
keyword ::= NO_WRITE_TO_BINLOG.
keyword ::= UNINSTALL.
keyword ::= CLONE.
keyword ::= CACHE.
keyword ::= FLUSH.
keyword ::= KILL.
keyword ::= RESTART.
keyword ::= SHUTDOWN.
keyword ::= GET.
keyword ::= SIGNAL.
keyword ::= RESIGNAL.
keyword ::= IF.
keyword ::= ELSEIF.
keyword ::= LOOP.
keyword ::= REPEAT.
keyword ::= UNTIL.
keyword ::= WHILE.
keyword ::= WRITE.
keyword ::= CASE.
keyword ::= WHEN.
keyword ::= WHERE.
keyword ::= TRIGGER.
keyword ::= DECLARE.
keyword ::= OPEN.
keyword ::= FETCH.
keyword ::= CLOSE.
keyword ::= RETURN.
keyword ::= LEAVE.
keyword ::= ITERATE.
keyword ::= LIKE.
keyword ::= LIMIT.
keyword ::= LOG.
keyword ::= FROM.
keyword ::= USER.
keyword ::= VIEW.
keyword ::= HAVING.
keyword ::= ELSE.
keyword ::= END.
keyword ::= AGGREGATE.
keyword ::= ALGORITHM.
keyword ::= DEFINER.
keyword ::= FULLTEXT.
keyword ::= LOGFILE.
keyword ::= OR.
keyword ::= PLUGIN.
keyword ::= PERSIST.
keyword ::= PERSIST_ONLY.
keyword ::= RESOURCE.
keyword ::= QUERY.
keyword ::= READ.
keyword ::= RECOVER.
keyword ::= REPLICATION.
keyword ::= SECURITY.
keyword ::= SQL.
keyword ::= SPATIAL.
keyword ::= TEMPORARY.
keyword ::= THEN.
keyword ::= TRANSACTION.
keyword ::= UNDO.
keyword ::= UNION.
keyword ::= UNIQUE.
keyword ::= XML.
keyword ::= CHARACTER.
keyword ::= CHARSET.
keyword ::= COLLATION.
keyword ::= COLUMNS.
keyword ::= COUNT.
keyword ::= DATABASES.
keyword ::= DEFAULT_AUTH.
keyword ::= ENGINE.
keyword ::= ENGINES.
keyword ::= ERRORS.
keyword ::= ERROR.
keyword ::= EVENTS.
keyword ::= EXTENDED.
keyword ::= FIELDS.
keyword ::= FIRST.
keyword ::= FILTER.
keyword ::= FOR.
keyword ::= FORMAT.
keyword ::= FULL.
keyword ::= GENERAL.
keyword ::= GLOBAL.
keyword ::= GRANTS.
keyword ::= HOSTS.
keyword ::= INDEXES.
keyword ::= KEY.
keyword ::= KEYS.
keyword ::= LAST.
keyword ::= PLUGINS.
keyword ::= LOGS.
keyword ::= OPTIMIZER_COSTS.
keyword ::= PRIVILEGES.
keyword ::= PROCESSLIST.
keyword ::= PROFILE.
keyword ::= PROFILES.
keyword ::= RELAYLOG.
keyword ::= RELAY.
keyword ::= REPLICAS.
keyword ::= SCHEMAS.
keyword ::= SESSION.
keyword ::= STATUS.
keyword ::= STOP.
keyword ::= STORAGE.
keyword ::= TRIGGERS.
keyword ::= USER_RESOURCES.
keyword ::= VARIABLES.
keyword ::= WARNINGS.
keyword ::= DELAYED.
keyword ::= DEFAULT.
keyword ::= DIAGNOSTICS.
keyword ::= ALL.
keyword ::= AND.
keyword ::= CHAIN.
keyword ::= CONDITION.
keyword ::= CONTINUE.
keyword ::= CURSOR.
keyword ::= EXIT.
keyword ::= HIGH_PRIORITY.
keyword ::= IGNORE.
keyword ::= INTO.
keyword ::= IO_THREAD.
keyword ::= LOW_PRIORITY.
keyword ::= NAMES.
keyword ::= NO.
keyword ::= OFFSET.
keyword ::= ON.
keyword ::= ORDER.
keyword ::= PASSWORD.
keyword ::= PLUGIN_DIR.
keyword ::= QUICK.
keyword ::= NEXT.
keyword ::= PREV.
keyword ::= RECURSIVE.
keyword ::= ROW.
keyword ::= SQLSTATE.
keyword ::= SQL_THREAD.
keyword ::= DISTINCT.
keyword ::= DISTINCTROW.
keyword ::= SQL_BIG_RESULT.
keyword ::= SQL_BUFFER_RESULT.
keyword ::= SQL_CALC_FOUND_ROWS.
keyword ::= SQL_SMALL_RESULT.
keyword ::= SSL.
keyword ::= STRAIGHT_JOIN.
keyword ::= TO.
keyword ::= WORK.

keyword_not_select_clause ::= SELECT.
keyword_not_select_clause ::= WITH.
keyword_not_select_clause ::= INSERT.
keyword_not_select_clause ::= REPLACE.
keyword_not_select_clause ::= UPDATE.
keyword_not_select_clause ::= DELETE.
keyword_not_select_clause ::= CREATE.
keyword_not_select_clause ::= CURRENT_USER.
keyword_not_select_clause ::= DATA.
keyword_not_select_clause ::= DATABASE.
keyword_not_select_clause ::= ALTER.
keyword_not_select_clause ::= DROP.
keyword_not_select_clause ::= TRUNCATE.
keyword_not_select_clause ::= RENAME.
keyword_not_select_clause ::= CALL.
keyword_not_select_clause ::= DO.
keyword_not_select_clause ::= LOAD.
keyword_not_select_clause ::= LOCAL.
keyword_not_select_clause ::= MASTER.
keyword_not_select_clause ::= TABLE.
keyword_not_select_clause ::= TABLES.
keyword_not_select_clause ::= TABLESPACE.
keyword_not_select_clause ::= VALUES.
keyword_not_select_clause ::= HANDLER.
keyword_not_select_clause ::= IMPORT.
keyword_not_select_clause ::= IN.
keyword_not_select_clause ::= START.
keyword_not_select_clause ::= STOP.
keyword_not_select_clause ::= BEGIN.
keyword_not_select_clause ::= COMMIT.
keyword_not_select_clause ::= COMPONENT.
keyword_not_select_clause ::= CONCURRENT.
keyword_not_select_clause ::= CONNECTION.
keyword_not_select_clause ::= ROLLBACK.
keyword_not_select_clause ::= SAVEPOINT.
keyword_not_select_clause ::= RELEASE.
keyword_not_select_clause ::= REPLICA.
keyword_not_select_clause ::= SET.
keyword_not_select_clause ::= SLAVE.
keyword_not_select_clause ::= SLOW.
keyword_not_select_clause ::= LOCK.
keyword_not_select_clause ::= UNLOCK.
keyword_not_select_clause ::= XA.
keyword_not_select_clause ::= BINARY.
keyword_not_select_clause ::= BINLOG.
keyword_not_select_clause ::= BY.
keyword_not_select_clause ::= CASCADE.
keyword_not_select_clause ::= PURGE.
keyword_not_select_clause ::= RESET.
keyword_not_select_clause ::= RESTRICT.
keyword_not_select_clause ::= CHANGE.
keyword_not_select_clause ::= PREPARE.
keyword_not_select_clause ::= PROCEDURE.
keyword_not_select_clause ::= EXECUTE.
keyword_not_select_clause ::= DEALLOCATE.
keyword_not_select_clause ::= GRANT.
keyword_not_select_clause ::= GROUP_REPLICATION.
keyword_not_select_clause ::= REVOKE.
keyword_not_select_clause ::= ROLE.
keyword_not_select_clause ::= SHOW.
keyword_not_select_clause ::= SCHEMA.
keyword_not_select_clause ::= SERVER.
keyword_not_select_clause ::= DESCRIBE.
keyword_not_select_clause ::= DESC.
keyword_not_select_clause ::= EXPLAIN.
keyword_not_select_clause ::= EVENT.
keyword_not_select_clause ::= FUNCTION.
keyword_not_select_clause ::= HELP.
keyword_not_select_clause ::= USE.
keyword_not_select_clause ::= USING.
keyword_not_select_clause ::= ANALYZE.
keyword_not_select_clause ::= CHECK.
keyword_not_select_clause ::= CHECKSUM.
keyword_not_select_clause ::= OPTIMIZE.
keyword_not_select_clause ::= REPAIR.
keyword_not_select_clause ::= REQUIRE.
keyword_not_select_clause ::= INSTALL.
keyword_not_select_clause ::= INDEX.
keyword_not_select_clause ::= INSTANCE.
keyword_not_select_clause ::= NO_WRITE_TO_BINLOG.
keyword_not_select_clause ::= UNINSTALL.
keyword_not_select_clause ::= CLONE.
keyword_not_select_clause ::= CACHE.
keyword_not_select_clause ::= FLUSH.
keyword_not_select_clause ::= KILL.
keyword_not_select_clause ::= RESTART.
keyword_not_select_clause ::= SHUTDOWN.
keyword_not_select_clause ::= GET.
keyword_not_select_clause ::= SIGNAL.
keyword_not_select_clause ::= RESIGNAL.
keyword_not_select_clause ::= IF.
keyword_not_select_clause ::= ELSEIF.
keyword_not_select_clause ::= LOOP.
keyword_not_select_clause ::= REPEAT.
keyword_not_select_clause ::= UNTIL.
keyword_not_select_clause ::= WHILE.
keyword_not_select_clause ::= WRITE.
keyword_not_select_clause ::= CASE.
keyword_not_select_clause ::= WHEN.
keyword_not_select_clause ::= WHERE.
keyword_not_select_clause ::= TRIGGER.
keyword_not_select_clause ::= DECLARE.
keyword_not_select_clause ::= OPEN.
keyword_not_select_clause ::= FETCH.
keyword_not_select_clause ::= CLOSE.
keyword_not_select_clause ::= RETURN.
keyword_not_select_clause ::= LEAVE.
keyword_not_select_clause ::= ITERATE.
keyword_not_select_clause ::= LIKE.
keyword_not_select_clause ::= LIMIT.
keyword_not_select_clause ::= LOG.
keyword_not_select_clause ::= USER.
keyword_not_select_clause ::= VIEW.
keyword_not_select_clause ::= ELSE.
keyword_not_select_clause ::= END.
keyword_not_select_clause ::= AGGREGATE.
keyword_not_select_clause ::= ALGORITHM.
keyword_not_select_clause ::= DEFINER.
keyword_not_select_clause ::= FULLTEXT.
keyword_not_select_clause ::= LOGFILE.
keyword_not_select_clause ::= OR.
keyword_not_select_clause ::= PLUGIN.
keyword_not_select_clause ::= PERSIST.
keyword_not_select_clause ::= PERSIST_ONLY.
keyword_not_select_clause ::= RESOURCE.
keyword_not_select_clause ::= QUERY.
keyword_not_select_clause ::= READ.
keyword_not_select_clause ::= RECOVER.
keyword_not_select_clause ::= REPLICATION.
keyword_not_select_clause ::= SECURITY.
keyword_not_select_clause ::= SQL.
keyword_not_select_clause ::= SPATIAL.
keyword_not_select_clause ::= TEMPORARY.
keyword_not_select_clause ::= THEN.
keyword_not_select_clause ::= TRANSACTION.
keyword_not_select_clause ::= UNDO.
keyword_not_select_clause ::= UNION.
keyword_not_select_clause ::= UNIQUE.
keyword_not_select_clause ::= XML.
keyword_not_select_clause ::= CHARACTER.
keyword_not_select_clause ::= CHARSET.
keyword_not_select_clause ::= COLLATION.
keyword_not_select_clause ::= COLUMNS.
keyword_not_select_clause ::= COUNT.
keyword_not_select_clause ::= DATABASES.
keyword_not_select_clause ::= DEFAULT_AUTH.
keyword_not_select_clause ::= ENGINE.
keyword_not_select_clause ::= ENGINES.
keyword_not_select_clause ::= ERRORS.
keyword_not_select_clause ::= ERROR.
keyword_not_select_clause ::= EVENTS.
keyword_not_select_clause ::= EXTENDED.
keyword_not_select_clause ::= FIELDS.
keyword_not_select_clause ::= FIRST.
keyword_not_select_clause ::= FILTER.
keyword_not_select_clause ::= FOR.
keyword_not_select_clause ::= FORMAT.
keyword_not_select_clause ::= FULL.
keyword_not_select_clause ::= GENERAL.
keyword_not_select_clause ::= GLOBAL.
keyword_not_select_clause ::= GRANTS.
keyword_not_select_clause ::= HOSTS.
keyword_not_select_clause ::= INDEXES.
keyword_not_select_clause ::= KEY.
keyword_not_select_clause ::= KEYS.
keyword_not_select_clause ::= LAST.
keyword_not_select_clause ::= PLUGINS.
keyword_not_select_clause ::= LOGS.
keyword_not_select_clause ::= OPTIMIZER_COSTS.
keyword_not_select_clause ::= PRIVILEGES.
keyword_not_select_clause ::= PROCESSLIST.
keyword_not_select_clause ::= PROFILE.
keyword_not_select_clause ::= PROFILES.
keyword_not_select_clause ::= RELAYLOG.
keyword_not_select_clause ::= RELAY.
keyword_not_select_clause ::= REPLICAS.
keyword_not_select_clause ::= SCHEMAS.
keyword_not_select_clause ::= SESSION.
keyword_not_select_clause ::= STATUS.
keyword_not_select_clause ::= STORAGE.
keyword_not_select_clause ::= TRIGGERS.
keyword_not_select_clause ::= USER_RESOURCES.
keyword_not_select_clause ::= VARIABLES.
keyword_not_select_clause ::= WARNINGS.
keyword_not_select_clause ::= DELAYED.
keyword_not_select_clause ::= DEFAULT.
keyword_not_select_clause ::= DIAGNOSTICS.
keyword_not_select_clause ::= ALL.
keyword_not_select_clause ::= AND.
keyword_not_select_clause ::= CHAIN.
keyword_not_select_clause ::= CONDITION.
keyword_not_select_clause ::= CONTINUE.
keyword_not_select_clause ::= CURSOR.
keyword_not_select_clause ::= EXIT.
keyword_not_select_clause ::= HIGH_PRIORITY.
keyword_not_select_clause ::= IGNORE.
keyword_not_select_clause ::= INTO.
keyword_not_select_clause ::= IO_THREAD.
keyword_not_select_clause ::= LOW_PRIORITY.
keyword_not_select_clause ::= NAMES.
keyword_not_select_clause ::= NO.
keyword_not_select_clause ::= OFFSET.
keyword_not_select_clause ::= ON.
keyword_not_select_clause ::= ORDER.
keyword_not_select_clause ::= PASSWORD.
keyword_not_select_clause ::= PLUGIN_DIR.
keyword_not_select_clause ::= QUICK.
keyword_not_select_clause ::= NEXT.
keyword_not_select_clause ::= PREV.
keyword_not_select_clause ::= RECURSIVE.
keyword_not_select_clause ::= ROW.
keyword_not_select_clause ::= SQLSTATE.
keyword_not_select_clause ::= SQL_THREAD.
keyword_not_select_clause ::= DISTINCT.
keyword_not_select_clause ::= DISTINCTROW.
keyword_not_select_clause ::= SQL_BIG_RESULT.
keyword_not_select_clause ::= SQL_BUFFER_RESULT.
keyword_not_select_clause ::= SQL_CALC_FOUND_ROWS.
keyword_not_select_clause ::= SQL_SMALL_RESULT.
keyword_not_select_clause ::= SSL.
keyword_not_select_clause ::= STRAIGHT_JOIN.
keyword_not_select_clause ::= TO.
keyword_not_select_clause ::= WORK.

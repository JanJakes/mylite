%name MyLiteLemon
%stack_size 0
%realloc mylite_lemon_realloc
%free mylite_lemon_free
%token_prefix ML_
%token_type {MyliteToken}
%default_type {MyliteToken}
%fallback ATOM ACTIVE ADD AFTER ASC AS AT AUTO AUTOEXTEND_SIZE AUTO_INCREMENT AVG_ROW_LENGTH BACKUP BEFORE BLOCK BUCKETS CASCADED CATALOG_NAME CHANGED CHANNEL CLASS_ORIGIN COALESCE CODE COLLATE COLUMN COLUMN_NAME COMMENT COMPLETION COMPRESSION CONSISTENT CONSTRAINT CONSTRAINT_CATALOG CONSTRAINT_NAME CONSTRAINT_SCHEMA CONTAINS CONTEXT CONVERT CPU CURRENT CURSOR_NAME CURRENT_USER DATAFILE DECIMAL DEFINITION DELAY_KEY_WRITE DESCRIPTION DETERMINISTIC DIRECTORY DISCARD DUMPFILE DUPLICATE EACH ENABLE ENCRYPTION ENFORCED ENGINE_ATTRIBUTE EVERY EXCHANGE EXCEPT EXISTS EXPORT EXTENT_SIZE FAST FAULTS FILE_BLOCK_SIZE FILTER FOLLOWS FORCE FOREIGN FOUND GENERAL GROUP GTIDS HISTOGRAM HOST IDENTIFIED INACTIVE INFILE INITIAL_SIZE INNODB INSERT_METHOD INT INTEGER INTERSECT INVOKER IO IPC JOIN JSON KEYRING KEY_BLOCK_SIZE LANGUAGE LEAVES LOG MANUAL MAX_ROWS MAX_SIZE MEDIUM MEMORY MERGE MESSAGE_TEXT MIGRATE MIN_ROWS MODIFIES MODIFY MUTEX MYSQL_ERRNO NAME NO NODEGROUP NONE NOT NULL NUMBER OFF ONE ONLY OPTIONS ORGANIZATION OUTFILE OWNER PACK_KEYS PAGE PARSE_TREE PARTITION PARTITIONING PHASE PORT PRECEDES PRESERVE PRIMARY RANDOM READS REAL REBUILD REDO_BUFFER_SIZE REDO_LOG REFERENCE RELAY_LOG_FILE RELAY_LOG_POS RELOAD REMOVE REORGANIZE REPLICATE_DO_DB REPLICATE_DO_TABLE REPLICATE_IGNORE_DB REPLICATE_IGNORE_TABLE REPLICATE_REWRITE_DB REPLICATE_WILD_DO_TABLE REPLICATE_WILD_IGNORE_TABLE REQUIRE RESUME RETAIN RETURNED_SQLSTATE RETURNS ROTATE ROW_COUNT ROW_FORMAT SCHEDULE SCHEMA_NAME SECONDARY_ENGINE SECONDARY_ENGINE_ATTRIBUTE SECONDARY_LOAD SECONDARY_UNLOAD SLOW SNAPSHOT SOCKET SONAME SOURCE SOURCE_LOG_FILE SOURCE_LOG_POS SQL_AFTER_GTIDS SQL_AFTER_MTS_GAPS SQL_BEFORE_GTIDS SSL STATS_AUTO_RECALC STATS_PERSISTENT STATS_SAMPLE_PAGES STREAM STRING SUBCLASS_ORIGIN SUSPEND SWAPS SWITCHES SYSTEM TABLE_NAME TEMPTABLE THREAD_PRIORITY TLS TRADITIONAL TREE TYPE UNDEFINED UNDO_BUFFER_SIZE UNDOFILE UPGRADE USE_FRM VALIDATION VALUE VCPU WAIT WITHOUT WRAPPER XID ASSIGN COLON DOT DOUBLE_QUOTED_STRING EQUALS MINUS QUOTED_ID STAR AT_SIGN AT_EMPTY AT_HOST.
%fallback ATOM BOOLEAN_NUMBER ENCRYPTION_VALUE FACTOR_NUMBER NUMBER_LITERAL SQLSTATE_VALUE STRING_LITERAL.
%fallback ATOM GE GT LE LT.
%fallback ATOM STACKED.
%fallback ATOM ENCLOSED ESCAPED LINES OPTIONALLY ROWS STARTING TERMINATED.
%fallback ATOM COPY EXCLUSIVE INPLACE INSTANT SHARED.
%fallback ATOM INVISIBLE PARSER VISIBLE.
%fallback ATOM ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS GET_MASTER_PUBLIC_KEY GET_SOURCE_PUBLIC_KEY GTID_ONLY IGNORE_SERVER_IDS MASTER_AUTO_POSITION MASTER_BIND MASTER_COMPRESSION_ALGORITHMS MASTER_CONNECT_RETRY MASTER_DELAY MASTER_HEARTBEAT_PERIOD MASTER_HOST MASTER_LOG_FILE MASTER_LOG_POS MASTER_PASSWORD MASTER_PORT MASTER_PUBLIC_KEY_PATH MASTER_RETRY_COUNT MASTER_SSL MASTER_SSL_CA MASTER_SSL_CAPATH MASTER_SSL_CERT MASTER_SSL_CIPHER MASTER_SSL_CRL MASTER_SSL_CRLPATH MASTER_SSL_KEY MASTER_SSL_VERIFY_SERVER_CERT MASTER_TLS_CIPHERSUITES MASTER_TLS_VERSION MASTER_USER MASTER_ZSTD_COMPRESSION_LEVEL NETWORK_NAMESPACE PRIVILEGE_CHECKS_USER REQUIRE_ROW_FORMAT REQUIRE_TABLE_PRIMARY_KEY_CHECK SOURCE_AUTO_POSITION SOURCE_BIND SOURCE_COMPRESSION_ALGORITHMS SOURCE_CONNECT_RETRY SOURCE_CONNECTION_AUTO_FAILOVER SOURCE_DELAY SOURCE_HEARTBEAT_PERIOD SOURCE_HOST SOURCE_PASSWORD SOURCE_PORT SOURCE_PUBLIC_KEY_PATH SOURCE_RETRY_COUNT SOURCE_SSL SOURCE_SSL_CA SOURCE_SSL_CAPATH SOURCE_SSL_CERT SOURCE_SSL_CIPHER SOURCE_SSL_CRL SOURCE_SSL_CRLPATH SOURCE_SSL_KEY SOURCE_SSL_VERIFY_SERVER_CERT SOURCE_TLS_CIPHERSUITES SOURCE_TLS_VERSION SOURCE_USER SOURCE_ZSTD_COMPRESSION_LEVEL.
%type labeled_statement_start {MyliteStatementKind}
%type permissive_start {MyliteStatementKind}
%type drop_tail {MyliteStatementKind}
%type start_tail {MyliteStatementKind}
%type lock_tail {MyliteStatementKind}
%type unlock_tail {MyliteStatementKind}
%type alter_instance_reload_tls_tail {int}
%type alter_instance_reload_channel_tail {int}
%type alter_instance_reload_rollback_tail {int}
%extra_argument {MyliteParseContext *ctx}
%token_destructor { (void)ctx; (void)yypminor; }
%default_destructor { (void)ctx; (void)yypminor; }

%include {
#include <stdlib.h>
#include <string.h>

#include "mylite_parser_internal.h"

#define mylite_lemon_realloc(P, N, C) realloc((P), (N))
#define mylite_lemon_free(P, C) free(P)
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
create_tail ::= SPATIAL create_reference create_system create_if_not_exists_tail srs_id create_srs_attributes.
create_tail ::= OR REPLACE SPATIAL create_reference create_system srs_id create_srs_attributes.
create_tail ::= SERVER cache_name_part create_server_tail.
create_tail ::= TABLESPACE cache_name_part create_tablespace_tail.
create_tail ::= UNDO TABLESPACE cache_name_part create_undo_tablespace_tail.
create_tail ::= create_database_kind create_if_not_exists_tail cache_name_part create_database_tail.
create_tail ::= ROLE create_if_not_exists_tail drop_account_list.
create_tail ::= USER create_if_not_exists_tail create_user_list account_management_options account_management_permissive_tail.
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

create_index_tail ::= LP create_index_key_parts RP create_index_options_tail.

create_index_key_parts ::= create_index_key_part.
create_index_key_parts ::= create_index_key_parts COMMA create_index_key_part.

create_index_key_part ::= create_index_key_part_tokens.

create_index_key_part_tokens ::= create_index_key_part_token.
create_index_key_part_tokens ::= create_index_key_part_tokens create_index_key_part_token.

create_index_key_part_token ::= ATOM.
create_index_key_part_token ::= LABEL.
create_index_key_part_token ::= keyword.
create_index_key_part_token ::= DOT.
create_index_key_part_token ::= LP create_table_definition_tokens RP.
create_index_key_part_token ::= LB.
create_index_key_part_token ::= RB.
create_index_key_part_token ::= LC.
create_index_key_part_token ::= RC.

create_index_options_tail ::= .
create_index_options_tail ::= create_index_options_tail create_index_option.

create_index_option ::= KEY_BLOCK_SIZE drop_index_option_equals_tail index_number_value.
create_index_option ::= USING cache_name_part.
create_index_option ::= create_index_type cache_name_part.
create_index_option ::= WITH PARSER cache_name_part.
create_index_option ::= COMMENT string_literal.
create_index_option ::= VISIBLE.
create_index_option ::= INVISIBLE.
create_index_option ::= ENGINE_ATTRIBUTE drop_index_option_equals_tail string_literal.
create_index_option ::= SECONDARY_ENGINE_ATTRIBUTE drop_index_option_equals_tail string_literal.
create_index_option ::= drop_index_option.

index_number_value ::= BOOLEAN_NUMBER.
index_number_value ::= FACTOR_NUMBER.
index_number_value ::= NUMBER_LITERAL.

create_table_prefix ::= TABLE.
create_table_prefix ::= TEMPORARY TABLE.

create_database_kind ::= DATABASE.
create_database_kind ::= SCHEMA.

create_database_tail ::= create_database_options_tail.

create_table_tail ::= LP create_table_elements RP create_table_post_definition_tail.
create_table_tail ::= LIKE cache_table_ref.
create_table_tail ::= SELECT select_tail.
create_table_tail ::= AS SELECT select_tail.
create_table_tail ::= TABLE table_statement_target table_query_tail.
create_table_tail ::= AS TABLE table_statement_target table_query_tail.
create_table_tail ::= VALUES values_row_list values_query_tail.
create_table_tail ::= AS VALUES values_row_list values_query_tail.
create_table_tail ::= WITH with_recursive_tail with_cte_list with_query_body.
create_table_tail ::= AS WITH with_recursive_tail with_cte_list with_query_body.
create_table_tail ::= query_parenthesized_body.
create_table_tail ::= AS query_parenthesized_body.
create_table_tail ::= AS LP LP dml_write_query_start required_statement_tail.
create_table_tail ::= create_table_tail_option_start required_statement_tail.

create_table_post_definition_tail ::= .
create_table_post_definition_tail ::= create_table_tail_option_start required_statement_tail.
create_table_post_definition_tail ::= create_table_ctas_tail.

create_table_ctas_tail ::= create_table_ctas_modifier_tail create_table_ctas_body.

create_table_ctas_modifier_tail ::= .
create_table_ctas_modifier_tail ::= IGNORE.
create_table_ctas_modifier_tail ::= REPLACE.

create_table_ctas_body ::= SELECT select_tail.
create_table_ctas_body ::= AS SELECT select_tail.
create_table_ctas_body ::= TABLE table_statement_target table_query_tail.
create_table_ctas_body ::= AS TABLE table_statement_target table_query_tail.
create_table_ctas_body ::= VALUES values_row_list values_query_tail.
create_table_ctas_body ::= AS VALUES values_row_list values_query_tail.
create_table_ctas_body ::= WITH with_recursive_tail with_cte_list with_query_body.
create_table_ctas_body ::= AS WITH with_recursive_tail with_cte_list with_query_body.
create_table_ctas_body ::= query_parenthesized_body.
create_table_ctas_body ::= AS query_parenthesized_body.

create_table_elements ::= create_table_element.
create_table_elements ::= create_table_elements COMMA create_table_element.

create_table_element ::= create_table_element_name create_table_element_tokens.
create_table_element ::= create_table_constraint_start create_table_element_tokens.
create_table_element ::= CONSTRAINT create_table_constraint_start create_table_element_tokens.
create_table_element ::= CONSTRAINT create_table_element_name create_table_constraint_start create_table_element_tokens.

create_table_element_name ::= cache_name_part.
create_table_element_name ::= BINLOG.
create_table_element_name ::= CHECKSUM.
create_table_element_name ::= CONNECTION.
create_table_element_name ::= CURRENT.
create_table_element_name ::= DATA.
create_table_element_name ::= DEFINER.
create_table_element_name ::= DIAGNOSTICS.
create_table_element_name ::= END.
create_table_element_name ::= EVENT.
create_table_element_name ::= FORMAT.
create_table_element_name ::= LAST.
create_table_element_name ::= LIKE.
create_table_element_name ::= NUMBER.
create_table_element_name ::= OFFSET.
create_table_element_name ::= PASSWORD.
create_table_element_name ::= QUERY.
create_table_element_name ::= RETURNED_SQLSTATE.
create_table_element_name ::= START.
create_table_element_name ::= STATUS.
create_table_element_name ::= XML.

create_table_constraint_start ::= PRIMARY.
create_table_constraint_start ::= UNIQUE.
create_table_constraint_start ::= FULLTEXT.
create_table_constraint_start ::= SPATIAL.
create_table_constraint_start ::= INDEX.
create_table_constraint_start ::= KEY.
create_table_constraint_start ::= FOREIGN.
create_table_constraint_start ::= CHECK.

create_table_element_tokens ::= create_table_element_token.
create_table_element_tokens ::= create_table_element_tokens create_table_element_token.

create_table_element_token ::= ATOM.
create_table_element_token ::= LABEL.
create_table_element_token ::= keyword.
create_table_element_token ::= DOT.
create_table_element_token ::= LP create_table_definition_tokens RP.
create_table_element_token ::= LB.
create_table_element_token ::= RB.
create_table_element_token ::= LC.
create_table_element_token ::= RC.

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

create_table_tail_option_start ::= AUTOEXTEND_SIZE.
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
create_table_tail_option_start ::= SECONDARY_ENGINE.
create_table_tail_option_start ::= SECONDARY_ENGINE_ATTRIBUTE.
create_table_tail_option_start ::= START.
create_table_tail_option_start ::= STATS_AUTO_RECALC.
create_table_tail_option_start ::= STATS_PERSISTENT.
create_table_tail_option_start ::= STATS_SAMPLE_PAGES.
create_table_tail_option_start ::= STORAGE.
create_table_tail_option_start ::= TABLESPACE.
create_table_tail_option_start ::= UNION.

create_database_options_tail ::= .
create_database_options_tail ::= create_database_options_tail create_database_option.

create_database_option ::= database_default_tail database_character_set_option.
create_database_option ::= database_default_tail database_collate_option.
create_database_option ::= database_default_tail database_encryption_option.

database_default_tail ::= .
database_default_tail ::= DEFAULT.

database_character_set_option ::= CHARACTER SET drop_index_option_equals_tail set_charset_name.
database_character_set_option ::= CHARSET drop_index_option_equals_tail set_charset_name.

database_collate_option ::= COLLATE drop_index_option_equals_tail set_collation_value.

database_encryption_option ::= ENCRYPTION drop_index_option_equals_tail encryption_value.

encryption_value ::= ENCRYPTION_VALUE.

string_literal ::= STRING_LITERAL.
string_literal ::= SQLSTATE_VALUE.
string_literal ::= ENCRYPTION_VALUE.
string_literal ::= DOUBLE_QUOTED_STRING.

create_udf_tail ::= create_returns create_udf_return_type create_soname string_literal.

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
view_body ::= view_as TABLE table_statement_target view_table_tail.
view_body ::= view_as VALUES values_row_list view_values_tail.
view_body ::= view_as WITH with_recursive_tail with_cte_list with_query_body.
view_body ::= view_as query_parenthesized_body.

view_as ::= AS.

view_table_tail ::= .
view_table_tail ::= table_order_nonempty_tail table_limit_tail.
view_table_tail ::= table_limit_nonempty_tail.
view_table_tail ::= view_check_option.

view_values_tail ::= .
view_values_tail ::= values_query_tail_nonempty.
view_values_tail ::= view_check_option.

view_check_option ::= WITH view_check_scope_tail CHECK OPTION.

view_check_scope_tail ::= .
view_check_scope_tail ::= CASCADED.
view_check_scope_tail ::= LOCAL.

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

create_event_body ::= alter_event_schedule_clause alter_event_completion_tail alter_event_status_tail alter_event_comment_tail create_event_do event_statement_start statement_tail.

event_schedule_start ::= AT.
event_schedule_start ::= EVERY.

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
create_trigger_statement_start ::= CALL.
create_trigger_statement_start ::= CASE.
create_trigger_statement_start ::= CLOSE.
create_trigger_statement_start ::= DELETE.
create_trigger_statement_start ::= DO.
create_trigger_statement_start ::= FETCH.
create_trigger_statement_start ::= GET.
create_trigger_statement_start ::= IF.
create_trigger_statement_start ::= INSERT.
create_trigger_statement_start ::= ITERATE.
create_trigger_statement_start ::= LABEL.
create_trigger_statement_start ::= LEAVE.
create_trigger_statement_start ::= LOOP.
create_trigger_statement_start ::= OPEN.
create_trigger_statement_start ::= REPEAT.
create_trigger_statement_start ::= REPLACE.
create_trigger_statement_start ::= RESIGNAL.
create_trigger_statement_start ::= RELEASE.
create_trigger_statement_start ::= ROLLBACK.
create_trigger_statement_start ::= SELECT.
create_trigger_statement_start ::= SET.
create_trigger_statement_start ::= SIGNAL.
create_trigger_statement_start ::= UPDATE.
create_trigger_statement_start ::= WHILE.

create_function_tail ::= create_udf_tail.
create_function_tail ::= function_signature create_returns create_function_return_tail create_function_body_start statement_tail.

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

create_procedure_tail ::= procedure_signature create_procedure_tail_start statement_tail.

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

function_signature ::= LP RP.
function_signature ::= LP function_parameters RP.

function_parameters ::= function_parameter.
function_parameters ::= function_parameters COMMA function_parameter.

function_parameter ::= routine_parameter_name routine_parameter_tokens.

procedure_signature ::= LP RP.
procedure_signature ::= LP procedure_parameters RP.

procedure_parameters ::= procedure_parameter.
procedure_parameters ::= procedure_parameters COMMA procedure_parameter.

procedure_parameter ::= routine_parameter_name routine_parameter_tokens.
procedure_parameter ::= IN routine_parameter_name routine_parameter_tokens.
procedure_parameter ::= OUT routine_parameter_name routine_parameter_tokens.
procedure_parameter ::= INOUT routine_parameter_name routine_parameter_tokens.

routine_parameter_name ::= create_table_element_name.

routine_parameter_tokens ::= routine_parameter_token.
routine_parameter_tokens ::= routine_parameter_tokens routine_parameter_token.

routine_parameter_token ::= ATOM.
routine_parameter_token ::= LABEL.
routine_parameter_token ::= keyword.
routine_parameter_token ::= DOT.
routine_parameter_token ::= LP routine_parameter_nested_tokens RP.
routine_parameter_token ::= LB.
routine_parameter_token ::= RB.
routine_parameter_token ::= LC.
routine_parameter_token ::= RC.

routine_parameter_nested_tokens ::= .
routine_parameter_nested_tokens ::= routine_parameter_nested_tokens routine_parameter_nested_token.

routine_parameter_nested_token ::= ATOM.
routine_parameter_nested_token ::= LABEL.
routine_parameter_nested_token ::= keyword.
routine_parameter_nested_token ::= COMMA.
routine_parameter_nested_token ::= DOT.
routine_parameter_nested_token ::= LP routine_parameter_nested_tokens RP.
routine_parameter_nested_token ::= LB.
routine_parameter_nested_token ::= RB.
routine_parameter_nested_token ::= LC.
routine_parameter_nested_token ::= RC.

create_if_not_exists_tail ::= .
create_if_not_exists_tail ::= IF create_not reset_exists.

create_not ::= NOT.

create_options_tail ::= .
create_options_tail ::= create_options_tail statement_token.

create_user_list ::= create_user_spec.
create_user_list ::= create_user_list import_comma create_user_spec.

create_user_spec ::= drop_account_name create_user_auth_tail.

create_user_auth_tail ::= .
create_user_auth_tail ::= create_user_auth_option.

alter_user_list ::= alter_user_spec.
alter_user_list ::= alter_user_list import_comma alter_user_spec.

alter_user_spec ::= drop_account_name alter_user_account_option_tail.
alter_user_spec ::= current_user_ref alter_user_account_option_tail.

alter_user_account_option_tail ::= .
alter_user_account_option_tail ::= user_auth_option alter_user_auth_tail.
alter_user_account_option_tail ::= DISCARD OLD PASSWORD.
alter_user_account_option_tail ::= alter_user_factor_option.
alter_user_account_option_tail ::= account_registration_option.

alter_user_func_option_tail ::= .
alter_user_func_option_tail ::= account_password_auth_option alter_user_auth_tail.
alter_user_func_option_tail ::= DISCARD OLD PASSWORD.
alter_user_func_option_tail ::= account_registration_option.

alter_user_auth_tail ::= .
alter_user_auth_tail ::= REPLACE user_auth_string_value alter_user_retain_tail.
alter_user_auth_tail ::= RETAIN CURRENT PASSWORD.

alter_user_retain_tail ::= .
alter_user_retain_tail ::= RETAIN CURRENT PASSWORD.

create_user_auth_option ::= user_auth_option create_user_mfa_tail.
create_user_auth_option ::= IDENTIFIED WITH user_auth_plugin account_initial_auth_option.

create_user_mfa_tail ::= .
create_user_mfa_tail ::= AND create_user_factor_auth_option create_user_third_factor_tail.

create_user_third_factor_tail ::= .
create_user_third_factor_tail ::= AND create_user_factor_auth_option.

create_user_factor_auth_option ::= user_auth_option.

account_initial_auth_option ::= INITIAL AUTHENTICATION IDENTIFIED BY user_auth_value.
account_initial_auth_option ::= INITIAL AUTHENTICATION IDENTIFIED WITH user_auth_plugin AS user_auth_string_value.

user_auth_option ::= account_password_auth_option.
user_auth_option ::= IDENTIFIED WITH user_auth_plugin.
user_auth_option ::= IDENTIFIED WITH user_auth_plugin BY user_auth_value.
user_auth_option ::= IDENTIFIED WITH user_auth_plugin AS user_auth_string_value.

account_password_auth_option ::= IDENTIFIED BY user_auth_value.

alter_user_factor_option ::= alter_user_factor_add_option alter_user_factor_add_tail.
alter_user_factor_option ::= alter_user_factor_modify_option alter_user_factor_modify_tail.
alter_user_factor_option ::= alter_user_factor_drop_option alter_user_factor_drop_tail.

alter_user_factor_add_tail ::= .
alter_user_factor_add_tail ::= alter_user_factor_add_option.

alter_user_factor_modify_tail ::= .
alter_user_factor_modify_tail ::= alter_user_factor_modify_option.

alter_user_factor_drop_tail ::= .
alter_user_factor_drop_tail ::= alter_user_factor_drop_option.

alter_user_factor_add_option ::= ADD account_factor account_factor_auth_option.
alter_user_factor_modify_option ::= MODIFY account_factor account_factor_auth_option.
alter_user_factor_drop_option ::= DROP account_factor.

account_factor_auth_option ::= account_password_auth_option.
account_factor_auth_option ::= IDENTIFIED WITH user_auth_plugin BY user_auth_value.
account_factor_auth_option ::= IDENTIFIED WITH user_auth_plugin AS user_auth_string_value.

account_registration_option ::= account_factor INITIATE REGISTRATION.
account_registration_option ::= account_factor FINISH REGISTRATION SET CHALLENGE_RESPONSE AS user_auth_string_value.
account_registration_option ::= account_factor UNREGISTER.

account_factor ::= FACTOR_NUMBER FACTOR.

user_auth_value ::= user_auth_string_value.
user_auth_value ::= RANDOM PASSWORD.

user_auth_string_value ::= string_literal.

user_auth_plugin ::= user_option_value.

account_management_options ::= account_default_role_tail account_require_tail account_resource_tail account_password_lock_options account_comment_attribute_tail.

account_management_permissive_tail ::= .
account_management_permissive_tail ::= DOUBLE_QUOTED_STRING(A). {
  mylite_parser_require_permissive(ctx, A);
}

account_default_role_tail ::= .
account_default_role_tail ::= DEFAULT ROLE account_default_role_spec.

account_require_tail ::= .
account_require_tail ::= REQUIRE account_tls_requirement.

account_resource_tail ::= .
account_resource_tail ::= WITH account_resource_options.

account_password_lock_options ::= .
account_password_lock_options ::= account_password_lock_options account_password_lock_option.

account_password_lock_option ::= account_password_option.
account_password_lock_option ::= account_lock_option.

account_comment_attribute_tail ::= .
account_comment_attribute_tail ::= COMMENT string_literal.
account_comment_attribute_tail ::= ATTRIBUTE string_literal.

account_default_role_spec ::= NONE.
account_default_role_spec ::= ALL.
account_default_role_spec ::= drop_account_list.

account_tls_requirement ::= NONE.
account_tls_requirement ::= account_tls_option_list.

account_tls_option_list ::= account_tls_option.
account_tls_option_list ::= account_tls_option_list account_tls_and_tail account_tls_option.

account_tls_and_tail ::= .
account_tls_and_tail ::= AND.

account_tls_option ::= SSL.
account_tls_option ::= X509.
account_tls_option ::= CIPHER string_literal.
account_tls_option ::= ISSUER string_literal.
account_tls_option ::= SUBJECT string_literal.

account_resource_options ::= account_resource_option.
account_resource_options ::= account_resource_options account_resource_option.

account_resource_option ::= MAX_QUERIES_PER_HOUR account_resource_count.
account_resource_option ::= MAX_UPDATES_PER_HOUR account_resource_count.
account_resource_option ::= MAX_CONNECTIONS_PER_HOUR account_resource_count.
account_resource_option ::= MAX_USER_CONNECTIONS account_resource_count.

account_resource_count ::= BOOLEAN_NUMBER.
account_resource_count ::= FACTOR_NUMBER.
account_resource_count ::= NUMBER_LITERAL.

account_password_option ::= PASSWORD EXPIRE.
account_password_option ::= PASSWORD EXPIRE DEFAULT.
account_password_option ::= PASSWORD EXPIRE NEVER.
account_password_option ::= PASSWORD EXPIRE INTERVAL account_password_number_value DAY.
account_password_option ::= PASSWORD HISTORY account_default_or_value.
account_password_option ::= PASSWORD REUSE INTERVAL account_default_or_day_value.
account_password_option ::= PASSWORD REQUIRE CURRENT account_current_password_tail.
account_password_option ::= FAILED_LOGIN_ATTEMPTS account_password_number_value.
account_password_option ::= PASSWORD_LOCK_TIME account_password_lock_value.

account_default_or_value ::= DEFAULT.
account_default_or_value ::= account_password_number_value.

account_default_or_day_value ::= DEFAULT.
account_default_or_day_value ::= account_password_number_value DAY.

account_current_password_tail ::= .
account_current_password_tail ::= DEFAULT.
account_current_password_tail ::= OPTIONAL.

account_password_lock_value ::= account_password_number_value.
account_password_lock_value ::= UNBOUNDED.

account_password_number_value ::= BOOLEAN_NUMBER.
account_password_number_value ::= FACTOR_NUMBER.
account_password_number_value ::= NUMBER_LITERAL.

account_lock_option ::= ACCOUNT LOCK.
account_lock_option ::= ACCOUNT UNLOCK.

user_option_value ::= ATOM.
user_option_value ::= FACTOR_NUMBER.
user_option_value ::= LABEL.

%fallback ATOM ATTRIBUTE AUTHENTICATION CHALLENGE_RESPONSE CIPHER DAY EXPIRE FAILED_LOGIN_ATTEMPTS FINISH HISTORY INITIAL INITIATE INTERVAL ISSUER MAX_CONNECTIONS_PER_HOUR MAX_QUERIES_PER_HOUR MAX_UPDATES_PER_HOUR MAX_USER_CONNECTIONS NEVER OLD OPTIONAL PASSWORD_LOCK_TIME REGISTRATION REUSE SUBJECT UNBOUNDED UNREGISTER X509.

create_resource_group ::= GROUP.

create_resource_group_tail ::= create_resource_type create_resource_group_options_tail.

create_resource_group_options_tail ::= resource_group_vcpu_tail resource_group_thread_priority_tail resource_group_state_tail.

resource_group_vcpu_tail ::= .
resource_group_vcpu_tail ::= resource_group_vcpu_clause.

resource_group_vcpu_clause ::= VCPU resource_group_optional_equals resource_group_vcpu_list.

resource_group_vcpu_list ::= resource_group_vcpu_spec.
resource_group_vcpu_list ::= resource_group_vcpu_list import_comma resource_group_vcpu_spec.

resource_group_vcpu_spec ::= resource_group_number_value.
resource_group_vcpu_spec ::= resource_group_number_value MINUS resource_group_number_value.

resource_group_thread_priority_tail ::= .
resource_group_thread_priority_tail ::= resource_group_thread_priority_clause.

resource_group_thread_priority_clause ::= THREAD_PRIORITY resource_group_optional_equals resource_group_signed_atom.

resource_group_state_tail ::= .
resource_group_state_tail ::= ENABLE.
resource_group_state_tail ::= DISABLE.

resource_group_optional_equals ::= .
resource_group_optional_equals ::= diagnostics_equals.

resource_group_signed_atom ::= resource_group_number_value.

resource_group_number_value ::= BOOLEAN_NUMBER.
resource_group_number_value ::= FACTOR_NUMBER.
resource_group_number_value ::= NUMBER_LITERAL.

create_resource_type ::= create_type_marker diagnostics_equals create_resource_type_value.

create_type_marker ::= TYPE.

create_resource_type_value ::= USER.
create_resource_type_value ::= SYSTEM.

create_logfile_group ::= GROUP.

create_logfile_group_tail ::= create_add create_undofile string_literal create_logfile_group_options_tail logfile_group_engine_clause.

create_add ::= ADD.

create_datafile ::= DATAFILE.

create_undofile ::= UNDOFILE.

create_logfile_group_options_tail ::= .
create_logfile_group_options_tail ::= create_logfile_group_options_tail create_logfile_group_option.

create_logfile_group_option ::= INITIAL_SIZE drop_index_option_equals_tail tablespace_number_value.
create_logfile_group_option ::= UNDO_BUFFER_SIZE drop_index_option_equals_tail tablespace_number_value.
create_logfile_group_option ::= REDO_BUFFER_SIZE drop_index_option_equals_tail tablespace_number_value.
create_logfile_group_option ::= NODEGROUP drop_index_option_equals_tail tablespace_number_value.
create_logfile_group_option ::= WAIT.
create_logfile_group_option ::= COMMENT drop_index_option_equals_tail string_literal.

alter_logfile_group_tail ::= create_add create_undofile string_literal alter_logfile_group_options_tail logfile_group_engine_clause.

alter_logfile_group_options_tail ::= .
alter_logfile_group_options_tail ::= alter_logfile_group_options_tail alter_logfile_group_option.

alter_logfile_group_option ::= INITIAL_SIZE drop_index_option_equals_tail tablespace_number_value.
alter_logfile_group_option ::= WAIT.

logfile_group_engine_clause ::= ENGINE drop_index_option_equals_tail cache_name_part.

create_tablespace_tail ::= create_tablespace_options_tail.

create_tablespace_options_tail ::= .
create_tablespace_options_tail ::= create_tablespace_options_tail create_tablespace_option.

create_tablespace_option ::= create_add create_datafile string_literal.
create_tablespace_option ::= AUTOEXTEND_SIZE drop_index_option_equals_tail tablespace_number_value.
create_tablespace_option ::= FILE_BLOCK_SIZE drop_index_option_equals_tail tablespace_number_value.
create_tablespace_option ::= ENCRYPTION drop_index_option_equals_tail encryption_value.
create_tablespace_option ::= USE LOGFILE create_logfile_group cache_name_part.
create_tablespace_option ::= EXTENT_SIZE drop_index_option_equals_tail tablespace_number_value.
create_tablespace_option ::= INITIAL_SIZE drop_index_option_equals_tail tablespace_number_value.
create_tablespace_option ::= MAX_SIZE drop_index_option_equals_tail tablespace_number_value.
create_tablespace_option ::= NODEGROUP drop_index_option_equals_tail tablespace_number_value.
create_tablespace_option ::= WAIT.
create_tablespace_option ::= COMMENT drop_index_option_equals_tail string_literal.
create_tablespace_option ::= ENGINE drop_index_option_equals_tail cache_name_part.
create_tablespace_option ::= ENGINE_ATTRIBUTE drop_index_option_equals_tail string_literal.

create_undo_tablespace_tail ::= create_add create_datafile string_literal create_tablespace_post_datafile_options_tail.

create_tablespace_post_datafile_options_tail ::= .
create_tablespace_post_datafile_options_tail ::= create_tablespace_post_datafile_options_tail create_tablespace_post_datafile_option.

create_tablespace_post_datafile_option ::= AUTOEXTEND_SIZE drop_index_option_equals_tail tablespace_number_value.
create_tablespace_post_datafile_option ::= FILE_BLOCK_SIZE drop_index_option_equals_tail tablespace_number_value.
create_tablespace_post_datafile_option ::= ENCRYPTION drop_index_option_equals_tail encryption_value.
create_tablespace_post_datafile_option ::= ENGINE drop_index_option_equals_tail cache_name_part.
create_tablespace_post_datafile_option ::= ENGINE_ATTRIBUTE drop_index_option_equals_tail string_literal.

tablespace_number_value ::= BOOLEAN_NUMBER.
tablespace_number_value ::= FACTOR_NUMBER.
tablespace_number_value ::= NUMBER_LITERAL.

create_server_tail ::= create_foreign DATA create_wrapper cache_name_part create_server_options.

create_foreign ::= FOREIGN.

create_wrapper ::= WRAPPER.

create_server_options ::= create_options_marker LP create_server_option_list RP.

create_options_marker ::= OPTIONS.

create_server_option_list ::= create_server_option.
create_server_option_list ::= create_server_option_list import_comma create_server_option.

create_server_option ::= HOST string_literal.
create_server_option ::= DATABASE string_literal.
create_server_option ::= USER string_literal.
create_server_option ::= PASSWORD string_literal.
create_server_option ::= SOCKET string_literal.
create_server_option ::= OWNER string_literal.
create_server_option ::= PORT create_server_port.

create_server_port ::= BOOLEAN_NUMBER.
create_server_port ::= FACTOR_NUMBER.
create_server_port ::= NUMBER_LITERAL.

create_reference ::= REFERENCE.

create_system ::= SYSTEM.

srs_id ::= BOOLEAN_NUMBER.
srs_id ::= FACTOR_NUMBER.
srs_id ::= NUMBER_LITERAL.

create_srs_attributes ::= create_srs_attribute.
create_srs_attributes ::= create_srs_attributes create_srs_attribute.

create_srs_attribute ::= NAME string_literal.
create_srs_attribute ::= DEFINITION string_literal.
create_srs_attribute ::= DESCRIPTION string_literal.
create_srs_attribute ::= ORGANIZATION string_literal IDENTIFIED BY create_srs_authority_code.

create_srs_authority_code ::= BOOLEAN_NUMBER.
create_srs_authority_code ::= FACTOR_NUMBER.
create_srs_authority_code ::= NUMBER_LITERAL.

drop_statement ::= DROP drop_tail(A). {
  mylite_parser_record_statement(ctx, A);
}

drop_tail(A) ::= USER drop_if_exists_tail drop_user_ref_list drop_account_trailing_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= ROLE drop_if_exists_tail drop_account_list drop_account_trailing_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= drop_table_prefix drop_if_exists_tail drop_name_list drop_restrict_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= LOGFILE drop_logfile_group cache_name_part drop_tablespace_engine_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= RESOURCE drop_resource_group cache_name_part drop_resource_force_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= SPATIAL drop_reference drop_system drop_if_exists_tail srs_id. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= UNDO TABLESPACE cache_name_part drop_tablespace_engine_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= PREPARE prepared_statement_name. {
  A = MYLITE_STATEMENT_PREPARED;
}
drop_tail(A) ::= INDEX drop_index_name ON cache_table_ref drop_index_options_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= drop_database_kind drop_if_exists_tail cache_name_part. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= drop_routine_kind drop_if_exists_tail cache_table_ref. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= EVENT drop_if_exists_tail cache_table_ref. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= TRIGGER drop_if_exists_tail cache_table_ref. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= VIEW drop_if_exists_tail drop_name_list drop_restrict_tail. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= SERVER drop_if_exists_tail cache_name_part. {
  A = MYLITE_STATEMENT_DDL;
}
drop_tail(A) ::= TABLESPACE cache_name_part drop_tablespace_engine_tail. {
  A = MYLITE_STATEMENT_DDL;
}

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

drop_account_ident ::= set_charset_name_part.
drop_account_ident ::= MASTER.
drop_account_ident ::= ROLE.

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
drop_tablespace_engine_tail ::= ENGINE drop_index_option_equals_tail cache_name_part.

drop_index_name ::= cache_name_part.

drop_index_options_tail ::= .
drop_index_options_tail ::= drop_index_options_tail drop_index_option.

drop_index_option ::= ALGORITHM drop_index_option_equals_tail drop_index_algorithm.
drop_index_option ::= LOCK drop_index_option_equals_tail drop_index_lock.

drop_index_option_equals_tail ::= .
drop_index_option_equals_tail ::= diagnostics_equals.

drop_index_algorithm ::= DEFAULT.
drop_index_algorithm ::= INPLACE.
drop_index_algorithm ::= COPY.

drop_index_lock ::= DEFAULT.
drop_index_lock ::= NONE.
drop_index_lock ::= SHARED.
drop_index_lock ::= EXCLUSIVE.

drop_database_kind ::= DATABASE.
drop_database_kind ::= SCHEMA.

drop_if_exists_tail ::= .
drop_if_exists_tail ::= IF reset_exists.

alter_statement ::= ALTER alter_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

alter_tail ::= TABLE(A) cache_table_ref. {
  mylite_parser_require_permissive(ctx, A);
}
alter_tail ::= TABLE cache_table_ref alter_table_tail.
alter_tail ::= LOGFILE create_logfile_group cache_name_part alter_logfile_group_tail.
alter_tail ::= RESOURCE create_resource_group cache_name_part alter_resource_group_actions.
alter_tail ::= SERVER cache_name_part create_server_options.
alter_tail ::= TABLESPACE cache_name_part alter_tablespace_action.
alter_tail ::= UNDO TABLESPACE cache_name_part alter_undo_tablespace_action.
alter_tail ::= USER drop_if_exists_tail alter_user_list account_management_options account_management_permissive_tail.
alter_tail ::= USER drop_if_exists_tail USER LP RP alter_user_func_option_tail.
alter_tail ::= EVENT cache_table_ref alter_event_clauses.
alter_tail ::= alter_routine_kind cache_table_ref alter_routine_characteristics_tail.
alter_tail ::= alter_database_kind alter_database_options.
alter_tail ::= alter_database_kind alter_database_name alter_database_options.
alter_tail ::= VIEW cache_table_ref view_column_tail view_body.
alter_tail ::= alter_prefixed_view_tail.
alter_tail ::= create_definer_clause alter_definer_object_tail.
alter_tail ::= INSTANCE alter_instance_action.

alter_database_kind ::= DATABASE.
alter_database_kind ::= SCHEMA.

alter_database_name ::= set_charset_name_part.

alter_database_options ::= alter_database_option.
alter_database_options ::= alter_database_options alter_database_option.

alter_database_option ::= create_database_option.
alter_database_option ::= READ ONLY drop_index_option_equals_tail alter_database_read_value.

alter_database_read_value ::= DEFAULT.
alter_database_read_value ::= BOOLEAN_NUMBER.

alter_routine_kind ::= FUNCTION.
alter_routine_kind ::= PROCEDURE.

alter_routine_characteristics_tail ::= .
alter_routine_characteristics_tail ::= alter_routine_characteristics_tail alter_routine_characteristic.

alter_routine_characteristic ::= COMMENT routine_comment_value.
alter_routine_characteristic ::= LANGUAGE SQL.
alter_routine_characteristic ::= CONTAINS SQL.
alter_routine_characteristic ::= NO SQL.
alter_routine_characteristic ::= READS SQL DATA.
alter_routine_characteristic ::= MODIFIES SQL DATA.
alter_routine_characteristic ::= SQL SECURITY create_view_security_kind.

routine_comment_value ::= string_literal.

alter_table_tail ::= FORCE alter_table_force_option_tail.
alter_table_tail ::= alter_table_keys_action alter_table_force_option_tail.
alter_table_tail ::= alter_table_tablespace_transfer_kind TABLESPACE.
alter_table_tail ::= alter_table_tablespace_transfer_kind PARTITION load_partition_names TABLESPACE.
alter_table_tail ::= alter_table_algorithm_lock_option alter_table_algorithm_lock_tail.
alter_table_tail ::= alter_table_rename_action alter_table_rename_action_tail.
alter_table_tail ::= alter_table_drop_action alter_table_drop_action_tail.
alter_table_tail ::= alter_table_drop_partition_action.
alter_table_tail ::= alter_table_alter_action alter_table_alter_action_tail.
alter_table_tail ::= alter_table_alter_set_default_action.
alter_table_tail ::= alter_table_partition_action.
alter_table_tail ::= alter_table_partition_definition_action.
alter_table_tail ::= alter_table_add_action.
alter_table_tail ::= alter_table_change_action.
alter_table_tail ::= alter_table_modify_action.
alter_table_tail ::= alter_table_table_option create_options_tail.
alter_table_tail ::= alter_table_charset_action alter_table_charset_action_tail.
alter_table_tail ::= alter_table_order_action.
alter_table_tail ::= alter_table_tablespace_action.
alter_table_tail ::= alter_table_storage_action.
alter_table_tail ::= alter_table_union_action.
alter_table_tail ::= alter_table_reorganize_action.
alter_table_tail ::= alter_table_secondary_action.

alter_table_tablespace_transfer_kind ::= DISCARD.
alter_table_tablespace_transfer_kind ::= IMPORT.

alter_table_force_option_tail ::= .
alter_table_force_option_tail ::= COMMA alter_table_force_options.

alter_table_force_options ::= alter_table_force_option.
alter_table_force_options ::= alter_table_force_options COMMA alter_table_force_option.

alter_table_force_option ::= ALGORITHM drop_index_option_equals_tail alter_table_algorithm_value.
alter_table_force_option ::= LOCK drop_index_option_equals_tail drop_index_lock.

alter_table_algorithm_lock_tail ::= .
alter_table_algorithm_lock_tail ::= COMMA alter_table_algorithm_lock_after_comma.

alter_table_algorithm_lock_after_comma ::= alter_table_algorithm_lock_option alter_table_algorithm_lock_tail.
alter_table_algorithm_lock_after_comma ::= alter_table_rename_action alter_table_rename_action_tail.
alter_table_algorithm_lock_after_comma ::= alter_table_drop_action alter_table_drop_action_tail.
alter_table_algorithm_lock_after_comma ::= alter_table_drop_partition_action.
alter_table_algorithm_lock_after_comma ::= alter_table_alter_action alter_table_alter_action_tail.
alter_table_algorithm_lock_after_comma ::= alter_table_alter_set_default_action.
alter_table_algorithm_lock_after_comma ::= alter_table_partition_action.
alter_table_algorithm_lock_after_comma ::= alter_table_partition_definition_action.
alter_table_algorithm_lock_after_comma ::= alter_table_add_action.
alter_table_algorithm_lock_after_comma ::= alter_table_change_action.
alter_table_algorithm_lock_after_comma ::= alter_table_modify_action.
alter_table_algorithm_lock_after_comma ::= alter_table_table_option create_options_tail.
alter_table_algorithm_lock_after_comma ::= alter_table_charset_action alter_table_charset_action_tail.
alter_table_algorithm_lock_after_comma ::= alter_table_order_action.
alter_table_algorithm_lock_after_comma ::= alter_table_tablespace_action.
alter_table_algorithm_lock_after_comma ::= alter_table_storage_action.
alter_table_algorithm_lock_after_comma ::= alter_table_union_action.
alter_table_algorithm_lock_after_comma ::= alter_table_reorganize_action.
alter_table_algorithm_lock_after_comma ::= alter_table_keys_action alter_table_force_option_tail.
alter_table_algorithm_lock_after_comma ::= alter_table_secondary_action.

alter_table_algorithm_lock_option ::= ALGORITHM drop_index_option_equals_tail alter_table_algorithm_value.
alter_table_algorithm_lock_option ::= LOCK drop_index_option_equals_tail drop_index_lock.

alter_table_algorithm_value ::= DEFAULT.
alter_table_algorithm_value ::= INSTANT.
alter_table_algorithm_value ::= INPLACE.
alter_table_algorithm_value ::= COPY.

alter_table_rename_action_tail ::= .
alter_table_rename_action_tail ::= COMMA alter_table_rename_after_comma.
alter_table_rename_action_tail ::= alter_table_trailing_partition_option.

alter_table_rename_after_comma ::= alter_table_algorithm_lock_option alter_table_algorithm_lock_tail.
alter_table_rename_after_comma ::= alter_table_rename_action alter_table_rename_action_tail.
alter_table_rename_after_comma ::= alter_table_drop_action alter_table_drop_action_tail.
alter_table_rename_after_comma ::= alter_table_drop_partition_action.
alter_table_rename_after_comma ::= alter_table_alter_action alter_table_alter_action_tail.
alter_table_rename_after_comma ::= alter_table_alter_set_default_action.
alter_table_rename_after_comma ::= alter_table_partition_action.
alter_table_rename_after_comma ::= alter_table_partition_definition_action.
alter_table_rename_after_comma ::= alter_table_add_action.
alter_table_rename_after_comma ::= alter_table_change_action.
alter_table_rename_after_comma ::= alter_table_modify_action.
alter_table_rename_after_comma ::= alter_table_table_option create_options_tail.
alter_table_rename_after_comma ::= alter_table_charset_action alter_table_charset_action_tail.
alter_table_rename_after_comma ::= alter_table_order_action.
alter_table_rename_after_comma ::= alter_table_tablespace_action.
alter_table_rename_after_comma ::= alter_table_storage_action.
alter_table_rename_after_comma ::= alter_table_union_action.
alter_table_rename_after_comma ::= alter_table_reorganize_action.
alter_table_rename_after_comma ::= alter_table_keys_action alter_table_force_option_tail.

alter_table_rename_action ::= RENAME alter_table_rename_table_connector cache_table_ref.
alter_table_rename_action ::= RENAME COLUMN alter_table_rename_identifier TO alter_table_rename_identifier.
alter_table_rename_action ::= RENAME alter_table_rename_index_kind alter_table_rename_identifier TO alter_table_rename_identifier.

alter_table_rename_table_connector ::= .
alter_table_rename_table_connector ::= TO.
alter_table_rename_table_connector ::= AS.

alter_table_rename_index_kind ::= INDEX.
alter_table_rename_index_kind ::= KEY.

alter_table_rename_identifier ::= cache_name_part.

alter_table_drop_action_tail ::= .
alter_table_drop_action_tail ::= COMMA alter_table_drop_after_comma.
alter_table_drop_action_tail ::= alter_table_trailing_partition_option.

alter_table_drop_after_comma ::= alter_table_algorithm_lock_option alter_table_algorithm_lock_tail.
alter_table_drop_after_comma ::= alter_table_rename_action alter_table_rename_action_tail.
alter_table_drop_after_comma ::= alter_table_drop_action alter_table_drop_action_tail.
alter_table_drop_after_comma ::= alter_table_drop_partition_action.
alter_table_drop_after_comma ::= alter_table_alter_action alter_table_alter_action_tail.
alter_table_drop_after_comma ::= alter_table_alter_set_default_action.
alter_table_drop_after_comma ::= alter_table_partition_action.
alter_table_drop_after_comma ::= alter_table_partition_definition_action.
alter_table_drop_after_comma ::= alter_table_add_action.
alter_table_drop_after_comma ::= alter_table_change_action.
alter_table_drop_after_comma ::= alter_table_modify_action.
alter_table_drop_after_comma ::= alter_table_table_option create_options_tail.
alter_table_drop_after_comma ::= alter_table_charset_action alter_table_charset_action_tail.
alter_table_drop_after_comma ::= alter_table_order_action.
alter_table_drop_after_comma ::= alter_table_tablespace_action.
alter_table_drop_after_comma ::= alter_table_storage_action.
alter_table_drop_after_comma ::= alter_table_union_action.
alter_table_drop_after_comma ::= alter_table_reorganize_action.
alter_table_drop_after_comma ::= alter_table_keys_action alter_table_force_option_tail.

alter_table_drop_action ::= DROP alter_table_column_keyword_tail alter_table_drop_identifier.
alter_table_drop_action ::= DROP PRIMARY KEY.
alter_table_drop_action ::= DROP alter_table_drop_index_kind alter_table_drop_identifier.
alter_table_drop_action ::= DROP FOREIGN KEY alter_table_drop_identifier.
alter_table_drop_action ::= DROP CHECK alter_table_drop_identifier.
alter_table_drop_action ::= DROP CONSTRAINT alter_table_drop_identifier.

alter_table_drop_partition_action ::= DROP PARTITION alter_table_partition_names.

alter_table_column_keyword_tail ::= .
alter_table_column_keyword_tail ::= COLUMN.

alter_table_drop_index_kind ::= INDEX.
alter_table_drop_index_kind ::= KEY.

alter_table_drop_identifier ::= cache_name_part.

alter_table_alter_action_tail ::= .
alter_table_alter_action_tail ::= COMMA alter_table_alter_after_comma.
alter_table_alter_action_tail ::= alter_table_trailing_partition_option.

alter_table_alter_after_comma ::= alter_table_algorithm_lock_option alter_table_algorithm_lock_tail.
alter_table_alter_after_comma ::= alter_table_rename_action alter_table_rename_action_tail.
alter_table_alter_after_comma ::= alter_table_drop_action alter_table_drop_action_tail.
alter_table_alter_after_comma ::= alter_table_drop_partition_action.
alter_table_alter_after_comma ::= alter_table_alter_action alter_table_alter_action_tail.
alter_table_alter_after_comma ::= alter_table_alter_set_default_action.
alter_table_alter_after_comma ::= alter_table_partition_action.
alter_table_alter_after_comma ::= alter_table_partition_definition_action.
alter_table_alter_after_comma ::= alter_table_add_action.
alter_table_alter_after_comma ::= alter_table_change_action.
alter_table_alter_after_comma ::= alter_table_modify_action.
alter_table_alter_after_comma ::= alter_table_table_option create_options_tail.
alter_table_alter_after_comma ::= alter_table_charset_action alter_table_charset_action_tail.
alter_table_alter_after_comma ::= alter_table_order_action.
alter_table_alter_after_comma ::= alter_table_tablespace_action.
alter_table_alter_after_comma ::= alter_table_storage_action.
alter_table_alter_after_comma ::= alter_table_union_action.
alter_table_alter_after_comma ::= alter_table_reorganize_action.
alter_table_alter_after_comma ::= alter_table_keys_action alter_table_force_option_tail.

alter_table_alter_set_default_action ::= ALTER alter_table_column_keyword_tail alter_table_drop_identifier SET DEFAULT required_statement_tail.

alter_table_alter_action ::= ALTER alter_table_column_keyword_tail alter_table_drop_identifier DROP DEFAULT.
alter_table_alter_action ::= ALTER alter_table_column_keyword_tail alter_table_drop_identifier SET alter_table_visibility.
alter_table_alter_action ::= ALTER INDEX alter_table_drop_identifier alter_table_visibility.
alter_table_alter_action ::= ALTER alter_table_check_constraint_kind alter_table_drop_identifier alter_table_enforcement.

alter_table_check_constraint_kind ::= CHECK.
alter_table_check_constraint_kind ::= CONSTRAINT.

alter_table_enforcement ::= ENFORCED.
alter_table_enforcement ::= NOT ENFORCED.

alter_table_visibility ::= VISIBLE.
alter_table_visibility ::= INVISIBLE.

alter_table_partition_action ::= alter_table_partition_maintenance_kind PARTITION load_partition_names.
alter_table_partition_action ::= COALESCE PARTITION alter_table_partition_count.
alter_table_partition_action ::= REMOVE PARTITIONING.
alter_table_partition_action ::= EXCHANGE PARTITION alter_table_partition_name WITH TABLE cache_table_ref alter_table_exchange_validation_tail.

alter_table_partition_count ::= BOOLEAN_NUMBER.
alter_table_partition_count ::= FACTOR_NUMBER.
alter_table_partition_count ::= NUMBER_LITERAL.

alter_table_partition_definition_action ::= PARTITION BY required_statement_tail.

alter_table_trailing_partition_option ::= REMOVE PARTITIONING.
alter_table_trailing_partition_option ::= PARTITION BY required_statement_tail.

alter_table_add_action ::= ADD LP create_table_definition_tokens RP alter_table_definition_action_tail.
alter_table_add_action ::= ADD COLUMN LP create_table_definition_tokens RP alter_table_definition_action_tail.
alter_table_add_action ::= ADD alter_table_add_start alter_table_definition_tokens alter_table_definition_action_tail.

alter_table_add_start ::= alter_table_column_keyword_tail alter_table_column_definition_start.
alter_table_add_start ::= alter_table_index_definition_start.
alter_table_add_start ::= CONSTRAINT.
alter_table_add_start ::= FOREIGN.
alter_table_add_start ::= CHECK.
alter_table_add_start ::= PARTITION.

alter_table_column_definition_start ::= alter_table_column_name.

alter_table_index_definition_start ::= PRIMARY.
alter_table_index_definition_start ::= UNIQUE.
alter_table_index_definition_start ::= FULLTEXT.
alter_table_index_definition_start ::= SPATIAL.
alter_table_index_definition_start ::= INDEX.
alter_table_index_definition_start ::= KEY.

alter_table_change_action ::= CHANGE alter_table_column_keyword_tail alter_table_column_name alter_table_column_name alter_table_definition_tokens alter_table_definition_action_tail.

alter_table_column_name ::= cache_name_part.
alter_table_column_name ::= DATA.

alter_table_modify_action ::= MODIFY alter_table_column_keyword_tail alter_table_column_name alter_table_definition_tokens alter_table_definition_action_tail.

alter_table_definition_action_tail ::= .
alter_table_definition_action_tail ::= COMMA alter_table_algorithm_lock_after_comma.

alter_table_definition_tokens ::= alter_table_definition_token.
alter_table_definition_tokens ::= alter_table_definition_tokens alter_table_definition_token.

alter_table_definition_token ::= ATOM.
alter_table_definition_token ::= LABEL.
alter_table_definition_token ::= keyword.
alter_table_definition_token ::= DOT.
alter_table_definition_token ::= LP create_table_definition_tokens RP.
alter_table_definition_token ::= LB.
alter_table_definition_token ::= RB.
alter_table_definition_token ::= LC.
alter_table_definition_token ::= RC.

alter_table_partition_maintenance_kind ::= ANALYZE.
alter_table_partition_maintenance_kind ::= CHECK.
alter_table_partition_maintenance_kind ::= OPTIMIZE.
alter_table_partition_maintenance_kind ::= REBUILD.
alter_table_partition_maintenance_kind ::= REPAIR.
alter_table_partition_maintenance_kind ::= TRUNCATE.

alter_table_exchange_validation_tail ::= .
alter_table_exchange_validation_tail ::= WITH VALIDATION.
alter_table_exchange_validation_tail ::= WITHOUT VALIDATION.

alter_table_table_option ::= alter_table_number_table_option drop_index_option_equals_tail alter_table_number_value.
alter_table_table_option ::= alter_table_boolean_table_option drop_index_option_equals_tail BOOLEAN_NUMBER.
alter_table_table_option ::= alter_table_default_boolean_table_option drop_index_option_equals_tail alter_table_default_boolean_value.
alter_table_table_option ::= STATS_SAMPLE_PAGES drop_index_option_equals_tail alter_table_default_number_value.
alter_table_table_option ::= COMMENT drop_index_option_equals_tail string_literal.
alter_table_table_option ::= ENCRYPTION drop_index_option_equals_tail encryption_value.
alter_table_table_option ::= ENGINE_ATTRIBUTE drop_index_option_equals_tail string_literal.
alter_table_table_option ::= ENGINE drop_index_option_equals_tail alter_table_table_option_value.
alter_table_table_option ::= INSERT_METHOD drop_index_option_equals_tail alter_table_table_option_value.
alter_table_table_option ::= ROW_FORMAT drop_index_option_equals_tail alter_table_table_option_value.
alter_table_table_option ::= SECONDARY_ENGINE drop_index_option_equals_tail alter_table_table_option_value.
alter_table_table_option ::= SECONDARY_ENGINE_ATTRIBUTE drop_index_option_equals_tail string_literal.

alter_table_number_table_option ::= AUTO_INCREMENT.
alter_table_number_table_option ::= AVG_ROW_LENGTH.
alter_table_number_table_option ::= KEY_BLOCK_SIZE.
alter_table_number_table_option ::= MAX_ROWS.
alter_table_number_table_option ::= MIN_ROWS.

alter_table_boolean_table_option ::= CHECKSUM.
alter_table_boolean_table_option ::= DELAY_KEY_WRITE.

alter_table_default_boolean_table_option ::= PACK_KEYS.
alter_table_default_boolean_table_option ::= STATS_AUTO_RECALC.
alter_table_default_boolean_table_option ::= STATS_PERSISTENT.

alter_table_number_value ::= BOOLEAN_NUMBER.
alter_table_number_value ::= FACTOR_NUMBER.
alter_table_number_value ::= NUMBER_LITERAL.

alter_table_default_number_value ::= alter_table_number_value.
alter_table_default_number_value ::= DEFAULT.

alter_table_default_boolean_value ::= BOOLEAN_NUMBER.
alter_table_default_boolean_value ::= DEFAULT.

alter_table_table_option_value ::= cache_name_part.
alter_table_table_option_value ::= NO.

alter_table_charset_action_tail ::= .
alter_table_charset_action_tail ::= COMMA alter_table_charset_after_comma.

alter_table_charset_after_comma ::= alter_table_algorithm_lock_option alter_table_algorithm_lock_tail.
alter_table_charset_after_comma ::= alter_table_rename_action alter_table_rename_action_tail.
alter_table_charset_after_comma ::= alter_table_drop_action alter_table_drop_action_tail.
alter_table_charset_after_comma ::= alter_table_drop_partition_action.
alter_table_charset_after_comma ::= alter_table_alter_action alter_table_alter_action_tail.
alter_table_charset_after_comma ::= alter_table_alter_set_default_action.
alter_table_charset_after_comma ::= alter_table_partition_action.
alter_table_charset_after_comma ::= alter_table_partition_definition_action.
alter_table_charset_after_comma ::= alter_table_add_action.
alter_table_charset_after_comma ::= alter_table_change_action.
alter_table_charset_after_comma ::= alter_table_modify_action.
alter_table_charset_after_comma ::= alter_table_table_option create_options_tail.
alter_table_charset_after_comma ::= alter_table_charset_action alter_table_charset_action_tail.
alter_table_charset_after_comma ::= alter_table_order_action.
alter_table_charset_after_comma ::= alter_table_tablespace_action.
alter_table_charset_after_comma ::= alter_table_storage_action.
alter_table_charset_after_comma ::= alter_table_union_action.
alter_table_charset_after_comma ::= alter_table_reorganize_action.
alter_table_charset_after_comma ::= alter_table_keys_action alter_table_force_option_tail.

alter_table_charset_action ::= database_default_tail alter_table_character_set_kind drop_index_option_equals_tail alter_table_charset_name alter_table_collate_tail.
alter_table_charset_action ::= CONVERT TO alter_table_character_set_kind drop_index_option_equals_tail alter_table_charset_name alter_table_collate_tail.

alter_table_character_set_kind ::= CHARACTER SET.
alter_table_character_set_kind ::= CHARSET.

alter_table_charset_name ::= set_charset_name.
alter_table_charset_name ::= DEFAULT.

alter_table_collate_tail ::= .
alter_table_collate_tail ::= database_collate_option.

alter_table_order_action ::= ORDER BY required_statement_tail.

alter_table_tablespace_action ::= TABLESPACE alter_table_drop_identifier create_options_tail.

alter_table_storage_action ::= STORAGE alter_table_table_option_value create_options_tail.

alter_table_union_action ::= UNION drop_index_option_equals_tail LP alter_table_union_tables_tail RP.

alter_table_union_tables_tail ::= .
alter_table_union_tables_tail ::= alter_table_union_tables.

alter_table_union_tables ::= cache_table_ref.
alter_table_union_tables ::= alter_table_union_tables COMMA cache_table_ref.

alter_table_reorganize_action ::= REORGANIZE PARTITION alter_table_partition_names INTO LP alter_table_reorganize_definitions RP.
alter_table_reorganize_action ::= REORGANIZE(A) PARTITION. {
  mylite_parser_require_permissive(ctx, A);
}

alter_table_reorganize_definitions ::= create_table_definition_token.
alter_table_reorganize_definitions ::= alter_table_reorganize_definitions create_table_definition_token.

alter_table_secondary_action ::= SECONDARY_LOAD alter_table_secondary_partition_tail.
alter_table_secondary_action ::= SECONDARY_UNLOAD alter_table_secondary_partition_tail.

alter_table_secondary_partition_tail ::= .
alter_table_secondary_partition_tail ::= PARTITION LP load_partition_names RP.

alter_table_partition_names ::= alter_table_partition_name.
alter_table_partition_names ::= alter_table_partition_names COMMA alter_table_partition_name.

alter_table_partition_name ::= cache_name_part.

alter_table_keys_action ::= ENABLE KEYS.
alter_table_keys_action ::= DISABLE KEYS.

alter_prefixed_view_tail ::= alter_view_prefix VIEW cache_table_ref view_column_tail view_body.

alter_view_prefix ::= create_view_algorithm create_view_definer_tail create_view_sql_security_tail.
alter_view_prefix ::= create_definer_clause create_view_sql_security_tail.
alter_view_prefix ::= create_view_sql_security.

alter_definer_object_tail ::= EVENT cache_table_ref alter_event_clauses.

alter_event_clauses ::= alter_event_schedule_clause alter_event_completion_tail alter_event_rename_tail alter_event_status_tail alter_event_comment_tail alter_event_do_tail.
alter_event_clauses ::= alter_event_completion_clause alter_event_rename_tail alter_event_status_tail alter_event_comment_tail alter_event_do_tail.
alter_event_clauses ::= alter_event_rename_clause alter_event_status_tail alter_event_comment_tail alter_event_do_tail.
alter_event_clauses ::= alter_event_status_clause alter_event_comment_tail alter_event_do_tail.
alter_event_clauses ::= alter_event_comment_clause alter_event_do_tail.
alter_event_clauses ::= alter_event_do_clause.

create_event_do ::= DO.

alter_event_schedule_clause ::= ON SCHEDULE event_schedule_start alter_event_schedule_tokens.

alter_event_completion_tail ::= .
alter_event_completion_tail ::= alter_event_completion_clause.

alter_event_completion_clause ::= ON COMPLETION alter_event_completion.

alter_event_completion ::= PRESERVE.
alter_event_completion ::= NOT PRESERVE.

alter_event_rename_tail ::= .
alter_event_rename_tail ::= alter_event_rename_clause.

alter_event_rename_clause ::= RENAME TO cache_table_ref.

alter_event_status_tail ::= .
alter_event_status_tail ::= alter_event_status_clause.

alter_event_status_clause ::= ENABLE.
alter_event_status_clause ::= DISABLE alter_event_disable_tail.

alter_event_disable_tail ::= .
alter_event_disable_tail ::= ON REPLICA.
alter_event_disable_tail ::= ON SLAVE.

alter_event_comment_tail ::= .
alter_event_comment_tail ::= alter_event_comment_clause.

alter_event_comment_clause ::= COMMENT routine_comment_value.

alter_event_do_tail ::= .
alter_event_do_tail ::= alter_event_do_clause.

alter_event_do_clause ::= DO event_statement_start statement_tail.

alter_event_schedule_tokens ::= alter_event_schedule_token.
alter_event_schedule_tokens ::= alter_event_schedule_tokens alter_event_schedule_token.

alter_event_schedule_nested ::= .
alter_event_schedule_nested ::= alter_event_schedule_tokens.

alter_event_schedule_token ::= ATOM.
alter_event_schedule_token ::= LABEL.
alter_event_schedule_token ::= AT_SIGN.
alter_event_schedule_token ::= COMMA.
alter_event_schedule_token ::= DOT.
alter_event_schedule_token ::= EQUALS.
alter_event_schedule_token ::= MINUS.
alter_event_schedule_token ::= STAR.
alter_event_schedule_token ::= AS.
alter_event_schedule_token ::= BY.
alter_event_schedule_token ::= CURRENT.
alter_event_schedule_token ::= CURRENT_USER.
alter_event_schedule_token ::= DAY.
alter_event_schedule_token ::= EXISTS.
alter_event_schedule_token ::= FROM.
alter_event_schedule_token ::= IN.
alter_event_schedule_token ::= INTERVAL.
alter_event_schedule_token ::= LIMIT.
alter_event_schedule_token ::= NOT.
alter_event_schedule_token ::= ORDER.
alter_event_schedule_token ::= SELECT.
alter_event_schedule_token ::= WHERE.
alter_event_schedule_token ::= LP alter_event_schedule_nested RP.

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
alter_tablespace_action ::= ADD create_datafile string_literal alter_tablespace_datafile_options_tail.
alter_tablespace_action ::= DROP create_datafile string_literal alter_tablespace_datafile_options_tail.
alter_tablespace_action ::= AUTOEXTEND_SIZE drop_index_option_equals_tail tablespace_number_value.
alter_tablespace_action ::= ENCRYPTION drop_index_option_equals_tail encryption_value.
alter_tablespace_action ::= ENGINE drop_index_option_equals_tail cache_name_part.
alter_tablespace_action ::= ENGINE_ATTRIBUTE drop_index_option_equals_tail string_literal.

alter_tablespace_datafile_options_tail ::= .
alter_tablespace_datafile_options_tail ::= alter_tablespace_datafile_options_tail alter_tablespace_datafile_option.

alter_tablespace_datafile_option ::= INITIAL_SIZE drop_index_option_equals_tail tablespace_number_value.
alter_tablespace_datafile_option ::= WAIT.
alter_tablespace_datafile_option ::= ENGINE drop_index_option_equals_tail cache_name_part.

alter_undo_tablespace_action ::= SET alter_undo_tablespace_state drop_tablespace_engine_tail.

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

rename_user_account ::= drop_account_name.
rename_user_account ::= current_user_ref.

current_user_ref ::= CURRENT_USER.
current_user_ref ::= CURRENT_USER LP RP.

truncate_statement ::= TRUNCATE truncate_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

truncate_tail ::= TABLE truncate_table_ref.
truncate_tail ::= truncate_table_ref.

truncate_table_ref ::= truncate_table_part.
truncate_table_ref ::= truncate_table_part DOT truncate_table_part.

truncate_table_part ::= cache_name_part.

load_statement ::= LOAD load_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

load_tail ::= DATA load_file_tail.
load_tail ::= XML load_xml_tail.
load_tail ::= INDEX INTO CACHE load_index_table_list.

load_file_tail ::= load_file_priority_tail load_file_local_tail load_infile load_file_name load_duplicate_tail INTO TABLE cache_table_ref load_data_options.
load_xml_tail ::= load_file_priority_tail load_file_local_tail load_infile load_file_name load_duplicate_tail INTO TABLE cache_table_ref load_xml_options.

load_file_priority_tail ::= .
load_file_priority_tail ::= LOW_PRIORITY.
load_file_priority_tail ::= CONCURRENT.

load_file_local_tail ::= .
load_file_local_tail ::= LOCAL.

load_infile ::= INFILE.

load_file_name ::= string_literal.

load_duplicate_tail ::= .
load_duplicate_tail ::= IGNORE.
load_duplicate_tail ::= REPLACE.

load_data_options ::= load_partition_tail load_character_set_tail load_fields_tail load_lines_tail load_ignore_tail load_column_list_tail load_set_tail.
load_xml_options ::= load_character_set_tail load_xml_rows_tail load_ignore_tail load_column_list_tail load_set_tail.

load_partition_tail ::= .
load_partition_tail ::= load_partition LP load_partition_names RP.

load_character_set_tail ::= .
load_character_set_tail ::= CHARACTER SET set_charset_name.

load_fields_tail ::= .
load_fields_tail ::= load_fields_kind load_field_options.

load_fields_kind ::= FIELDS.
load_fields_kind ::= COLUMNS.

load_field_options ::= .
load_field_options ::= load_field_options load_field_option.

load_field_option ::= TERMINATED BY load_file_name.
load_field_option ::= OPTIONALLY ENCLOSED BY load_file_name.
load_field_option ::= ENCLOSED BY load_file_name.
load_field_option ::= ESCAPED BY load_file_name.

load_lines_tail ::= .
load_lines_tail ::= LINES load_line_options.

load_line_options ::= .
load_line_options ::= load_line_options load_line_option.

load_line_option ::= STARTING BY load_file_name.
load_line_option ::= TERMINATED BY load_file_name.

load_xml_rows_tail ::= .
load_xml_rows_tail ::= ROWS IDENTIFIED BY load_file_name.

load_ignore_tail ::= .
load_ignore_tail ::= IGNORE load_ignore_count load_ignore_unit.

load_ignore_count ::= BOOLEAN_NUMBER.
load_ignore_count ::= FACTOR_NUMBER.
load_ignore_count ::= NUMBER_LITERAL.

load_ignore_unit ::= LINES.
load_ignore_unit ::= ROWS.

load_column_list_tail ::= .
load_column_list_tail ::= LP RP.
load_column_list_tail ::= LP load_column_list RP.

load_column_list ::= load_column_ref.
load_column_list ::= load_column_list import_comma load_column_ref.

load_column_ref ::= cache_name_part.
load_column_ref ::= user_variable_name.

load_set_tail ::= .
load_set_tail ::= SET update_assignment_start.

load_index_table_list ::= load_index_table_spec.
load_index_table_list ::= load_index_table_list import_comma load_index_table_spec.

load_index_table_spec ::= cache_table_ref load_index_partition_tail load_index_key_tail load_index_ignore_tail.

load_index_partition_tail ::= .
load_index_partition_tail ::= load_index_partition.

load_index_key_tail ::= .
load_index_key_tail ::= cache_index_kind cache_key_list.

load_index_ignore_tail ::= .
load_index_ignore_tail ::= IGNORE load_leaves.

load_index_partition ::= load_partition LP load_partition_names RP.

load_partition ::= PARTITION.

load_partition_names ::= load_partition_name.
load_partition_names ::= load_partition_names import_comma load_partition_name.

load_partition_name ::= cache_name_part.
load_partition_name ::= ALL.

load_leaves ::= LEAVES.

start_statement ::= START start_tail(A). {
  mylite_parser_record_statement(ctx, A);
}

start_tail(A) ::= TRANSACTION start_transaction_tail. {
  A = MYLITE_STATEMENT_TRANSACTION;
}
start_tail(A) ::= REPLICA start_replica_tail. {
  A = MYLITE_STATEMENT_REPLICATION;
}
start_tail(A) ::= SLAVE(B) start_replica_tail. {
  mylite_parser_require_permissive(ctx, B);
  A = MYLITE_STATEMENT_REPLICATION;
}
start_tail(A) ::= GROUP_REPLICATION start_group_replication_tail. {
  A = MYLITE_STATEMENT_REPLICATION;
}

start_transaction_tail ::= .
start_transaction_tail ::= transaction_characteristics.

transaction_characteristics ::= transaction_characteristic.
transaction_characteristics ::= transaction_characteristics import_comma transaction_characteristic.

transaction_characteristic ::= READ transaction_access_mode.
transaction_characteristic ::= WITH transaction_consistent transaction_snapshot.

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

start_until_spec ::= SQL_BEFORE_GTIDS start_option_equals string_literal.
start_until_spec ::= SQL_AFTER_GTIDS start_option_equals string_literal.
start_until_spec ::= SOURCE_LOG_FILE start_option_equals string_literal import_comma SOURCE_LOG_POS start_option_equals start_log_position_value.
start_until_spec ::= RELAY_LOG_FILE start_option_equals string_literal import_comma RELAY_LOG_POS start_option_equals start_log_position_value.
start_until_spec ::= SQL_AFTER_MTS_GAPS.

start_log_position_value ::= BOOLEAN_NUMBER.
start_log_position_value ::= FACTOR_NUMBER.
start_log_position_value ::= NUMBER_LITERAL.

start_connection_tail ::= start_user_option start_password_option start_default_auth_option start_plugin_dir_option.

start_user_option ::= .
start_user_option ::= USER start_option_equals string_literal.

start_password_option ::= .
start_password_option ::= PASSWORD start_option_equals string_literal.

start_default_auth_option ::= .
start_default_auth_option ::= DEFAULT_AUTH start_option_equals string_literal.

start_plugin_dir_option ::= .
start_plugin_dir_option ::= PLUGIN_DIR start_option_equals string_literal.

start_group_replication_tail ::= .
start_group_replication_tail ::= start_group_replication_options.

start_group_replication_options ::= start_group_replication_option.
start_group_replication_options ::= start_group_replication_options import_comma start_group_replication_option.

start_group_replication_option ::= USER start_option_equals string_literal.
start_group_replication_option ::= PASSWORD start_option_equals string_literal.
start_group_replication_option ::= DEFAULT_AUTH start_option_equals string_literal.

start_option_equals ::= EQUALS.

stop_statement ::= STOP stop_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

stop_tail ::= REPLICA stop_replica_tail.
stop_tail ::= SLAVE(A) stop_replica_tail. {
  mylite_parser_require_permissive(ctx, A);
}
stop_tail ::= GROUP_REPLICATION.

stop_replica_tail ::= start_thread_tail show_channel_tail.

savepoint_statement ::= SAVEPOINT savepoint_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

savepoint_name ::= cache_name_part.

release_statement ::= RELEASE SAVEPOINT savepoint_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

lock_statement ::= LOCK lock_tail(A). {
  mylite_parser_record_statement(ctx, A);
}

lock_tail(A) ::= lock_table_kind lock_table_list. {
  A = MYLITE_STATEMENT_TRANSACTION;
}
lock_tail(A) ::= INSTANCE FOR lock_backup. {
  A = MYLITE_STATEMENT_ADMIN;
}

lock_backup ::= BACKUP.

lock_table_kind ::= TABLE.
lock_table_kind ::= TABLES.

lock_table_list ::= lock_table_spec.
lock_table_list ::= lock_table_list import_comma lock_table_spec.

lock_table_spec ::= cache_table_ref lock_table_alias lock_type.
lock_table_spec ::= cache_table_ref lock_type.

lock_table_alias ::= handler_as lock_alias.
lock_table_alias ::= lock_alias.

lock_alias ::= cache_name_part.

lock_type ::= READ.
lock_type ::= READ LOCAL.
lock_type ::= WRITE.
lock_type ::= LOW_PRIORITY(A) WRITE. {
  mylite_parser_require_permissive(ctx, A);
}

unlock_statement ::= UNLOCK unlock_tail(A). {
  mylite_parser_record_statement(ctx, A);
}

unlock_tail(A) ::= unlock_table_kind. {
  A = MYLITE_STATEMENT_TRANSACTION;
}
unlock_tail(A) ::= INSTANCE. {
  A = MYLITE_STATEMENT_ADMIN;
}

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

analyze_table_tail ::= table_admin_optional_binlog table_admin_table_keyword cache_table_ref analyze_table_after_first_table.

analyze_table_after_first_table ::= .
analyze_table_after_first_table ::= analyze_histogram_options.
analyze_table_after_first_table ::= import_comma analyze_table_remaining_list.
analyze_table_after_first_table ::= import_comma analyze_table_remaining_list UPDATE(A) analyze_histogram_marker ON table_admin_column_list analyze_histogram_bucket_tail analyze_histogram_update_mode_tail. {
  mylite_parser_require_permissive(ctx, A);
}
analyze_table_after_first_table ::= import_comma analyze_table_remaining_list DROP(A) analyze_histogram_marker ON table_admin_column_list. {
  mylite_parser_require_permissive(ctx, A);
}

analyze_table_remaining_list ::= cache_table_ref.
analyze_table_remaining_list ::= analyze_table_remaining_list import_comma cache_table_ref.

analyze_histogram_options ::= UPDATE analyze_histogram_marker ON table_admin_column_list analyze_histogram_bucket_tail analyze_histogram_update_mode_tail.
analyze_histogram_options ::= UPDATE analyze_histogram_marker ON table_admin_column USING DATA string_literal.
analyze_histogram_options ::= DROP analyze_histogram_marker ON table_admin_column_list.

analyze_histogram_marker ::= HISTOGRAM.

analyze_histogram_bucket_tail ::= .
analyze_histogram_bucket_tail ::= WITH analyze_histogram_bucket_count analyze_buckets_marker.

analyze_histogram_update_mode_tail ::= .
analyze_histogram_update_mode_tail ::= MANUAL UPDATE.
analyze_histogram_update_mode_tail ::= AUTO UPDATE.

analyze_histogram_bucket_count ::= BOOLEAN_NUMBER.
analyze_histogram_bucket_count ::= FACTOR_NUMBER.
analyze_histogram_bucket_count ::= NUMBER_LITERAL.

analyze_buckets_marker ::= BUCKETS.

table_admin_column_list ::= table_admin_column.
table_admin_column_list ::= table_admin_column_list import_comma table_admin_column.

table_admin_column ::= cache_name_part.

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

plugin_admin_tail ::= PLUGIN plugin_name plugin_soname string_literal.
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

component_file ::= string_literal.

plugin_soname ::= SONAME.

plugin_name ::= cache_name_part.

import_statement ::= IMPORT TABLE FROM import_file_list. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

import_file_list ::= import_file.
import_file_list ::= import_file_list import_comma import_file.

import_comma ::= COMMA.

import_file ::= string_literal.

cache_statement ::= CACHE INDEX cache_table_list IN cache_keycache. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

cache_table_list ::= cache_table_spec.
cache_table_list ::= cache_table_list import_comma cache_table_spec.

cache_table_spec ::= cache_table_ref.
cache_table_spec ::= cache_table_ref cache_index_kind cache_key_list.
cache_table_spec ::= cache_table_ref PARTITION LP cache_partition_list RP.
cache_table_spec ::= cache_table_ref PARTITION LP cache_partition_list RP cache_index_kind cache_key_list.

cache_partition_list ::= ALL.
cache_partition_list ::= cache_partition_names.

cache_partition_names ::= cache_name_part.
cache_partition_names ::= cache_partition_names import_comma cache_name_part.

cache_index_kind ::= INDEX.
cache_index_kind ::= KEY.

cache_table_ref ::= cache_name_part.
cache_table_ref ::= cache_name_part DOT cache_name_part.

cache_name_part ::= ATOM.
cache_name_part ::= ACCOUNT.
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

cache_key_name ::= cache_name_part.

cache_keycache ::= cache_name_part.

kill_statement ::= KILL kill_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

kill_tail ::= kill_target.
kill_tail ::= kill_mode kill_target.

kill_mode ::= CONNECTION.
kill_mode ::= QUERY.

kill_target ::= cache_name_part.
kill_target ::= user_variable_name.

deallocate_statement ::= DEALLOCATE PREPARE prepared_statement_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_PREPARED);
}

reset_statement ::= RESET reset_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

reset_tail ::= BINARY LOGS AND reset_gtids reset_binary_logs_tail.
reset_tail ::= MASTER(A). {
  mylite_parser_require_permissive(ctx, A);
}
reset_tail ::= PERSIST reset_persist_tail.
reset_tail ::= REPLICA reset_replica_tail.
reset_tail ::= SLAVE(A) reset_replica_tail. {
  mylite_parser_require_permissive(ctx, A);
}

reset_binary_logs_tail ::= .
reset_binary_logs_tail ::= TO reset_binary_logs_index.

reset_binary_logs_index ::= BOOLEAN_NUMBER.
reset_binary_logs_index ::= FACTOR_NUMBER.
reset_binary_logs_index ::= NUMBER_LITERAL.

reset_gtids ::= GTIDS.

reset_persist_tail ::= .
reset_persist_tail ::= reset_persist_target.
reset_persist_tail ::= IF reset_exists reset_persist_target.

reset_exists ::= EXISTS.

reset_persist_target ::= reset_persist_name.
reset_persist_target ::= reset_persist_name DOT reset_persist_name.

reset_persist_name ::= cache_name_part.

reset_replica_tail ::= .
reset_replica_tail ::= ALL.
reset_replica_tail ::= reset_channel_tail.
reset_replica_tail ::= ALL reset_channel_tail.

reset_channel_tail ::= FOR reset_channel reset_channel_name.

reset_channel ::= CHANNEL.

reset_channel_name ::= replication_channel_name.

purge_statement ::= PURGE purge_log_kind LOGS purge_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

purge_tail ::= TO purge_log_name.
purge_tail ::= purge_before expression_start statement_tail.

purge_before ::= BEFORE.

purge_log_kind ::= BINARY.
purge_log_kind ::= MASTER(A). {
  mylite_parser_require_permissive(ctx, A);
}

purge_log_name ::= string_literal.

change_statement ::= CHANGE change_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

change_tail ::= MASTER(A) TO change_master_options change_for_channel_tail. {
  mylite_parser_require_permissive(ctx, A);
}
change_tail ::= REPLICATION FILTER change_replication_filters change_for_channel_tail.
change_tail ::= REPLICATION change_replication_source TO change_source_options change_for_channel_tail.

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

change_source_options ::= change_source_option.
change_source_options ::= change_source_options import_comma change_source_option.

change_source_option ::= change_shared_string_option_name diagnostics_equals change_option_string_value.
change_source_option ::= ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS diagnostics_equals change_assign_gtids_value.
change_source_option ::= IGNORE_SERVER_IDS diagnostics_equals change_ignore_server_ids_value.
change_source_option ::= PRIVILEGE_CHECKS_USER diagnostics_equals change_privilege_checks_user_value.
change_source_option ::= REQUIRE_TABLE_PRIMARY_KEY_CHECK diagnostics_equals change_primary_key_check_value.
change_source_option ::= change_shared_number_option_name diagnostics_equals change_option_number_value.
change_source_option ::= change_shared_boolean_option_name diagnostics_equals change_option_boolean_value.
change_source_option ::= change_source_string_option_name diagnostics_equals change_option_string_value.
change_source_option ::= change_source_number_option_name diagnostics_equals change_option_number_value.
change_source_option ::= change_source_boolean_option_name diagnostics_equals change_option_boolean_value.

change_source_string_option_name ::= SOURCE_BIND.
change_source_string_option_name ::= SOURCE_HOST.
change_source_string_option_name ::= SOURCE_USER.
change_source_string_option_name ::= SOURCE_PASSWORD.
change_source_string_option_name ::= SOURCE_LOG_FILE.
change_source_string_option_name ::= SOURCE_COMPRESSION_ALGORITHMS.
change_source_string_option_name ::= SOURCE_SSL_CA.
change_source_string_option_name ::= SOURCE_SSL_CAPATH.
change_source_string_option_name ::= SOURCE_SSL_CERT.
change_source_string_option_name ::= SOURCE_SSL_CRL.
change_source_string_option_name ::= SOURCE_SSL_CRLPATH.
change_source_string_option_name ::= SOURCE_SSL_KEY.
change_source_string_option_name ::= SOURCE_SSL_CIPHER.
change_source_string_option_name ::= SOURCE_TLS_VERSION.
change_source_string_option_name ::= SOURCE_TLS_CIPHERSUITES.
change_source_string_option_name ::= SOURCE_PUBLIC_KEY_PATH.

change_source_number_option_name ::= SOURCE_PORT.
change_source_number_option_name ::= SOURCE_LOG_POS.
change_source_number_option_name ::= SOURCE_HEARTBEAT_PERIOD.
change_source_number_option_name ::= SOURCE_CONNECT_RETRY.
change_source_number_option_name ::= SOURCE_RETRY_COUNT.
change_source_number_option_name ::= SOURCE_DELAY.
change_source_number_option_name ::= SOURCE_ZSTD_COMPRESSION_LEVEL.

change_source_boolean_option_name ::= SOURCE_AUTO_POSITION.
change_source_boolean_option_name ::= SOURCE_SSL.
change_source_boolean_option_name ::= SOURCE_SSL_VERIFY_SERVER_CERT.
change_source_boolean_option_name ::= GET_SOURCE_PUBLIC_KEY.

change_master_options ::= change_master_option.
change_master_options ::= change_master_options import_comma change_master_option.

change_master_option ::= change_shared_string_option_name diagnostics_equals change_option_string_value.
change_master_option ::= ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS diagnostics_equals change_assign_gtids_value.
change_master_option ::= IGNORE_SERVER_IDS diagnostics_equals change_ignore_server_ids_value.
change_master_option ::= PRIVILEGE_CHECKS_USER diagnostics_equals change_privilege_checks_user_value.
change_master_option ::= REQUIRE_TABLE_PRIMARY_KEY_CHECK diagnostics_equals change_primary_key_check_value.
change_master_option ::= change_shared_number_option_name diagnostics_equals change_option_number_value.
change_master_option ::= change_shared_boolean_option_name diagnostics_equals change_option_boolean_value.
change_master_option ::= change_master_string_option_name diagnostics_equals change_option_string_value.
change_master_option ::= change_master_number_option_name diagnostics_equals change_option_number_value.
change_master_option ::= change_master_boolean_option_name diagnostics_equals change_option_boolean_value.

change_master_string_option_name ::= MASTER_BIND.
change_master_string_option_name ::= MASTER_HOST.
change_master_string_option_name ::= MASTER_USER.
change_master_string_option_name ::= MASTER_PASSWORD.
change_master_string_option_name ::= MASTER_LOG_FILE.
change_master_string_option_name ::= MASTER_COMPRESSION_ALGORITHMS.
change_master_string_option_name ::= MASTER_SSL_CA.
change_master_string_option_name ::= MASTER_SSL_CAPATH.
change_master_string_option_name ::= MASTER_SSL_CERT.
change_master_string_option_name ::= MASTER_SSL_CRL.
change_master_string_option_name ::= MASTER_SSL_CRLPATH.
change_master_string_option_name ::= MASTER_SSL_KEY.
change_master_string_option_name ::= MASTER_SSL_CIPHER.
change_master_string_option_name ::= MASTER_TLS_VERSION.
change_master_string_option_name ::= MASTER_TLS_CIPHERSUITES.
change_master_string_option_name ::= MASTER_PUBLIC_KEY_PATH.

change_master_number_option_name ::= MASTER_PORT.
change_master_number_option_name ::= MASTER_LOG_POS.
change_master_number_option_name ::= MASTER_HEARTBEAT_PERIOD.
change_master_number_option_name ::= MASTER_CONNECT_RETRY.
change_master_number_option_name ::= MASTER_RETRY_COUNT.
change_master_number_option_name ::= MASTER_DELAY.
change_master_number_option_name ::= MASTER_ZSTD_COMPRESSION_LEVEL.

change_master_boolean_option_name ::= MASTER_AUTO_POSITION.
change_master_boolean_option_name ::= MASTER_SSL.
change_master_boolean_option_name ::= MASTER_SSL_VERIFY_SERVER_CERT.
change_master_boolean_option_name ::= GET_MASTER_PUBLIC_KEY.

change_shared_string_option_name ::= NETWORK_NAMESPACE.
change_shared_string_option_name ::= RELAY_LOG_FILE.

change_shared_number_option_name ::= RELAY_LOG_POS.

change_shared_boolean_option_name ::= GTID_ONLY.
change_shared_boolean_option_name ::= REQUIRE_ROW_FORMAT.
change_shared_boolean_option_name ::= SOURCE_CONNECTION_AUTO_FAILOVER.

change_option_number_value ::= BOOLEAN_NUMBER.
change_option_number_value ::= FACTOR_NUMBER.
change_option_number_value ::= NUMBER_LITERAL.

change_option_boolean_value ::= BOOLEAN_NUMBER.

change_option_string_value ::= string_literal.
change_option_string_value ::= NULL.

change_assign_gtids_value ::= LOCAL.
change_assign_gtids_value ::= OFF.
change_assign_gtids_value ::= string_literal.

change_ignore_server_ids_value ::= LP change_ignore_server_ids RP.

change_ignore_server_ids ::= .
change_ignore_server_ids ::= change_ignore_server_id_list.

change_ignore_server_id_list ::= change_option_number_value.
change_ignore_server_id_list ::= change_ignore_server_id_list import_comma change_option_number_value.

change_privilege_checks_user_value ::= NULL.
change_privilege_checks_user_value ::= drop_account_name.

change_primary_key_check_value ::= STREAM.
change_primary_key_check_value ::= ON.
change_primary_key_check_value ::= OFF.

change_for_channel_tail ::= .
change_for_channel_tail ::= FOR reset_channel change_channel_name.

change_channel_name ::= replication_channel_name.

replication_channel_name ::= cache_name_part.

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

xa_xid ::= xa_xid_part.
xa_xid ::= xa_xid_part import_comma xa_xid_part.
xa_xid ::= xa_xid_part import_comma xa_xid_part import_comma xa_format_id.

xa_xid_part ::= string_literal.
xa_xid_part ::= NUMBER_LITERAL.

xa_format_id ::= BOOLEAN_NUMBER.
xa_format_id ::= FACTOR_NUMBER.
xa_format_id ::= NUMBER_LITERAL.

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
show_tail ::= MASTER(A) STATUS. {
  mylite_parser_require_permissive(ctx, A);
}
show_tail ::= SLAVE(A) show_slave_tail. {
  mylite_parser_require_permissive(ctx, A);
}
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
show_tail ::= MASTER(A) LOGS. {
  mylite_parser_require_permissive(ctx, A);
}
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
show_filter_tail ::= LIKE string_literal.
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

show_engine_name ::= cache_name_part.

show_engine_kind ::= STATUS.
show_engine_kind ::= LOGS.
show_engine_kind ::= MUTEX.

show_log_events_tail ::= show_log_file_tail show_log_from_tail show_limit_tail.

show_log_file_tail ::= .
show_log_file_tail ::= IN string_literal.

show_log_from_tail ::= .
show_log_from_tail ::= FROM show_log_position.

show_log_position ::= BOOLEAN_NUMBER.
show_log_position ::= FACTOR_NUMBER.
show_log_position ::= NUMBER_LITERAL.

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
show_profile_for_tail ::= FOR QUERY show_profile_query_id.

show_profile_query_id ::= BOOLEAN_NUMBER.
show_profile_query_id ::= FACTOR_NUMBER.
show_profile_query_id ::= NUMBER_LITERAL.

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
show_limit_tail ::= LIMIT show_limit_value.
show_limit_tail ::= LIMIT show_limit_value import_comma show_limit_value.
show_limit_tail ::= LIMIT show_limit_value OFFSET show_limit_value.

show_limit_value ::= BOOLEAN_NUMBER.
show_limit_value ::= FACTOR_NUMBER.
show_limit_value ::= NUMBER_LITERAL.

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

describe_column_ref ::= cache_name_part.

describe_name_part ::= cache_name_part.

describe_explain_tail ::= describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_schema_spec describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_clause describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_clause explain_schema_spec describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_json_clause explain_into_tail describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_format_json_clause explain_into_tail explain_schema_spec describe_explain_query_start required_statement_tail.
describe_explain_tail ::= explain_analyze_tail.
describe_explain_tail ::= FOR CONNECTION explain_connection_id.
describe_explain_tail ::= explain_format_clause FOR CONNECTION explain_connection_id.

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

explain_tail ::= describe_tail.
explain_tail ::= explain_query_start required_statement_tail.
explain_tail ::= explain_schema_spec explain_query_start required_statement_tail.
explain_tail ::= explain_format_clause explain_query_start required_statement_tail.
explain_tail ::= explain_format_clause explain_schema_spec explain_query_start required_statement_tail.
explain_tail ::= explain_format_json_clause explain_into_tail explain_query_start required_statement_tail.
explain_tail ::= explain_format_json_clause explain_into_tail explain_schema_spec explain_query_start required_statement_tail.
explain_tail ::= explain_analyze_tail.
explain_tail ::= FOR CONNECTION explain_connection_id.
explain_tail ::= explain_format_clause FOR CONNECTION explain_connection_id.

explain_format_clause ::= FORMAT diagnostics_equals explain_format_name.

explain_format_json_clause ::= FORMAT diagnostics_equals JSON.

explain_into_tail ::= INTO user_variable_name.

explain_schema_spec ::= FOR explain_schema_kind cache_name_part.

explain_schema_kind ::= SCHEMA.
explain_schema_kind ::= DATABASE.

explain_connection_id ::= BOOLEAN_NUMBER.
explain_connection_id ::= FACTOR_NUMBER.
explain_connection_id ::= NUMBER_LITERAL.

explain_analyze_tail ::= ANALYZE explain_analyze_format_tail explain_schema_tail explain_analyze_query_start required_statement_tail.

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

use_statement ::= USE use_target. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

use_target ::= cache_name_part.

handler_statement ::= HANDLER handler_name handler_operation. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

handler_name ::= handler_identifier.
handler_name ::= handler_identifier DOT handler_identifier.

handler_identifier ::= cache_name_part.

handler_operation ::= OPEN.
handler_operation ::= OPEN handler_alias.
handler_operation ::= OPEN handler_as handler_alias.
handler_operation ::= READ handler_read_tail.
handler_operation ::= CLOSE.

handler_as ::= AS.

handler_alias ::= cache_name_part.

handler_read_tail ::= handler_read_direction handler_read_suffix.
handler_read_tail ::= handler_read_index handler_read_direction handler_read_suffix.
handler_read_tail ::= handler_read_index handler_read_operator handler_read_tuple handler_read_suffix.

handler_read_index ::= cache_name_part.

handler_read_direction ::= FIRST.
handler_read_direction ::= NEXT.
handler_read_direction ::= PREV.
handler_read_direction ::= LAST.

handler_read_operator ::= EQUALS.
handler_read_operator ::= LE.
handler_read_operator ::= GE.
handler_read_operator ::= LT.
handler_read_operator ::= GT.

handler_read_tuple ::= LP values_row_contents RP.

handler_read_suffix ::= .
handler_read_suffix ::= WHERE expression_start statement_tail.
handler_read_suffix ::= handler_limit_tail.

handler_limit_tail ::= LIMIT handler_limit_value.
handler_limit_tail ::= LIMIT handler_limit_value import_comma handler_limit_value.
handler_limit_tail ::= LIMIT handler_limit_value OFFSET handler_limit_value.

handler_limit_value ::= BOOLEAN_NUMBER.
handler_limit_value ::= FACTOR_NUMBER.
handler_limit_value ::= NUMBER_LITERAL.

call_statement ::= CALL call_name call_arguments. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

call_name ::= call_identifier.
call_name ::= call_identifier DOT call_identifier.

call_identifier ::= cache_name_part.

call_arguments ::= .
call_arguments ::= LP RP.
call_arguments ::= LP call_argument_list RP.
call_arguments ::= LP(A) call_argument_list. {
  mylite_parser_require_permissive(ctx, A);
}

call_argument_list ::= call_argument.
call_argument_list ::= call_argument_list COMMA call_argument.

call_argument ::= call_argument_tokens.

call_argument_tokens ::= call_argument_token.
call_argument_tokens ::= call_argument_tokens call_argument_token.

call_argument_token ::= ATOM.
call_argument_token ::= LABEL.
call_argument_token ::= keyword.
call_argument_token ::= DOT.
call_argument_token ::= LP call_argument_nested_tokens RP.
call_argument_token ::= LB.
call_argument_token ::= RB.
call_argument_token ::= LC.
call_argument_token ::= RC.

call_argument_nested_tokens ::= .
call_argument_nested_tokens ::= call_argument_nested_tokens call_argument_nested_token.

call_argument_nested_token ::= ATOM.
call_argument_nested_token ::= LABEL.
call_argument_nested_token ::= keyword.
call_argument_nested_token ::= COMMA.
call_argument_nested_token ::= DOT.
call_argument_nested_token ::= LP call_argument_nested_tokens RP.
call_argument_nested_token ::= LB.
call_argument_nested_token ::= RB.
call_argument_nested_token ::= LC.
call_argument_nested_token ::= RC.

binlog_statement ::= BINLOG binlog_payload. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

binlog_payload ::= string_literal.

clone_statement ::= CLONE clone_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

clone_tail ::= INSTANCE FROM clone_instance_source clone_identified BY clone_password clone_remote_tail.
clone_tail ::= LOCAL DATA clone_directory clone_directory_equals_tail clone_directory_path.

clone_instance_source ::= clone_account_name clone_colon clone_port.

clone_account_name ::= drop_account_principal clone_account_host.

clone_account_host ::= AT_HOST drop_host_dot_tail.
clone_account_host ::= AT_SIGN drop_host_name.

clone_colon ::= COLON.

clone_port ::= BOOLEAN_NUMBER.
clone_port ::= FACTOR_NUMBER.
clone_port ::= NUMBER_LITERAL.

clone_identified ::= IDENTIFIED.

clone_password ::= string_literal.

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

clone_directory_path ::= string_literal.

flush_statement ::= FLUSH flush_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

flush_tail ::= flush_binlog_modifier flush_target.

flush_binlog_modifier ::= .
flush_binlog_modifier ::= LOCAL.
flush_binlog_modifier ::= NO_WRITE_TO_BINLOG.

flush_target ::= flush_simple_list.
flush_target ::= flush_table_kind flush_table_tail.
flush_target ::= BINARY LOGS.
flush_target ::= ENGINE LOGS.
flush_target ::= ERROR LOGS.
flush_target ::= GENERAL LOGS.
flush_target ::= RELAY LOGS show_channel_tail.
flush_target ::= SLOW LOGS.

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

dml_write_payload ::= LP RP dml_write_after_column_list.
dml_write_payload ::= LP dml_write_column_list RP dml_write_after_column_list.
dml_write_payload ::= LP dml_write_query_start dml_write_parenthesized_query_tail RP dml_write_after_parenthesized_query.
dml_write_payload ::= SET update_assignment_start.
dml_write_payload ::= SELECT select_tail.
dml_write_payload ::= TABLE table_statement_target table_query_tail.
dml_write_payload ::= WITH with_recursive_tail with_cte_list with_query_body.
dml_write_payload ::= dml_write_partition_clause dml_write_payload.
dml_write_payload ::= dml_write_start required_statement_tail.

dml_write_column_list ::= dml_write_column_ref.
dml_write_column_list ::= dml_write_column_list COMMA dml_write_column_ref.

dml_write_column_ref ::= dml_write_column_part.
dml_write_column_ref ::= dml_write_column_ref DOT dml_write_column_part.

dml_write_column_part ::= cache_name_part.
dml_write_column_part ::= CONNECTION.
dml_write_column_part ::= DATA.
dml_write_column_part ::= DIAGNOSTICS.
dml_write_column_part ::= END.
dml_write_column_part ::= NO.
dml_write_column_part ::= QUERY.
dml_write_column_part ::= START.
dml_write_column_part ::= STATUS.
dml_write_column_part ::= XML.

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

table_query_tail ::= table_order_tail table_limit_tail table_into_tail.
table_query_tail ::= values_set_operator values_union_tail.

table_order_tail ::= .
table_order_tail ::= table_order_nonempty_tail.

table_order_nonempty_tail ::= ORDER BY table_order_list.

table_order_list ::= table_order_item.
table_order_list ::= table_order_list import_comma table_order_item.

table_order_item ::= table_order_expression values_order_direction.

table_order_expression ::= table_order_part.
table_order_expression ::= table_order_expression DOT table_order_part.

table_order_part ::= cache_name_part.
table_order_part ::= BOOLEAN_NUMBER.
table_order_part ::= FACTOR_NUMBER.
table_order_part ::= NUMBER_LITERAL.

table_limit_tail ::= .
table_limit_tail ::= table_limit_nonempty_tail.

table_limit_nonempty_tail ::= LIMIT table_limit_value.
table_limit_nonempty_tail ::= LIMIT table_limit_value import_comma table_limit_value.
table_limit_nonempty_tail ::= LIMIT table_limit_value OFFSET table_limit_value.

table_limit_value ::= BOOLEAN_NUMBER.
table_limit_value ::= FACTOR_NUMBER.
table_limit_value ::= NUMBER_LITERAL.

table_into_tail ::= .
table_into_tail ::= INTO table_output_target.

table_output_target ::= OUTFILE string_literal load_fields_tail load_lines_tail.
table_output_target ::= DUMPFILE string_literal.
table_output_target ::= table_into_variable_list.

table_into_variable_list ::= table_into_variable.
table_into_variable_list ::= table_into_variable_list import_comma table_into_variable.

table_into_variable ::= user_variable_name.
table_into_variable ::= cache_name_part.

values_statement ::= VALUES values_row_list values_query_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

values_row_list ::= values_row.
values_row_list ::= values_row_list import_comma values_row.

values_row ::= ROW LP values_row_contents RP.

values_row_contents ::= values_row_value_list.

values_row_value_list ::= values_row_value.
values_row_value_list ::= values_row_value_list COMMA values_row_value.

values_row_value ::= values_row_value_tokens.

values_row_value_tokens ::= values_row_value_token.
values_row_value_tokens ::= values_row_value_tokens values_row_value_token.

values_row_value_token ::= ATOM.
values_row_value_token ::= LABEL.
values_row_value_token ::= keyword.
values_row_value_token ::= DOT.
values_row_value_token ::= LP values_row_nested_tokens RP.
values_row_value_token ::= LB.
values_row_value_token ::= RB.
values_row_value_token ::= LC.
values_row_value_token ::= RC.

values_row_nested_tokens ::= .
values_row_nested_tokens ::= values_row_nested_tokens values_row_nested_token.

values_row_nested_token ::= ATOM.
values_row_nested_token ::= LABEL.
values_row_nested_token ::= keyword.
values_row_nested_token ::= COMMA.
values_row_nested_token ::= DOT.
values_row_nested_token ::= LP values_row_nested_tokens RP.
values_row_nested_token ::= LB.
values_row_nested_token ::= RB.
values_row_nested_token ::= LC.
values_row_nested_token ::= RC.

values_query_tail ::= .
values_query_tail ::= values_set_operator values_union_tail.
values_query_tail ::= ORDER BY values_order_list values_limit_optional_tail.
values_query_tail ::= values_limit_tail.

values_query_tail_nonempty ::= values_set_operator values_union_tail.
values_query_tail_nonempty ::= ORDER BY values_order_list values_limit_optional_tail.
values_query_tail_nonempty ::= values_limit_tail.

values_limit_optional_tail ::= .
values_limit_optional_tail ::= values_limit_tail.

values_set_operator ::= UNION.
values_set_operator ::= EXCEPT.
values_set_operator ::= INTERSECT.

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

prepare_source ::= string_literal.
prepare_source ::= user_variable_name.

execute_statement ::= EXECUTE prepared_statement_name execute_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_PREPARED);
}

execute_tail ::= .
execute_tail ::= USING execute_using_list.

execute_using_list ::= execute_using_arg.
execute_using_list ::= execute_using_list import_comma execute_using_arg.

execute_using_arg ::= user_variable_name.

user_variable_name ::= AT_HOST set_variable_dot_tail.
user_variable_name ::= AT_SIGN set_variable_part set_variable_dot_tail.

prepared_statement_name ::= cache_name_part.

get_statement ::= GET diagnostics_area_tail DIAGNOSTICS diagnostics_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

diagnostics_area_tail ::= .
diagnostics_area_tail ::= CURRENT.
diagnostics_area_tail ::= STACKED.

diagnostics_tail ::= diagnostics_statement_items.
diagnostics_tail ::= CONDITION diagnostics_condition_value diagnostics_condition_items.

diagnostics_statement_items ::= diagnostics_statement_item.
diagnostics_statement_items ::= diagnostics_statement_items import_comma diagnostics_statement_item.

diagnostics_statement_item ::= diagnostics_target diagnostics_equals diagnostics_statement_item_name.

diagnostics_condition_items ::= diagnostics_condition_item.
diagnostics_condition_items ::= diagnostics_condition_items import_comma diagnostics_condition_item.

diagnostics_condition_item ::= diagnostics_target diagnostics_equals diagnostics_condition_item_name.

diagnostics_target ::= diagnostics_variable_name.

diagnostics_condition_value ::= ATOM.
diagnostics_condition_value ::= LABEL.
diagnostics_condition_value ::= user_variable_name.

diagnostics_variable_name ::= cache_name_part.
diagnostics_variable_name ::= user_variable_name.

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
signal_condition_value ::= SQLSTATE signal_sqlstate_value_tail SQLSTATE_VALUE.

signal_sqlstate_value_tail ::= .
signal_sqlstate_value_tail ::= VALUE.

signal_named_condition ::= cache_name_part.

signal_set_tail ::= .
signal_set_tail ::= SET signal_information_items.

signal_information_items ::= signal_information_item.
signal_information_items ::= signal_information_items import_comma signal_information_item.

signal_information_item ::= signal_condition_item_name diagnostics_equals signal_information_value.
signal_information_item ::= MYSQL_ERRNO diagnostics_equals signal_mysql_errno_value.

signal_condition_item_name ::= CATALOG_NAME.
signal_condition_item_name ::= CLASS_ORIGIN.
signal_condition_item_name ::= COLUMN_NAME.
signal_condition_item_name ::= CONSTRAINT_CATALOG.
signal_condition_item_name ::= CONSTRAINT_NAME.
signal_condition_item_name ::= CONSTRAINT_SCHEMA.
signal_condition_item_name ::= CURSOR_NAME.
signal_condition_item_name ::= MESSAGE_TEXT.
signal_condition_item_name ::= SCHEMA_NAME.
signal_condition_item_name ::= SUBCLASS_ORIGIN.
signal_condition_item_name ::= TABLE_NAME.

signal_mysql_errno_value ::= BOOLEAN_NUMBER.
signal_mysql_errno_value ::= FACTOR_NUMBER.
signal_mysql_errno_value ::= NUMBER_LITERAL.

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

savepoint_reference ::= cache_name_part.

set_statement ::= SET set_names_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET CHARACTER SET set_character_set_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET CHARSET set_character_set_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET PASSWORD set_password_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET ROLE set_role_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET DEFAULT ROLE set_default_role_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET RESOURCE create_resource_group cache_name_part set_resource_group_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET TRANSACTION set_transaction_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET set_assignment_list. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}
set_statement ::= SET set_transaction_scope TRANSACTION set_transaction_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

set_names_tail ::= NAMES set_names_value set_comma_assignment_tail.

set_names_value ::= DEFAULT.
set_names_value ::= set_charset_name set_names_collate_tail.

set_names_collate_tail ::= .
set_names_collate_tail ::= COLLATE set_collation_value.

set_character_set_tail ::= set_charset_value set_comma_assignment_tail.

set_comma_assignment_tail ::= .
set_comma_assignment_tail ::= import_comma set_assignment_list.

set_charset_value ::= DEFAULT.
set_charset_value ::= set_charset_name.

set_charset_name ::= set_charset_name_part.

set_collation_value ::= set_charset_name_part.

set_charset_name_part ::= ATOM.
set_charset_name_part ::= ACCOUNT.
set_charset_name_part ::= CASCADE.
set_charset_name_part ::= COMPONENT.
set_charset_name_part ::= COUNT.
set_charset_name_part ::= DATABASE.
set_charset_name_part ::= LABEL.
set_charset_name_part ::= ENGINE.
set_charset_name_part ::= EVENTS.
set_charset_name_part ::= FIRST.
set_charset_name_part ::= FULL.
set_charset_name_part ::= GRANTS.
set_charset_name_part ::= PLUGIN.
set_charset_name_part ::= PROCESSLIST.
set_charset_name_part ::= RESTRICT.
set_charset_name_part ::= TABLES.
set_charset_name_part ::= TABLESPACE.
set_charset_name_part ::= TRIGGERS.
set_charset_name_part ::= USER.
set_charset_name_part ::= VARIABLES.
set_charset_name_part ::= BINARY.

set_resource_group_tail ::= .
set_resource_group_tail ::= FOR set_resource_group_thread_list.

set_resource_group_thread_list ::= set_resource_group_thread.
set_resource_group_thread_list ::= set_resource_group_thread_list import_comma set_resource_group_thread.

set_resource_group_thread ::= resource_group_number_value.

set_password_tail ::= set_password_target_tail set_password_auth_option set_password_replace_tail set_password_retain_tail.

set_password_target_tail ::= .
set_password_target_tail ::= FOR set_password_target.

set_password_auth_option ::= EQUALS string_literal.
set_password_auth_option ::= TO RANDOM.

set_password_replace_tail ::= .
set_password_replace_tail ::= REPLACE string_literal.

set_password_retain_tail ::= .
set_password_retain_tail ::= RETAIN CURRENT PASSWORD.

set_password_target ::= drop_account_name.
set_password_target ::= current_user_ref.

set_role_tail ::= DEFAULT.
set_role_tail ::= NONE.
set_role_tail ::= ALL.
set_role_tail ::= ALL EXCEPT drop_account_list.
set_role_tail ::= drop_account_list.

set_default_role_tail ::= set_default_role_spec TO drop_account_list.

set_default_role_spec ::= NONE.
set_default_role_spec ::= ALL.
set_default_role_spec ::= drop_account_list.

set_transaction_tail ::= set_transaction_characteristics.

set_transaction_scope ::= GLOBAL.
set_transaction_scope ::= LOCAL.
set_transaction_scope ::= SESSION.

set_transaction_characteristics ::= set_transaction_characteristic.
set_transaction_characteristics ::= set_transaction_characteristics import_comma set_transaction_characteristic.

set_transaction_characteristic ::= ISOLATION LEVEL set_transaction_isolation_level.
set_transaction_characteristic ::= READ transaction_access_mode.

set_transaction_isolation_level ::= REPEATABLE READ.
set_transaction_isolation_level ::= READ COMMITTED.
set_transaction_isolation_level ::= READ UNCOMMITTED.
set_transaction_isolation_level ::= SERIALIZABLE.

%fallback ATOM COMMITTED ISOLATION LEVEL REPEATABLE SERIALIZABLE UNCOMMITTED.

set_assignment_scope ::= GLOBAL.
set_assignment_scope ::= LOCAL.
set_assignment_scope ::= PERSIST.
set_assignment_scope ::= PERSIST_ONLY.
set_assignment_scope ::= SESSION.

set_assignment_list ::= set_assignment.
set_assignment_list ::= set_assignment_list import_comma set_assignment.

set_assignment ::= set_variable_name set_assignment_operator set_assignment_value.
set_assignment ::= set_assignment_scope set_variable_name set_assignment_operator set_assignment_value.

set_assignment_value ::= values_row_value_tokens.

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

set_variable_part ::= cache_name_part.
set_variable_part ::= AT_HOST.
set_variable_part ::= FLUSH.
set_variable_part ::= SQL_BUFFER_RESULT.

grant_statement ::= GRANT grant_subject_list grant_destination_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

revoke_statement ::= REVOKE revoke_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

grant_destination_tail ::= ON grant_object TO grant_recipient_list grant_tail_options.
grant_destination_tail ::= TO grant_recipient_list grant_tail_options.

revoke_tail ::= grant_subject_list revoke_destination_tail.
revoke_tail ::= IF reset_exists grant_subject_list revoke_destination_tail.

revoke_destination_tail ::= ON grant_object FROM grant_recipient_list revoke_ignore_unknown_tail.
revoke_destination_tail ::= FROM grant_recipient_list revoke_ignore_unknown_tail.

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
grant_subject_token ::= PROXY.
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

grant_recipient ::= grant_account_ref grant_recipient_auth_tail.

grant_account_ref ::= drop_account_name.
grant_account_ref ::= current_user_ref.

grant_recipient_auth_tail ::= .
grant_recipient_auth_tail ::= grant_recipient_auth_option.

grant_recipient_auth_option ::= IDENTIFIED BY user_auth_value.
grant_recipient_auth_option ::= IDENTIFIED WITH user_auth_plugin.
grant_recipient_auth_option ::= IDENTIFIED WITH user_auth_plugin BY user_auth_value.

grant_tail_options ::= grant_require_tail grant_with_tails grant_as_tail.

grant_require_tail ::= .
grant_require_tail ::= REQUIRE account_tls_requirement.

grant_with_tails ::= .
grant_with_tails ::= grant_with_tails WITH grant_with_clause.

grant_with_clause ::= account_resource_options.
grant_with_clause ::= GRANT OPTION.
grant_with_clause ::= ADMIN OPTION.

grant_as_tail ::= .
grant_as_tail ::= AS grant_as_user grant_as_role_tail.

grant_as_user ::= drop_account_name.
grant_as_user ::= current_user_ref.

grant_as_role_tail ::= .
grant_as_role_tail ::= WITH ROLE grant_as_role_spec.

grant_as_role_spec ::= DEFAULT.
grant_as_role_spec ::= NONE.
grant_as_role_spec ::= ALL.
grant_as_role_spec ::= ALL EXCEPT drop_account_list.
grant_as_role_spec ::= drop_account_list.

revoke_ignore_unknown_tail ::= .
revoke_ignore_unknown_tail ::= IGNORE UNKNOWN USER.

%fallback ATOM ADMIN OPTION PROXY UNKNOWN.

leave_statement ::= LEAVE stored_program_label_ref. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

iterate_statement ::= ITERATE stored_program_label_ref. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

stored_program_label_ref ::= cache_name_part.

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
expression_start_keyword ::= ACCOUNT.
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

fetch_statement ::= FETCH fetch_cursor_ref INTO fetch_target_list. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

fetch_cursor_ref ::= stored_program_label_ref.
fetch_cursor_ref ::= FROM stored_program_label_ref.
fetch_cursor_ref ::= NEXT FROM stored_program_label_ref.

fetch_target_list ::= fetch_target.
fetch_target_list ::= fetch_target_list import_comma fetch_target.

fetch_target ::= cache_name_part.

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

declare_name ::= cache_name_part.

declare_type_start ::= ATOM.
declare_type_start ::= BINARY.
declare_type_start ::= CHARACTER.
declare_type_start ::= CHARSET.
declare_type_start ::= DATA.
declare_type_start ::= DEFAULT.
declare_type_start ::= LABEL.

declare_condition_value ::= declare_condition_number_value.
declare_condition_value ::= SQLSTATE signal_sqlstate_value_tail SQLSTATE_VALUE.

declare_condition_number_value ::= BOOLEAN_NUMBER.
declare_condition_number_value ::= FACTOR_NUMBER.
declare_condition_number_value ::= NUMBER_LITERAL.

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
declare_handler_condition ::= declare_condition_name.
declare_handler_condition ::= declare_not declare_found.

declare_condition_name ::= ATOM.
declare_condition_name ::= LABEL.

declare_not ::= NOT.

declare_found ::= FOUND.

declare_handler_statement_start ::= BEGIN.
declare_handler_statement_start ::= CALL.
declare_handler_statement_start ::= CASE.
declare_handler_statement_start ::= CLOSE.
declare_handler_statement_start ::= DELETE.
declare_handler_statement_start ::= DO.
declare_handler_statement_start ::= FETCH.
declare_handler_statement_start ::= GET.
declare_handler_statement_start ::= IF.
declare_handler_statement_start ::= INSERT.
declare_handler_statement_start ::= ITERATE.
declare_handler_statement_start ::= LABEL.
declare_handler_statement_start ::= LEAVE.
declare_handler_statement_start ::= LOOP.
declare_handler_statement_start ::= OPEN.
declare_handler_statement_start ::= REPEAT.
declare_handler_statement_start ::= REPLACE.
declare_handler_statement_start ::= RESIGNAL.
declare_handler_statement_start ::= RETURN.
declare_handler_statement_start ::= SELECT.
declare_handler_statement_start ::= SET.
declare_handler_statement_start ::= SIGNAL.
declare_handler_statement_start ::= UPDATE.
declare_handler_statement_start ::= WHILE.

end_statement ::= END end_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

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
permissive_start(A) ::= DOUBLE_QUOTED_STRING(B). {
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
keyword ::= CASCADED.
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
keyword ::= CONSTRAINT.
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
keyword ::= COLUMN.
keyword ::= COLUMNS.
keyword ::= COPY.
keyword ::= COUNT.
keyword ::= DATABASES.
keyword ::= DEFAULT_AUTH.
keyword ::= ENGINE.
keyword ::= ENGINES.
keyword ::= ENFORCED.
keyword ::= ERRORS.
keyword ::= ERROR.
keyword ::= ESCAPED.
keyword ::= EVENTS.
keyword ::= EXCLUSIVE.
keyword ::= EXTENDED.
keyword ::= ENCLOSED.
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
keyword ::= INSTANT.
keyword ::= INPLACE.
keyword ::= INVISIBLE.
keyword ::= KEY.
keyword ::= KEYS.
keyword ::= LAST.
keyword ::= LINES.
keyword ::= PLUGINS.
keyword ::= LOGS.
keyword ::= OPTIMIZER_COSTS.
keyword ::= PRIVILEGES.
keyword ::= PRIMARY.
keyword ::= PROCESSLIST.
keyword ::= PROFILE.
keyword ::= PROFILES.
keyword ::= RELAYLOG.
keyword ::= RELAY.
keyword ::= REPLICAS.
keyword ::= SCHEMAS.
keyword ::= SESSION.
keyword ::= SHARED.
keyword ::= STACKED.
keyword ::= STATUS.
keyword ::= STOP.
keyword ::= STORAGE.
keyword ::= STREAM.
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
keyword ::= INTERSECT.
keyword ::= INTO.
keyword ::= INOUT.
keyword ::= IO_THREAD.
keyword ::= LOW_PRIORITY.
keyword ::= NAMES.
keyword ::= NO.
keyword ::= NULL.
keyword ::= OFFSET.
keyword ::= OFF.
keyword ::= ON.
keyword ::= ORDER.
keyword ::= OUT.
keyword ::= OUTFILE.
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
keyword ::= DUMPFILE.
keyword ::= SQL_BIG_RESULT.
keyword ::= SQL_BUFFER_RESULT.
keyword ::= SQL_CALC_FOUND_ROWS.
keyword ::= SQL_SMALL_RESULT.
keyword ::= SSL.
keyword ::= STRAIGHT_JOIN.
keyword ::= TO.
keyword ::= WORK.
keyword ::= COMMITTED.
keyword ::= ISOLATION.
keyword ::= LEVEL.
keyword ::= REPEATABLE.
keyword ::= SERIALIZABLE.
keyword ::= UNCOMMITTED.
keyword ::= ACCOUNT.
keyword ::= ADMIN.
keyword ::= ATTRIBUTE.
keyword ::= AUTHENTICATION.
keyword ::= CHALLENGE_RESPONSE.
keyword ::= CIPHER.
keyword ::= DAY.
keyword ::= DISCARD.
keyword ::= EXPIRE.
keyword ::= FACTOR.
keyword ::= FAILED_LOGIN_ATTEMPTS.
keyword ::= FINISH.
keyword ::= HISTORY.
keyword ::= INITIAL.
keyword ::= INITIATE.
keyword ::= INTERVAL.
keyword ::= ISSUER.
keyword ::= MAX_CONNECTIONS_PER_HOUR.
keyword ::= MAX_QUERIES_PER_HOUR.
keyword ::= MAX_UPDATES_PER_HOUR.
keyword ::= MAX_USER_CONNECTIONS.
keyword ::= NEVER.
keyword ::= OLD.
keyword ::= OPTION.
keyword ::= OPTIONAL.
keyword ::= OPTIONALLY.
keyword ::= PARSER.
keyword ::= PASSWORD_LOCK_TIME.
keyword ::= PROXY.
keyword ::= REGISTRATION.
keyword ::= REUSE.
keyword ::= ROWS.
keyword ::= SECONDARY_LOAD.
keyword ::= SECONDARY_UNLOAD.
keyword ::= STARTING.
keyword ::= SUBJECT.
keyword ::= TERMINATED.
keyword ::= UNBOUNDED.
keyword ::= UNREGISTER.
keyword ::= UNKNOWN.
keyword ::= PARTITIONING.
keyword ::= VALIDATION.
keyword ::= VISIBLE.
keyword ::= WITHOUT.
keyword ::= X509.

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
keyword_not_select_clause ::= CASCADED.
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
keyword_not_select_clause ::= CONSTRAINT.
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
keyword_not_select_clause ::= COLUMN.
keyword_not_select_clause ::= COLUMNS.
keyword_not_select_clause ::= COPY.
keyword_not_select_clause ::= COUNT.
keyword_not_select_clause ::= DATABASES.
keyword_not_select_clause ::= DEFAULT_AUTH.
keyword_not_select_clause ::= ENGINE.
keyword_not_select_clause ::= ENGINES.
keyword_not_select_clause ::= ENFORCED.
keyword_not_select_clause ::= ERRORS.
keyword_not_select_clause ::= ERROR.
keyword_not_select_clause ::= ESCAPED.
keyword_not_select_clause ::= EVENTS.
keyword_not_select_clause ::= EXCLUSIVE.
keyword_not_select_clause ::= EXTENDED.
keyword_not_select_clause ::= ENCLOSED.
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
keyword_not_select_clause ::= INSTANT.
keyword_not_select_clause ::= INPLACE.
keyword_not_select_clause ::= INVISIBLE.
keyword_not_select_clause ::= KEY.
keyword_not_select_clause ::= KEYS.
keyword_not_select_clause ::= LAST.
keyword_not_select_clause ::= LINES.
keyword_not_select_clause ::= PLUGINS.
keyword_not_select_clause ::= LOGS.
keyword_not_select_clause ::= OPTIMIZER_COSTS.
keyword_not_select_clause ::= PRIVILEGES.
keyword_not_select_clause ::= PRIMARY.
keyword_not_select_clause ::= PROCESSLIST.
keyword_not_select_clause ::= PROFILE.
keyword_not_select_clause ::= PROFILES.
keyword_not_select_clause ::= RELAYLOG.
keyword_not_select_clause ::= RELAY.
keyword_not_select_clause ::= REPLICAS.
keyword_not_select_clause ::= SCHEMAS.
keyword_not_select_clause ::= SESSION.
keyword_not_select_clause ::= SHARED.
keyword_not_select_clause ::= STACKED.
keyword_not_select_clause ::= STATUS.
keyword_not_select_clause ::= STORAGE.
keyword_not_select_clause ::= STREAM.
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
keyword_not_select_clause ::= INTERSECT.
keyword_not_select_clause ::= INTO.
keyword_not_select_clause ::= INOUT.
keyword_not_select_clause ::= IO_THREAD.
keyword_not_select_clause ::= LOW_PRIORITY.
keyword_not_select_clause ::= NAMES.
keyword_not_select_clause ::= NO.
keyword_not_select_clause ::= NULL.
keyword_not_select_clause ::= OFFSET.
keyword_not_select_clause ::= OFF.
keyword_not_select_clause ::= ON.
keyword_not_select_clause ::= ORDER.
keyword_not_select_clause ::= OUT.
keyword_not_select_clause ::= OUTFILE.
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
keyword_not_select_clause ::= DUMPFILE.
keyword_not_select_clause ::= SQL_BIG_RESULT.
keyword_not_select_clause ::= SQL_BUFFER_RESULT.
keyword_not_select_clause ::= SQL_CALC_FOUND_ROWS.
keyword_not_select_clause ::= SQL_SMALL_RESULT.
keyword_not_select_clause ::= SSL.
keyword_not_select_clause ::= STRAIGHT_JOIN.
keyword_not_select_clause ::= TO.
keyword_not_select_clause ::= WORK.
keyword_not_select_clause ::= COMMITTED.
keyword_not_select_clause ::= ISOLATION.
keyword_not_select_clause ::= LEVEL.
keyword_not_select_clause ::= REPEATABLE.
keyword_not_select_clause ::= SERIALIZABLE.
keyword_not_select_clause ::= UNCOMMITTED.
keyword_not_select_clause ::= ACCOUNT.
keyword_not_select_clause ::= ADMIN.
keyword_not_select_clause ::= ATTRIBUTE.
keyword_not_select_clause ::= AUTHENTICATION.
keyword_not_select_clause ::= CHALLENGE_RESPONSE.
keyword_not_select_clause ::= CIPHER.
keyword_not_select_clause ::= DAY.
keyword_not_select_clause ::= DISCARD.
keyword_not_select_clause ::= EXPIRE.
keyword_not_select_clause ::= FACTOR.
keyword_not_select_clause ::= FAILED_LOGIN_ATTEMPTS.
keyword_not_select_clause ::= FINISH.
keyword_not_select_clause ::= HISTORY.
keyword_not_select_clause ::= INITIAL.
keyword_not_select_clause ::= INITIATE.
keyword_not_select_clause ::= INTERVAL.
keyword_not_select_clause ::= ISSUER.
keyword_not_select_clause ::= MAX_CONNECTIONS_PER_HOUR.
keyword_not_select_clause ::= MAX_QUERIES_PER_HOUR.
keyword_not_select_clause ::= MAX_UPDATES_PER_HOUR.
keyword_not_select_clause ::= MAX_USER_CONNECTIONS.
keyword_not_select_clause ::= NEVER.
keyword_not_select_clause ::= OLD.
keyword_not_select_clause ::= OPTION.
keyword_not_select_clause ::= OPTIONAL.
keyword_not_select_clause ::= OPTIONALLY.
keyword_not_select_clause ::= PARSER.
keyword_not_select_clause ::= PASSWORD_LOCK_TIME.
keyword_not_select_clause ::= PROXY.
keyword_not_select_clause ::= REGISTRATION.
keyword_not_select_clause ::= REUSE.
keyword_not_select_clause ::= ROWS.
keyword_not_select_clause ::= SECONDARY_LOAD.
keyword_not_select_clause ::= SECONDARY_UNLOAD.
keyword_not_select_clause ::= STARTING.
keyword_not_select_clause ::= SUBJECT.
keyword_not_select_clause ::= TERMINATED.
keyword_not_select_clause ::= UNBOUNDED.
keyword_not_select_clause ::= UNREGISTER.
keyword_not_select_clause ::= UNKNOWN.
keyword_not_select_clause ::= PARTITIONING.
keyword_not_select_clause ::= VALIDATION.
keyword_not_select_clause ::= VISIBLE.
keyword_not_select_clause ::= WITHOUT.
keyword_not_select_clause ::= X509.

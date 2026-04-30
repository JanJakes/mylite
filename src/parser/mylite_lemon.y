%name MyLiteLemon
%token_prefix ML_
%token_type {MyliteToken}
%default_type {MyliteToken}
%fallback ATOM DOT.
%type labeled_statement_start {MyliteStatementKind}
%type permissive_start {MyliteStatementKind}
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

create_tail ::= create_object_kind required_statement_tail.
create_tail ::= TEMPORARY TABLE required_statement_tail.
create_tail ::= create_index_kind INDEX required_statement_tail.
create_tail ::= AGGREGATE FUNCTION required_statement_tail.
create_tail ::= LOGFILE ATOM required_statement_tail.
create_tail ::= RESOURCE ATOM required_statement_tail.
create_tail ::= UNDO TABLESPACE required_statement_tail.

create_index_kind ::= UNIQUE.
create_index_kind ::= FULLTEXT.
create_index_kind ::= SPATIAL.

create_object_kind ::= TABLE.
create_object_kind ::= VIEW.
create_object_kind ::= OR.
create_object_kind ::= ALGORITHM.
create_object_kind ::= SQL.
create_object_kind ::= DEFINER.
create_object_kind ::= DATABASE.
create_object_kind ::= SCHEMA.
create_object_kind ::= INDEX.
create_object_kind ::= EVENT.
create_object_kind ::= FUNCTION.
create_object_kind ::= PROCEDURE.
create_object_kind ::= TRIGGER.
create_object_kind ::= USER.
create_object_kind ::= ROLE.
create_object_kind ::= SERVER.
create_object_kind ::= TABLESPACE.

drop_statement ::= DROP drop_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

drop_tail ::= drop_object_kind required_statement_tail.
drop_tail ::= TEMPORARY drop_table_kind required_statement_tail.
drop_tail ::= LOGFILE ATOM required_statement_tail.
drop_tail ::= RESOURCE ATOM required_statement_tail.
drop_tail ::= SPATIAL ATOM required_statement_tail.
drop_tail ::= UNDO TABLESPACE required_statement_tail.

drop_table_kind ::= TABLE.
drop_table_kind ::= TABLES.

drop_object_kind ::= TABLE.
drop_object_kind ::= TABLES.
drop_object_kind ::= DATABASE.
drop_object_kind ::= SCHEMA.
drop_object_kind ::= VIEW.
drop_object_kind ::= EVENT.
drop_object_kind ::= FUNCTION.
drop_object_kind ::= PROCEDURE.
drop_object_kind ::= PREPARE.
drop_object_kind ::= TRIGGER.
drop_object_kind ::= USER.
drop_object_kind ::= ROLE.
drop_object_kind ::= SERVER.
drop_object_kind ::= INDEX.
drop_object_kind ::= TABLESPACE.

alter_statement ::= ALTER alter_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

alter_tail ::= alter_object_kind required_statement_tail.
alter_tail ::= LOGFILE ATOM required_statement_tail.
alter_tail ::= RESOURCE ATOM required_statement_tail.
alter_tail ::= UNDO TABLESPACE required_statement_tail.

alter_object_kind ::= TABLE.
alter_object_kind ::= ALGORITHM.
alter_object_kind ::= DATABASE.
alter_object_kind ::= DEFINER.
alter_object_kind ::= SCHEMA.
alter_object_kind ::= VIEW.
alter_object_kind ::= EVENT.
alter_object_kind ::= FUNCTION.
alter_object_kind ::= PROCEDURE.
alter_object_kind ::= USER.
alter_object_kind ::= INSTANCE.
alter_object_kind ::= SERVER.
alter_object_kind ::= TABLESPACE.

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

rename_user_token ::= ATOM.
rename_user_token ::= LABEL.
rename_user_token ::= MASTER.
rename_user_token ::= ROLE.
rename_user_token ::= USER.

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

load_infile ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "INFILE");
}

load_file_table_tail ::= INTO TABLE required_statement_tail.
load_file_table_tail ::= load_duplicate_handling INTO TABLE required_statement_tail.

load_duplicate_handling ::= IGNORE.
load_duplicate_handling ::= REPLACE.

load_index_tail ::= .
load_index_tail ::= load_index_partition.
load_index_tail ::= IGNORE load_leaves.
load_index_tail ::= load_index_partition IGNORE load_leaves.

load_index_partition ::= load_partition LP load_partition_names RP.

load_partition ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "PARTITION");
}

load_partition_names ::= load_partition_name.
load_partition_names ::= load_partition_names import_comma load_partition_name.

load_partition_name ::= ATOM.
load_partition_name ::= LABEL.
load_partition_name ::= ALL.

load_leaves ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "LEAVES");
}

start_statement ::= START start_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

start_tail ::= TRANSACTION start_transaction_tail.
start_tail ::= REPLICA statement_tail.
start_tail ::= SLAVE statement_tail.
start_tail ::= GROUP_REPLICATION statement_tail.

start_transaction_tail ::= .
start_transaction_tail ::= transaction_characteristics.

transaction_characteristics ::= transaction_characteristic.
transaction_characteristics ::= transaction_characteristics import_comma transaction_characteristic.

transaction_characteristic ::= READ transaction_access_mode.
transaction_characteristic ::= WITH transaction_consistent transaction_snapshot.

transaction_access_mode ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "ONLY");
}
transaction_access_mode ::= WRITE.

transaction_consistent ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "CONSISTENT");
}
transaction_snapshot ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "SNAPSHOT");
}

savepoint_statement ::= SAVEPOINT savepoint_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

savepoint_name ::= ATOM.
savepoint_name ::= LABEL.

release_statement ::= RELEASE SAVEPOINT savepoint_name. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

lock_statement ::= LOCK lock_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

lock_tail ::= lock_table_kind lock_table_list.
lock_tail ::= INSTANCE FOR lock_backup.

lock_backup ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "BACKUP");
}

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

table_admin_statement ::= ANALYZE table_admin_with_optional_binlog. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= CHECK table_admin_table_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= CHECKSUM table_admin_table_tail_singular. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= OPTIMIZE table_admin_with_optional_binlog. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= REPAIR table_admin_with_optional_binlog. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

table_admin_with_optional_binlog ::= table_admin_table_tail.
table_admin_with_optional_binlog ::= LOCAL table_admin_table_tail.
table_admin_with_optional_binlog ::= NO_WRITE_TO_BINLOG table_admin_table_tail.

table_admin_table_tail ::= table_admin_table_keyword table_admin_table_list table_admin_options.

table_admin_table_tail_singular ::= TABLE table_admin_table_list table_admin_options.

table_admin_table_keyword ::= TABLE.
table_admin_table_keyword ::= TABLES.

table_admin_table_list ::= cache_table_ref.
table_admin_table_list ::= table_admin_table_list import_comma cache_table_ref.

table_admin_options ::= .
table_admin_options ::= table_admin_option_start statement_tail.

table_admin_option_start ::= UPDATE.
table_admin_option_start ::= DROP.
table_admin_option_start ::= EXTENDED.
table_admin_option_start ::= FOR.
table_admin_option_start ::= QUICK.
table_admin_option_start ::= ATOM(A). {
  mylite_parser_require_table_admin_option(ctx, A);
}

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
component_install_tail ::= SET required_statement_tail.

component_file_list ::= component_file.
component_file_list ::= component_file_list import_comma component_file.

component_file ::= ATOM.

plugin_soname ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "SONAME");
}

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
cache_name_part ::= COMPONENT.
cache_name_part ::= LABEL.
cache_name_part ::= DEFAULT.
cache_name_part ::= ENGINE.
cache_name_part ::= EVENTS.
cache_name_part ::= GRANTS.
cache_name_part ::= PLUGIN.
cache_name_part ::= PROCESSLIST.
cache_name_part ::= TABLES.
cache_name_part ::= TABLESPACE.
cache_name_part ::= TRIGGERS.
cache_name_part ::= USER.

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

reset_gtids ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "GTIDS");
}

reset_persist_tail ::= .
reset_persist_tail ::= reset_persist_target.
reset_persist_tail ::= IF reset_exists reset_persist_target.

reset_exists ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "EXISTS");
}

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

reset_channel ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "CHANNEL");
}

reset_channel_name ::= ATOM.
reset_channel_name ::= LABEL.

purge_statement ::= PURGE purge_log_kind LOGS purge_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

purge_tail ::= TO purge_log_name.
purge_tail ::= purge_before expression_start statement_tail.

purge_before ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "BEFORE");
}

purge_log_kind ::= BINARY.
purge_log_kind ::= MASTER.

purge_log_name ::= ATOM.
purge_log_name ::= LABEL.

change_statement ::= CHANGE change_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

change_tail ::= MASTER TO change_options change_for_channel_tail.
change_tail ::= REPLICATION change_replication_source TO change_options change_for_channel_tail.

change_replication_source ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "SOURCE");
}

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

xa_recover_convert ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "CONVERT");
}
xa_recover_xid ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "XID");
}

xa_xid ::= ATOM.
xa_xid ::= ATOM import_comma ATOM.
xa_xid ::= ATOM import_comma ATOM import_comma ATOM.

xa_start_tail ::= .
xa_start_tail ::= xa_start_option.

xa_start_option ::= ATOM(A). {
  mylite_parser_require_token_text_any(ctx, A, "JOIN", "RESUME");
}

xa_end_tail ::= .
xa_end_tail ::= xa_suspend.
xa_end_tail ::= xa_suspend FOR xa_migrate.

xa_suspend ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "SUSPEND");
}
xa_migrate ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "MIGRATE");
}

xa_commit_tail ::= .
xa_commit_tail ::= xa_one xa_phase.

xa_one ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "ONE");
}
xa_phase ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "PHASE");
}

show_statement ::= SHOW show_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}

show_tail ::= show_unprefixed_kind statement_tail.
show_tail ::= show_full_tail statement_tail.
show_tail ::= show_scope_prefix show_scoped_kind show_filter_tail.
show_tail ::= COUNT LP show_count_star RP show_count_kind.
show_tail ::= CREATE show_create_tail.
show_tail ::= show_diagnostics_kind show_limit_tail.
show_tail ::= show_simple_kind.
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

show_count_star ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "*");
}

show_count_kind ::= ERRORS.
show_count_kind ::= WARNINGS.

show_create_tail ::= show_create_named_kind cache_table_ref.
show_create_tail ::= USER rename_user_account.

show_create_named_kind ::= DATABASE.
show_create_named_kind ::= EVENT.
show_create_named_kind ::= FUNCTION.
show_create_named_kind ::= PROCEDURE.
show_create_named_kind ::= SCHEMA.
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
show_slave_tail ::= STATUS.

show_grants_tail ::= .
show_grants_tail ::= FOR show_grants_principal show_grants_using_tail.

show_grants_using_tail ::= .
show_grants_using_tail ::= USING show_grants_principal_list.

show_grants_principal_list ::= show_grants_principal.
show_grants_principal_list ::= show_grants_principal_list import_comma show_grants_principal.

show_grants_principal ::= rename_user_account.
show_grants_principal ::= ATOM LP RP.

show_full_kind ::= PROCESSLIST.

show_unprefixed_kind ::= BINARY.
show_unprefixed_kind ::= BINLOG.
show_unprefixed_kind ::= CHARACTER.
show_unprefixed_kind ::= CHARSET.
show_unprefixed_kind ::= COLLATION.
show_unprefixed_kind ::= ENGINE.
show_unprefixed_kind ::= FUNCTION.
show_unprefixed_kind ::= PROCEDURE.
show_unprefixed_kind ::= PROFILE.
show_unprefixed_kind ::= RELAYLOG.
show_unprefixed_kind ::= REPLICA.
show_unprefixed_kind ::= STORAGE.

describe_statement ::= DESCRIBE describe_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}
describe_statement ::= DESC describe_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}

describe_tail ::= SELECT select_tail.
describe_tail ::= describe_table_ref.
describe_tail ::= describe_table_ref describe_column_ref.

describe_table_ref ::= describe_name_part.
describe_table_ref ::= describe_name_part DOT describe_name_part.

describe_column_ref ::= ATOM.
describe_column_ref ::= LABEL.

describe_name_part ::= ATOM.
describe_name_part ::= LABEL.

explain_statement ::= EXPLAIN explain_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SHOW);
}

explain_tail ::= explain_query_start statement_tail.
explain_tail ::= FORMAT ATOM required_statement_tail.
explain_tail ::= FOR CONNECTION required_statement_tail.

explain_query_start ::= ANALYZE.
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

handler_as ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "AS");
}

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

handler_read_operator ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "=");
}

handler_read_tuple ::= LP values_row_contents RP.

handler_read_suffix ::= .
handler_read_suffix ::= WHERE required_statement_tail.
handler_read_suffix ::= LIMIT required_statement_tail.

call_statement ::= CALL call_name call_arguments. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

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

clone_tail ::= INSTANCE FROM clone_instance_source clone_identified BY clone_password.
clone_tail ::= LOCAL DATA clone_directory diagnostics_equals clone_directory_path.

clone_instance_source ::= clone_user clone_at clone_host clone_colon ATOM.

clone_user ::= ATOM.
clone_user ::= LABEL.

clone_at ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "@");
}

clone_host ::= ATOM.
clone_host ::= LABEL.

clone_colon ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, ":");
}

clone_identified ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "IDENTIFIED");
}

clone_password ::= ATOM.

clone_directory ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "DIRECTORY");
}

clone_directory_path ::= ATOM.

flush_statement ::= FLUSH flush_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

flush_tail ::= flush_simple_list.
flush_tail ::= flush_table_kind flush_table_tail.
flush_tail ::= LOCAL flush_table_kind flush_table_tail.
flush_tail ::= NO_WRITE_TO_BINLOG flush_table_kind flush_table_tail.
flush_tail ::= BINARY LOGS.
flush_tail ::= ERROR LOGS.
flush_tail ::= RELAY LOGS.

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

flush_export ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "EXPORT");
}

restart_statement ::= RESTART. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

shutdown_statement ::= SHUTDOWN. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

insert_statement ::= INSERT insert_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_INSERT);
}

insert_tail ::= dml_insert_target required_statement_tail.
insert_tail ::= dml_insert_modifiers dml_insert_target required_statement_tail.

dml_insert_modifiers ::= dml_insert_modifier.
dml_insert_modifiers ::= dml_insert_modifiers dml_insert_modifier.

dml_insert_modifier ::= DELAYED.
dml_insert_modifier ::= HIGH_PRIORITY.
dml_insert_modifier ::= IGNORE.
dml_insert_modifier ::= LOW_PRIORITY.

dml_insert_target ::= ATOM.
dml_insert_target ::= INTO.
dml_insert_target ::= LABEL.

replace_statement ::= REPLACE replace_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLACE);
}

replace_tail ::= dml_replace_target required_statement_tail.
replace_tail ::= dml_replace_modifiers dml_replace_target required_statement_tail.

dml_replace_modifiers ::= dml_replace_modifier.
dml_replace_modifiers ::= dml_replace_modifiers dml_replace_modifier.

dml_replace_modifier ::= DELAYED.
dml_replace_modifier ::= LOW_PRIORITY.

dml_replace_target ::= ATOM.
dml_replace_target ::= INTO.
dml_replace_target ::= LABEL.

update_statement ::= UPDATE update_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UPDATE);
}

update_tail ::= dml_update_target required_statement_tail.
update_tail ::= dml_update_modifiers dml_update_target required_statement_tail.

dml_update_modifiers ::= dml_update_modifier.
dml_update_modifiers ::= dml_update_modifiers dml_update_modifier.

dml_update_modifier ::= IGNORE.
dml_update_modifier ::= LOW_PRIORITY.

dml_update_target ::= ATOM.
dml_update_target ::= LABEL.
dml_update_target ::= LP.
dml_update_target ::= TRIGGERS.
dml_update_target ::= USER.

delete_statement ::= DELETE delete_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DELETE);
}

delete_tail ::= dml_delete_target required_statement_tail.
delete_tail ::= dml_delete_modifiers dml_delete_target required_statement_tail.

dml_delete_modifiers ::= dml_delete_modifier.
dml_delete_modifiers ::= dml_delete_modifiers dml_delete_modifier.

dml_delete_modifier ::= IGNORE.
dml_delete_modifier ::= LOW_PRIORITY.
dml_delete_modifier ::= QUICK.

dml_delete_target ::= ATOM.
dml_delete_target ::= FROM.
dml_delete_target ::= LABEL.

with_statement ::= WITH with_first_token required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

with_first_token ::= ATOM.
with_first_token ::= LABEL.
with_first_token ::= RECURSIVE.

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
values_query_tail ::= UNION required_statement_tail.
values_query_tail ::= ORDER BY required_statement_tail.
values_query_tail ::= LIMIT required_statement_tail.

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

diagnostics_equals ::= ATOM(A). {
  mylite_parser_require_token_text(ctx, A, "=");
}

diagnostics_statement_item_name ::= ATOM(A). {
  mylite_parser_require_diagnostics_statement_item(ctx, A);
}

diagnostics_condition_item_name ::= ATOM(A). {
  mylite_parser_require_diagnostics_condition_item(ctx, A);
}

signal_statement ::= SIGNAL signal_condition_value signal_set_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

signal_condition_value ::= signal_named_condition.
signal_condition_value ::= SQLSTATE ATOM.

signal_named_condition ::= ATOM.
signal_named_condition ::= LABEL.

signal_set_tail ::= .
signal_set_tail ::= SET required_statement_tail.

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

set_statement ::= SET set_first_token required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

set_first_token ::= ATOM.
set_first_token ::= CHARACTER.
set_first_token ::= CHARSET.
set_first_token ::= DEFAULT.
set_first_token ::= GLOBAL.
set_first_token ::= LABEL.
set_first_token ::= LOCAL.
set_first_token ::= NAMES.
set_first_token ::= PASSWORD.
set_first_token ::= PERSIST.
set_first_token ::= RESOURCE.
set_first_token ::= ROLE.
set_first_token ::= SESSION.
set_first_token ::= SQL_BUFFER_RESULT.
set_first_token ::= TRANSACTION.

grant_statement ::= GRANT privilege_first_token required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

revoke_statement ::= REVOKE privilege_first_token required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

privilege_first_token ::= ALTER.
privilege_first_token ::= ALL.
privilege_first_token ::= ATOM.
privilege_first_token ::= CREATE.
privilege_first_token ::= DELETE.
privilege_first_token ::= DROP.
privilege_first_token ::= EVENT.
privilege_first_token ::= EXECUTE.
privilege_first_token ::= GRANT.
privilege_first_token ::= IF.
privilege_first_token ::= INDEX.
privilege_first_token ::= INSERT.
privilege_first_token ::= LABEL.
privilege_first_token ::= LOCK.
privilege_first_token ::= REPLICATION.
privilege_first_token ::= ROLE.
privilege_first_token ::= SELECT.
privilege_first_token ::= SHOW.
privilege_first_token ::= SHUTDOWN.
privilege_first_token ::= TRIGGER.
privilege_first_token ::= UPDATE.

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
expression_start_keyword ::= REPEAT.
expression_start_keyword ::= REPLACE.
expression_start_keyword ::= ROW.
expression_start_keyword ::= STATUS.
expression_start_keyword ::= TRUNCATE.
expression_start_keyword ::= USER.
expression_start_keyword ::= VALUES.

if_statement ::= IF expression_start required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

elseif_statement ::= ELSEIF expression_start required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

return_statement ::= RETURN expression_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

resignal_statement ::= RESIGNAL resignal_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

resignal_tail ::= .
resignal_tail ::= SET required_statement_tail.
resignal_tail ::= signal_condition_value signal_set_tail.

while_statement ::= WHILE expression_start required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

until_statement ::= UNTIL expression_start statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

when_statement ::= WHEN expression_start required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

open_statement ::= OPEN stored_program_label_ref. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

fetch_statement ::= FETCH stored_program_label_ref INTO required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

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

declare_statement ::= DECLARE declare_first_token statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

declare_first_token ::= ATOM.
declare_first_token ::= CONDITION.
declare_first_token ::= CONTINUE.
declare_first_token ::= CURSOR.
declare_first_token ::= EXIT.
declare_first_token ::= LABEL.
declare_first_token ::= UNDO.

end_statement ::= END statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_STORED_PROGRAM);
}

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
keyword ::= LOCK.
keyword ::= UNLOCK.
keyword ::= XA.
keyword ::= BINARY.
keyword ::= BINLOG.
keyword ::= BY.
keyword ::= PURGE.
keyword ::= RESET.
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
keyword ::= RESOURCE.
keyword ::= QUERY.
keyword ::= READ.
keyword ::= RECOVER.
keyword ::= REPLICATION.
keyword ::= SECURITY.
keyword ::= SQL.
keyword ::= SPATIAL.
keyword ::= TEMPORARY.
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
keyword ::= ENGINE.
keyword ::= ENGINES.
keyword ::= ERRORS.
keyword ::= ERROR.
keyword ::= EVENTS.
keyword ::= EXTENDED.
keyword ::= FIELDS.
keyword ::= FIRST.
keyword ::= FOR.
keyword ::= FORMAT.
keyword ::= FULL.
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
keyword ::= LOW_PRIORITY.
keyword ::= NAMES.
keyword ::= NO.
keyword ::= OFFSET.
keyword ::= ORDER.
keyword ::= PASSWORD.
keyword ::= QUICK.
keyword ::= NEXT.
keyword ::= PREV.
keyword ::= RECURSIVE.
keyword ::= ROW.
keyword ::= SQLSTATE.
keyword ::= DISTINCT.
keyword ::= DISTINCTROW.
keyword ::= SQL_BIG_RESULT.
keyword ::= SQL_BUFFER_RESULT.
keyword ::= SQL_CALC_FOUND_ROWS.
keyword ::= SQL_SMALL_RESULT.
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
keyword_not_select_clause ::= LOCK.
keyword_not_select_clause ::= UNLOCK.
keyword_not_select_clause ::= XA.
keyword_not_select_clause ::= BINARY.
keyword_not_select_clause ::= BINLOG.
keyword_not_select_clause ::= BY.
keyword_not_select_clause ::= PURGE.
keyword_not_select_clause ::= RESET.
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
keyword_not_select_clause ::= RESOURCE.
keyword_not_select_clause ::= QUERY.
keyword_not_select_clause ::= READ.
keyword_not_select_clause ::= RECOVER.
keyword_not_select_clause ::= REPLICATION.
keyword_not_select_clause ::= SECURITY.
keyword_not_select_clause ::= SQL.
keyword_not_select_clause ::= SPATIAL.
keyword_not_select_clause ::= TEMPORARY.
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
keyword_not_select_clause ::= ENGINE.
keyword_not_select_clause ::= ENGINES.
keyword_not_select_clause ::= ERRORS.
keyword_not_select_clause ::= ERROR.
keyword_not_select_clause ::= EVENTS.
keyword_not_select_clause ::= EXTENDED.
keyword_not_select_clause ::= FIELDS.
keyword_not_select_clause ::= FIRST.
keyword_not_select_clause ::= FOR.
keyword_not_select_clause ::= FORMAT.
keyword_not_select_clause ::= FULL.
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
keyword_not_select_clause ::= LOW_PRIORITY.
keyword_not_select_clause ::= NAMES.
keyword_not_select_clause ::= NO.
keyword_not_select_clause ::= OFFSET.
keyword_not_select_clause ::= ORDER.
keyword_not_select_clause ::= PASSWORD.
keyword_not_select_clause ::= QUICK.
keyword_not_select_clause ::= NEXT.
keyword_not_select_clause ::= PREV.
keyword_not_select_clause ::= RECURSIVE.
keyword_not_select_clause ::= ROW.
keyword_not_select_clause ::= SQLSTATE.
keyword_not_select_clause ::= DISTINCT.
keyword_not_select_clause ::= DISTINCTROW.
keyword_not_select_clause ::= SQL_BIG_RESULT.
keyword_not_select_clause ::= SQL_BUFFER_RESULT.
keyword_not_select_clause ::= SQL_CALC_FOUND_ROWS.
keyword_not_select_clause ::= SQL_SMALL_RESULT.
keyword_not_select_clause ::= STRAIGHT_JOIN.
keyword_not_select_clause ::= TO.
keyword_not_select_clause ::= WORK.

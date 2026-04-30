%name MyLiteLemon
%token_prefix ML_
%token_type {MyliteToken}
%default_type {MyliteToken}
%type required_tail_start {MyliteStatementKind}
%type optional_tail_start {MyliteStatementKind}
%type statement_start {MyliteStatementKind}
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
statement ::= required_tail_start(A) required_statement_tail. {
  mylite_parser_record_statement(ctx, A);
}
statement ::= optional_tail_start(A) statement_tail. {
  mylite_parser_record_statement(ctx, A);
}
statement ::= LABEL statement_start(A) statement_tail. {
  mylite_parser_record_statement(ctx, A);
}
statement ::= permissive_start(A) statement_tail. {
  mylite_parser_record_statement(ctx, A);
}

statement_start(A) ::= SELECT. { A = MYLITE_STATEMENT_SELECT; }
statement_start(A) ::= CREATE. { A = MYLITE_STATEMENT_DDL; }
statement_start(A) ::= required_tail_start(B). { A = B; }
statement_start(A) ::= optional_tail_start(B). { A = B; }

required_tail_start(A) ::= WITH. { A = MYLITE_STATEMENT_SELECT; }
required_tail_start(A) ::= INSERT. { A = MYLITE_STATEMENT_INSERT; }
required_tail_start(A) ::= REPLACE. { A = MYLITE_STATEMENT_REPLACE; }
required_tail_start(A) ::= UPDATE. { A = MYLITE_STATEMENT_UPDATE; }
required_tail_start(A) ::= DELETE. { A = MYLITE_STATEMENT_DELETE; }
required_tail_start(A) ::= CALL. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= DO. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= TABLE. { A = MYLITE_STATEMENT_SELECT; }
required_tail_start(A) ::= VALUES. { A = MYLITE_STATEMENT_SELECT; }
required_tail_start(A) ::= HANDLER. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= SET. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= BINLOG. { A = MYLITE_STATEMENT_REPLICATION; }
required_tail_start(A) ::= PREPARE. { A = MYLITE_STATEMENT_PREPARED; }
required_tail_start(A) ::= EXECUTE. { A = MYLITE_STATEMENT_PREPARED; }
required_tail_start(A) ::= GRANT. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= REVOKE. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= SHOW. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= DESCRIBE. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= DESC. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= EXPLAIN. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= HELP. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= USE. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= CLONE. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= GET. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= SIGNAL. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= IF. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= ELSEIF. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= RETURN. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= LEAVE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= ITERATE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }

select_statement ::= SELECT select_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_SELECT);
}

select_tail ::= select_first_token.
select_tail ::= select_tail statement_token.

select_first_token ::= ATOM.
select_first_token ::= LABEL.
select_first_token ::= keyword_not_select_clause.
select_first_token ::= LP.
select_first_token ::= RP.
select_first_token ::= LB.
select_first_token ::= RB.
select_first_token ::= LC.
select_first_token ::= RC.

create_statement ::= CREATE create_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

create_tail ::= create_first_token required_statement_tail.

create_first_token ::= TABLE.
create_first_token ::= TEMPORARY.
create_first_token ::= VIEW.
create_first_token ::= OR.
create_first_token ::= ALGORITHM.
create_first_token ::= SQL.
create_first_token ::= DEFINER.
create_first_token ::= DATABASE.
create_first_token ::= SCHEMA.
create_first_token ::= INDEX.
create_first_token ::= UNIQUE.
create_first_token ::= FULLTEXT.
create_first_token ::= SPATIAL.
create_first_token ::= EVENT.
create_first_token ::= FUNCTION.
create_first_token ::= AGGREGATE.
create_first_token ::= PROCEDURE.
create_first_token ::= TRIGGER.
create_first_token ::= USER.
create_first_token ::= ROLE.
create_first_token ::= RESOURCE.
create_first_token ::= SERVER.
create_first_token ::= LOGFILE.
create_first_token ::= TABLESPACE.
create_first_token ::= UNDO.

drop_statement ::= DROP drop_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

drop_tail ::= drop_first_token required_statement_tail.

drop_first_token ::= TABLE.
drop_first_token ::= TABLES.
drop_first_token ::= TEMPORARY.
drop_first_token ::= DATABASE.
drop_first_token ::= SCHEMA.
drop_first_token ::= VIEW.
drop_first_token ::= EVENT.
drop_first_token ::= FUNCTION.
drop_first_token ::= PROCEDURE.
drop_first_token ::= PREPARE.
drop_first_token ::= TRIGGER.
drop_first_token ::= USER.
drop_first_token ::= ROLE.
drop_first_token ::= SERVER.
drop_first_token ::= INDEX.
drop_first_token ::= LOGFILE.
drop_first_token ::= TABLESPACE.
drop_first_token ::= UNDO.
drop_first_token ::= SPATIAL.
drop_first_token ::= RESOURCE.

alter_statement ::= ALTER alter_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

alter_tail ::= alter_first_token required_statement_tail.

alter_first_token ::= TABLE.
alter_first_token ::= ALGORITHM.
alter_first_token ::= DATABASE.
alter_first_token ::= DEFINER.
alter_first_token ::= SCHEMA.
alter_first_token ::= VIEW.
alter_first_token ::= EVENT.
alter_first_token ::= FUNCTION.
alter_first_token ::= PROCEDURE.
alter_first_token ::= USER.
alter_first_token ::= INSTANCE.
alter_first_token ::= LOGFILE.
alter_first_token ::= RESOURCE.
alter_first_token ::= SERVER.
alter_first_token ::= TABLESPACE.
alter_first_token ::= UNDO.

rename_statement ::= RENAME rename_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

rename_tail ::= rename_first_token required_statement_tail.

rename_first_token ::= TABLE.
rename_first_token ::= TABLES.
rename_first_token ::= USER.

truncate_statement ::= TRUNCATE truncate_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_DDL);
}

truncate_tail ::= TABLE required_statement_tail.
truncate_tail ::= truncate_table_name statement_tail.

truncate_table_name ::= ATOM.
truncate_table_name ::= LABEL.

load_statement ::= LOAD load_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

load_tail ::= load_first_token required_statement_tail.

load_first_token ::= DATA.
load_first_token ::= XML.
load_first_token ::= INDEX.

start_statement ::= START start_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

start_tail ::= start_first_token statement_tail.

start_first_token ::= TRANSACTION.
start_first_token ::= REPLICA.
start_first_token ::= SLAVE.
start_first_token ::= GROUP_REPLICATION.

savepoint_statement ::= SAVEPOINT savepoint_name statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

savepoint_name ::= ATOM.
savepoint_name ::= LABEL.

release_statement ::= RELEASE SAVEPOINT required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

lock_statement ::= LOCK lock_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

lock_tail ::= lock_first_token required_statement_tail.

lock_first_token ::= TABLE.
lock_first_token ::= TABLES.
lock_first_token ::= INSTANCE.

unlock_statement ::= UNLOCK unlock_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_TRANSACTION);
}

unlock_tail ::= unlock_first_token statement_tail.

unlock_first_token ::= TABLE.
unlock_first_token ::= TABLES.
unlock_first_token ::= INSTANCE.

table_admin_statement ::= ANALYZE table_admin_with_optional_binlog. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= CHECK table_admin_table_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
table_admin_statement ::= CHECKSUM table_admin_table_tail. {
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

table_admin_table_tail ::= table_admin_table_keyword required_statement_tail.

table_admin_table_keyword ::= TABLE.
table_admin_table_keyword ::= TABLES.

plugin_admin_statement ::= INSTALL plugin_admin_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}
plugin_admin_statement ::= UNINSTALL plugin_admin_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

plugin_admin_tail ::= plugin_admin_object required_statement_tail.

plugin_admin_object ::= COMPONENT.
plugin_admin_object ::= PLUGIN.

import_statement ::= IMPORT TABLE required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_UTILITY);
}

cache_statement ::= CACHE INDEX required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

kill_statement ::= KILL kill_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

kill_tail ::= kill_target statement_tail.
kill_tail ::= kill_mode required_statement_tail.

kill_mode ::= CONNECTION.
kill_mode ::= QUERY.

kill_target ::= ATOM.
kill_target ::= LABEL.

deallocate_statement ::= DEALLOCATE PREPARE required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_PREPARED);
}

reset_statement ::= RESET reset_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_ADMIN);
}

reset_tail ::= reset_first_token statement_tail.

reset_first_token ::= BINARY.
reset_first_token ::= MASTER.
reset_first_token ::= PERSIST.
reset_first_token ::= REPLICA.
reset_first_token ::= SLAVE.

purge_statement ::= PURGE purge_first_token required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

purge_first_token ::= BINARY.
purge_first_token ::= MASTER.

change_statement ::= CHANGE change_first_token required_statement_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

change_first_token ::= MASTER.
change_first_token ::= REPLICATION.

xa_statement ::= XA xa_tail. {
  mylite_parser_record_statement(ctx, MYLITE_STATEMENT_REPLICATION);
}

xa_tail ::= RECOVER statement_tail.
xa_tail ::= xa_first_token required_statement_tail.

xa_first_token ::= START.
xa_first_token ::= BEGIN.
xa_first_token ::= END.
xa_first_token ::= PREPARE.
xa_first_token ::= COMMIT.
xa_first_token ::= ROLLBACK.

optional_tail_start(A) ::= BEGIN. { A = MYLITE_STATEMENT_TRANSACTION; }
optional_tail_start(A) ::= COMMIT. { A = MYLITE_STATEMENT_TRANSACTION; }
optional_tail_start(A) ::= ROLLBACK. { A = MYLITE_STATEMENT_TRANSACTION; }
optional_tail_start(A) ::= FLUSH. { A = MYLITE_STATEMENT_ADMIN; }
optional_tail_start(A) ::= RESTART. { A = MYLITE_STATEMENT_ADMIN; }
optional_tail_start(A) ::= SHUTDOWN. { A = MYLITE_STATEMENT_ADMIN; }
optional_tail_start(A) ::= RESIGNAL. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= ELSE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= LOOP. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= REPEAT. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= UNTIL. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= WHILE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= CASE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= WHEN. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= DECLARE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= END. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= OPEN. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= FETCH. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= CLOSE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
optional_tail_start(A) ::= LP. { A = MYLITE_STATEMENT_SELECT; }

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
keyword ::= START.
keyword ::= BEGIN.
keyword ::= COMMIT.
keyword ::= COMPONENT.
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
keyword ::= CASE.
keyword ::= WHEN.
keyword ::= TRIGGER.
keyword ::= DECLARE.
keyword ::= OPEN.
keyword ::= FETCH.
keyword ::= CLOSE.
keyword ::= RETURN.
keyword ::= LEAVE.
keyword ::= ITERATE.
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
keyword ::= RECOVER.
keyword ::= REPLICATION.
keyword ::= SECURITY.
keyword ::= SQL.
keyword ::= SPATIAL.
keyword ::= TEMPORARY.
keyword ::= TRANSACTION.
keyword ::= UNDO.
keyword ::= UNIQUE.
keyword ::= XML.

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
keyword_not_select_clause ::= START.
keyword_not_select_clause ::= BEGIN.
keyword_not_select_clause ::= COMMIT.
keyword_not_select_clause ::= COMPONENT.
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
keyword_not_select_clause ::= CASE.
keyword_not_select_clause ::= WHEN.
keyword_not_select_clause ::= TRIGGER.
keyword_not_select_clause ::= DECLARE.
keyword_not_select_clause ::= OPEN.
keyword_not_select_clause ::= FETCH.
keyword_not_select_clause ::= CLOSE.
keyword_not_select_clause ::= RETURN.
keyword_not_select_clause ::= LEAVE.
keyword_not_select_clause ::= ITERATE.
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
keyword_not_select_clause ::= RECOVER.
keyword_not_select_clause ::= REPLICATION.
keyword_not_select_clause ::= SECURITY.
keyword_not_select_clause ::= SQL.
keyword_not_select_clause ::= SPATIAL.
keyword_not_select_clause ::= TEMPORARY.
keyword_not_select_clause ::= TRANSACTION.
keyword_not_select_clause ::= UNDO.
keyword_not_select_clause ::= UNIQUE.
keyword_not_select_clause ::= XML.

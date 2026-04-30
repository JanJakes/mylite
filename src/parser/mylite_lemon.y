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

statement_start(A) ::= required_tail_start(B). { A = B; }
statement_start(A) ::= optional_tail_start(B). { A = B; }

required_tail_start(A) ::= SELECT. { A = MYLITE_STATEMENT_SELECT; }
required_tail_start(A) ::= WITH. { A = MYLITE_STATEMENT_SELECT; }
required_tail_start(A) ::= INSERT. { A = MYLITE_STATEMENT_INSERT; }
required_tail_start(A) ::= REPLACE. { A = MYLITE_STATEMENT_REPLACE; }
required_tail_start(A) ::= UPDATE. { A = MYLITE_STATEMENT_UPDATE; }
required_tail_start(A) ::= DELETE. { A = MYLITE_STATEMENT_DELETE; }
required_tail_start(A) ::= CREATE. { A = MYLITE_STATEMENT_DDL; }
required_tail_start(A) ::= ALTER. { A = MYLITE_STATEMENT_DDL; }
required_tail_start(A) ::= DROP. { A = MYLITE_STATEMENT_DDL; }
required_tail_start(A) ::= TRUNCATE. { A = MYLITE_STATEMENT_DDL; }
required_tail_start(A) ::= RENAME. { A = MYLITE_STATEMENT_DDL; }
required_tail_start(A) ::= CALL. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= DO. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= LOAD. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= TABLE. { A = MYLITE_STATEMENT_SELECT; }
required_tail_start(A) ::= VALUES. { A = MYLITE_STATEMENT_SELECT; }
required_tail_start(A) ::= HANDLER. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= IMPORT. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= START. { A = MYLITE_STATEMENT_TRANSACTION; }
required_tail_start(A) ::= SAVEPOINT. { A = MYLITE_STATEMENT_TRANSACTION; }
required_tail_start(A) ::= RELEASE. { A = MYLITE_STATEMENT_TRANSACTION; }
required_tail_start(A) ::= SET. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= LOCK. { A = MYLITE_STATEMENT_TRANSACTION; }
required_tail_start(A) ::= UNLOCK. { A = MYLITE_STATEMENT_TRANSACTION; }
required_tail_start(A) ::= XA. { A = MYLITE_STATEMENT_REPLICATION; }
required_tail_start(A) ::= BINLOG. { A = MYLITE_STATEMENT_REPLICATION; }
required_tail_start(A) ::= PURGE. { A = MYLITE_STATEMENT_REPLICATION; }
required_tail_start(A) ::= RESET. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= CHANGE. { A = MYLITE_STATEMENT_REPLICATION; }
required_tail_start(A) ::= PREPARE. { A = MYLITE_STATEMENT_PREPARED; }
required_tail_start(A) ::= EXECUTE. { A = MYLITE_STATEMENT_PREPARED; }
required_tail_start(A) ::= DEALLOCATE. { A = MYLITE_STATEMENT_PREPARED; }
required_tail_start(A) ::= GRANT. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= REVOKE. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= SHOW. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= DESCRIBE. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= DESC. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= EXPLAIN. { A = MYLITE_STATEMENT_SHOW; }
required_tail_start(A) ::= HELP. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= USE. { A = MYLITE_STATEMENT_UTILITY; }
required_tail_start(A) ::= ANALYZE. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= CHECK. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= CHECKSUM. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= OPTIMIZE. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= REPAIR. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= INSTALL. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= UNINSTALL. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= CLONE. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= CACHE. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= KILL. { A = MYLITE_STATEMENT_ADMIN; }
required_tail_start(A) ::= GET. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= SIGNAL. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= IF. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= ELSEIF. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= RETURN. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= LEAVE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }
required_tail_start(A) ::= ITERATE. { A = MYLITE_STATEMENT_STORED_PROGRAM; }

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
keyword ::= DATABASE.
keyword ::= ALTER.
keyword ::= DROP.
keyword ::= TRUNCATE.
keyword ::= RENAME.
keyword ::= CALL.
keyword ::= DO.
keyword ::= LOAD.
keyword ::= TABLE.
keyword ::= TABLESPACE.
keyword ::= VALUES.
keyword ::= HANDLER.
keyword ::= IMPORT.
keyword ::= START.
keyword ::= BEGIN.
keyword ::= COMMIT.
keyword ::= ROLLBACK.
keyword ::= SAVEPOINT.
keyword ::= RELEASE.
keyword ::= SET.
keyword ::= LOCK.
keyword ::= UNLOCK.
keyword ::= XA.
keyword ::= BINLOG.
keyword ::= PURGE.
keyword ::= RESET.
keyword ::= CHANGE.
keyword ::= PREPARE.
keyword ::= PROCEDURE.
keyword ::= EXECUTE.
keyword ::= DEALLOCATE.
keyword ::= GRANT.
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

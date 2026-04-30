%name MyLiteLemon
%token_prefix ML_
%token_type {MyliteToken}
%default_type {MyliteToken}
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

statement_chunk ::= SEMI.
statement_chunk ::= statement SEMI.

statement ::= statement_start statement_tail.
statement ::= LABEL statement_start statement_tail.
statement ::= permissive_start statement_tail.

statement_start ::= SELECT.
statement_start ::= WITH.
statement_start ::= INSERT.
statement_start ::= REPLACE.
statement_start ::= UPDATE.
statement_start ::= DELETE.
statement_start ::= CREATE.
statement_start ::= ALTER.
statement_start ::= DROP.
statement_start ::= TRUNCATE.
statement_start ::= RENAME.
statement_start ::= CALL.
statement_start ::= DO.
statement_start ::= LOAD.
statement_start ::= TABLE.
statement_start ::= VALUES.
statement_start ::= HANDLER.
statement_start ::= IMPORT.
statement_start ::= START.
statement_start ::= BEGIN.
statement_start ::= COMMIT.
statement_start ::= ROLLBACK.
statement_start ::= SAVEPOINT.
statement_start ::= RELEASE.
statement_start ::= SET.
statement_start ::= LOCK.
statement_start ::= UNLOCK.
statement_start ::= XA.
statement_start ::= BINLOG.
statement_start ::= PURGE.
statement_start ::= RESET.
statement_start ::= CHANGE.
statement_start ::= PREPARE.
statement_start ::= EXECUTE.
statement_start ::= DEALLOCATE.
statement_start ::= GRANT.
statement_start ::= REVOKE.
statement_start ::= SHOW.
statement_start ::= DESCRIBE.
statement_start ::= DESC.
statement_start ::= EXPLAIN.
statement_start ::= HELP.
statement_start ::= USE.
statement_start ::= ANALYZE.
statement_start ::= CHECK.
statement_start ::= CHECKSUM.
statement_start ::= OPTIMIZE.
statement_start ::= REPAIR.
statement_start ::= INSTALL.
statement_start ::= UNINSTALL.
statement_start ::= CLONE.
statement_start ::= CACHE.
statement_start ::= FLUSH.
statement_start ::= KILL.
statement_start ::= RESTART.
statement_start ::= SHUTDOWN.
statement_start ::= GET.
statement_start ::= SIGNAL.
statement_start ::= RESIGNAL.
statement_start ::= IF.
statement_start ::= ELSE.
statement_start ::= ELSEIF.
statement_start ::= LOOP.
statement_start ::= REPEAT.
statement_start ::= UNTIL.
statement_start ::= WHILE.
statement_start ::= CASE.
statement_start ::= WHEN.
statement_start ::= DECLARE.
statement_start ::= END.
statement_start ::= OPEN.
statement_start ::= FETCH.
statement_start ::= CLOSE.
statement_start ::= RETURN.
statement_start ::= LEAVE.
statement_start ::= ITERATE.
statement_start ::= LP.

permissive_start ::= ATOM(A). { mylite_parser_require_permissive(ctx, A); }
permissive_start ::= FROM(A). { mylite_parser_require_permissive(ctx, A); }
permissive_start ::= HAVING(A). { mylite_parser_require_permissive(ctx, A); }
permissive_start ::= RP(A). { mylite_parser_require_permissive(ctx, A); }
permissive_start ::= LB(A). { mylite_parser_require_permissive(ctx, A); }
permissive_start ::= RB(A). { mylite_parser_require_permissive(ctx, A); }
permissive_start ::= LC(A). { mylite_parser_require_permissive(ctx, A); }
permissive_start ::= RC(A). { mylite_parser_require_permissive(ctx, A); }

statement_tail ::= .
statement_tail ::= statement_tail statement_token.

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
keyword ::= ALTER.
keyword ::= DROP.
keyword ::= TRUNCATE.
keyword ::= RENAME.
keyword ::= CALL.
keyword ::= DO.
keyword ::= LOAD.
keyword ::= TABLE.
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
keyword ::= EXECUTE.
keyword ::= DEALLOCATE.
keyword ::= GRANT.
keyword ::= REVOKE.
keyword ::= SHOW.
keyword ::= DESCRIBE.
keyword ::= DESC.
keyword ::= EXPLAIN.
keyword ::= HELP.
keyword ::= USE.
keyword ::= ANALYZE.
keyword ::= CHECK.
keyword ::= CHECKSUM.
keyword ::= OPTIMIZE.
keyword ::= REPAIR.
keyword ::= INSTALL.
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
keyword ::= DECLARE.
keyword ::= OPEN.
keyword ::= FETCH.
keyword ::= CLOSE.
keyword ::= RETURN.
keyword ::= LEAVE.
keyword ::= ITERATE.
keyword ::= FROM.
keyword ::= HAVING.
keyword ::= ELSE.
keyword ::= END.

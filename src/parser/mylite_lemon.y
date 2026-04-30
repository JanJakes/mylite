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

statement ::= required_tail_start required_statement_tail.
statement ::= optional_tail_start statement_tail.
statement ::= LABEL statement_start statement_tail.
statement ::= permissive_start statement_tail.

statement_start ::= required_tail_start.
statement_start ::= optional_tail_start.

required_tail_start ::= SELECT.
required_tail_start ::= WITH.
required_tail_start ::= INSERT.
required_tail_start ::= REPLACE.
required_tail_start ::= UPDATE.
required_tail_start ::= DELETE.
required_tail_start ::= CREATE.
required_tail_start ::= ALTER.
required_tail_start ::= DROP.
required_tail_start ::= TRUNCATE.
required_tail_start ::= RENAME.
required_tail_start ::= CALL.
required_tail_start ::= DO.
required_tail_start ::= LOAD.
required_tail_start ::= TABLE.
required_tail_start ::= VALUES.
required_tail_start ::= HANDLER.
required_tail_start ::= IMPORT.
required_tail_start ::= START.
required_tail_start ::= SAVEPOINT.
required_tail_start ::= RELEASE.
required_tail_start ::= SET.
required_tail_start ::= LOCK.
required_tail_start ::= UNLOCK.
required_tail_start ::= XA.
required_tail_start ::= BINLOG.
required_tail_start ::= PURGE.
required_tail_start ::= RESET.
required_tail_start ::= CHANGE.
required_tail_start ::= PREPARE.
required_tail_start ::= EXECUTE.
required_tail_start ::= DEALLOCATE.
required_tail_start ::= GRANT.
required_tail_start ::= REVOKE.
required_tail_start ::= SHOW.
required_tail_start ::= DESCRIBE.
required_tail_start ::= DESC.
required_tail_start ::= EXPLAIN.
required_tail_start ::= HELP.
required_tail_start ::= USE.
required_tail_start ::= ANALYZE.
required_tail_start ::= CHECK.
required_tail_start ::= CHECKSUM.
required_tail_start ::= OPTIMIZE.
required_tail_start ::= REPAIR.
required_tail_start ::= INSTALL.
required_tail_start ::= UNINSTALL.
required_tail_start ::= CLONE.
required_tail_start ::= CACHE.
required_tail_start ::= KILL.
required_tail_start ::= GET.
required_tail_start ::= SIGNAL.
required_tail_start ::= IF.
required_tail_start ::= ELSEIF.
required_tail_start ::= RETURN.
required_tail_start ::= LEAVE.
required_tail_start ::= ITERATE.

optional_tail_start ::= BEGIN.
optional_tail_start ::= COMMIT.
optional_tail_start ::= ROLLBACK.
optional_tail_start ::= FLUSH.
optional_tail_start ::= RESTART.
optional_tail_start ::= SHUTDOWN.
optional_tail_start ::= RESIGNAL.
optional_tail_start ::= ELSE.
optional_tail_start ::= LOOP.
optional_tail_start ::= REPEAT.
optional_tail_start ::= UNTIL.
optional_tail_start ::= WHILE.
optional_tail_start ::= CASE.
optional_tail_start ::= WHEN.
optional_tail_start ::= DECLARE.
optional_tail_start ::= END.
optional_tail_start ::= OPEN.
optional_tail_start ::= FETCH.
optional_tail_start ::= CLOSE.
optional_tail_start ::= LP.

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

# Parser admin and stored-program placeholders

This slice broadens MyLite's parser/runtime surface for MySQL 8.4.9 statements
that real applications and MySQL server-test corpora issue even when they do
not need server-side effects from an embedded database. It separates two
compatibility classes:

- server, account, role, privilege, resource-group, and persisted-variable
  management statements are accepted as embedded no-ops with one warning;
- stored program DDL and broad routine invocation syntax are parsed, then
  rejected with a clear unsupported diagnostic.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-user.html
- https://dev.mysql.com/doc/refman/8.4/en/grant.html
- https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html
- https://dev.mysql.com/doc/mysql/en/call.html
- https://dev.mysql.com/doc/refman/8.4/en/create-trigger.html
- https://dev.mysql.com/doc/refman/8.4/en/create-event.html
- https://dev.mysql.com/doc/refman/8.4/en/using-system-variables.html
- https://dev.mysql.com/doc/refman/8.4/en/sql-server-administration-statements.html
- https://dev.mysql.com/doc/refman/8.4/en/flush.html
- https://dev.mysql.com/doc/refman/8.4/en/kill.html
- https://dev.mysql.com/doc/refman/8.4/en/reset.html
- https://dev.mysql.com/doc/refman/8.4/en/purge-binary-logs.html

## Scope

### Embedded admin no-ops

MyLite accepts the following statement families as a compatibility placeholder:

- `CREATE USER`, `ALTER USER`, `DROP USER`, and `RENAME USER`
- `CREATE ROLE`, `DROP ROLE`, `SET ROLE`, and `SET DEFAULT ROLE`
- `GRANT` and `REVOKE`
- `SET PASSWORD`
- `CREATE RESOURCE GROUP`, `ALTER RESOURCE GROUP`, `DROP RESOURCE GROUP`, and
  `SET RESOURCE GROUP`
- `SET PERSIST ...`, `SET PERSIST_ONLY ...`, `SET @@PERSIST.name = ...`, and
  `SET @@PERSIST_ONLY.name = ...`
- `RESET`, including `RESET PERSIST`
- `FLUSH`, `PURGE`, `KILL`, `CACHE INDEX`, `LOAD INDEX INTO CACHE`,
  `ALTER INSTANCE`, `RESTART`, `SHUTDOWN`, `CLONE`, and `BINLOG`
- `SHOW CREATE USER`

The runtime behavior is:

- return success with no columns and no rows;
- report affected rows `0`;
- set `ROW_COUNT()` to `0`;
- append one warning, code `1105`, SQLSTATE `HY000`, message
  `MyLite accepted this server-only statement as an embedded no-op`;
- leave catalogs, session privileges, roles, passwords, persisted system
  variables, process state, binary logs, cache state, and user transactions
  unchanged.

MyLite intentionally does not emulate MySQL server-side implicit commits for
these embedded no-ops. There is no server state to mutate, and committing the
user transaction would be a surprising side effect for an admitted placeholder.

Multiple admin statements in one SQL string remain rejected by MyLite's
single-statement execution rule.

### Stored program parse-and-error surface

MyLite accepts broad syntax for the following stored-program surfaces, but
runtime execution returns `1064 / 42000` with an unsupported message:

- `CREATE FUNCTION` for stored functions
- `ALTER FUNCTION`
- `DROP FUNCTION`
- `CREATE TRIGGER`
- `DROP TRIGGER`
- `CREATE EVENT`
- `ALTER EVENT`
- `DROP EVENT`
- `SHOW CREATE FUNCTION`
- `SHOW CREATE TRIGGER`
- `SHOW CREATE EVENT`
- `CREATE PROCEDURE` forms outside the current limited no-argument
  single-`SELECT` bridge
- `ALTER PROCEDURE`
- `CALL` forms with arguments or unsupported parameter syntax

Existing supported routine behavior is preserved:

- session-local `CREATE PROCEDURE p() BEGIN SELECT ...; END`;
- `CALL p` and `CALL p()` for that no-argument procedure;
- `DROP PROCEDURE [IF EXISTS]` for that descriptor;
- `SHOW CREATE PROCEDURE` for that descriptor.

Stored program support remains intentionally incomplete. Correct support needs
persistent descriptors, definer/security metadata, parameters, routine
variables, handlers, cursors, multiple result sets, trigger `OLD`/`NEW` binding,
trigger firing during writes, event scheduling, privilege interactions, and
transaction/error behavior. Native SQLite triggers are not sufficient by
themselves because MyLite must own the MySQL semantic boundary.

## Parser approach

The normal Lemon grammar still handles MyLite-supported SQL. This slice adds a
post-parse placeholder recognizer that runs only after a syntax error. It
retokenizes the single SQL input, classifies statement families from their
leading tokens, and creates one of two raw statement AST nodes:

- `admin_noop_statement`
- `unsupported_stored_program_statement`

The recognizer does not use external grammar text. It admits broad suffix
syntax for the classified statement families because the runtime behavior is
placeholder-only.

`CALL name(expr, ...)` is part of the normal grammar and now stores its argument
list as the second child of `call_statement`; `CALL` forms with unsupported
parameter syntax fall back to the raw unsupported stored-program node.

`SET @@PERSIST.name = value` and `SET @@PERSIST_ONLY.name = value` are parsed by
the normal `SET` grammar. The runtime recognizes all assignments in such a
parsed statement as persisted-variable placeholders and routes the statement to
the admin no-op handler.

## MyLite grammar snippets

These snippets describe the intended MyLite-owned grammar shape rather than
copying MySQL grammar.

```lemon
call_statement ::= CALL table_name.
call_statement ::= CALL table_name LPAREN RPAREN.
call_statement ::= CALL table_name LPAREN function_argument_list RPAREN.
```

Post-parse placeholder classifier:

```text
admin_noop_statement:
    CREATE USER ...
  | CREATE ROLE ...
  | CREATE RESOURCE ...
  | ALTER USER ...
  | ALTER RESOURCE ...
  | ALTER INSTANCE ...
  | DROP USER ...
  | DROP ROLE ...
  | DROP RESOURCE ...
  | RENAME USER ...
  | GRANT ...
  | REVOKE ...
  | SET PASSWORD ...
  | SET ROLE ...
  | SET DEFAULT ROLE ...
  | SET RESOURCE ...
  | SET PERSIST ...
  | SET PERSIST_ONLY ...
  | RESET ...
  | FLUSH ...
  | PURGE ...
  | KILL ...
  | CACHE ...
  | LOAD INDEX ...
  | RESTART
  | SHUTDOWN
  | CLONE ...
  | BINLOG ...
  | SHOW CREATE USER ...

unsupported_stored_program_statement:
    CREATE [DEFINER ...] PROCEDURE ...
  | CREATE [DEFINER ...] FUNCTION ...
  | CREATE [DEFINER ...] TRIGGER ...
  | CREATE [DEFINER ...] EVENT ...
  | ALTER PROCEDURE ...
  | ALTER FUNCTION ...
  | ALTER EVENT ...
  | DROP FUNCTION ...
  | DROP TRIGGER ...
  | DROP EVENT ...
  | SHOW CREATE FUNCTION ...
  | SHOW CREATE TRIGGER ...
  | SHOW CREATE EVENT ...
  | CALL ...
```

## Tests

Coverage added:

- parser acceptance for representative admin no-op statement families;
- parser acceptance for stored functions, triggers, events, routine alteration,
  stored-program `SHOW CREATE`, and unsupported `CALL` syntax;
- normal parser support for argument-bearing `CALL`;
- runtime success/no rows/warning/`ROW_COUNT() = 0` for admin no-ops;
- runtime preservation of a user transaction across an admin no-op;
- runtime unsupported diagnostics for stored-program placeholders;
- regression coverage for the existing no-argument single-`SELECT` procedure
  bridge.

## Non-goals

- no account, password, role, grant, resource-group, or privilege storage;
- no privilege enforcement;
- no persisted system-variable file or shared server-global mutable state;
- no process kill, binary log, table cache, restart, shutdown, clone, or flush
  side effects;
- no persistent routine, trigger, or event descriptors beyond the existing
  session-local procedure bridge;
- no trigger execution, event scheduling, stored-function execution, routine
  parameters, variables, handlers, cursors, or multiple result sets.

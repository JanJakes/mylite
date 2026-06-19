# Baseline SHOW CREATE Residuals

## Status

This slice admits and implements the remaining baseline `SHOW CREATE` forms
whose detailed compatibility rows were still red:

- `SHOW CREATE USER`
- `SHOW CREATE FUNCTION`
- `SHOW CREATE TRIGGER`
- `SHOW CREATE EVENT`

It does not add stored function, trigger, event, account, password, privilege,
or scheduler catalogs. MyLite exposes an embedded-root `SHOW CREATE USER`
result and MySQL-shaped missing-object diagnostics for the unsupported
routine, trigger, and event catalogs.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing embedded account/grant baseline:
  `docs/specs/parser-corpus-show-grants-surfaces/specs.md`
- Existing limited routine bridge:
  `docs/specs/baseline-stored-procedure-select-call/specs.md`
- MySQL 8.4 Reference Manual, `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- MySQL 8.4 Reference Manual, `SHOW CREATE USER`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-user.html
- MySQL 8.4 Reference Manual, `SHOW CREATE FUNCTION` / `PROCEDURE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-procedure.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TRIGGER`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-trigger.html
- MySQL 8.4 Reference Manual, `SHOW CREATE EVENT`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-event.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SHOW CREATE USER CURRENT_USER`, `CURRENT_USER()`, `root`, and `root@'%'`
  return a one-column result whose column label is
  `CREATE USER for root@%`.
- The observed embedded-root value is:

  ```text
  CREATE USER `root`@`%` IDENTIFIED WITH 'caching_sha2_password' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
  ```

- Unknown users fail with error `1396`, SQLSTATE `HY000`, and text
  `Operation SHOW CREATE USER failed for '<user>'@'<host>'`.
- If no default schema is selected, unqualified `SHOW CREATE FUNCTION`,
  `SHOW CREATE EVENT`, and `SHOW CREATE TRIGGER` fail with error `1046`,
  SQLSTATE `3D000`.
- With a selected schema, a missing stored function fails with error `1305`,
  SQLSTATE `42000`, and `FUNCTION <name> does not exist`. A missing qualified
  schema is not reported first for this form.
- With a selected schema, a missing event fails with error `1539`, SQLSTATE
  `HY000`, and `Unknown event '<name>'`. A missing qualified schema is not
  reported first for this form.
- With a selected schema, a missing trigger fails with error `1360`, SQLSTATE
  `HY000`, and `Trigger does not exist`. A missing qualified schema fails
  first with error `1049`, SQLSTATE `42000`.
- Existing MySQL stored function, trigger, and event objects expose stable
  column labels used by this slice's expectation artifact, but MyLite does not
  create or persist those objects yet.

## Scope

Supported SQL examples:

```sql
SHOW CREATE USER CURRENT_USER
SHOW CREATE USER CURRENT_USER()
SHOW CREATE USER root
SHOW CREATE USER root@'%'
SHOW CREATE FUNCTION missing_function
SHOW CREATE FUNCTION missing_schema.missing_function
SHOW CREATE TRIGGER missing_trigger
SHOW CREATE TRIGGER missing_schema.missing_trigger
SHOW CREATE EVENT missing_event
SHOW CREATE EVENT missing_schema.missing_event
```

`SHOW CREATE USER` returns the embedded `root@%` account definition for the
same account forms accepted by `SHOW GRANTS`. Other named accounts return the
MySQL-shaped `1396 / HY000` failure. Omitted account hosts default to `%`; a
trailing bare `@` is the empty host.

`SHOW CREATE FUNCTION`, `SHOW CREATE EVENT`, and `SHOW CREATE TRIGGER` are
accepted as syntax and produce MySQL-shaped missing-object diagnostics because
MyLite has no stored function, event, or trigger catalog. Name resolution still
preserves MySQL's no-selected-database and trigger missing-schema behavior.

## Non-Goals

This feature does not implement:

- `CREATE FUNCTION` stored functions or loadable UDF registration;
- trigger creation, persistence, execution, `NEW`/`OLD` row semantics, or
  `INFORMATION_SCHEMA.TRIGGERS` object rows;
- event creation, event scheduler state, event execution, or
  `INFORMATION_SCHEMA.EVENTS` object rows;
- account creation, password hashes, authentication plugins, roles, grants,
  privilege checks, `print_identified_with_as_hex`, or password policies;
- privilege-sensitive redaction for `SHOW CREATE USER`;
- SQLite fork changes.

## MyLite Grammar Snippets

These snippets describe MyLite-owned Lemon grammar shape and do not copy MySQL
grammar:

```lemon
show_create_function_statement ::= SHOW CREATE FUNCTION table_name.
show_create_trigger_statement ::= SHOW CREATE TRIGGER table_name.
show_create_event_statement ::= SHOW CREATE EVENT table_name.
show_create_user_statement ::= SHOW CREATE USER show_create_user_target.

show_create_user_target ::= CURRENT_USER.
show_create_user_target ::= CURRENT_USER LPAREN RPAREN.
show_create_user_target ::= show_grants_account_name.
```

The runtime stores a single child under each statement: an object name for
function, trigger, and event forms, or an account/current-user target for the
user form.

## Runtime And Storage

No SQLite fork hook is needed. The implementation is a MyLite runtime
translation layer:

- parser and AST own syntax admission and source spans;
- account target handling reuses the existing `SHOW GRANTS` account parser;
- runtime emits a constant one-row embedded-root result for `SHOW CREATE USER`;
- runtime emits MySQL-shaped errors for missing function, trigger, and event
  targets;
- no catalog, file-format, SQLite schema, account, trigger, event, or routine
  storage changes occur.

Successful `SHOW CREATE USER` returns a result set with zero warnings and the
normal row-result `ROW_COUNT()` convention. Missing-object forms fail and
snapshot their diagnostics for following diagnostic statements.

## Tests

Fast C coverage:

- `packages/libmylite/tests/runtime_show_create_residuals_test.c`
- `packages/libmylite/tests/parser_show_test.c`

MySQL 8.4.9 expectation artifact:

- `packages/libmylite/tests/mysql_baseline_show_create_residuals_expectations.sh`

The tests cover accepted grammar, embedded-root `SHOW CREATE USER` output,
unsupported named accounts, no-selected-database diagnostics, missing
function/event/trigger diagnostics, and the differing qualified missing-schema
behavior for trigger versus function/event.

## Compatibility Status

This slice moves the detailed `SHOW CREATE USER`, `SHOW CREATE FUNCTION`,
`SHOW CREATE TRIGGER`, and `SHOW CREATE EVENT` rows from unsupported to
limited baseline support. The rows remain yellow because object catalogs,
object DDL/execution, account storage, and privilege-sensitive output are not
implemented.

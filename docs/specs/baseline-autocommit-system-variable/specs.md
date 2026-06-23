# Baseline Autocommit System Variable

## Status

This feature specifies MyLite's baseline `@@autocommit` slice.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, `SET` assignment handling, diagnostics lifecycle, explicit
transaction support, savepoint support, and statement-level write atomicity.
MySQL exposes `autocommit` as mutable global and session state tied to
transaction boundaries. MyLite implements the session-local baseline that real
application setup code depends on while keeping global mutation and protocol
status flags outside the embedded baseline.

This is not full autocommit or transaction compatibility. It implements
session/local/unscoped `SET autocommit` assignment, scalar/`SHOW VARIABLES`
readback, current DML commit/rollback effects, `START TRANSACTION` and DDL
implicit-commit interaction, and savepoints in an autocommit-disabled
transaction. It does not implement mutable global autocommit, protocol status
flags, server session-state tracking packets, MVCC snapshot parity, lock
semantics, or privilege/persisted variable behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, transaction control statements:
  https://dev.mysql.com/doc/refman/8.4/en/commit.html
- MySQL 8.4 Reference Manual, InnoDB autocommit behavior:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-autocommit-commit-rollback.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_autocommit_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@autocommit`, `@@global.autocommit`,
  `@@session.autocommit`, `@@local.autocommit`, and `@@AUTOCOMMIT`
  return `1` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION autocommit=0`, unscoped, `session`, and `local` reads return
  `0`, while `global` still returns `1`; resetting the session value to `1`
  restores the default.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.autocommit``, are syntax
  errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- `SET autocommit=0` starts reporting session/local/unscoped value `0` while
  `@@global.autocommit` remains `1`.
- With session autocommit disabled, supported DML remains pending until
  `COMMIT`, is discarded by `ROLLBACK`, and is committed when
  `SET autocommit=1` re-enables autocommit.
- `START TRANSACTION` while autocommit-disabled work is pending commits the
  previous transaction before opening the requested transaction.
- User-visible `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`, and `COMMIT` operate
  inside an autocommit-disabled transaction.
- Integer and boolean-token assignment values `0`, `1`, `TRUE`, `FALSE`, `ON`,
  `OFF`, and `DEFAULT` follow the observed MySQL boolean behavior for this
  variable; string values `'ON'` and `'OFF'` are accepted while string values
  such as `'1'`, `'0'`, `'TRUE'`, and `'FALSE'` are rejected.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as `SELECT @@autocommit + 1`.
  Those forms remain outside this MyLite slice.

The official MySQL system-variable documentation classifies `autocommit` as a
dynamic boolean variable with global and session scope and default value `ON`.
The transaction-control documentation states that connections begin with
autocommit enabled by default.

## Scope

The implementation must add:

- runtime recognition of `autocommit` inside the existing scalar `SELECT`
  subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- session-local readback for no scope, `session`, and `local`, with fixed
  global readback `1`;
- `SHOW VARIABLES` readback as `ON` / `OFF` for session scope and fixed `ON`
  for global scope;
- `SET autocommit` for no scope, `SESSION`, `LOCAL`, direct `@@autocommit`,
  `@@session.autocommit`, and `@@local.autocommit`;
- current DML commit, rollback, `SET autocommit=1`, `START TRANSACTION`, DDL
  implicit commit, savepoint, and close-time rollback interaction for the
  supported statement subset;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@autocommit
SELECT @@autocommit FROM DUAL
SELECT @@session.autocommit, @@local.autocommit
SELECT @@global.autocommit
SELECT @@session.`autocommit`, @@`autocommit`
SELECT @@autocommit, @@warning_count, ROW_COUNT()
SET autocommit = 0
SET SESSION autocommit = ON
SET LOCAL `autocommit` = DEFAULT
SET @@autocommit = @saved_autocommit
SET autocommit = 0; INSERT INTO t VALUES (1); ROLLBACK
SET autocommit = 0; SAVEPOINT s; INSERT INTO t VALUES (2); ROLLBACK TO s
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, mutable global
  autocommit state, privileges, or session-state tracking packets;
- variables other than `autocommit`;
- MVCC snapshot parity, lock release behavior, concurrency isolation, XA, or
  protocol transaction status flags;
- full always-open transaction semantics for read-only statements while
  autocommit is disabled;
- Performance Schema variable tables;
- protocol OK-packet autocommit status flags;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  functions over variables, parameters, prepared statements, or arbitrary
  SQLite pass-through;
- catalog mutations, storage mutations, SQLite metadata reads, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result ownership, row-count state,
  diagnostics snapshot replacement, and failure cleanup.
- Statement context continues to reset live diagnostics at statement start and
  preserve the previous diagnostics snapshot until nondiagnostic successful
  completion replaces it.
- Lexer/parser/AST own syntax admission and source spans for
  `SYSTEM_VARIABLE` expressions. No new grammar is needed beyond the existing
  `expression ::= SYSTEM_VARIABLE` rule.
- Runtime execution owns system-variable path parsing, scope validation,
  session/global value selection, assignment validation, and diagnostics for
  unsupported names.
- Session state owns the handle-local autocommit flag. It is internal and not
  part of the public ABI.
- Statement transaction helpers own the transition from autocommit-disabled
  idle state to an active SQLite-backed MyLite transaction for supported
  writes and savepoints.
- Result builder owns scalar result column labels and one-row text values.
- Catalog, storage, VFS, and SQLite physical row storage are not involved.
  This feature must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@autocommit
@@session.autocommit
@@local.autocommit
@@global.autocommit
```

The existing scalar `SELECT` limits continue to apply:

```lemon
select_statement ::= SELECT select_item_list from_dual_opt.
select_item ::= expression.
from_dual_opt ::= .
from_dual_opt ::= FROM DUAL.
```

System variables are admitted only when every selected expression is in the
existing scalar expression set. Clauses such as `WHERE`, `ORDER BY`, `LIMIT`,
table-backed `FROM`, aliases, and general expressions remain outside this
slice.

## Variable Resolution

Runtime parses the raw token as a `@@` system variable:

- it accepts no scope, `session`, `local`, or `global`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects malformed paths and unsupported variables with MySQL error
  `1193`, SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

Unscoped, `session`, and `local` references read the handle-local session
value. `global` references read MyLite's fixed embedded global default `1`.

## Runtime Semantics

The supported variable returns:

| Variable path | Value |
| --- | --- |
| `@@autocommit`, `@@session.autocommit`, `@@local.autocommit` | current session value, default `1` |
| `@@global.autocommit` | fixed global default `1` |

The session value is independent per handle, defaults to enabled for new and
reopened handles, and is not stored in the `.mylite` file. Selected schema does
not affect the value.

When session autocommit is enabled, supported writes continue to use MyLite's
statement-level transaction wrapper and are committed at successful statement
completion. When session autocommit is disabled, the first supported write or
savepoint starts a user transaction if none is active; supported writes then
use an internal statement savepoint and remain pending until explicit
completion. `COMMIT` persists pending changes, `ROLLBACK` discards them,
`SET autocommit=1` commits any active transaction before reporting `1`, and
`mylite_close()` rolls back an active transaction. `START TRANSACTION` and
supported implicit-commit statements commit an active autocommit-disabled
transaction before continuing.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads of `@@autocommit` do not emit warnings. This slice does not
emit warnings for successful assignments. Invalid values use MySQL-compatible
system-variable value diagnostics where the existing MyLite value parser
supports the observed form.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, and version
  variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema, close/reopen, and independent handles do not change the
  default value;
- session-local assignment readback and fixed global readback;
- rollback, commit, `SET autocommit=1`, `START TRANSACTION`, and savepoint
  transaction side effects while autocommit is disabled;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation;
- existing parser/runtime/system-variable tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records wider MySQL behavior that this
slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/runtime-session-sql-modes.md`;
- `docs/compatibility/sql-transactions.md`.

Do not overclaim mutable global variables, persisted variables, protocol status
flags, session-state tracking, MVCC snapshots, lock semantics, privilege
semantics, or full transaction compatibility.

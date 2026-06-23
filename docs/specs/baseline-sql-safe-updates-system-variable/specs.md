# Baseline SQL Safe Updates System Variable

## Status

This feature specifies the embedded baseline for `@@sql_safe_updates`: scalar
and expression readback, handle-local session assignment, `SHOW VARIABLES`
reflection, and safe-update checks for supported single-table `UPDATE` and
`DELETE` statements.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and MyLite's current descriptor
DML paths. MySQL exposes `sql_safe_updates` as dynamic global and session state
that can reject selected `UPDATE` and `DELETE` statements when enabled. MyLite
keeps the global value fixed at the embedded default `OFF`, stores session
overrides on the handle, and applies a descriptor-driven safe-update guard to
the currently supported single-table DML subset.

This is not the mysql client `--safe-updates` mode. MyLite does not change
`sql_select_limit` or `max_join_size`, persist state, mutate a shared server
global, or emulate complete optimizer key-use decisions.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline delete lifecycle:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, mysql client safe-updates tips:
  https://dev.mysql.com/doc/refman/8.4/en/mysql-tips.html
- MySQL 8.4 Reference Manual, range optimization safe-update note:
  https://dev.mysql.com/doc/refman/8.4/en/range-optimization.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_safe_updates_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@sql_safe_updates`, `@@global.sql_safe_updates`,
  `@@session.sql_safe_updates`, `@@local.sql_safe_updates`, and
  `@@SQL_SAFE_UPDATES` return `0` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION sql_safe_updates=1`, unscoped, `session`, and `local` reads
  return `1`, while `global` still returns `0`; resetting the session value to
  `0` restores the default.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_safe_updates``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as `SELECT @@sql_safe_updates + 1`.
- `SHOW VARIABLES LIKE 'sql_safe_updates'` reflects the current session value
  as `ON` or `OFF`; `SHOW GLOBAL VARIABLES` continues to expose the global
  value.
- With `sql_safe_updates=1`, MySQL allows supported `UPDATE` and `DELETE`
  statements that use a key in the `WHERE` clause or include `LIMIT`.
- Safe-updates rejection uses error `1175`, SQLSTATE `HY000`, with a message
  about safe update mode and a missing key-column `WHERE`.
- For composite secondary indexes, predicates on a non-leading key part such
  as `WHERE b = 1` for `KEY(a,b)` do not satisfy safe updates in the observed
  runtime.
- Invisible secondary indexes do not satisfy safe updates in the observed
  runtime.
- For `OR`, every branch must be key-constrained in the observed runtime;
  `key OR nonkey` is rejected, while `key OR key` and `key AND nonkey` are
  accepted.

The official MySQL system-variable documentation classifies
`sql_safe_updates` as a dynamic boolean variable with global and session scope
and default value `OFF`. For `UPDATE` and `DELETE`, the optimizer raises an
error under safe updates when execution would fall back to a full table scan.
The mysql client can initialize this and related variables with its
`--safe-updates` option. MyLite implements the handle-local variable and a
conservative descriptor-predicate equivalent for the supported single-table
DML subset.

## Scope

The implementation must add:

- runtime recognition of `sql_safe_updates` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed global value `0`/`OFF`;
- handle-local session value for no-scope, `session`, and `local` reads;
- Boolean `SET`, `DEFAULT`, and user-variable assignment forms already
  admitted by the generic session-variable assignment path;
- `SHOW VARIABLES` session readback and fixed `SHOW GLOBAL VARIABLES`
  readback;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- safe-update rejection for supported single-table `UPDATE` and `DELETE`
  statements when no `LIMIT` is present and the supported predicate tree does
  not use a descriptor key;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_safe_updates
SELECT @@sql_safe_updates FROM DUAL
SELECT @@session.sql_safe_updates, @@local.sql_safe_updates
SELECT @@global.sql_safe_updates
SELECT @@session.`sql_safe_updates`, @@`sql_safe_updates`
SELECT @@sql_safe_updates, @@warning_count, ROW_COUNT()
SET SESSION sql_safe_updates = ON
SHOW VARIABLES LIKE 'sql_safe_updates'
UPDATE t SET c = 1 WHERE id = 1
DELETE FROM t LIMIT 1
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, mysql client
  `--safe-updates` initialization, or mutable shared global
  `sql_safe_updates` state;
- variables other than `sql_safe_updates`;
- `sql_select_limit`, `max_join_size`, or any safe-updates side effects;
- exact optimizer access-path checks, cost estimation, EXPLAIN integration, or
  all possible MySQL key-use cases;
- joined or multi-table `UPDATE`/`DELETE` safe-update enforcement;
- Performance Schema variable tables;
- arbitrary SQLite pass-through;
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
  handle-local session override selection, fixed global value selection, and
  diagnostics for unsupported names.
- The generic session-variable assignment path owns `SET` validation and
  handle-local state storage.
- Descriptor-driven `DELETE` and `UPDATE` planning owns the safe-update guard.
  The guard checks the current session value, existing `LIMIT`, and the
  planned predicate tree against catalog index descriptors.
- The catalog remains authoritative for descriptors. This variable slice does
  not create key descriptors or affect table lifecycle behavior.
- Result builder owns scalar result column labels and one-row text values.
- Storage, VFS, and SQLite physical row storage are not involved. This feature
  must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@sql_safe_updates
@@session.sql_safe_updates
@@local.sql_safe_updates
@@global.sql_safe_updates
```

The existing scalar `SELECT` and session-variable assignment grammar applies:

```lemon
select_statement ::= SELECT select_item_list from_dual_opt.
select_item ::= expression.
from_dual_opt ::= .
from_dual_opt ::= FROM DUAL.
set_statement ::= SET system_variable_assignment_list.
```

System variables are admitted when selected expressions are in the existing
scalar expression set. The DML guard does not add new `UPDATE` or `DELETE`
grammar; it uses the already planned single-table statement subset.

## Variable Resolution

Runtime parses the raw token as a `@@` system variable:

- it accepts no scope, `session`, `local`, or `global`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects malformed paths and unsupported variables with MySQL error `1193`,
  SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, unscoped, `session`, and `local` reads use the handle-local
session override when one exists. Global reads always return the fixed embedded
default.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `@@global.sql_safe_updates` | `0` / `OFF` |
| `@@session.sql_safe_updates` | handle-local `0` / `1` |
| `@@local.sql_safe_updates` | handle-local `0` / `1` |
| `@@sql_safe_updates` | handle-local `0` / `1` |

The session value is independent of selected schema, table DDL, close/reopen,
and other handles. It is not persisted. The global value is fixed disabled.
When the session value is disabled, descriptor-driven `UPDATE` and `DELETE`
behavior is unchanged.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

When the session value is enabled, supported single-table `UPDATE` and
`DELETE` planning allows the statement when:

- the statement has `LIMIT`; or
- the planned predicate tree is key-constrained by a descriptor key.

The key predicate check uses visible leading index parts. A primary-key or
visible secondary-index first part is considered a key column. Invisible
secondary indexes and non-leading composite secondary parts do not satisfy the
guard by themselves. For Boolean predicate structure, `AND` is key-constrained
when either side is key-constrained, while `OR` is key-constrained only when
both sides are key-constrained. Unsupported predicate node kinds do not satisfy
safe updates.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select
  clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- safe-update DML rejection: error `1175`, SQLSTATE `HY000`;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads and assignments of `@@sql_safe_updates` do not emit warnings.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- default `0` value, fixed global value, and handle-local session values;
- `SET SESSION`, `SET LOCAL`, `SET @@...`, `DEFAULT`, and user-variable
  assignment forms;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES`;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, foreign-key-check, unique-check, updatable-view, and version
  variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- expression reads such as `@@sql_safe_updates + 1`;
- selected schema, close/reopen, table DDL, and independent handles do not
  persist or share the session value;
- representative existing `UPDATE` and `DELETE` statements still execute under
  the disabled value;
- safe-update rejection for UPDATE/DELETE without `LIMIT` or key predicates;
- allowed UPDATE/DELETE with primary-key, leading secondary-key, `AND`,
  key-only `OR`, and `LIMIT` forms;
- rejected UPDATE/DELETE with non-leading composite key predicates, invisible
  secondary-key predicates, and key-or-nonkey `OR` forms;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation after
  variable reads;
- existing parser/runtime/system-variable and table lifecycle tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms, session mutability, expression reads, `SHOW
VARIABLES`, and representative safe-update DML decisions.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-table-dml.md`.

Do not overclaim mutable shared globals, persisted variables, mysql client
safe-updates initialization, `sql_select_limit`, `max_join_size`, joined DML,
or exact optimizer access-path equivalence.

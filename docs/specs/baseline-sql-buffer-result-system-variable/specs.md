# Baseline SQL Buffer Result System Variable

## Status

This feature specifies the baseline session-variable slice for
`@@sql_buffer_result`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, generic Boolean session-variable assignment, diagnostics
lifecycle, `SHOW VARIABLES`, and descriptor-driven table read paths. MySQL
exposes `sql_buffer_result` as mutable global and session state that forces
`SELECT` results into temporary tables. MyLite implements the embedded
compatibility baseline: session `SET`/readback and `SHOW VARIABLES` state,
fixed global default `OFF`, and no changed query execution.

This is not physical result-buffering support. It does not implement
server-global mutation, persisted startup state, temporary result table
materialization, lock-release behavior, optimizer effects, or protocol changes.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline select/order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_buffer_result_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@sql_buffer_result`, `@@global.sql_buffer_result`,
  `@@session.sql_buffer_result`, `@@local.sql_buffer_result`, and
  `@@SQL_BUFFER_RESULT` return `0` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION sql_buffer_result=1`, unscoped, `session`, and `local` reads
  return `1`, while `global` still returns `0`; assigning `DEFAULT` restores
  the default session value.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_buffer_result``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@sql_buffer_result + 1`. Those forms remain outside this MyLite
  slice.

The official MySQL system-variable documentation classifies
`sql_buffer_result` as a dynamic Boolean variable with global and session
scope and default value `OFF`. When enabled, MySQL materializes `SELECT`
results into temporary tables so table locks can be released earlier. MyLite
records the session value but leaves descriptor-backed `SELECT` execution
unchanged in the embedded baseline.

## Scope

The implementation must add:

- runtime recognition of `sql_buffer_result` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- session `SET sql_buffer_result = 0|1|ON|OFF|TRUE|FALSE|DEFAULT` handling
  through the existing Boolean system-variable override path;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` values for the session/global
  baseline;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed global value `0` and session-local values that default to `0`;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_buffer_result
SELECT @@sql_buffer_result FROM DUAL
SELECT @@session.sql_buffer_result, @@local.sql_buffer_result
SELECT @@global.sql_buffer_result
SELECT @@session.`sql_buffer_result`, @@`sql_buffer_result`
SELECT @@sql_buffer_result, @@warning_count, ROW_COUNT()
SET SESSION sql_buffer_result = 1
SHOW VARIABLES LIKE 'sql_buffer_result'
SHOW GLOBAL VARIABLES LIKE 'sql_buffer_result'
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, or mutable
  server-global `sql_buffer_result` state;
- temporary result table materialization, server cursor buffering,
  lock-release behavior, optimizer effects, memory/disk spill policy, or
  protocol changes;
- variables other than `sql_buffer_result`;
- changed descriptor-backed `SELECT` result production;
- Performance Schema variable tables;
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
  session/global value selection, and diagnostics for unsupported names.
- Descriptor-driven `SELECT` execution remains unchanged because the embedded
  baseline does not materialize server-side temporary result tables.
- The catalog remains authoritative for descriptors. This variable slice does
  not affect table lifecycle or query planning.
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
@@sql_buffer_result
@@session.sql_buffer_result
@@local.sql_buffer_result
@@global.sql_buffer_result
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
- it rejects malformed paths and unsupported variables with MySQL error `1193`,
  SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, global scope returns the fixed embedded default `0`. Unscoped,
`session`, and `local` scope read the current session override when one exists,
and otherwise return `0`.

## Runtime Semantics

The supported variable returns:

| Scope | Value |
| --- | --- |
| Global | fixed `0` |
| Session/local/unscoped default | `0` |
| Session/local/unscoped after `SET SESSION sql_buffer_result = 1` | `1` |

The session value is independent per handle and resets on close/reopen.
Existing descriptor-driven `SELECT` behavior must not change: enabling the
variable records compatibility state but does not force temporary result-table
materialization or alter row production.

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

- syntax errors, including quoted scopes and unsupported scalar-select
  clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads and session assignments of `@@sql_buffer_result` do not emit
warnings. Unsupported global assignment uses the existing embedded
global-variable diagnostic path, and physical result-buffering effects remain
out of scope.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- default `0` values, mutable session values, fixed global `0`, and
  close/reopen reset behavior;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, foreign-key-check, unique-check, updatable-view,
  safe-updates, select-limit, notes, warning-reporting, and version variables;
- session and global `SHOW VARIABLES` rows after session assignment;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema, table DDL, DML, and independent handles do not leak session
  state;
- representative descriptor-backed `SELECT` statements still return normal
  result rows while the session value is enabled;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation after
  variable reads;
- existing parser/runtime/system-variable and table lifecycle tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms, session mutability, and `SHOW VARIABLES` readback.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-query-expressions.md`.

Do not overclaim server-global mutation, persisted state, Performance Schema
variable tables, physical result buffering, lock-release behavior, optimizer
effects, or changed descriptor-backed `SELECT` behavior.

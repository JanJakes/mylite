# Baseline SQL Warnings System Variable

## Status

This feature specifies the baseline `sql_warnings` system-variable slice.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and MyLite's current strict
integer/`NULL` DML behavior. MySQL exposes `sql_warnings` as mutable global and
session state that controls whether single-row `INSERT` statements produce an
information string when warnings occur. MyLite implements session-local
readback and SET/SHOW compatibility for application probes, while preserving
the global default and leaving protocol information strings outside the
embedded API surface for now.

This is not full insert information reporting support. It does not implement
global mutation, persisted variables, privilege checks, server startup options,
or protocol metadata changes.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline insert set lifecycle:
  `docs/specs/baseline-insert-set-lifecycle/specs.md`
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
`packages/libmylite/tests/mysql_baseline_sql_warnings_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@sql_warnings`, `@@global.sql_warnings`,
  `@@session.sql_warnings`, `@@local.sql_warnings`, and `@@SQL_WARNINGS`
  return `0` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION sql_warnings=1`, unscoped, `session`, and `local` reads return
  `1`, while `global` still returns `0`; resetting the session value to `0`
  restores the default.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_warnings``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- `SHOW VARIABLES LIKE 'sql_warnings'` reflects the session value as `ON` or
  `OFF`; `SHOW GLOBAL VARIABLES LIKE 'sql_warnings'` reflects the global
  default `OFF`.
- MySQL accepts expression forms such as `SELECT @@sql_warnings + 1` and
  `HEX(@@sql_warnings)`.

The official MySQL system-variable documentation classifies `sql_warnings` as
a dynamic boolean variable with global and session scope and default value
`OFF`. When enabled, MySQL emits an information string for single-row `INSERT`
statements that generate warnings. MyLite records the session variable value
but does not expose protocol information strings.

## Scope

The implementation must add:

- runtime recognition of `sql_warnings` inside scalar `SELECT` and supported
  expression contexts;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- global default value `0`;
- session-local `SET SESSION`, `SET LOCAL`, unscoped `SET @@sql_warnings`,
  `DEFAULT`, and user-variable assignment readback;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` values;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_warnings
SELECT @@sql_warnings FROM DUAL
SELECT @@session.sql_warnings, @@local.sql_warnings
SELECT @@global.sql_warnings
SELECT @@session.`sql_warnings`, @@`sql_warnings`
SET SESSION sql_warnings = 1
SET LOCAL sql_warnings = FALSE
SET @@sql_warnings = DEFAULT
SHOW VARIABLES LIKE 'sql_warnings'
SELECT @@sql_warnings, @@warning_count, ROW_COUNT()
SELECT HEX(@@sql_warnings), @@sql_warnings + 1
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, privilege checks, or
  mutable global `sql_warnings` state;
- variables other than `sql_warnings`;
- warning-producing DML conversions, `INSERT IGNORE`, strict-mode warning
  demotion, or protocol information strings;
- changed `INSERT ... VALUES` or `INSERT ... SET` behavior;
- Performance Schema variable tables;
- arbitrary table-backed variable evaluation, parameters, prepared statements,
  or SQLite pass-through beyond the documented scalar/expression support;
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
  session readback, SET/SHOW formatting, and diagnostics for unsupported names.
- Descriptor-driven `INSERT` execution remains unchanged because MyLite does
  not expose MySQL protocol information strings yet.
- The catalog remains authoritative for descriptors. This variable slice does
  not affect table lifecycle or DML behavior.
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
@@sql_warnings
@@session.sql_warnings
@@local.sql_warnings
@@global.sql_warnings
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
table-backed `FROM`, and aliases remain outside this slice.

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

For this slice, `global` always returns the default value. Unscoped, `session`,
and `local` reads use the session override when one has been set, otherwise
they return the default.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `@@global.sql_warnings` | `0` |
| `@@sql_warnings`, `@@session.sql_warnings`, `@@local.sql_warnings` | session override or `0` |

Session overrides are in-memory handle state. They are independent per handle,
do not persist across close/reopen, and do not mutate catalog or storage state.
Existing descriptor-driven `INSERT` behavior must not change.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

Supported `SET` statements store only the session readback value. They do not
change warning conversion, insert execution, or protocol information strings.
`SHOW VARIABLES LIKE 'sql_warnings'` renders the session value as `ON` or
`OFF`; `SHOW GLOBAL VARIABLES LIKE 'sql_warnings'` renders `OFF`.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select
  clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unsupported SET values and unsupported global mutation;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads of `@@sql_warnings` do not emit warnings. Supported
assignments follow the generic Boolean system-variable diagnostics for invalid
values. Insert information-string side effects are out of scope.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- default `0` value for all supported scopes;
- session mutation with `SET SESSION`, `SET LOCAL`, `SET @@...=DEFAULT`, and
  user-variable assignment;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` values;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, foreign-key-check, unique-check, updatable-view,
  safe-updates, and version variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- numeric expression use with `HEX(@@sql_warnings)` and
  `@@sql_warnings + 1`;
- selected schema, close/reopen, table DDL, and independent handles do not
  change the global default or persist session overrides;
- representative existing `INSERT ... VALUES` and `INSERT ... SET` statements
  still execute under the fixed disabled value;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation after
  variable reads;
- existing parser/runtime/system-variable and table lifecycle tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records protocol-side behavior that
this slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/error-warning-result-semantics.md`.

Do not overclaim global mutation, persisted state, warning-producing DML
conversions, `INSERT IGNORE`, strict-mode warning demotion, protocol
information strings, or changed `INSERT` behavior.

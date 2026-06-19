# Baseline SQL Notes System Variable

## Status

This feature specifies the baseline `sql_notes` system-variable slice.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and MyLite's current limited
diagnostics area. MySQL exposes `sql_notes` as mutable global and session
state that controls whether note-level diagnostics increment `warning_count`
and are stored. MyLite implements handle-local session `sql_notes` state for
the note producers it currently has, plus fixed default global readback.

This is not complete server-global variable or diagnostics-stack support. It
does not implement persisted variables, startup configuration, `SET_VAR`
hints, shared global mutation, complete note-producing statement coverage, or
mysqldump reload behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline diagnostics count variables:
  `docs/specs/baseline-diagnostics-count-variables/specs.md`
- Baseline SHOW warnings diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
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
`packages/libmylite/tests/mysql_baseline_sql_notes_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@sql_notes`, `@@global.sql_notes`, `@@session.sql_notes`,
  `@@local.sql_notes`, and `@@SQL_NOTES` return `1` in the tested default
  runtime.
- The variable has global and session scope. After
  `SET SESSION sql_notes=0`, unscoped, `session`, and `local` reads return
  `0`, while `global` still returns `1`; assigning `DEFAULT` restores the
  default session value.
- With the session value set to `1`, `DROP TABLE IF EXISTS missing_table`
  records a note and increments `warning_count`. With the session value set to
  `0`, the same statement records no note and leaves `warning_count` at `0`.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_notes``, are syntax
  errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as `SELECT @@sql_notes + 1`.
  Those forms remain outside this MyLite slice.

The official MySQL system-variable documentation classifies `sql_notes` as a
dynamic boolean variable with global and session scope and default value `ON`.
When enabled, MySQL records note-level diagnostics and includes them in
`warning_count`. MyLite mirrors that behavior for current note producers with
handle-local session state; global readback remains the fixed default `1`.

## Scope

The implementation must support:

- runtime recognition of `sql_notes` inside the existing scalar `SELECT`
  subset;
- `SHOW VARIABLES` readback through the existing system-variable descriptor
  surface;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `1` for global scope and handle-local mutable values for no
  scope, `session`, and `local`;
- `SET sql_notes = ...`, `SET SESSION sql_notes = ...`, and
  `SET LOCAL sql_notes = ...` through the existing boolean system-variable
  assignment path, including `DEFAULT`;
- suppression of current note-level diagnostics when the session value is `0`;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_notes
SELECT @@sql_notes FROM DUAL
SELECT @@session.sql_notes, @@local.sql_notes
SELECT @@global.sql_notes
SELECT @@session.`sql_notes`, @@`sql_notes`
SELECT @@sql_notes, @@warning_count, ROW_COUNT()
SHOW SESSION VARIABLES LIKE 'sql_notes'
SET sql_notes = 0
SET SESSION sql_notes = DEFAULT
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, mysqldump reload
  behavior, or shared mutable global `sql_notes` state;
- variables other than `sql_notes`;
- complete note-level diagnostics coverage beyond currently implemented note
  producers;
- diagnostics stacks, `GET DIAGNOSTICS`, or Performance Schema variable tables;
- arbitrary changes to `SHOW WARNINGS`, `SHOW ERRORS`, `@@warning_count`, or
  `@@error_count` beyond note suppression and the separately specified
  `max_error_count` interaction;
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
- Runtime execution owns system-variable path parsing, scope validation, global
  default readback, handle-local session value selection, assignment, and
  diagnostics for unsupported names.
- Diagnostics owns note suppression and retained warning-row capping. Statement
  context applies the current session `sql_notes` and `max_error_count` values
  to the live diagnostics area at statement start.
- The catalog remains authoritative for descriptors. This variable slice does
  not affect table lifecycle, DDL, or DML behavior.
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
@@sql_notes
@@session.sql_notes
@@local.sql_notes
@@global.sql_notes
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

For this slice:

- global scope returns the fixed default value `1`;
- no scope, `session`, and `local` return the handle-local session value;
- handle-local session assignments do not mutate other open handles;
- `DEFAULT` restores the session value to `1`.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `@@global.sql_notes` | `1` |
| `@@sql_notes`, `@@session.sql_notes`, `@@local.sql_notes` | current handle-local session value |

The session value is independent of selected schema, table DDL, DML, and other
handles. It is not persisted across close/reopen. Disabling the variable skips
note-level diagnostics before they are counted or retained.

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

Supported reads and assignments of `@@sql_notes` do not emit warnings. When
the session value is `0`, current note producers such as missing-table
`DROP TABLE IF EXISTS` record no note, leave `@@warning_count` at `0`, and
leave `SHOW WARNINGS` empty for that statement.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- fixed `1` value for global scope;
- mutable handle-local values for no scope, `session`, and `local`;
- `SET` boolean and `DEFAULT` assignment forms;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, foreign-key-check, unique-check, updatable-view,
  safe-updates, select-limit, warning-reporting, and version variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema and table DDL do not change the session value, independent
  handles keep separate session values, close/reopen restores the default
  session value, and global readback remains fixed;
- note suppression while disabled and restored note production after `DEFAULT`;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation after
  variable reads;
- existing parser/runtime/system-variable and table lifecycle tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and note-suppression behavior.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/error-warning-result-semantics.md`.

Do not overclaim shared global mutation, persistence, startup options,
Performance Schema variable tables, diagnostics stacks, complete note
producer coverage, or arbitrary-expression support.

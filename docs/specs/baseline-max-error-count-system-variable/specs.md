# Baseline Max Error Count System Variable

## Status

This feature specifies a narrow `max_error_count` diagnostics slice. MySQL uses
the variable to cap how many warning, note, and error rows are retained for
`SHOW WARNINGS` / `SHOW ERRORS`, while count readbacks still expose the total
warning/note count for warning-producing statements. MyLite implements
handle-local session state, fixed default global readback, retained-row capping,
and `sql_notes` interaction for note producers.

It is not a complete server-global system-variable implementation. MyLite does
not share mutable global variable state across handles and does not implement
Performance Schema variable tables, persisted variables, privileges, or
diagnostics stacks.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline SQL notes system variable:
  `docs/specs/baseline-sql-notes-system-variable/specs.md`
- Baseline diagnostics count variables:
  `docs/specs/baseline-diagnostics-count-variables/specs.md`
- Baseline SHOW warnings diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- Baseline SHOW errors diagnostics:
  `docs/specs/baseline-show-errors-diagnostics/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_max_error_count_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `@@max_error_count`, `@@session.max_error_count`, and
  `@@global.max_error_count` default to `1024`.
- `SHOW VARIABLES LIKE 'max_error_count'` and `SHOW GLOBAL VARIABLES LIKE
  'max_error_count'` expose value `1024` in the default test runtime.
- `SET SESSION max_error_count = N` changes only the session value. `SET
  max_error_count = DEFAULT` restores `1024`.
- The accepted integer range is `0..65535`. Negative values clamp to `0` with
  warning `1292`; values above `65535` clamp to `65535` with warning `1292`.
- `TRUE` and `FALSE` assign `1` and `0`. Quoted strings and `NULL` fail with
  error `1232`, SQLSTATE `42000`.
- The cap used for warnings emitted by a `SET max_error_count` statement is the
  value active at statement start.
- With `max_error_count = 1`, a `DROP TABLE IF EXISTS a, b, c` statement has
  `@@warning_count = 3` and `SHOW COUNT(*) WARNINGS = 3`, but `SHOW WARNINGS`
  displays only the first note row.
- With `max_error_count = 0`, the same warning-producing statement has count
  `3` and displays no retained warning rows.
- With `max_error_count = 0`, a parse error remains an immediate statement
  error for the client, but no previous diagnostics row is retained for
  `SHOW WARNINGS`, `SHOW ERRORS`, `@@warning_count`, or `@@error_count`.
- `sql_notes = 0` suppresses note-level diagnostics before the
  `max_error_count` cap is applied.

## Scope

The implementation must add:

- scalar `SELECT` support for `@@max_error_count`,
  `@@session.max_error_count`, `@@local.max_error_count`, and
  `@@global.max_error_count`;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` rows;
- handle-local session `SET` support for unscoped, `SESSION`, `LOCAL`, direct
  `@@max_error_count`, `@@session.max_error_count`, and
  `@@local.max_error_count` targets;
- `DEFAULT`, unsigned integer, unary-signed integer, `TRUE`, `FALSE`, and
  integer user-variable assignment values;
- range clamping to `0..65535` with warning `1292`;
- retained diagnostics row capping for `SHOW WARNINGS` / `SHOW ERRORS`;
- total warning-count preservation for `SHOW COUNT(*) WARNINGS`,
  `@@warning_count`, and public result warning counts;
- `sql_notes = 0` suppression for note-level diagnostics;
- MySQL-shaped rejection for unsupported values and deterministic rejection of
  mutable global assignment beyond fixed default no-ops;
- fast C tests and a MySQL 8.4.9 expectation artifact.

## Non-Goals

This feature must not implement:

- shared mutable global `max_error_count` state across handles;
- persisted system variables, `SET PERSIST`, `SET_VAR` hints, privilege checks,
  startup options, or Performance Schema variable tables;
- diagnostics stacks, `GET DIAGNOSTICS`, stored-program condition handlers,
  `SIGNAL`, or `RESIGNAL`;
- new warning producers beyond already-supported MyLite diagnostics;
- broader `sql_notes` behavior outside note suppression for existing note
  producers;
- protocol information strings or complete wire-protocol warning packet parity;
- SQLite fork changes.

## Runtime Semantics

`max_error_count` is stored in `mylite_session_state` and initialized to
`1024`. Global scalar and `SHOW GLOBAL VARIABLES` readbacks remain fixed at
`1024`.

At statement start, MyLite copies the session value into the live diagnostics
area. Warning, note, and error records increment total diagnostics counters.
Rows are retained only while the retained-row count is below the statement
cap. Note appends are skipped entirely when the current session `sql_notes`
value is disabled.

After statement completion, the previous diagnostics snapshot preserves both
the retained rows and the total counters. Diagnostic statements (`SHOW
WARNINGS`, `SHOW ERRORS`, and their count forms) continue to preserve the
previous snapshot. Ordinary successful statements replace it. Failed statements
replace it with the error condition unless the statement-start cap was `0`, in
which case the immediate statement error remains visible through the public
handle diagnostics but no previous diagnostics row is retained.

## Supported SQL Grammar

No new grammar is required. This slice uses the existing system-variable and
`SET` assignment grammar:

```lemon
expression ::= SYSTEM_VARIABLE.
set_assignment ::= set_system_variable_target EQ set_value.
```

Supported examples:

```sql
SELECT @@max_error_count
SELECT @@session.max_error_count, @@local.max_error_count
SELECT @@global.max_error_count
SHOW VARIABLES LIKE 'max_error_count'
SET max_error_count = 1
SET SESSION max_error_count = DEFAULT
SET @@max_error_count = TRUE
SET @n = 2
SET max_error_count = @n
```

## Compatibility Documentation

`COMPATIBILITY.md`, `docs/compatibility/runtime-system-variables.md`, and
`docs/compatibility/error-warning-result-semantics.md` must describe the new
greenable session behavior, the fixed global readback limitation, the
interaction with `sql_notes`, and remaining global/persisted diagnostics gaps.

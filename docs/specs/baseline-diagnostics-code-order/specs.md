# Baseline Diagnostics Codes And Order

## Status

This slice documents and verifies the limited MySQL-compatible diagnostics
catalog behavior that MyLite already relies on for baseline statements:
SQLSTATE values, numeric error codes, numeric warning/note codes, and warning
record order/counting for the currently admitted diagnostics-producing
surfaces.

It extends the baseline `SHOW WARNINGS`, `SHOW ERRORS`, and diagnostics count
variable slices. It is not a complete MySQL condition catalog.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `SHOW WARNINGS` diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- Baseline `SHOW ERRORS` diagnostics:
  `docs/specs/baseline-show-errors-diagnostics/specs.md`
- Baseline diagnostics count variables:
  `docs/specs/baseline-diagnostics-count-variables/specs.md`
- MySQL 8.4 Reference Manual, diagnostics area:
  https://dev.mysql.com/doc/refman/8.4/en/diagnostics-area.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- MySQL 8.4 Reference Manual, `SHOW ERRORS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-errors.html
- MySQL 8.4 Reference Manual, server error message reference:
  https://dev.mysql.com/doc/refman/8.4/en/server-error-reference.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- A parse failure reports error `1064`, SQLSTATE `42000`.
- `@@global.warning_count` reports error `1238`, SQLSTATE `HY000`, because
  `warning_count` is session-only.
- An unknown system variable reports error `1193`, SQLSTATE `HY000`.
- Strict overlength `VARCHAR` insert failure reports error `1406`, SQLSTATE
  `22001`.
- `NULL` inserted into a `NOT NULL` column reports error `1048`, SQLSTATE
  `23000`.
- A warning-only statement such as `SHOW PROCESSLIST` reports a stored
  warning row with level `Warning`, code `1287`, and SQLSTATE `HY000`.
- Inserting overlength trailing-space values into two `VARCHAR(3)` columns
  succeeds with two note rows. `SHOW WARNINGS` returns them in column order:
  first column `v`, then column `n`, both level `Note`, code `1265`.
- `SHOW COUNT(*) WARNINGS` reports the stored warning/note row count and does
  not clear the diagnostics snapshot.
- A scalar `SELECT @@warning_count, @@error_count, ROW_COUNT()` is a
  nondiagnostic statement: it returns the previous counts and then clears the
  snapshot for following diagnostics statements.

The MySQL expectation script for this slice records the exact commands used to
verify those observations.

## Scope

MyLite supports a limited diagnostics catalog for conditions it can currently
emit. The supported surface includes:

- public `mylite_errcode()`, `mylite_sqlstate()`, and `mylite_errmsg()` for
  statement errors;
- `mylite_result_warning_count()` for successful statements that store warning
  or note records;
- `SHOW WARNINGS`, `SHOW ERRORS`, `SHOW COUNT(*) WARNINGS`, and
  `SHOW COUNT(*) ERRORS` over the previous diagnostics snapshot;
- scalar `@@warning_count` and `@@error_count` over the previous diagnostics
  snapshot;
- deterministic FIFO record order for stored warning, note, and error rows;
- MySQL-compatible numeric codes and SQLSTATEs for the conditions covered by
  the current baseline tests.

This slice adds explicit coverage for representative error classes, warning
classes, note ordering, and count lifecycle rather than changing the public
ABI.

## Non-Goals

This slice does not implement:

- the full MySQL server error catalog;
- complete SQLSTATE coverage for features outside MyLite's implemented
  baseline;
- `GET DIAGNOSTICS`;
- diagnostics stacks, stored-program diagnostics areas, `max_error_count`, or
  counted-but-not-stored conditions;
- mutable `sql_notes` or `sql_warnings` behavior;
- privilege-sensitive diagnostics text;
- wire protocol error packets or OK-packet status flags;
- SQLite fork changes.

## Ownership Boundary

- `mylite_diagnostics` owns the in-memory condition record and ordered warning
  record vector for each connection handle.
- Runtime execution owns mapping supported MySQL compatibility conditions to
  numeric code, SQLSTATE, level, and message text.
- `mylite_result` owns the successful statement warning count visible through
  the public result API.
- `SHOW WARNINGS`, `SHOW ERRORS`, count statements, and diagnostics count
  variables read the previous diagnostics snapshot.
- Catalog, storage, VFS, and SQLite physical row storage are not involved.

## Supported Diagnostic Semantics

The current baseline guarantees:

- statement-start clears live diagnostics before execution;
- failed statements set the public error condition and snapshot it for
  following diagnostic statements;
- successful statements snapshot their warning/note list and clear previous
  diagnostics unless the statement itself is diagnostic;
- diagnostic statements preserve the previous snapshot;
- warning/note rows are appended in the order MyLite detects them and are
  displayed in that order;
- `SHOW ERRORS` filters the previous snapshot to error-level rows;
- `SHOW WARNINGS` displays warning, note, and error rows from the previous
  snapshot;
- `@@warning_count` counts the previous error condition plus stored warning and
  note records, matching the current MySQL baseline for admitted statements;
- `@@error_count` counts the previous error condition.

## SQLite Integration

This feature is implemented wholly in MyLite runtime structures and result
builders. It uses public SQLite APIs only indirectly through statements that
already execute in MyLite. No SQLite fork hook is needed.

## Tests

Fast C coverage:

- `packages/libmylite/tests/runtime_diagnostics_code_order_test.c`

MySQL 8.4.9 expectation artifact:

- `packages/libmylite/tests/mysql_baseline_diagnostics_code_order_expectations.sh`

The C test verifies public error code/SQLSTATE exposure, warning result counts,
ordered `SHOW WARNINGS` note rows, diagnostic count preservation, and clearing
after scalar diagnostics count reads.

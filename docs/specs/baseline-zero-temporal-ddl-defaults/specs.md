# Baseline Zero Temporal DDL Defaults

## Status

This slice tightens the existing zero-temporal SQL-mode implementation for DDL
default changes. The earlier `baseline-zero-temporal-sql-modes` slice already
covers direct `CREATE TABLE`, row DML, predicates, descriptor copies, and
persistence for canonical `DATE`, `DATETIME`, and `TIMESTAMP` zero values. This
slice focuses on the remaining DDL paths that create or mutate defaults after a
table exists:

- `ALTER TABLE ... ADD COLUMN ... DEFAULT ...`;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT ...`;
- supported `ALTER TABLE ... MODIFY [COLUMN] ... DEFAULT ...` for the current
  `DATETIME` / `TIMESTAMP` replacement subset;
- supported `ALTER TABLE ... CHANGE [COLUMN] ... DEFAULT ...` for the current
  `DATETIME` / `TIMESTAMP` replacement subset.

The goal is the WordPress-relevant behavior where legacy code can set
`sql_mode=''` or other non-strict modes and still use zero temporal defaults,
while strict `NO_ZERO_DATE` / `NO_ZERO_IN_DATE` combinations reject the same
defaults with MySQL-compatible diagnostics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline SQL-mode state:
  `docs/specs/baseline-sql-mode-session-state/specs.md`
- Baseline zero-temporal SQL modes:
  `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
- Baseline temporal type specs:
  `docs/specs/baseline-date-type/specs.md`,
  `docs/specs/baseline-datetime-type/specs.md`,
  `docs/specs/baseline-timestamp-type/specs.md`, and
  `docs/specs/baseline-time-type/specs.md`
- Baseline ALTER specs:
  `docs/specs/baseline-alter-table-add-column-positioning/specs.md`,
  `docs/specs/baseline-alter-table-modify-column/specs.md`,
  `docs/specs/baseline-alter-change-modify-temporal-positioning/specs.md`,
  and `docs/specs/baseline-alter-column-set-default/specs.md`
- MySQL 8.4 Reference Manual, server SQL modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_zero_temporal_ddl_defaults_expectations.sh`
records the runtime probes for this feature. Probes were run against MySQL
8.4.9 with `time_zone = '+00:00'`.

Observed behavior shaping this slice:

- With `sql_mode=''`, `ALTER TABLE ... ADD COLUMN` and `ALTER COLUMN ... SET
  DEFAULT` accept full-zero `DATE`, `DATETIME`, and nullable `TIMESTAMP`
  defaults without warnings. Current MyLite-supported `MODIFY COLUMN` and
  `CHANGE COLUMN` temporal replacements admit the same behavior for `DATETIME`
  and nullable `TIMESTAMP`.
- With non-strict `NO_ZERO_DATE`, those ALTER forms accept full-zero temporal
  defaults and preserve them in metadata, but the statement warning count
  reflects all full-zero temporal defaults present on the table after the
  statement. Adding or setting a second full-zero default reports two warnings,
  and adding or setting a third reports three warnings.
- With `STRICT_TRANS_TABLES,NO_ZERO_DATE`, the same full-zero defaults fail with
  `1067 / 42000` and message text containing
  `Invalid default value for '<column>'`.
- With non-strict `NO_ZERO_IN_DATE`, partial-zero `DATE` and `DATETIME`
  defaults on those ALTER forms are accepted, converted to the full-zero value
  for the descriptor default, and report one warning for the changed column
  when no other partial-zero defaults remain on the table.
- With `STRICT_TRANS_TABLES,NO_ZERO_IN_DATE`, partial-zero `DATE` and
  `DATETIME` defaults fail with `1067 / 42000` and message text containing the
  target column name.
- `TIME DEFAULT '00:00:00'` and `YEAR DEFAULT '0000'` remain accepted without
  warnings under `NO_ZERO_IN_DATE`; this SQL mode is a date-part rule and must
  not be generalized to those types.
- Successful supported statements report `ROW_COUNT() = 0`. Omitted-column
  inserts after accepted default changes materialize the descriptor default and
  report normal DML affected-row and warning counts.

## Scope

The implementation must add or verify:

- mode-aware validation and warning generation for supported zero temporal
  defaults in `ALTER TABLE ... ADD COLUMN`, `ALTER COLUMN ... SET DEFAULT`,
  `MODIFY COLUMN`, and `CHANGE COLUMN`;
- strict rejection of full-zero `DATE`, `DATETIME`, and `TIMESTAMP` defaults
  when `NO_ZERO_DATE` is active with either strict mode, within the ALTER paths
  that currently admit those target types;
- non-strict `NO_ZERO_DATE` warning counts that account for every full-zero
  `DATE`, `DATETIME`, and `TIMESTAMP` descriptor default present on the table
  after the successful ALTER;
- strict rejection of partial-zero `DATE` and `DATETIME` defaults when
  `NO_ZERO_IN_DATE` is active with either strict mode, within the ALTER paths
  that currently admit those target types;
- non-strict `NO_ZERO_IN_DATE` adjustment of the changed partial-zero `DATE` or
  `DATETIME` default to the type's full-zero default and warning reporting;
- unchanged `TIME` and `YEAR` zero default behavior under zero-date SQL modes;
- descriptor metadata and omitted-column DML readback for accepted defaults;
- no mutation of unrelated catalog generation, descriptor versions, storage
  file preamble, or SQLite schema text beyond the ALTER operation already
  required by the specific DDL path.

## Non-Goals

This feature must not implement:

- relaxed temporal string formats, `T` separators, offsets, fractional seconds,
  numeric temporal defaults, two-digit year default parsing, casts, or arbitrary
  expression defaults;
- zero-date behavior for functions, generated columns, triggers, privileges,
  table partitions, temporary ALTER support, or unsupported ALTER actions;
- SQL-mode behavior for `TIME` or `YEAR` beyond preserving their existing
  independent zero/default rules;
- full MySQL online-DDL algorithm/lock behavior;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own call
  validation, result ownership, diagnostics exposure, and cleanup.
- Session state owns the current SQL-mode bitset. DDL default conversion reads
  `STRICT_TRANS_TABLES`, `STRICT_ALL_TABLES`, `NO_ZERO_DATE`, and
  `NO_ZERO_IN_DATE` from the handle-local statement context.
- Lexer/parser/AST own the existing ALTER grammar and literal/default AST
  nodes. This slice does not add syntax.
- Analyzer/runtime conversion owns descriptor default classification, SQL-mode
  decisions, MySQL-compatible warning/error generation, and descriptor default
  storage.
- Catalog descriptors remain authoritative for logical type, nullability,
  default metadata, column names, and physical table identity. SQLite schema
  text remains non-authoritative for MySQL metadata.
- SQLite physical storage is used only through existing ALTER/rebuild paths.
  Generated SQL must continue to quote identifiers and bind values through
  prepared statements where values are materialized.
- Storage/VFS owns `.mylite` preamble invariants. This feature does not change
  the file format.

## Grammar

No new grammar is added. Existing ALTER grammar continues to provide these
literal positions:

```sql
alter_add_column_action:
    ADD optional_column column_definition

alter_set_default_action:
    ALTER optional_column identifier SET DEFAULT default_value

alter_modify_column_action:
    MODIFY optional_column identifier column_type column_attributes

alter_change_column_action:
    CHANGE optional_column old_identifier new_identifier column_type column_attributes

default_value:
    string_literal
  | NULL
  | admitted_current_temporal_function
  | admitted_parenthesized_constant_expression
```

The semantic change is that string literal defaults targeting `DATE`,
`DATETIME`, or `TIMESTAMP` descriptors use the current SQL-mode temporal
default rules in every supported ALTER path.

## Mode Rules

This slice reuses the classification from
`baseline-zero-temporal-sql-modes/specs.md`:

- `full_zero`: `0000-00-00` or `0000-00-00 00:00:00`;
- `partial_zero`: `DATE` / `DATETIME` values inside the current MyLite
  `1000..9999` year envelope where the month or day is zero but the value is
  not the full-zero value;
- `normal`: valid canonical values inside the descriptor's existing supported
  range;
- `invalid`: unsupported or invalid canonical-shaped values outside those
  categories.

For `DATE` and `DATETIME` defaults:

- `normal`: store unchanged.
- `full_zero`: store unchanged if `NO_ZERO_DATE` is absent. If
  `NO_ZERO_DATE` is present without strict mode, store unchanged and append
  warning `1264 / 01000`. If strict mode is present, fail with
  `1067 / 42000`.
- `partial_zero`: store unchanged if `NO_ZERO_IN_DATE` is absent. If
  `NO_ZERO_IN_DATE` is present without strict mode, store the type's full-zero
  default and append warning `1264 / 01000`. If strict mode is present, fail
  with `1067 / 42000`.
- `invalid`: keep the existing temporal default behavior from the underlying
  type slice. This feature does not widen invalid-date default support.

For `TIMESTAMP` defaults:

- `normal`: store unchanged inside the existing fixed-UTC timestamp range.
- `full_zero`: follows `NO_ZERO_DATE` like `DATETIME`.
- partial-zero timestamp defaults remain invalid outside this slice.

For `TIME` and `YEAR` defaults:

- keep the existing type-specific default conversion. `NO_ZERO_DATE` and
  `NO_ZERO_IN_DATE` must not add warnings or errors for `TIME '00:00:00'` or
  `YEAR '0000'`.

## ALTER Warning Accounting

MySQL 8.4.9 revalidates zero temporal defaults for the table during successful
metadata-changing ALTER statements. For the supported subset, MyLite should
match the visible warning count:

- after a successful non-strict `NO_ZERO_DATE` ALTER, report one warning for
  each descriptor default on the visible table that is full-zero `DATE`,
  `DATETIME`, or `TIMESTAMP`;
- after a successful non-strict `NO_ZERO_IN_DATE` ALTER that adjusts the changed
  default, report the warnings produced by the changed default conversion;
- ordinary successful supported ALTER statements outside those zero-date
  warning cases continue to report `warning_count == 0`.

The stored warning messages should use the existing MyLite temporal warning
diagnostics for the current zero-temporal slice. Tests assert warning counts and
representative messages rather than overfitting every duplicate warning row.

## Result And Metadata Behavior

- Successful supported ALTER statements return through the existing non-row
  result convention, with `affected_rows == 0` for catalog-only default changes
  and the existing row-count behavior for ALTER paths that rebuild or validate
  rows.
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS`
  render the stored descriptor default after conversion.
- Later omitted-column `INSERT` / `REPLACE` paths materialize the stored
  descriptor default without revalidating it against the session SQL mode.
- Accepted defaults persist across close/reopen and descriptor cloning through
  the existing metadata paths.

## Diagnostics

Supported strict invalid-default failures use MySQL-compatible default
diagnostics:

- code `1067`;
- SQLSTATE `42000`;
- message text containing `Invalid default value for '<column>'`, where
  `CHANGE COLUMN` uses the new column name.

Unsupported syntax and unsupported default expressions keep their existing
deterministic MyLite diagnostics. Allocation, catalog, storage, and SQLite
physical failures keep the existing runtime failure policy.

## Physical SQLite Handling

No new physical SQLite primitive is required.

- `ALTER COLUMN SET DEFAULT` is catalog-only.
- `ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN` continue to use existing
  MyLite descriptor-driven physical append or rebuild paths.
- Default values stored in descriptors are converted before physical default
  materialization. Generated physical SQL must not embed raw user literal text
  as authoritative MySQL metadata.
- `.mylite` preamble and shifted SQLite payload invariants are unchanged.

## Tests

Add MySQL-runtime-verified expectations and fast C tests covering:

- `ALTER TABLE ... ADD COLUMN` with full-zero `DATE`, `DATETIME`, and nullable
  `TIMESTAMP` defaults under empty mode, non-strict `NO_ZERO_DATE`, and strict
  `NO_ZERO_DATE`;
- `ALTER COLUMN ... SET DEFAULT` with the same full-zero temporal defaults and
  warning-count accumulation under non-strict `NO_ZERO_DATE`;
- partial-zero `DATE` and `DATETIME` defaults under empty mode, non-strict
  `NO_ZERO_IN_DATE`, and strict `NO_ZERO_IN_DATE`;
- supported `MODIFY COLUMN` and `CHANGE COLUMN` `DATETIME` / `TIMESTAMP`
  default handling, including strict error messages using the replacement
  column name for `CHANGE COLUMN`;
- `TIME DEFAULT '00:00:00'` and `YEAR DEFAULT '0000'` as zero-date SQL-mode
  non-regression guards;
- metadata rendering through `SHOW COLUMNS`;
- omitted-column insert materialization after accepted ALTER defaults;
- persistence and `.mylite` preamble preservation;
- existing zero-temporal, temporal-type, ALTER, parser, runtime, and full check
  workflows.

# Baseline Zero Temporal SQL Modes

## Status

This feature extends the existing `SET sql_mode` session-state slice into the
currently supported `DATE`, `DATETIME`, and `TIMESTAMP` conversion paths. The
goal is not full temporal coercion. The goal is the common MySQL baseline where
legacy applications can disable strict zero-date checks with `SET sql_mode=''`
and then create, insert, update, compare, and copy zero temporal values through
MyLite-owned descriptors.

The slice is deliberately limited to already-admitted canonical text forms:
`YYYY-MM-DD` for `DATE` and `YYYY-MM-DD HH:MM:SS` for `DATETIME` /
`TIMESTAMP`. It does not broaden relaxed delimiters, numeric temporal values,
fractional seconds, time zones, temporal functions, casts, generated defaults,
or arbitrary expression evaluation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline SQL mode session state:
  `docs/specs/baseline-sql-mode-session-state/specs.md`
- Baseline `DATE`, `DATETIME`, and `TIMESTAMP` type specs:
  `docs/specs/baseline-date-type/specs.md`,
  `docs/specs/baseline-datetime-type/specs.md`, and
  `docs/specs/baseline-timestamp-type/specs.md`
- Existing DML, default, predicate, ordering, index, and introspection specs
  under `docs/specs/`
- MySQL 8.4 Reference Manual, server SQL modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, date and time data types:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-types.html
- MySQL 8.4 Reference Manual, `DATE`, `DATETIME`, and `TIMESTAMP`:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` defaults:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_zero_temporal_sql_modes_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- MySQL 8.4.9's default session mode includes `STRICT_TRANS_TABLES`,
  `NO_ZERO_IN_DATE`, and `NO_ZERO_DATE`; direct zero `DATE`, `DATETIME`, and
  `TIMESTAMP` inputs fail under that default.
- `SET sql_mode=''` admits full zero values: `0000-00-00` and
  `0000-00-00 00:00:00` store and read back without warnings.
- `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES` alone still admits full zero
  temporal values without warnings. Strictness becomes an error for full zero
  values only when `NO_ZERO_DATE` is also enabled.
- `NO_ZERO_DATE` without strict mode stores full zero values and defaults, but
  appends warning `1264` / `01000` for each affected temporal column. With
  strict mode, full zero values fail with error `1292` / `22007`, and full
  zero defaults fail with `1067` / `42000`.
- For `DATE` and `DATETIME`, partial-zero month/day values such as
  `2024-00-01` and `2024-01-00 00:00:00` are stored unchanged when
  `NO_ZERO_IN_DATE` is not enabled, even in strict mode. `NO_ZERO_IN_DATE`
  without strict mode appends warning `1264` and stores the type's full zero
  value. With strict mode, partial-zero values fail with `1292`, and
  partial-zero defaults fail with `1067`.
- `TIMESTAMP` does not store partial-zero or otherwise invalid date parts. With
  strict mode disabled, those canonical-shaped invalid timestamp inputs append
  warning `1264` and store `0000-00-00 00:00:00`. With strict mode enabled,
  they fail with `1292`.
- With strict mode disabled and `ALLOW_INVALID_DATES` absent, canonical-shaped
  invalid `DATE` / `DATETIME` inputs such as `2024-02-31` append warning `1264`
  and store the type's full zero value. With `ALLOW_INVALID_DATES`, `DATE` and
  `DATETIME` store invalid day-of-month values when the month is `1..12` and
  the day is `1..31`; this does not make invalid `TIMESTAMP` values valid.
- Predicate literals use MySQL's temporal literal validation for the current SQL
  mode. The supported MyLite slice accepts zero and partial-zero predicate
  literals only when the current SQL mode admits them for the descriptor type.

## Scope

The implementation must add:

- mode-aware conversion for canonical-shaped `DATE`, `DATETIME`, and
  `TIMESTAMP` string literals in supported `CREATE TABLE`, `ALTER TABLE ...
  ADD COLUMN`, `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT`, `INSERT`,
  `INSERT ... SET`, `REPLACE`, `REPLACE ... SET`, and single-table `UPDATE`
  paths;
- direct storage of full zero `DATE`, `DATETIME`, and `TIMESTAMP` values when
  `NO_ZERO_DATE` is absent;
- nonstrict `NO_ZERO_DATE` warning behavior for full zero values and defaults,
  with strict `NO_ZERO_DATE` errors preserved;
- direct storage of partial-zero `DATE` and `DATETIME` values when
  `NO_ZERO_IN_DATE` is absent;
- nonstrict `NO_ZERO_IN_DATE` adjustment of partial-zero `DATE` and `DATETIME`
  values/defaults to the full zero value with warning `1264`;
- strict `NO_ZERO_IN_DATE` rejection of partial-zero `DATE` and `DATETIME`
  values/defaults;
- strict-disabled invalid canonical-shaped `DATE`, `DATETIME`, and `TIMESTAMP`
  adjustment to full zero with warning `1264`;
- `ALLOW_INVALID_DATES` storage for canonical-shaped `DATE` and `DATETIME`
  values whose month is `1..12` and day is `1..31`, while keeping
  `TIMESTAMP` invalid-date conversion unchanged;
- descriptor-backed predicate conversion for the supported zero and partial-zero
  `DATE` / `DATETIME` forms, plus full-zero `TIMESTAMP` forms, in the existing
  comparison, `BETWEEN`, and `IN` predicate surfaces;
- preservation of zero and partial-zero temporal readback, ordering, copying,
  secondary-index metadata/enforcement, `SHOW`, and `INFORMATION_SCHEMA`
  behavior already provided by descriptor-backed text storage;
- MySQL 8.4.9 expectation coverage for the supported mode matrix and deferred
  wider temporal conversions.

## Non-Goals

This feature must not implement:

- relaxed temporal string formats, `T` separators, date/time literal introducers,
  numeric temporal values, two-digit years, fractional seconds, or time-zone
  conversion;
- `TIME` SQL-mode changes. `TIME` has no date part and already owns its
  separate zero/out-of-range behavior;
- automatic `CURRENT_TIMESTAMP`, `NOW()`, `ON UPDATE`, generated defaults,
  generated columns, or temporal arithmetic;
- `DATE_FORMAT()` behavior for zero or partial-zero temporal inputs;
- MySQL's full predicate diagnostic surface for every unsupported temporal
  expression. Unsupported shapes continue to return deterministic MyLite
  diagnostics unless this spec explicitly admits them;
- noncanonical invalid strings such as `2024/02/31`, nondelimited dates,
  standard `DATE '...'` / `TIMESTAMP '...'` literals, or numeric values;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own call
  validation, result ownership, diagnostics exposure, and cleanup.
- Session state owns the current `sql_mode` bit set and canonical string
  readback. This feature consumes the existing session bits for
  `STRICT_TRANS_TABLES`, `STRICT_ALL_TABLES`, `NO_ZERO_DATE`,
  `NO_ZERO_IN_DATE`, and `ALLOW_INVALID_DATES`.
- Lexer/parser/AST own syntax admission for existing string literals and do not
  interpret SQL modes.
- Analyzer/planner/runtime conversion owns temporal classification, mode
  decisions, warning/error generation, and binding converted values into
  SQLite prepared statements.
- Catalog descriptors remain authoritative for logical type, nullability,
  defaults, visibility, indexes, and physical table identity. SQLite schema text
  remains non-authoritative.
- SQLite stores admitted temporal values as descriptor-owned canonical or
  zero/partial-zero `TEXT`. Generated physical SQL still quotes identifiers and
  binds values through prepared statements.
- Storage/VFS owns `.mylite` preamble invariants. This feature writes only
  through existing SQLite payload paths and must not change the preamble.

## Supported Grammar

No new grammar is added. Existing grammar continues to provide the admitted
string literal positions:

```sql
column_default_value:
    string_literal

insert_value:
    string_literal

replace_value:
    string_literal

update_value:
    string_literal

predicate_value:
    string_literal
```

The change is semantic: the same literal text is interpreted through the
current session SQL mode when the target descriptor is `DATE`, `DATETIME`, or
`TIMESTAMP`.

## Temporal Classification

MyLite classifies canonical-shaped date text before applying mode rules:

- `normal`: a nonzero valid date in the currently supported range;
- `full_zero`: `0000-00-00`;
- `partial_zero`: month or day is `00`, excluding `full_zero`;
- `allow_invalid_candidate`: `year >= 1000`, month `1..12`, and day `1..31`,
  but not a real calendar date;
- `invalid`: all other canonical-shaped values.

For `DATETIME`, the date part is classified the same way and the time part must
remain `00..23:00..59:00..59`. Invalid time parts are `invalid`.

For `TIMESTAMP`, only `normal` values inside the existing fixed-UTC range and
`full_zero` are directly storable. Partial-zero, zero-year non-full-zero,
invalid calendar dates, and out-of-range values are invalid timestamp inputs
that may only become the full zero timestamp when strict mode is disabled.

## Mode Rules

Let `strict` mean either `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES`.

For `DATE` and `DATETIME` values/defaults:

- `normal`: store unchanged.
- `full_zero`: if `NO_ZERO_DATE` is absent, store unchanged. If
  `NO_ZERO_DATE` is present and `strict` is false, append warning `1264` and
  store the full zero value. If both are present, reject with `1292` for row
  values or `1067` for defaults.
- `partial_zero`: if `NO_ZERO_IN_DATE` is absent, store unchanged. If
  `NO_ZERO_IN_DATE` is present and `strict` is false, append warning `1264` and
  store the full zero value. If both are present, reject with `1292` for row
  values or `1067` for defaults.
- `allow_invalid_candidate`: if `ALLOW_INVALID_DATES` is present, store
  unchanged. Otherwise, append warning `1264` and store the full zero value
  when `strict` is false; reject with `1292` or `1067` when `strict` is true.
- `invalid`: append warning `1264` and store the full zero value when `strict`
  is false; reject with `1292` or `1067` when `strict` is true.

For `TIMESTAMP` values/defaults:

- in-range `normal`: store unchanged;
- `full_zero`: use the `NO_ZERO_DATE` rule above;
- every other canonical-shaped non-storable timestamp: append warning `1264`
  and store `0000-00-00 00:00:00` when `strict` is false; reject with `1292` or
  `1067` when `strict` is true.

Unsupported noncanonical literals remain unsupported regardless of SQL mode.

## Defaults

Supported zero and partial-zero defaults are descriptor metadata, not SQLite
schema text. `SHOW COLUMNS`, `SHOW CREATE TABLE`, and
`INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` render the stored descriptor default.
When nonstrict `NO_ZERO_DATE` or `NO_ZERO_IN_DATE` adjusts a default, the
descriptor stores the adjusted full zero value and the statement warning count
reflects the adjustment.

## Predicates

Predicate conversion follows the same current-mode admission rules for already
supported comparison, `BETWEEN`, and `IN` predicate literal positions. For
`DATE` and `DATETIME`, admitted partial-zero literals compare as their stored
text values. For `TIMESTAMP`, only in-range normal values and the currently
admitted full zero value compare directly; partial-zero timestamp predicate
literals remain invalid.

This feature does not broaden predicate expressions, function predicates,
column-to-column predicates, or relaxed temporal literals.

## Physical SQLite Handling

No SQLite fork changes are required. Physical tables remain generated MyLite
rowid tables. Temporal values continue to bind as `TEXT` parameters into
descriptor-built SQLite statements. Because the admitted zero/partial-zero
values are fixed-width text in MySQL display order, existing equality,
ordering, copying, and secondary-index paths remain descriptor-driven and do
not need generated SQLite expressions.

## Diagnostics

Supported successful in-range and directly admitted zero/partial-zero
conversions report `warning_count == 0`. Nonstrict mode adjustments append one
warning per adjusted temporal column:

- `1264 / 01000`: `Out of range value for column 'col' at row N`.

Strict row-value failures use the existing MyLite temporal diagnostics:

- `1292 / 22007`: `Incorrect date value: 'value' for column 'col' at row N`;
- `1292 / 22007`: `Incorrect datetime value: 'value' for column 'col' at row N`.

Strict default failures use:

- `1067 / 42000`: `Invalid default value for 'col'`.

Allocation failures, physical SQLite failures, public API misuse, unsupported
literal shapes, unknown columns, unsupported object kinds, and catalog failures
continue to use existing project diagnostics.

## Tests

Fast C tests must cover:

- full zero `DATE`, `DATETIME`, and `TIMESTAMP` row values under
  `sql_mode=''`, `STRICT_TRANS_TABLES`, `NO_ZERO_DATE`, and
  `STRICT_TRANS_TABLES,NO_ZERO_DATE`;
- partial-zero `DATE` and `DATETIME` row values under `sql_mode=''`,
  `STRICT_TRANS_TABLES`, `NO_ZERO_IN_DATE`, and
  `STRICT_TRANS_TABLES,NO_ZERO_IN_DATE`;
- strict-disabled invalid canonical-shaped values adjusting to full zero with
  warnings;
- `ALLOW_INVALID_DATES` for invalid-but-range-shaped `DATE` and `DATETIME`,
  while `TIMESTAMP` still adjusts or errors according to strictness;
- zero and partial-zero defaults, metadata readback, and invalid-default
  diagnostics;
- supported `INSERT`, `REPLACE`, and `UPDATE` assignments;
- predicate equality, `BETWEEN`, and `IN` over admitted zero and partial-zero
  values;
- copy/reopen behavior and `.mylite` preamble preservation;
- independent handles with different session modes;
- adjacent SQL-mode, temporal type, DML, predicate, ordering, default, and
  introspection tests.

The MySQL expectation script must verify the same user-visible mode matrix
against MySQL 8.4.9 before implementation expectations are treated as stable.

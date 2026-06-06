# Baseline Relaxed Temporal DML Literals

## Summary

This feature widens the descriptor-owned temporal string literal conversion
used by `DATE`, `DATETIME`, and `TIMESTAMP` storage positions. MyLite already
stores these descriptor families as canonical text and already admits
`T` separators, numeric offsets, and single trailing `Z` / `z` suffixes in
limited predicate positions. This slice admits the same narrow family in:

- column defaults for `CREATE TABLE` and existing default-finalization paths;
- `INSERT ... VALUES` and `INSERT ... SET`;
- `REPLACE ... VALUES` and `REPLACE ... SET`;
- supported single-table `UPDATE` assignments;
- supported duplicate-key update assignment conversion paths that reuse row
  storage conversion.

The feature does not add general temporal parsing. It admits only
second-precision string literals whose decoded text has one of the exact forms
listed below.

## Compatibility Authority

Primary references:

- MySQL 8.4 Reference Manual, date and time literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, date and time types:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-types.html
- MySQL 8.4 Reference Manual, SQL modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_relaxed_temporal_dml_literals_expectations.sh`

The MySQL manual documents the `T` date/time separator, numeric time-zone
offsets for inserted `DATETIME` and `TIMESTAMP` values, and strict SQL-mode
effects for data-change and DDL conversion errors. Runtime probes against
MySQL 8.4.9 establish the details that matter for this MyLite slice.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

With `sql_mode = ''` and `time_zone = '+00:00'`:

- `DATETIME` and `TIMESTAMP` storage/default strings with a `T` separator store
  as the equivalent `YYYY-MM-DD HH:MM:SS` value and record no warning.
- `DATETIME` and `TIMESTAMP` storage/default strings with a one-digit hour in
  either `YYYY-MM-DD H:MM:SS` or `YYYY-MM-DDTH:MM:SS` form store with a padded
  two-digit hour and record no warning.
- `DATETIME` and `TIMESTAMP` storage/default strings with a valid numeric
  offset store after converting from the literal offset to the session target
  offset. With session `+00:00`, `'2024-01-02 03:04:05+02:30'` stores as
  `2024-01-02 00:34:05`.
- With session `+02:00`, `DATETIME '2024-01-02T03:04:05+00:00'` stores as
  `2024-01-02 05:04:05`.
- `TIMESTAMP` readback in MySQL still follows the session time-zone conversion
  rules. MyLite's current timestamp row storage baseline remains fixed UTC, so
  this slice normalizes explicit offsets to MyLite's existing fixed UTC target
  and does not implement full mutable timestamp storage/readback conversion.
- `DATETIME` and `TIMESTAMP` storage/default strings ending in one `Z` or `z`
  store by truncating the suffix and record `1265 / 01000`
  `Data truncated for column '<column>' at row 1`.
- `DATE` storage/default strings with a datetime-shaped time part store the
  date portion. A non-midnight time part records a note; midnight records no
  warning.
- `DATE` storage/default strings with valid numeric offsets are first shifted
  from the literal offset to the session target offset, then the date portion
  is stored. A non-midnight source time part records a note; a midnight source
  time part records no warning, even if the offset shift changes the day.
- `DATE` storage/default strings ending in one `Z` or `z` store the date
  portion with a `1265 / 01000` truncation warning when strict mode is not
  active.

With `STRICT_TRANS_TABLES` active:

- `DATETIME` and `TIMESTAMP` `T` and valid-offset storage/default strings
  remain accepted.
- single trailing `Z` / `z` storage values for `DATETIME` and `TIMESTAMP`
  produce `1292 / 22007` incorrect datetime value diagnostics.
- single trailing `Z` / `z` defaults for `DATETIME` and `TIMESTAMP` produce
  `1067 / 42000` invalid default value diagnostics.
- `DATE` datetime-shaped storage/default strings without trailing `Z` remain
  accepted with a truncation note.
- `DATE` trailing-`Z` storage values produce `1292 / 22007` incorrect date
  value diagnostics, and trailing-`Z` defaults produce `1067 / 42000`.

## Scope

Admitted decoded string forms:

```text
YYYY-MM-DD HH:MM:SS
YYYY-MM-DDTHH:MM:SS
YYYY-MM-DD H:MM:SS
YYYY-MM-DDTH:MM:SS
YYYY-MM-DD HH:MM:SS+HH:MM
YYYY-MM-DDTHH:MM:SS+HH:MM
YYYY-MM-DD HH:MM:SS-HH:MM
YYYY-MM-DDTHH:MM:SS-HH:MM
YYYY-MM-DD HH:MM:SSZ
YYYY-MM-DDTHH:MM:SSZ
YYYY-MM-DD HH:MM:SSz
YYYY-MM-DDTHH:MM:SSz
```

`DATE` targets also continue to admit canonical `YYYY-MM-DD`. `DATETIME` and
`TIMESTAMP` targets continue to admit canonical `YYYY-MM-DD HH:MM:SS`.

Numeric-offset validation is intentionally the same narrow predicate subset:

- hour and minute fields must have two digits;
- `-00:00` is rejected;
- the absolute offset must not exceed `14:00`;
- named zones and `SYSTEM` are not admitted;
- fractional seconds and extra trailing text are not admitted.

## Ownership Boundary

- Public API: no ABI changes. `mylite_execute()` and existing result APIs keep
  exposing affected rows, row sets, warning counts, and diagnostics.
- Statement context/session state: existing SQL mode and time-zone fields
  remain handle-owned. This feature reads them during descriptor conversion.
- Lexer/parser/AST: no new grammar is needed. Existing string literal nodes
  carry the SQL surface.
- Analyzer/planner/runtime: descriptor conversion owns decoding, normalization,
  zero-temporal SQL-mode handling, diagnostics, and prepared-statement values.
- Catalog: logical descriptors remain authoritative for target type, target
  name, default text, and metadata. SQLite schema text is not consulted.
- Result builder/diagnostics: existing diagnostic appenders report truncation
  notes/warnings and strict-mode errors.
- SQLite physical storage: generated MyLite user tables keep storing canonical
  text values through bound parameters. No SQLite date/time parser is used.
- Storage/VFS and `.mylite`: no file-format, preamble, VFS, or SQLite fork
  changes are needed.

## Grammar

No parser expansion is required. The existing literal/default/DML grammar
already admits string literals in descriptor-backed value positions:

```lemon
column_default ::= DEFAULT signed_literal.
insert_value ::= string_literal.
update_assignment ::= identifier EQ string_literal.
```

Semantic conversion applies only after descriptor resolution proves the target
column family is `DATE`, `DATETIME`, or `TIMESTAMP`.

## Conversion Semantics

### `DATE`

Canonical `YYYY-MM-DD` strings keep the existing conversion path.

Datetime-shaped strings are first validated as second-precision datetime text,
including valid-offset spelling when an offset is present. MyLite then stores
canonical `YYYY-MM-DD`.

For no-offset forms, the stored date is the literal date part. For
numeric-offset forms, the datetime is shifted from the literal offset to the
current session time-zone offset before the date part is extracted. This is
intentionally different from the existing `DATE` predicate-offset policy,
because MySQL's storage conversion and predicate conversion differ here.

For no-suffix and numeric-offset forms, MyLite appends a note only when the
source time part is not `00:00:00`. Without strict mode the note is
`1265 / 01000` data truncated. With strict mode active, MySQL reports note
`1292 / 22007` incorrect date value but still stores the row/default.

For single trailing `Z` / `z` forms:

- if strict mode is active and the statement is not an ignore-adjustment path,
  MyLite reports the existing incorrect date value/default diagnostic using
  the original literal text;
- otherwise MyLite appends data-truncated warning `1265 / 01000`, then stores
  the canonical date portion.

### `DATETIME`

`YYYY-MM-DDTHH:MM:SS` normalizes `T` to a space and then follows the existing
canonical `DATETIME` SQL-mode conversion.

`YYYY-MM-DD H:MM:SS` and `YYYY-MM-DDTH:MM:SS` normalize by padding the hour
with a leading `0` and normalizing `T` to a space. They then follow the
existing canonical `DATETIME` SQL-mode conversion.

Numeric-offset strings validate the offset and shift the datetime from the
literal offset to the current session time-zone offset before applying the
existing canonical `DATETIME` SQL-mode conversion.

Single trailing `Z` / `z` strings normalize by truncating the suffix and
normalizing `T` to a space. In strict mode they report an incorrect datetime
value or invalid default; otherwise they append data-truncated warning
`1265 / 01000` and store the canonical datetime.

### `TIMESTAMP`

`TIMESTAMP` uses the same string-shape rules as `DATETIME`, but explicit
numeric offsets normalize to MyLite's current fixed UTC timestamp storage
baseline. This preserves the existing documented limitation that MyLite does
not yet perform full mutable session time-zone `TIMESTAMP` storage/readback
conversion.

Single trailing `Z` / `z` diagnostics match the `DATETIME` storage/default
policy for this slice.

## Diagnostics

Supported truncation cases append:

```text
Note|1265|Data truncated for column '<column>' at row <n>
Warning|1265|Data truncated for column '<column>' at row <n>
```

The `DATE` no-suffix/offset datetime-shaped storage path uses a note. The
single trailing `Z` / `z` storage path uses a warning when it is admitted.

Strict single trailing `Z` / `z` DML values report existing MySQL-compatible
incorrect temporal value diagnostics:

```text
1292 / 22007 Incorrect date value: '<literal>' for column '<column>' at row <n>
1292 / 22007 Incorrect datetime value: '<literal>' for column '<column>' at row <n>
```

Strict single trailing `Z` / `z` defaults report:

```text
1067 / 42000 Invalid default value for '<column>'
```

Malformed offsets, unsupported relaxed delimiters, fractional seconds,
two-digit years, whitespace-relaxed forms, numeric temporal literals, temporal
literal introducers, and broader trailing garbage remain unsupported and use
the existing deterministic MyLite diagnostics for unsupported temporal value
forms.

## Physical SQLite Handling

Converted temporal constants are bound into SQLite prepared statements as text
values. Generated SQL shape, physical table names, identifier quoting, and
descriptor-owned target resolution stay unchanged. This feature does not add
row materialization, temporary tables, SQLite schema introspection, SQLite
date/time functions, or SQLite fork patches.

## Performance

Each literal is decoded and normalized once during descriptor conversion.
SQLite still executes the physical DML statement and stores the already
canonical text value through bound parameters. The only added work is constant
normalization and warning generation, so the storage path stays close to the
existing optimal pushed-down DML path.

## Non-Goals

- full MySQL relaxed delimiter parsing;
- fractional seconds;
- two-digit years;
- numeric temporal literals;
- standard `DATE '...'`, `TIME '...'`, or `TIMESTAMP '...'` literal
  introducers;
- time-zone names or `SYSTEM` offsets;
- MySQL's non-strict zero-adjustment behavior for malformed offsets;
- full mutable session time-zone `TIMESTAMP` storage/readback conversion;
- general expression coercion, casts, generated columns, or function
  arguments beyond existing supported surfaces;
- SQLite fork patches.

## Test Plan

Tests must cover:

- MySQL 8.4.9 expectation script coverage for `DATE`, `DATETIME`, and
  `TIMESTAMP` `T` separators, valid numeric offsets, trailing `Z` / `z`,
  defaults, `INSERT`, `REPLACE`, `UPDATE`, strict errors, non-strict
  warnings/notes, and normalized readback;
- MyLite C runtime coverage for the same successful DML/default paths and
  warnings through result warning counts and `SHOW WARNINGS`;
- strict-mode rejection for trailing `Z` / `z` DML values and defaults;
- persistence after close/reopen for normalized storage values;
- compatibility documentation updates that remove the previous
  predicate-only limitation without claiming broader relaxed temporal parsing;
- focused temporal runtime tests, the MySQL expectation script, build/tidy, and
  `cmake --workflow --preset check`.

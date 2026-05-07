# SQLite Fork TIMESTAMP Column Descriptors

## Status

Implemented for the first executable SQLite-fork foundation slice. Remaining
work is limited to the deferred compatibility items listed below.

## References

- MySQL 8.4 Reference Manual, The DATE, DATETIME, and TIMESTAMP Types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, Fractional Seconds in Time Values:
  https://dev.mysql.com/doc/refman/8.4/en/fractional-seconds.html
- MySQL 8.4 Reference Manual, Date and Time Literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, UNIX_TIMESTAMP():
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official MySQL
documentation, observed MySQL 8.4.9 behavior, SQLite public documentation, and
the current MyLite codebase. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## Scope

Implement the first executable `TIMESTAMP` column-storage slice:

- parse `TIMESTAMP` and `TIMESTAMP(fsp)` column declarations;
- reject fractional precision outside `0..6`;
- write and read MySQL-compatible catalog metadata for `DATA_TYPE`,
  `COLUMN_TYPE`, and `DATETIME_PRECISION`;
- expose a distinct SQLite-fork column descriptor for `TIMESTAMP`;
- validate non-NULL assignments at the SQLite-fork record-construction
  boundary for public MyLite DML and direct annotated SQLite writes;
- normalize stored values to MySQL display text at the declared fractional
  precision;
- preserve SQL `NULL` when column nullability allows it;
- support public MyLite `INSERT`, `UPDATE`, `DELETE`, `TRUNCATE`, and
  `DROP TABLE` CRUD paths for tables containing `TIMESTAMP` columns.

Out of scope for this slice:

- session `time_zone` conversion between displayed session time and UTC
  physical storage;
- named time zones and time-zone tables;
- `explicit_defaults_for_timestamp`, implicit `DEFAULT CURRENT_TIMESTAMP`, and
  implicit `ON UPDATE CURRENT_TIMESTAMP`;
- `TIME_TRUNCATE_FRACTIONAL`;
- non-strict and `INSERT IGNORE` warning demotion;
- complete zero-timestamp SQL-mode behavior;
- direct SQLite parser support for MySQL `TIMESTAMP` syntax and catalog
  descriptor hydration after reopening a database through the fork alone.

## MySQL Semantics

`TIMESTAMP` stores a date and time value with optional fractional seconds. The
declared precision `fsp` is in `0..6`; omitted precision is equivalent to `0`.
Assignments with more fractional digits are rounded to the target precision
unless `TIME_TRUNCATE_FRACTIONAL` is enabled. This slice implements the default
rounding mode only.

Observed MySQL 8.4.9 strict-mode assignment behavior for ordinary UTC/SYSTEM
zero-offset sessions:

- `1970-01-01 00:00:01` is the first accepted non-zero value;
- `1970-01-01 00:00:00` raises error `1292`, SQLSTATE `22007`;
- `2038-01-19 03:14:07.999999` is accepted by `TIMESTAMP(6)`;
- `2038-01-19 03:14:08` raises error `1292`, SQLSTATE `22007`;
- fractional rounding is applied before range validation, so
  `2038-01-19 03:14:07.5` for `TIMESTAMP` and
  `2038-01-19 03:14:07.9995` for `TIMESTAMP(3)` raise the same condition.

The official MySQL `DATE`, `DATETIME`, and `TIMESTAMP` type page currently
lists the fractional `TIMESTAMP` upper endpoint as
`2038-01-19 03:14:07.499999`, while the MySQL 8.4 `UNIX_TIMESTAMP()` page and
observed MySQL 8.4.9 runtime behavior use `2038-01-19 03:14:07.999999`.
MyLite follows the runtime-verified 8.4.9 behavior for assignment validation.

MySQL converts `TIMESTAMP` values from the session time zone to UTC for storage
and back on retrieval. MyLite's current effective temporal runtime is UTC-like,
so this slice stores the displayed timestamp text directly and documents
session time-zone conversion as the next required temporal fork point.

## SQLite Fork Design

The existing column-descriptor fork point is the correct integration point.
SQLite's public extension surface can provide scalar functions, collations, and
virtual tables, but it cannot transparently enforce MySQL `TIMESTAMP`
assignment semantics:

- assignment conversion must happen before `OP_MakeRecord` stores the row;
- SQL wrapper functions would have to be emitted by every DML lowering path and
  would not cover direct annotated SQLite writes;
- CHECK constraints cannot preserve MySQL condition code `1292` and SQLSTATE
  `22007` through the diagnostics bridge;
- public SQLite type affinity cannot express post-rounding range checks,
  fractional precision, or future session time-zone conversion.

Add a distinct `TIMESTAMP` descriptor kind to `MyliteColumnType` and the public
fork descriptor API. `OP_MyliteTypeCheck` uses the existing temporal parser and
formatter shared by `DATE` and `DATETIME`, then applies the narrower
`TIMESTAMP` range after fractional rounding. The physical storage for this
slice remains SQLite `TEXT` in canonical MySQL display format. This keeps the
first slice compact and stable while leaving a future binary/UTC storage
decision to the time-zone feature.

## MyLite SQL Integration

The MyLite parser already accepts temporal declarations equivalent to:

```lemon
column_type ::= TIMESTAMP opt_fsp.
opt_fsp ::= .
opt_fsp ::= LP integer_literal RP.
```

The descriptor layer writes:

- `DATA_TYPE = 'timestamp'`;
- `COLUMN_TYPE = 'timestamp'` or `timestamp(fsp)`;
- `DATETIME_PRECISION = fsp`.

Physical SQLite tables use `TEXT` affinity for `TIMESTAMP` columns in this
slice. The catalog-fed public write-table loader maps `DATA_TYPE='timestamp'`
and `DATETIME_PRECISION` back to the fork descriptor for `INSERT`, `UPDATE`,
`REPLACE`, and duplicate-key update paths.

## Fixture

The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-timestamp-column-descriptors/mysql-timestamp-column-crud.sql`
captures declaration metadata, insert/update/delete/truncate/drop behavior,
auto-increment reset, compact numeric datetime input, fractional rounding, and
range errors after rounding.

The current MyLite runtime test mirrors the WordPress-like CRUD portion through
the public SQL API and separately covers direct annotated SQLite writes through
`mylite_sqlite_fork_set_column_type()`.

## Compatibility Status

MyLite now has partial executable `TIMESTAMP` support: parser/catalog
integration, fork assignment validation, public CRUD coverage, strict
range-after-rounding diagnostics, and direct annotated SQLite-fork tests are
implemented. Session time-zone conversion, implicit timestamp defaults and
`ON UPDATE`, SQL-mode warning demotion and zero handling, exact diagnostic
messages, binary/UTC physical storage, and direct SQLite parser MySQL syntax
remain deferred.

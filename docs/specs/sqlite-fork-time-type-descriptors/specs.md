# SQLite Fork TIME Type Descriptors

## Status

Implemented as the next SQLite-fork assignment descriptor slice.

## References

- MySQL 8.4 Reference Manual, The TIME Type:
  https://dev.mysql.com/doc/refman/8.4/en/time.html
- MySQL 8.4 Reference Manual, Fractional Seconds in Time Values:
  https://dev.mysql.com/doc/refman/8.4/en/fractional-seconds.html
- MySQL-runtime fixture:
  `docs/specs/sqlite-fork-time-type-descriptors/mysql-time-coercion.sql`

## Scope

This slice extends MyLite's private SQLite fork with a native `TIME(fsp)`
assignment descriptor. It covers the row-write semantics needed by public
MyLite `INSERT`, `UPDATE`, `REPLACE`, and `ON DUPLICATE KEY UPDATE` paths:

- elapsed-time values with hours beyond a clock-day hour range
- negative values
- day-plus-time strings such as `1 02:03:04`
- colon-abbreviated values such as `34:56` and `:12`
- compact numeric and string values such as `123456`, `1234`, and `12`
- fractional-second rounding to declared precision 0 through 6
- strict-mode invalid-value diagnostics for malformed and out-of-range values
- canonical text storage suitable for SQLite comparison and round-tripping

The slice does not implement `TIMESTAMP`, `YEAR`, SQL-mode-dependent
`TIME_TRUNCATE_FRACTIONAL`, warning demotion outside the current
`INSERT IGNORE` foundation, temporal arithmetic functions, timezone behavior,
or direct MySQL syntax inside SQLite's parser.

## SQLite Extension Surface Evaluation

SQLite's public extension APIs are not enough for this feature. Scalar
functions can coerce values when generated SQL calls them, but they cannot
transparently enforce every physical write path, direct annotated SQLite write,
`UPDATE` changed-column check, `REPLACE`, or future trigger/generated-column
write without repeating wrappers in generated SQL.

The correct extension point is the existing MyLite-owned column descriptor on
SQLite `Column`, enforced by `OP_MyliteTypeCheck` in the VDBE row-write path.
This keeps the runtime fast: coercion is a direct opcode branch attached to
schema metadata, and ordinary SQLite affinity remains untouched for columns
without a MyLite descriptor.

## Semantics

`TIME` and `TIME(0)` use fractional precision 0. `TIME(fsp)` accepts precision
0 through 6. The descriptor stores canonical text:

- `HH:MM:SS` for precision 0
- `HH:MM:SS.fff...` for precision greater than 0
- a leading `-` for nonzero negative values
- hours are at least two digits and can be three digits at MySQL's upper range

The valid final range is `-838:59:59.000000` through
`838:59:59.000000`. Values that exceed that range after fractional rounding
are errors under MyLite's current strict assignment behavior. MySQL 8.4.9 also
rejects max-boundary inputs with a nonzero discarded fraction, such as
`838:59:59.000001`, even for `TIME(0)`, so the descriptor validates that
boundary before applying declared-precision rounding.

The supported input forms are intentionally based on observed MySQL 8.4.9
runtime behavior:

- `HH:MM[:SS][.fraction]`
- `:MM[:SS][.fraction]`
- `D HH:MM[:SS][.fraction]`
- compact numeric or string digits with optional fractional seconds

Colon forms are interpreted as clock-style abbreviated times. For example,
`34:56` becomes `34:56:00`, and `:12` becomes `00:12:00`.
Compact non-colon forms are interpreted from the right, with the rightmost two
digits as seconds and the previous two as minutes. For example, `1234` becomes
`00:12:34`, and `12` becomes `00:00:12`.

Fractional seconds are rounded, not truncated. Negative values round by
magnitude and then reapply the sign. Negative zero canonicalizes to positive
zero.

Invalid strict assignments return MySQL condition `1292` with SQLSTATE
`22007`.

## MyLite Integration

The public `mylite_sqlite_fork_column_type` ABI gains
`MYLITE_SQLITE_FORK_COLUMN_TYPE_TIME`. The existing `datetime_precision` field
continues to carry temporal fractional-second precision for `TIME`, `DATETIME`,
and future `TIMESTAMP` descriptors.

`mylite_dml_load_write_table()` maps catalog rows whose `DATA_TYPE` is `time`
and whose `DATETIME_PRECISION` is known to the native descriptor. Insert default
and literal binding keeps `TIME` values as text where preserving MySQL's
compact and colon abbreviation rules matters; numeric expression values can
still be bound numerically and are coerced by the descriptor.

## Tests

The test suite covers:

- direct annotated SQLite table writes through the fork descriptor
- MyLite public SQL `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and
  `REPLACE`
- compact and colon-abbreviated input
- day-plus-time input
- negative values and negative-zero canonicalization
- fractional rounding at precision 0, 3, and 6
- range-boundary rounding to `838:59:59`
- strict diagnostics for malformed and out-of-range values
- MySQL-runtime fixture diff against `mysql-time-coercion.expected.tsv`

## Follow-Up Work

- Add `TIMESTAMP(fsp)` with time-zone/session-state semantics.
- Add `YEAR` and zero-year behavior.
- Add SQL-mode-sensitive temporal warning and truncation behavior.
- Revisit temporal storage if comparison, indexing, and protocol metadata need
  a denser native encoding than canonical text.

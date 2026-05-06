# SQLite Fork YEAR Type Descriptors

## Status

Implemented as a SQLite-fork assignment descriptor slice for MySQL `YEAR`.

## References

- MySQL 8.4 Reference Manual, The `YEAR` Type:
  https://dev.mysql.com/doc/refman/8.4/en/year.html
- MySQL-runtime fixture:
  `docs/specs/sqlite-fork-year-type-descriptors/mysql-year-coercion.sql`

## Scope

This slice extends MyLite's private SQLite fork with a native `YEAR`
assignment descriptor. It covers public MyLite `INSERT`, `UPDATE`, `REPLACE`,
and `ON DUPLICATE KEY UPDATE`, plus direct annotated SQLite writes through the
fork API.

Implemented behavior:

- `0` numeric assignment stores `0000`
- textual `0` and `00` assignment stores `2000`
- textual `0000` assignment stores `0000`
- one- and two-digit years map to `2001..2069` and `1970..1999`
- four-digit years in `1901..2155` store directly
- numeric and textual fractional inputs round half away from zero before year
  mapping
- strict out-of-range diagnostics use MySQL condition 1264, SQLSTATE `22003`
- strict invalid text diagnostics use MySQL condition 1366, SQLSTATE `HY000`
- stored values use canonical four-character text

Deferred behavior:

- deprecation warning 1287 for `YEAR(4)` declarations
- non-strict clipping/warning behavior
- exact MySQL error-message text with row interpolation
- direct SQLite parser/catalog descriptor reload without MyLite's catalog
  bridge

## SQLite Extension Surface Evaluation

SQLite public APIs cannot transparently implement `YEAR` assignment semantics.
The quoted-versus-numeric zero distinction must be visible at the write
boundary, and every physical write path must apply the same mapping without
custom generated SQL wrappers. CHECK constraints can reject ranges, but they
cannot canonicalize values, distinguish quoted from numeric zero after MyLite
lowering has already coerced a value, or publish MySQL condition codes.

The existing MyLite column descriptor and `OP_MyliteTypeCheck` VDBE fork point
is the correct integration point.

## MyLite Integration

The public fork ABI adds `MYLITE_SQLITE_FORK_COLUMN_TYPE_YEAR`.
`mylite_dml_load_write_table()` maps catalog rows whose `DATA_TYPE` is `year`
to the descriptor.

The public insert resolver also treats quoted `YEAR` values as text storage so
`'0'` and `0` remain distinguishable until the descriptor performs assignment
conversion. Unquoted numeric expressions still reach the descriptor as numeric
values.

Select expressions over text-backed `YEAR` storage also require MySQL numeric
context behavior. MyLite now treats integer-looking text as an integer numeric
input for arithmetic, so `YEAR + 0` displays like MySQL while preserving
`YEAR`'s canonical text representation when selected directly.

## Tests

The test suite covers:

- direct annotated SQLite descriptor `INSERT` and `UPDATE`
- public MyLite `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`
- numeric zero versus quoted zero
- two-digit year mapping
- `0000` zero-year sentinel
- fractional rounding before year mapping
- out-of-range and invalid-text diagnostics
- MySQL-runtime fixture diff against `mysql-year-coercion.expected.tsv`

## Follow-Up Work

- Add warning demotion for non-strict SQL modes and `INSERT IGNORE`.
- Add declaration-time warning storage for deprecated `YEAR(4)`.
- Attach descriptors directly in the SQLite parser/schema builder when MySQL
  type syntax moves into the fork parser.

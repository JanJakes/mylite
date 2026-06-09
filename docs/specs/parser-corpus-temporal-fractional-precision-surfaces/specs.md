# Parser Corpus Temporal Fractional Precision Surfaces

## Status

Implemented as a parser-corpus compatibility slice. MyLite accepts the MySQL
8.4.9 syntax for selected temporal fractional-second precision forms and
normalizes `fsp = 0` column types to the existing seconds-only temporal storage
path. Nonzero fractional temporal storage and runtime values remain explicitly
unsupported.

## MySQL 8.4.9 Authority

Official MySQL 8.4 documentation defines fractional seconds precision (`fsp`)
for `TIME`, `DATETIME`, and `TIMESTAMP`; valid precision is `0` through `6`,
omitted precision defaults to `0`, and `0` means no fractional part. MySQL also
allows temporal functions such as `CURTIME`, `CURRENT_TIME`, `UTC_TIME`, and
`UTC_TIMESTAMP` to take an optional `fsp` argument in that range. `CAST(... AS
DATETIME(fsp))` accepts an optional fractional precision; invalid precision
uses MySQL error `1426 / 42000`.

Runtime probes were run against the local MySQL 8.4.9 container
`mylite-mysql-849`:

```sql
DROP DATABASE IF EXISTS mylite_tmp_temporal_fsp;
CREATE DATABASE mylite_tmp_temporal_fsp;
USE mylite_tmp_temporal_fsp;
CREATE TABLE fsp_cols (
  tm0 TIME(0),
  tm6 TIME(6),
  dt0 DATETIME(0),
  dt6 DATETIME(6),
  ts0 TIMESTAMP(0) NULL DEFAULT NULL,
  ts6 TIMESTAMP(6) NULL DEFAULT NULL
);
SHOW COLUMNS FROM fsp_cols;
SELECT COLUMN_NAME, DATA_TYPE, DATETIME_PRECISION, COLUMN_TYPE
FROM INFORMATION_SCHEMA.COLUMNS
WHERE TABLE_SCHEMA = 'mylite_tmp_temporal_fsp'
  AND TABLE_NAME = 'fsp_cols'
ORDER BY ORDINAL_POSITION;
SET timestamp = 1700000000;
SELECT CURTIME(0), CURTIME(6), CURRENT_TIME(0), CURRENT_TIME(6),
       UTC_TIME(0), UTC_TIME(6), UTC_TIMESTAMP(0), UTC_TIMESTAMP(6);
SELECT CAST('2024-01-02 03:04:05.123456' AS DATETIME(0)),
       CAST('2024-01-02 03:04:05.123456' AS DATETIME(6));
DROP DATABASE mylite_tmp_temporal_fsp;
```

Observed highlights:

- `SHOW COLUMNS` displays `time`, `datetime`, and `timestamp` for `fsp = 0`.
- `SHOW COLUMNS` displays `time(6)`, `datetime(6)`, and `timestamp(6)` for
  nonzero precision.
- `INFORMATION_SCHEMA.COLUMNS.DATETIME_PRECISION` is `0` for `fsp = 0` and the
  declared precision for nonzero precision.
- `CURTIME(0)`, `CURRENT_TIME(0)`, `UTC_TIME(0)`, and `UTC_TIMESTAMP(0)` return
  seconds-only text under the fixed test timestamp.
- The corresponding `(6)` forms include `.000000` under the fixed timestamp.
- `TIME(7)`, `DATETIME(7)`, `TIMESTAMP(7)`, `CURTIME(7)`,
  `CURRENT_TIME(7)`, `UTC_TIME(7)`, `UTC_TIMESTAMP(7)`, and
  `CAST(... AS DATETIME(7))` return `1426 / 42000` with a maximum of `6`.

## Scope

This slice admits these syntax surfaces:

- `TIME(fsp)`, `DATETIME(fsp)`, and `TIMESTAMP(fsp)` in column definitions used
  by `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, `ALTER TABLE ... MODIFY`,
  and `ALTER TABLE ... CHANGE`.
- `TIME(fsp)`, `DATETIME(fsp)`, and `TIMESTAMP(fsp)` in the current
  `CAST`/`CONVERT` basic target parser surface.
- `CURTIME(fsp)`, `CURRENT_TIME(fsp)`, `UTC_TIME(fsp)`, and
  `UTC_TIMESTAMP(fsp)` in expression, insert-value, update-value, and
  comparison-value positions.

This slice does not implement:

- Microsecond storage, comparison, rounding, truncation, metadata, or display
  for user temporal columns.
- Fractional precision for `CURRENT_TIMESTAMP`, `NOW`, `LOCALTIME`,
  `LOCALTIMESTAMP`, or `SYSDATE` beyond the syntax already admitted by existing
  tests.
- Correct runtime semantics for `CAST(... AS DATETIME(fsp))`; the parser
  accepts the syntax for corpus progress, but the existing cast target runtime
  remains the current coarse character-conversion path.
- `TIME_TRUNCATE_FRACTIONAL`, fractional temporal literals beyond existing
  literal behavior, or fractional descriptor metadata for user tables.

## MyLite Behavior

Column types:

- Omitted precision and explicit `fsp = 0` use the existing MyLite temporal
  descriptor: `TIME`, `DATETIME`, or `TIMESTAMP` with SQLite `TEXT` storage.
- `fsp = 1..6` is rejected during DDL planning with an explicit unsupported
  diagnostic before catalog or SQLite schema mutation.
- `fsp > 6` is rejected with MySQL-shaped `1426 / 42000`:
  `Too-big precision N specified for '<name>'. Maximum is 6.`

Temporal functions:

- Omitted precision and explicit `fsp = 0` use the existing seconds-only
  current-time/UTC-time/UTC-timestamp behavior.
- `fsp = 1..6` is rejected in scalar and DML value planning with an explicit
  unsupported diagnostic.
- `fsp > 6` is rejected with MySQL-shaped `1426 / 42000`. MySQL reports both
  `CURTIME(7)` and `CURRENT_TIME(7)` as `curtime`.
- `CURDATE(fsp)` and `UTC_DATE(fsp)` remain syntax errors because MySQL does not
  define an `fsp` argument for date-only functions.

Cast targets:

- The parser accepts the precision syntax for `TIME`, `DATETIME`, and
  `TIMESTAMP` cast targets.
- Runtime cast precision semantics are intentionally unchanged and tracked as a
  remaining compatibility gap.

## Parser Design

MyLite Lemon syntax snippet:

```lemon
datetime_type ::= DATETIME.
datetime_type ::= DATETIME LPAREN INTEGER RPAREN.
time_type ::= TIME.
time_type ::= TIME LPAREN INTEGER RPAREN.
timestamp_type ::= TIMESTAMP.
timestamp_type ::= TIMESTAMP LPAREN INTEGER RPAREN.

cast_basic_target ::= TIME.
cast_basic_target ::= TIME LPAREN INTEGER RPAREN.
cast_basic_target ::= DATETIME.
cast_basic_target ::= DATETIME LPAREN INTEGER RPAREN.
cast_basic_target ::= TIMESTAMP.
cast_basic_target ::= TIMESTAMP LPAREN INTEGER RPAREN.

current_time_value ::= CURRENT_TIME.
current_time_value ::= CURRENT_TIME LPAREN RPAREN.
current_time_value ::= CURRENT_TIME LPAREN INTEGER RPAREN.
current_time_value ::= CURTIME LPAREN RPAREN.
current_time_value ::= CURTIME LPAREN INTEGER RPAREN.

utc_time_value ::= UTC_TIME.
utc_time_value ::= UTC_TIME LPAREN RPAREN.
utc_time_value ::= UTC_TIME LPAREN INTEGER RPAREN.

utc_timestamp_value ::= UTC_TIMESTAMP.
utc_timestamp_value ::= UTC_TIMESTAMP LPAREN RPAREN.
utc_timestamp_value ::= UTC_TIMESTAMP LPAREN INTEGER RPAREN.
```

`CURTIME` retains the existing no-space function behavior unless
`IGNORE_SPACE` is enabled.

## Architecture

This is a MyLite wrapper/parser/runtime-planning change. It uses SQLite exactly
as before for seconds-only temporal `TEXT` storage. No SQLite fork hook or
public SQLite extension API change is needed for parser acceptance or explicit
unsupported diagnostics.

The AST stores only the optional precision source span and whether it was
present. Runtime parses the span when planning the affected column or scalar
value. This avoids turning `DATETIME(6)` into a raw logical type string that
would confuse existing temporal predicates before full microsecond descriptor
support exists.

## Performance

The added parser payload is small and allocated inside existing AST nodes. The
runtime validation is a single optional integer parse on DDL or scalar-value
planning paths and has no effect on ordinary omitted-precision temporal
statements.

## Tests

MySQL expectation coverage records:

- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` metadata for `fsp = 0` and
  `fsp = 6`.
- Deterministic temporal function output under `SET timestamp`.
- `CAST(... AS DATETIME(0|6))` behavior.
- Invalid `fsp = 7` diagnostics for column types, temporal functions, and cast.

MyLite coverage verifies:

- Parser acceptance for column definitions, `ALTER ... ADD/MODIFY/CHANGE`,
  cast targets, and temporal function `fsp` arguments.
- Parser rejection remains for `CURDATE(1)` and `UTC_DATE(1)`.
- Runtime acceptance and metadata normalization for `TIME(0)`,
  `DATETIME(0)`, and `TIMESTAMP(0)`.
- Runtime explicit rejection for nonzero fractional column precision and
  nonzero temporal function precision without catalog mutation.
- Runtime MySQL-shaped `fsp > 6` diagnostics.

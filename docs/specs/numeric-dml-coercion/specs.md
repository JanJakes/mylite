# Numeric DML Coercion

## Scope

This slice aligns write-time coercion for currently supported integer and
decimal columns across these DML paths:

- `INSERT ... VALUES`
- `INSERT ... SET`
- `REPLACE`
- insert and update branches of `ON DUPLICATE KEY UPDATE`
- single-table `UPDATE`
- joined `UPDATE`

This slice now includes signed and unsigned range clipping for `TINYINT`,
`SMALLINT`, `MEDIUMINT`, `INT`, and `BIGINT` write paths, plus `FLOAT` and
`DOUBLE` target range clipping for covered assignment paths. It also stores
exact `BIGINT UNSIGNED` assignment values above signed 64-bit range for covered
DML paths. String length truncation, `LOAD DATA`, and protocol SQLSTATE details
remain separate tasks.

## Sources

- MySQL 8.4 Reference Manual, type conversion in expression evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, numeric data types:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-types.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior.

## MySQL 8.4.9 Behavior Summary

For signed integer columns, numeric `4.5` stores as `5` in strict and
non-strict modes without warnings. String `'4.5'` also stores as `5` without a
warning. String `'4.5x'` stores as `5` with warning 1265 in non-strict mode and
rejects with error 1265 in strict mode. String `'abc'` stores as `0` with
warning 1366 in non-strict mode and rejects with error 1366 in strict mode.
For integer-family values outside the target column range, strict mode rejects
with error 1264 and non-strict mode clips to the nearest endpoint with warning
1264. `INSERT IGNORE` demotes covered strict range errors to warning 1264 and
stores the clipped endpoint. When a numeric string both has trailing garbage and
rounds outside the target range, the range condition is the reported condition
for the verified MySQL 8.4.9 cases.

Verified clipping endpoints:

| Type | Signed range | Unsigned range |
| --- | --- | --- |
| `TINYINT` | `-128` to `127` | `0` to `255` |
| `SMALLINT` | `-32768` to `32767` | `0` to `65535` |
| `MEDIUMINT` | `-8388608` to `8388607` | `0` to `16777215` |
| `INT` / `INTEGER` | `-2147483648` to `2147483647` | `0` to `4294967295` |
| `BIGINT` | `-9223372036854775808` to `9223372036854775807` | `0` to `18446744073709551615` |

For `BIGINT UNSIGNED`, MySQL accepts `18446744073709551615` and rejects
`18446744073709551616` in strict mode with error 1264. In non-strict or
`IGNORE` assignment paths, values above the unsigned endpoint clip to
`18446744073709551615` with warning 1264. A quoted unsigned endpoint with
trailing characters, such as `'18446744073709551615x'`, stores the endpoint in
non-strict mode with warning 1265 and rejects in strict mode with error 1265.
For signed `BIGINT`, MySQL accepts `-9223372036854775808` and rejects
`-9223372036854775809` in strict mode with error 1264. In non-strict or
`IGNORE` assignment paths, values below the signed endpoint clip to
`-9223372036854775808` with warning 1264. A quoted signed endpoint with
trailing characters, such as `'-9223372036854775808x'`, stores the endpoint in
non-strict mode with warning 1265; when the numeric prefix is already below the
signed endpoint, MySQL reports the range warning 1264 instead.

For `DECIMAL(5,2)`, numeric or string `4.567` stores as `4.57` and records note
1265 in both strict and non-strict modes. String `'4.567x'` rejects with error
1366 in strict mode. In non-strict mode it stores as `4.57` and records two
1265 notes: one for the trailing characters and one for scale rounding. String
`'4.5x'` stores as `4.50` with one 1265 note in non-strict mode. String `'abc'`
stores as `0.00` with warning 1366 in non-strict mode and rejects with error
1366 in strict mode.

For approximate numeric columns, strict mode rejects values outside the target
column range with error 1264. Non-strict mode and `INSERT IGNORE` clip to the
nearest endpoint with warning 1264. Verified endpoints are the single-precision
`FLOAT` maximum shown by MySQL as `3.40282e38` and the double-precision maximum
shown as `1.7976931348623157e308`. Invalid approximate text such as `'abc'`
stores as zero with warning 1265 in non-strict mode and rejects with error 1265
in strict mode.

## MyLite Design

MyLite coerces numeric DML values after scalar evaluation and before SQLite
binding. The DML table loader carries catalog `column_type` and numeric scale
into the write plan so runtime coercion can be based on MySQL column metadata.

For signed integer-family columns, MyLite:

- rounds real values with halves away from zero
- parses string values as numeric prefixes
- treats no numeric prefix as incorrect value 1366
- treats trailing garbage after a numeric prefix as data truncation 1265
- promotes incorrect/truncated values to errors in strict SQL modes and records
  warnings in non-strict modes
- clips out-of-range signed and unsigned `TINYINT`, `SMALLINT`, `MEDIUMINT`,
  `INT`, and `BIGINT` values to the nearest MySQL endpoint in non-strict and
  `IGNORE` paths
- parses signed integer text prefixes by magnitude for values at and below
  `INT64_MIN`, avoiding lossy floating-point coercion before range diagnostics
- stores covered `BIGINT UNSIGNED` values above `INT64_MAX` as canonical
  decimal text in MyLite's physical store so they can be read back exactly
- generates, advances, persists, and exposes `BIGINT UNSIGNED AUTO_INCREMENT`
  sequence values above `INT64_MAX` as exact decimal text for covered
  `CREATE TABLE`, `INSERT`, and `ALTER TABLE ... AUTO_INCREMENT` paths
- reports range condition 1264 before truncation condition 1265 when both
  apply to the same integer value

For decimal columns, MyLite:

- parses numeric and string inputs as decimal-compatible real values for this
  slice
- rounds to the catalog numeric scale
- formats the stored value with exactly the target scale
- records note 1265 when scale rounding changes the value
- treats no numeric prefix as incorrect decimal value 1366
- treats trailing garbage after a numeric prefix as strict error 1366 or
  non-strict 1265 note behavior

For approximate numeric columns, MyLite:

- stores `FLOAT` targets through single-precision rounding
- stores `DOUBLE` targets as double-precision values
- rejects out-of-range `FLOAT`/`DOUBLE` assignments in strict mode with 1264
- clips out-of-range assignments to the nearest finite endpoint in non-strict
  and `IGNORE` paths with warning 1264
- treats missing numeric prefixes as data truncation 1265, storing zero in
  non-strict mode

This slice intentionally does not claim full fixed-point precision. It improves
observable storage, warnings, and strict/non-strict control for common
application writes while keeping full decimal arithmetic, broad unsigned
arithmetic, and exhaustive approximate-number display formatting tracked
separately.

## Test Expectations

Runtime tests must verify MySQL 8.4.9-observed behavior for:

- signed integer real and numeric-string rounding
- strict rejection and non-strict warning coercion for truncated integer strings
- strict rejection and non-strict warning coercion for incorrect integer strings
- strict rejection, non-strict clipping, and `INSERT IGNORE` warning demotion
  for signed and unsigned `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`, and
  `BIGINT` endpoints
- exact `BIGINT UNSIGNED` endpoint storage and overflow diagnostics for direct
  numeric literals, quoted values, and `CAST(... AS UNSIGNED)` assignments
- exact `BIGINT UNSIGNED AUTO_INCREMENT` generation, explicit high-value
  advancement, `INFORMATION_SCHEMA.TABLES.AUTO_INCREMENT`, `SHOW CREATE TABLE`,
  and `ALTER TABLE ... AUTO_INCREMENT` behavior above signed 64-bit range
- exact signed `BIGINT` minimum storage, underflow diagnostics, non-strict
  clipping, and trailing-character precedence for direct and quoted values
- strict rejection, non-strict clipping, and `INSERT IGNORE` warning demotion
  for out-of-range `FLOAT` and `DOUBLE` target values
- invalid approximate-numeric text diagnostics for `FLOAT`/`DOUBLE` targets
- decimal scale rounding and stored text shape
- strict rejection and non-strict note coercion for decimal strings with
  trailing characters
- strict rejection and non-strict warning coercion for incorrect decimal strings
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE`, ODKU update assignments,
  single-table `UPDATE`, and joined `UPDATE`

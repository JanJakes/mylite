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

String length truncation, unsigned range handling, tiny/small/medium integer
range clipping, floating-point column edge cases, `LOAD DATA`, and protocol
SQLSTATE details remain separate tasks.

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

For `DECIMAL(5,2)`, numeric or string `4.567` stores as `4.57` and records note
1265 in both strict and non-strict modes. String `'4.567x'` rejects with error
1366 in strict mode. In non-strict mode it stores as `4.57` and records two
1265 notes: one for the trailing characters and one for scale rounding. String
`'4.5x'` stores as `4.50` with one 1265 note in non-strict mode. String `'abc'`
stores as `0.00` with warning 1366 in non-strict mode and rejects with error
1366 in strict mode.

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

For decimal columns, MyLite:

- parses numeric and string inputs as decimal-compatible real values for this
  slice
- rounds to the catalog numeric scale
- formats the stored value with exactly the target scale
- records note 1265 when scale rounding changes the value
- treats no numeric prefix as incorrect decimal value 1366
- treats trailing garbage after a numeric prefix as strict error 1366 or
  non-strict 1265 note behavior

This slice intentionally does not claim full fixed-point precision. It improves
observable storage, warnings, and strict/non-strict control for common
application writes while keeping full decimal arithmetic and range enforcement
tracked separately.

## Test Expectations

Runtime tests must verify MySQL 8.4.9-observed behavior for:

- signed integer real and numeric-string rounding
- strict rejection and non-strict warning coercion for truncated integer strings
- strict rejection and non-strict warning coercion for incorrect integer strings
- decimal scale rounding and stored text shape
- strict rejection and non-strict note coercion for decimal strings with
  trailing characters
- strict rejection and non-strict warning coercion for incorrect decimal strings
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE`, ODKU update assignments,
  single-table `UPDATE`, and joined `UPDATE`

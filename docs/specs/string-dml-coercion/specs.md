# String DML Coercion

## Scope

This slice aligns write-time length coercion for currently supported `CHAR`,
`VARCHAR`, `BINARY`, and `VARBINARY` columns across these DML paths:

- `INSERT ... VALUES`
- `INSERT ... SET`
- `REPLACE`
- insert and update branches of `ON DUPLICATE KEY UPDATE`
- single-table `UPDATE`
- joined `UPDATE`

TEXT/BLOB family maximum sizes, charset-specific byte validation, strict
SQLSTATE details, `IGNORE` demotion, `LOAD DATA`, and full numeric-to-string
format fidelity remain separate tasks.

## Sources

- MySQL 8.4 Reference Manual, string data types:
  https://dev.mysql.com/doc/refman/8.4/en/string-types.html
- MySQL 8.4 Reference Manual, server SQL modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior.

## MySQL 8.4.9 Behavior Summary

For `VARCHAR(4)` and `CHAR(4)`, inserting or updating string `'abcdef'` in the
default strict mode rejects the statement with error 1406, `Data too long for
column 'name' at row 1`, and leaves the row unchanged. If multiple string
columns are too long, MySQL reports the first failing column in assignment or
column order.

With `sql_mode = ''`, the same value stores as `'abcd'` and records warning
1265, `Data truncated for column 'name' at row 1`. A statement that truncates
two columns records two warnings in column/assignment order. `INSERT ... SET`,
`REPLACE`, ODKU update assignments, single-table `UPDATE`, and joined `UPDATE`
share this strict versus non-strict behavior for the covered forms.

## MyLite Design

MyLite carries `CHARACTER_MAXIMUM_LENGTH` from the catalog into DML write-table
metadata. String DML coercion runs after temporal and numeric coercion, and
before SQLite binding.

For character string columns, MyLite counts UTF-8 character starts and truncates
without splitting continuation bytes. This is compatible with current MyLite
UTF-8 text handling for covered tests; complete charset-specific character and
byte accounting remains deferred.

For binary string columns, MyLite applies the catalog length as bytes.

When a value exceeds the target length, MyLite:

- raises error 1406 in strict SQL modes
- appends warning 1265 and stores the truncated value in non-strict modes
- processes columns and assignments in the existing MyLite DML validation order

## Test Expectations

Runtime tests must verify MySQL 8.4.9-observed behavior for:

- strict rejection and diagnostic code 1406
- non-strict warning 1265 and stored truncation for both `VARCHAR` and `CHAR`
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE`, ODKU update assignments,
  single-table `UPDATE`, and joined `UPDATE`

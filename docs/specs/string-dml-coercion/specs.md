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

It also covers the first TEXT/BLOB-family runtime edges:

- `TINYTEXT` and `TINYBLOB` 255-byte write limits for `INSERT ... VALUES`,
  `INSERT IGNORE`, single-table `UPDATE`, and single-table `UPDATE IGNORE`
- plain `TEXT` and `BLOB` 65,535-byte write limits for `INSERT ... VALUES`,
  `INSERT IGNORE`, single-table `UPDATE`, and single-table `UPDATE IGNORE`

`MEDIUMTEXT`/`MEDIUMBLOB`/`LONGTEXT`/`LONGBLOB` maximum sizes, exhaustive
TEXT/BLOB coverage across all write forms, charset-specific byte validation
beyond the verified UTF-8 boundary behavior, strict SQLSTATE details,
`LOAD DATA`, and full
numeric-to-string format fidelity remain separate tasks.

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

For `BINARY(N)`, MySQL right-pads shorter assigned values with `0x00` bytes to
the declared length. `VARBINARY(N)` stores the original shorter byte sequence.
For values longer than `N`, strict mode rejects with error 1406; non-strict and
`IGNORE` forms store the first `N` bytes and emit warning 1265 for each
truncated binary column.

For `TINYTEXT` and `TINYBLOB`, MySQL enforces a 255-byte maximum. In strict
mode, assigning 256 ASCII bytes to `TINYTEXT` and `TINYBLOB` rejects the
statement at the first overflowing column with error 1406 and leaves rows
unchanged. In non-strict mode and the covered `IGNORE` forms, MySQL stores 255
bytes and emits warning 1265 per truncated column. For multibyte `TINYTEXT`,
non-strict truncation does not store a partial UTF-8 character; 128 two-byte
characters become 127 characters / 254 bytes with one warning.

For plain `TEXT` and `BLOB`, MySQL enforces a 65,535-byte maximum. In strict
mode, assigning 65,536 ASCII bytes to `TEXT` and `BLOB` rejects the statement at
the first overflowing column with error 1406 and leaves rows unchanged. In
non-strict mode and the covered `IGNORE` forms, MySQL stores 65,535 bytes and
emits warning 1265 per truncated column. For multibyte `TEXT`, non-strict
truncation does not store a partial UTF-8 character; 32,768 two-byte characters
become 32,767 characters / 65,534 bytes with one warning.

## MyLite Design

MyLite carries `CHARACTER_MAXIMUM_LENGTH` from the catalog into DML write-table
metadata. String DML coercion runs after temporal and numeric coercion, and
before SQLite binding.

For character string columns, MyLite counts UTF-8 character starts and truncates
without splitting continuation bytes. This is compatible with current MyLite
UTF-8 text handling for covered tests; complete charset-specific character and
byte accounting remains deferred.

For binary string columns, MyLite applies the catalog length as bytes.
`BINARY(N)` additionally pads shorter values with trailing `0x00` bytes before
SQLite binding, while `VARBINARY(N)` preserves shorter values without padding.

For the covered `TINYTEXT` and `TEXT` slices, MyLite applies the catalog length
as a byte limit and truncates to a complete UTF-8 prefix. For the covered
`TINYBLOB` and `BLOB` slices, MyLite applies the catalog length as raw bytes.

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
- `BINARY(N)` NUL-byte right-padding for shorter insert and update values,
  `VARBINARY(N)` non-padding, strict rejection for overlong values, non-strict
  truncation, and single-table `UPDATE IGNORE` padding/truncation
- strict rejection, non-strict truncation, `INSERT IGNORE`, and
  single-table `UPDATE IGNORE` for `TINYTEXT` and `TINYBLOB`
- `TINYTEXT` byte-limit truncation at a complete UTF-8 character boundary
- strict rejection, non-strict truncation, `INSERT IGNORE`, and
  single-table `UPDATE IGNORE` for `TEXT` and `BLOB`
- `TEXT` byte-limit truncation at a complete UTF-8 character boundary

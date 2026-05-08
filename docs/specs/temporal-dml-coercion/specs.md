# Temporal DML Coercion

## Scope

This slice aligns MyLite's `DATE`, `DATETIME`, and `TIMESTAMP` write-time
coercion for invalid and zero dates with MySQL 8.4.9 for the currently
supported DML surfaces:

- `INSERT ... VALUES`
- `INSERT ... SET`
- insert and update branches of `ON DUPLICATE KEY UPDATE`
- `REPLACE`
- single-table `UPDATE`
- joined `UPDATE`

Expression functions such as `DATE()`, `DATEDIFF()`, and date arithmetic keep
their existing scalar-function implementation and are not changed by this DML
slice.

## Sources

- MySQL 8.4 Reference Manual, date and time types:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-types.html
- MySQL 8.4 Reference Manual, server SQL modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

With the default MySQL 8.4.9 SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
invalid, zero, and zero-in-date values for `DATE` and `DATETIME` reject the
write with error 1292. `TIMESTAMP` also rejects zero-in-date values under
strict mode.

With `sql_mode=''`:

- impossible dates such as `2022-31-01` are coerced to the zero value and warn
  1265
- calendar-invalid dates such as `2022-02-31` are coerced to the zero value and
  warn 1264
- `DATE` and `DATETIME` preserve zero-in-date values such as `2022-00-01`
- `TIMESTAMP` coerces zero-in-date values to the zero timestamp and warns 1264
- full zero date/datetime/timestamp values are stored without a warning

With `ALLOW_INVALID_DATES`, `DATE` and `DATETIME` preserve calendar-invalid
dates whose month is 1-12 and day is 1-31, such as `2022-02-31`. Impossible
month/day shapes such as `2022-31-01` still reject in strict mode or coerce to
zero in non-strict mode. `TIMESTAMP` does not use `ALLOW_INVALID_DATES` to
preserve invalid calendar values.

With non-strict `NO_ZERO_DATE` or `NO_ZERO_IN_DATE`, MySQL coerces the affected
value to the type's zero value and warns 1264. With the same modes under strict
mode, MySQL rejects the value with error 1292.

## MyLite Design

MyLite validates temporal DML values after scalar evaluation but before the
SQLite write. This keeps all write forms on the same behavior:

- quoted and unquoted temporal literals are validated before binding
- non-null non-text insert candidate and assignment values are converted to
  their scalar text form for classification, so expression writes such as
  `VALUES (0+0)` and `SET d = 20220001+0` observe strict zero-date SQL modes
  before the SQLite write
- invalid or mode-rejected `DATE` values coerce to `0000-00-00` in non-strict
  mode
- invalid or mode-rejected `DATETIME` and `TIMESTAMP` values coerce to
  `0000-00-00 00:00:00` in non-strict mode
- already valid accepted temporal text keeps the existing MyLite storage shape
  in this slice
- non-null non-text update RHS values are converted to their scalar text form
  for classification, so numeric assignments such as `d = 20220001` still
  observe strict zero-date SQL modes before the SQLite write

For supported formats, MyLite classifies the input as:

- valid complete date/datetime
- full zero date
- zero-in-date
- invalid calendar date
- impossible or malformed temporal value

Strict-mode errors set the current error message and append MySQL condition
1292. Non-strict coercions append warning 1264 for out-of-range temporal values
or 1265 for malformed/impossible values. Coerced `DATE` values become
`0000-00-00`; coerced `DATETIME` and `TIMESTAMP` values become
`0000-00-00 00:00:00`.

This slice intentionally does not implement full MySQL temporal input parsing or
valid-value normalization. Compact `YYYYMMDD` and `YYYYMMDDHHMMSS`, delimited
`YYYY-MM-DD`, and delimited `YYYY-MM-DD HH:MM:SS[.fraction]` are covered for
classification because these forms map directly to the reported compatibility
gaps and current MyLite literal support.

## Test Expectations

Runtime tests must verify MySQL 8.4.9-observed behavior for:

- default strict/no-zero rejection
- non-strict malformed-date coercion with warnings
- non-strict zero-in-date preservation for `DATE`/`DATETIME`
- non-strict zero-in-date coercion for `TIMESTAMP`
- `ALLOW_INVALID_DATES` preservation for calendar-invalid `DATE`/`DATETIME`
- non-strict `NO_ZERO_DATE` and `NO_ZERO_IN_DATE` warnings
- `REPLACE` and `ON DUPLICATE KEY UPDATE` write-path coercion
- update-path coercion and strict rejection without mutating existing rows
- numeric update RHS strict rejection for zero and zero-in-date values
- numeric and scalar-expression insert candidate strict rejection for zero and
  zero-in-date values without mutating existing rows

# `UNIX_TIMESTAMP()`

## Scope

MyLite implements `UNIX_TIMESTAMP()` and `UNIX_TIMESTAMP(expr)` for scalar and
table-backed expression contexts.

## Behavior

- `UNIX_TIMESTAMP()` returns the statement timestamp as whole seconds since
  `1970-01-01 00:00:00` UTC.
- `UNIX_TIMESTAMP(expr)` converts a valid date, datetime, timestamp-like text,
  or numeric temporal value to seconds since the Unix epoch.
- Fractional seconds are preserved in the return value when the input includes a
  fractional part.
- `NULL` input returns `NULL`.
- Valid dates before `1970-01-01 00:00:00` or after
  `3001-01-18 23:59:59.999999` return `0` without a warning.
- Invalid temporal input returns `0.000000` and emits warning 1292
  `Incorrect datetime value`.
- MyLite uses UTC because it does not yet model MySQL session time zones.

## Verified Expectations

Verified against MySQL 8.4.9:

| Expression | Result |
| --- | --- |
| `UNIX_TIMESTAMP('1970-01-01 00:00:00')` | `0` |
| `UNIX_TIMESTAMP('1970-01-01 00:00:01')` | `1` |
| `UNIX_TIMESTAMP('2000-01-02 03:04:05.123456')` | `946782245.123456` |
| `UNIX_TIMESTAMP('3001-01-18 23:59:59.999999')` | `32536771199.999999` |
| `UNIX_TIMESTAMP('1969-12-31 23:59:59')` | `0` |
| `UNIX_TIMESTAMP('3001-01-19 00:00:00')` | `0` |
| `UNIX_TIMESTAMP(NULL)` | `NULL` |
| `UNIX_TIMESTAMP('bad')` | `0.000000` plus warning 1292 |

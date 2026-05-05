# TIMESTAMP function

## Scope

This feature implements `TIMESTAMP(expr)` and `TIMESTAMP(expr1, expr2)` for the
scalar expression contexts MyLite currently executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- exact native parser diagnostics for unsupported arities
- time-zone-sensitive `TIMESTAMP` column conversion
- exact statement-stable current-date behavior for every typed `TIME` edge case
- broader permissive temporal literal variants beyond MyLite's current date,
  datetime, and time parsers
- broader SQL-mode variants beyond MyLite's current strict-mode warning policy

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/time-function/specs.md`
  - `docs/specs/timediff-function/specs.md`

Runtime behavior was verified against the local MySQL 8.4.9 container with:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 Behavior

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

Verified scalar results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `TIMESTAMP('2024-01-02')` | `2024-01-02 00:00:00` | none |
| `TIMESTAMP('2024-01-02 03:04:05')` | `2024-01-02 03:04:05` | none |
| `TIMESTAMP('2024-01-02 03:04:05.123456')` | `2024-01-02 03:04:05.123456` | none |
| `TIMESTAMP('20240102030405')` | `2024-01-02 03:04:05` | none |
| `TIMESTAMP(20240102030405)` | `2024-01-02 03:04:05` | none |
| `TIMESTAMP(NULL)` | `NULL` | none |
| `TIMESTAMP('bad')` | `NULL` | 1292 incorrect datetime |
| `TIMESTAMP('12:00:00')` | `NULL` | 1292 incorrect datetime |
| `TIMESTAMP('2024-01-02','03:04:05')` | `2024-01-02 03:04:05` | none |
| `TIMESTAMP('2024-01-02 01:02:03','03:04:05')` | `2024-01-02 04:06:08` | none |
| `TIMESTAMP('2024-01-02','27:00:00')` | `2024-01-03 03:00:00` | none |
| `TIMESTAMP('2024-01-02','-03:00:00')` | `2024-01-01 21:00:00` | none |
| `TIMESTAMP('2024-01-02','00:00:00.123456')` | `2024-01-02 00:00:00.123456` | none |
| `TIMESTAMP('2024-01-02 23:59:59.900000','00:00:00.200000')` | `2024-01-03 00:00:00.100000` | none |
| `TIMESTAMP(NULL,'bad')` | `NULL` | none |
| `TIMESTAMP('bad',NULL)` | `NULL` | 1292 incorrect datetime |
| `TIMESTAMP('bad','bad')` | `NULL` | 1292 incorrect datetime only |
| `TIMESTAMP('2024-01-02','bad')` | `NULL` | 1292 truncated time |
| `TIMESTAMP('2024-01-02','2024-01-03')` | `2024-01-02 00:20:24` | 1292 truncated time |
| `TIMESTAMP('9999-12-31 23:59:59','00:00:01')` | `NULL` | 1441 add_time overflow |

Typed temporal values differ from untyped strings:

| Source expression | Result | Warnings |
| --- | --- | --- |
| `TIMESTAMP(date_col)` | date at midnight | none |
| `TIMESTAMP(datetime_col)` | datetime value with column precision | none |
| `TIMESTAMP(timestamp_col)` | timestamp value with column precision | none |
| `TIMESTAMP(time_col)` | current statement date plus the time value | none |
| `TIMESTAMP(varchar_col)` for text `2024-01-02` | datetime at midnight | none |
| `TIMESTAMP(date_col, time_col)` | date plus time | none |
| `TIMESTAMP(datetime_col, time_col)` | datetime plus time | none |
| `TIMESTAMP(date_col, date_col)` | `NULL` | none |
| `TIMESTAMP(datetime_col, date_col)` | `NULL` | none |
| `TIMESTAMP(time_col, time_col)` | current statement date plus both times | none |

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| whole-second result | `DATETIME` | 19 | 0 | binary | `BINARY` | yes |
| fractional result with max scale `N` | `DATETIME` | `20 + N` | `N` | binary | `BINARY` | yes |
| unresolved textual precision | `DATETIME` | 26 | 6 | binary | `BINARY` | yes |
| `TIMESTAMP(NULL)` | `DATETIME` | 19 | 0 | binary | `BINARY` | yes |

## MyLite Compatibility Decisions

`TIMESTAMP()` accepts one or two arguments. The one-argument form converts its
argument to a datetime value. `NULL` returns `NULL`. Invalid datetime input
returns `NULL` with warning 1292. Untyped time-looking strings are not accepted
as datetimes.

Typed `TIME` values in the first argument are special: MySQL combines them with
the current statement date. MyLite uses its current UTC statement-date model for
this first slice, matching the existing current-temporal function decision until
mutable session time zones are implemented.

The two-argument form first converts `expr1` to the same datetime base. If the
first argument is `NULL` or invalid, `TIMESTAMP()` returns `NULL` without
coercing the second argument. Otherwise `expr2` is converted as a time value and
added to the base datetime. Untyped date-shaped strings in the second argument
therefore follow MySQL's time parser and can produce values such as
`00:20:24` with warning 1292. Typed `DATE`, `DATETIME`, and `TIMESTAMP` values
are not valid second operands and return `NULL` without warnings; typed `TIME`
values are valid.

Result fractional precision is the maximum precision of the datetime base and
time interval. Datetime overflow above MySQL's maximum datetime returns `NULL`
and emits warning 1441 with add-time wording. Arithmetic results before year
`0001` return `NULL`, matching observed MySQL behavior for the first slice.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

`TIMESTAMP()` uses the ordinary scalar function call shape.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression RPAREN.
scalar_function_call ::= function_name LPAREN expression COMMA expression RPAREN.

function_name ::= identifier.
```

`TIMESTAMP` remains available as a temporal type name in column definitions and
as an ordinary function name in expression calls.

## Runtime Semantics

One-argument form:

1. Evaluate the argument.
2. Return `NULL` without warnings when it is `NULL`.
3. Convert typed `TIME` to current statement date plus the time value.
4. Convert all other values through datetime coercion.
5. Return `NULL` when conversion fails.
6. Return a nullable binary `DATETIME` value.

Two-argument form:

1. Evaluate `expr1`.
2. Return `NULL` without warnings when `expr1` is `NULL`.
3. Convert `expr1` to a datetime base; return `NULL` when invalid.
4. Evaluate `expr2`.
5. Return `NULL` when `expr2` is `NULL`.
6. Convert `expr2` to a time interval, rejecting typed date/datetime/timestamp
   operands without warnings.
7. Add signed interval microseconds to the datetime base.
8. Return `NULL` and warning 1441 when the result is above the maximum datetime.
9. Return `NULL` when the result is before year `0001`.
10. Format the `DATETIME` result with the selected fractional precision.

## Tests

Parser tests should cover:

- ordinary one-argument and two-argument function-call parsing
- use of `timestamp` as an identifier when not called
- rejected zero-argument and three-argument forms

Runtime tests should cover:

- result metadata for date, datetime, fractional, `NULL`, and unresolved text
  shapes
- one-argument date, datetime, compact datetime, numeric datetime, invalid, time
  string, `NULL`, and typed `TIME` inputs
- two-argument date-plus-time, datetime-plus-time, over-24-hour time, negative
  time, fractional carry, invalid left, invalid right, `NULL`, date-shaped right
  string, and overflow
- typed `DATE`, `TIME(6)`, `DATETIME(6)`, and `TIMESTAMP(6)` column behavior
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment, `DELETE` predicate, and strict DML warning promotion

## Compatibility Status

After implementation, `TIMESTAMP()` has partial compatibility for supported
scalar expression paths and MyLite's current temporal coercion surface. Exact
native diagnostics, full session time-zone behavior, broader temporal literal
variants, exact prepared metadata for all unresolved expressions, and protocol
metadata remain deferred.

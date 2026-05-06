# ADDTIME() and SUBTIME() functions

## Scope

This feature implements the first MyLite slice of MySQL-compatible
`ADDTIME(expr1, expr2)` and `SUBTIME(expr1, expr2)` for scalar expression
contexts that already use MyLite's shared expression evaluator:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`, and
  `REPLACE ... SET` source expressions
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- exact native wrong-arity error-code reporting
- exact prepared-statement dynamic-parameter metadata
- time-zone-sensitive `TIMESTAMP` conversion
- broader permissive temporal literal variants beyond MyLite's current
  date/datetime and time parsers
- broader SQL-mode variants beyond MyLite's current strict warning policy

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/time-function/specs.md`
  - `docs/specs/timediff-function/specs.md`
  - `docs/specs/timestamp-function/specs.md`

Runtime behavior was verified against the local MySQL 8.4.9 container with:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 behavior

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

Verified scalar results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `ADDTIME('12:00:00','01:30:00')` | `13:30:00` | none |
| `SUBTIME('12:00:00','01:30:00')` | `10:30:00` | none |
| `ADDTIME('2024-01-01 12:00:00','01:02:03')` | `2024-01-01 13:02:03` | none |
| `SUBTIME('2024-01-01 00:00:00','01:00:00')` | `2023-12-31 23:00:00` | none |
| `ADDTIME('12:00:00.123456','00:00:00.654321')` | `12:00:00.777777` | none |
| `SUBTIME('12:00:00.123456','00:00:00.654321')` | `11:59:59.469135` | none |
| `ADDTIME(NULL,'01:00:00')` | `NULL` | none |
| `ADDTIME('12:00:00',NULL)` | `NULL` | none |
| `ADDTIME('bad','01:00:00')` | `NULL` | 1292 truncated time |
| `ADDTIME('12:00:00','bad')` | `NULL` | 1292 truncated time |
| `ADDTIME('839:00:00','00:00:00')` | `838:59:59` | 1292 truncated time |
| `ADDTIME('838:00:00','01:00:00')` | `838:59:59` | 1292 truncated result |
| `SUBTIME('00:00:00','839:00:00')` | `-838:59:59` | 1292 truncated time |
| `SUBTIME('00:00:00','838:00:00')` | `-838:00:00` | none |
| `ADDTIME('2024-01-01','01:00:00')` | `01:20:24` | 1292 truncated time |
| `ADDTIME('2024-01-01 00:00:00','839:00:00')` | `2024-02-04 22:59:59` | 1292 truncated interval |
| `ADDTIME('9999-12-31 23:59:59','00:00:01')` | `NULL` | 1441 add_time overflow |
| `SUBTIME('0001-01-01 00:00:00','00:00:01')` | `NULL` | none |

Typed temporal values follow the resolved type of the first argument:

| Source expression | Result | Warnings |
| --- | --- | --- |
| `ADDTIME(date_col,'01:00:00')` | `YYYY-MM-DD 01:00:00` | none |
| `SUBTIME(date_col,'01:00:00')` | previous day `23:00:00` | none |
| `ADDTIME(time_col,'01:00:00.1')` | `TIME` value with fractional precision | none |
| `ADDTIME(datetime_col,'01:00:00.1')` | `DATETIME` value with fractional precision | none |
| `ADDTIME(time_col, datetime_col)` | `NULL` | none |
| `ADDTIME(datetime_col, datetime_col)` | `NULL` | none |

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Nullable |
| --- | --- | ---: | ---: | --- | --- |
| string-literal first argument | `STRING` | `29` | `31` | connection collation | yes |
| typed `TIME(N)` first argument | `TIME` | `10` or `11 + N` | `N` | binary | yes |
| typed `DATE`, `DATETIME(N)`, or `TIMESTAMP(N)` first argument | `DATETIME` | `19` or `20 + N` | `N` | binary | yes |

## MyLite compatibility decisions

`ADDTIME()` and `SUBTIME()` accept exactly two arguments. Both functions
evaluate and convert the first argument before touching the second argument. If
the first argument evaluates to `NULL` or cannot be converted, the result is
`NULL` and the second argument is not evaluated. If the first argument is valid,
the second argument is evaluated and converted; a `NULL` second argument returns
`NULL`.

The first argument is classified with the same operand rules as `TIMEDIFF()`:

- typed `TIME` values produce `TIME` arithmetic
- typed `DATE`, `DATETIME`, and `TIMESTAMP` values produce `DATETIME`
  arithmetic, with typed `DATE` values using midnight
- untyped datetime strings that include a time part produce `DATETIME`
  arithmetic
- other untyped values are parsed with the time parser, including MySQL's
  warning-producing date-shaped string behavior

The second argument is always a time operand. Typed `DATE`, `DATETIME`, and
`TIMESTAMP` second arguments are invalid and produce `NULL` without additional
warnings. Untyped datetime strings with a time part are also invalid second
arguments and produce `NULL` without additional warnings.

For time arithmetic, MyLite adds or subtracts signed microsecond totals and
clips results outside MySQL's `TIME` range to `-838:59:59` or `838:59:59`,
emitting warning 1292 for the out-of-range result. Fractional precision is the
maximum successful operand precision.

For datetime arithmetic, MyLite adds or subtracts the time interval from the
first argument's day-number and time-of-day. Upper overflow above
`9999-12-31 23:59:59.999999` returns `NULL` and emits warning 1441 with
`add_time` wording. Lower underflow returns `NULL` without a warning, matching
the observed MySQL behavior.

The first metadata slice follows MySQL's first-argument typing for typed table
columns and reports conservative string metadata for unresolved string-shaped
arguments. Exact prepared-statement dynamic-parameter metadata remains
deferred.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon grammar snippets

`ADDTIME()` and `SUBTIME()` use the ordinary scalar function call shape.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression COMMA expression RPAREN.

function_name ::= identifier.
```

`ADDTIME` and `SUBTIME` remain ordinary nonreserved function names and may be
used as identifiers when not called.

## Runtime semantics

1. Evaluate `expr1`.
2. If `expr1` is `NULL`, return `NULL`.
3. Convert `expr1` to a time or datetime operand; return `NULL` on failure.
4. Evaluate `expr2`.
5. If `expr2` is `NULL`, return `NULL`.
6. Convert `expr2` to a time operand; return `NULL` on failure.
7. For `ADDTIME`, add the interval; for `SUBTIME`, subtract it.
8. Clip out-of-range `TIME` results with warning 1292.
9. Return `NULL` for out-of-range `DATETIME` results, warning only for upper
    overflow.

## Tests

Parser tests should cover:

- ordinary two-argument function-call parsing
- use of `addtime` and `subtime` as identifiers when not called

Prepare/runtime tests should cover unsupported arities.

Runtime tests should cover:

- result metadata for typed `TIME`, `DATE`, and `DATETIME` first arguments
- positive, negative, fractional, over-24-hour, and `NULL` time inputs
- datetime addition/subtraction across date boundaries
- typed first-argument behavior for `DATE`, `TIME`, and `DATETIME`
- invalid first and second operands
- input clipping, result clipping, and datetime overflow warnings
- table projection, `WHERE`, and `ORDER BY`
- `INSERT`/`REPLACE` source expressions
- `UPDATE` assignment, `DELETE` predicate, and strict DML warning promotion

## Compatibility status

After implementation, `ADDTIME()` and `SUBTIME()` have partial compatibility
for supported scalar expression paths and MyLite's current temporal coercion
surface. Exact native diagnostics, time-zone-sensitive `TIMESTAMP` behavior,
dynamic-parameter metadata, broader temporal literal variants, and protocol
metadata remain deferred.

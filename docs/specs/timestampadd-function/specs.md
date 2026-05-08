# TIMESTAMPADD() scalar function

## Scope

This feature implements the first MySQL 8.4.9-compatible
`TIMESTAMPADD(unit, interval, datetime_expr)` slice for expression contexts
MyLite already executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` and `DELETE` expression paths

The supported units in this slice are:

- `DAY`
- `WEEK`
- `MONTH`
- `YEAR`
- `HOUR`
- `MINUTE`
- `SECOND`
- `QUARTER`
- `MICROSECOND`

The MySQL-supported `SQL_TSI_YEAR`, `SQL_TSI_QUARTER`, `SQL_TSI_MONTH`,
`SQL_TSI_WEEK`, `SQL_TSI_DAY`, `SQL_TSI_HOUR`, `SQL_TSI_MINUTE`, and
`SQL_TSI_SECOND` aliases are accepted and normalized to the corresponding
unit. MySQL 8.4.9 rejects `SQL_TSI_MICROSECOND`, and MyLite rejects it too.

Out of scope:

- combined interval units such as `YEAR_MONTH`, which MySQL rejects for
  `TIMESTAMPADD()`
- time-only input conversion
- named time zones and `TIMESTAMP` session time-zone conversion
- exact native parser diagnostic text and native scalar arity error-code parity
- overflow diagnostics at the edges of MySQL's temporal range

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/date-add-sub-functions/specs.md`
  - `docs/specs/timestampdiff-function/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`

Runtime behavior was verified against the official `mysql:8.4.9` Docker image
in container `mylite-mysql-849`, using:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## MySQL 8.4.9 behavior

Runtime probes used MySQL 8.4.9 with `time_zone = '+00:00'` and
`timestamp = 1700000000.987654` where current temporal functions mattered.

`TIMESTAMPADD()` adds the integer interval amount in the requested unit to the
date or datetime expression. Date-only values return dates for date-only units
and datetimes for time units. Datetime values preserve fractional seconds.

Verified results for this slice:

| Expression | Result | Warnings |
| --- | --- | --- |
| `TIMESTAMPADD(DAY,2,'2024-02-28')` | `2024-03-01` | none |
| `TIMESTAMPADD(WEEK,2,'2024-02-01')` | `2024-02-15` | none |
| `TIMESTAMPADD(MONTH,1,'2024-01-31')` | `2024-02-29` | none |
| `TIMESTAMPADD(YEAR,1,'2024-02-29')` | `2025-02-28` | none |
| `TIMESTAMPADD(HOUR,1,'2024-01-01')` | `2024-01-01 01:00:00` | none |
| `TIMESTAMPADD(MINUTE,-1,'2024-01-01 00:01:00')` | `2024-01-01 00:00:00` | none |
| `TIMESTAMPADD(SECOND,1,'2024-01-01 23:59:59.999999')` | `2024-01-02 00:00:00.999999` | none |
| `TIMESTAMPADD(QUARTER,1,'2024-01-31')` | `2024-04-30` | none |
| `TIMESTAMPADD(MICROSECOND,2,'2024-01-01')` | `2024-01-01 00:00:00.000002` | none |
| `TIMESTAMPADD(MICROSECOND,1000000,'2024-01-01')` | `2024-01-01 00:00:01` | none |
| `TIMESTAMPADD(SQL_TSI_DAY,1,'2024-01-01')` | `2024-01-02` | none |
| `TIMESTAMPADD(SQL_TSI_QUARTER,1,'2024-01-31')` | `2024-04-30` | none |
| `TIMESTAMPADD(DAY,1.9,'2024-01-01')` | `2024-01-03` | none |
| `TIMESTAMPADD(DAY,-1.9,'2024-01-02')` | `2023-12-31` | none |
| `TIMESTAMPADD(SECOND,1,NOW(6))` after the timestamp setting above | `2023-11-14 22:13:21.987654` | none |

Month and year arithmetic clips the day to the last valid day of the target
month. This covers leap-day and month-end cases such as January 31 plus one
month and February 29 plus one year.

`NULL` and invalid values:

| Expression | Result | Warnings |
| --- | --- | --- |
| `TIMESTAMPADD(DAY,NULL,'2024-01-01')` | `NULL` | none |
| `TIMESTAMPADD(DAY,1,NULL)` | `NULL` | none |
| `TIMESTAMPADD(DAY,'bad','2024-01-01')` | `2024-01-01` | 1292 truncated incorrect integer |
| `TIMESTAMPADD(DAY,1,'bad')` | `NULL` | 1292 incorrect datetime value |
| `TIMESTAMPADD(DAY,'bad',NULL)` | `NULL` | none |
| `TIMESTAMPADD(DAY,1,'0000-00-00')` | `NULL` | 1292 incorrect datetime value |

The supported input conversion surface is the same as MyLite's existing
complete-date temporal parser and date-arithmetic interval amount conversion.
Invalid interval text contributes warning 1292 and uses zero as the interval
amount after the datetime expression has produced a non-`NULL`, valid temporal
value.

For string datetime inputs, MySQL prints a fractional part only when the
resulting microsecond component is nonzero. For typed `DATE`, `DATETIME`, and
`TIMESTAMP` inputs, `MICROSECOND` arithmetic returns a typed `DATETIME(6)`,
including `.000000` when the result lands on a whole second.

Malformed syntax:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT TIMESTAMPADD(YEAR_MONTH,1,'2024-01-01')` | syntax error 1064 |
| `SELECT TIMESTAMPADD(SQL_TSI_MICROSECOND,1,'2024-01-01')` | syntax error 1064 |
| `SELECT TIMESTAMPADD('DAY',1,'2024-01-01')` | syntax error 1064 |
| `SELECT TIMESTAMPADD(DAY,1)` | syntax error 1064 |
| `SELECT TIMESTAMPADD(DAY,1,'2024-01-01','x')` | syntax error 1064 |

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| `TIMESTAMPADD(DAY,2,'2024-02-28')` | `STRING` | `29` bytes under `latin1`; `116` bytes under MyLite's default `utf8mb4` | `31` | connection collation | none | yes |
| `TIMESTAMPADD(DAY,2,CURDATE())` | `DATE` | `10` | `0` | binary (63) | `BINARY` | yes |
| `TIMESTAMPADD(HOUR,2,CURDATE())` | `DATETIME` | `19` | `0` | binary (63) | `BINARY` | yes |
| `TIMESTAMPADD(SECOND,1,NOW(6))` | `DATETIME` | `26` | `6` | binary (63) | `BINARY` | yes |
| `TIMESTAMPADD(MICROSECOND,2,CURDATE())` | `DATETIME` | `26` | `6` | binary (63) | `BINARY` | yes |
| `TIMESTAMPADD(MICROSECOND,2,NOW(3))` | `DATETIME` | `26` | `6` | binary (63) | `BINARY` | yes |

## MyLite compatibility decisions

MyLite stores the interval unit as metadata on the function-call AST node. The
argument list contains exactly the two evaluatable expressions in runtime
order:

1. `datetime_expr`
2. `interval`

This normalized order intentionally differs from the source syntax so
`TIMESTAMPADD()` can use the same evaluator path as `DATE_ADD()` while
preserving MySQL's datetime-first short-circuit behavior.

The supported simple units listed in scope are accepted as interval-unit
keywords. MySQL's `SQL_TSI_` aliases are accepted for all observed units except
`SQL_TSI_MICROSECOND`, which MySQL rejects. Combined interval units remain
rejected, matching MySQL for `YEAR_MONTH`.

For invalid temporal values, MyLite should match the existing temporal parser's
warning text family and default strict-mode warning policy. In `UPDATE` and
`DELETE`, warnings from invalid `TIMESTAMPADD()` operands are promoted through
MyLite's existing DML warning-promotion path so the statement fails and changes
are rolled back.

## MyLite Lemon grammar snippets

These snippets describe the intended MyLite grammar shape; they are not copied
from MySQL grammar.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.

interval_unit ::= DAY.
interval_unit ::= WEEK.
interval_unit ::= MONTH.
interval_unit ::= YEAR.
interval_unit ::= HOUR.
interval_unit ::= MINUTE.
interval_unit ::= SECOND.
interval_unit ::= QUARTER.
interval_unit ::= MICROSECOND.
```

When `function_name` text is `TIMESTAMPADD`, MyLite validates that the argument
list has exactly three parsed arguments, that the first argument is one of the
supported interval-unit identifiers above or a supported `SQL_TSI_` alias, and
then stores the unit as function-call metadata while preserving `datetime_expr`
and `interval` as runtime arguments. `TIMESTAMPADD` remains a nonreserved
identifier when it is not used as a call name.

## Runtime semantics

1. Evaluate `datetime_expr`.
2. If `datetime_expr` is `NULL`, return `NULL` without evaluating or converting
   `interval`.
3. Convert `datetime_expr` to a complete date/datetime. On failure, append
   warning 1292 and return `NULL`.
4. Evaluate `interval`.
5. If `interval` is `NULL`, return `NULL`.
6. Convert `interval` to a signed integer using MyLite's MySQL-compatible
   integer conversion path.
7. Add the signed amount in the requested unit to the temporal value.
8. Return a date for date-only input plus date-only units; otherwise return a
   datetime preserving available fractional precision. `MICROSECOND` arithmetic
   over typed temporal inputs returns `DATETIME(6)`.

`DAY`, `WEEK`, `HOUR`, `MINUTE`, and `SECOND` use MyLite's day and second
arithmetic helpers. `MONTH` and `YEAR` use calendar month arithmetic with
target-month clipping; `QUARTER` is month arithmetic scaled by three.

## Tests

Parser tests should cover:

- all supported units
- normalized AST unit metadata and runtime argument order
- `timestampadd` as a nonreserved identifier outside calls
- supported `SQL_TSI_` aliases and rejected `SQL_TSI_MICROSECOND`
- rejected quoted-unit, missing-argument, extra-argument, and combined-unit
  forms

Runtime tests should cover:

- no-table scalar results for every supported unit
- month-end and leap-day clipping
- negative and fractional interval conversion
- fractional datetime preservation
- `NULL` propagation and datetime-first short-circuiting
- invalid datetime and interval warnings
- current and nested temporal inputs
- result metadata for string, date, datetime, and fractional datetime shapes
- one-table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` and `DELETE` expression paths
- DML warning promotion and rollback for invalid datetime and invalid interval
  operands

## Compatibility status

This feature moves `TIMESTAMPADD()` to partial compatibility for the supported
simple units and supported scalar expression call sites. Time-only conversion,
time-zone-sensitive `TIMESTAMP` behavior, exact native syntax diagnostics, and
broader expression contexts remain tracked as incomplete scalar temporal
function work.

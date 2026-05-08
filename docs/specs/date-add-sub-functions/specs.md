# DATE_ADD(), DATE_SUB(), ADDDATE(), and SUBDATE() interval functions

## Scope

This slice implements MySQL-compatible interval arithmetic function calls
for:

- `DATE_ADD(expr, INTERVAL expr unit)`
- `DATE_SUB(expr, INTERVAL expr unit)`
- `ADDDATE(expr, INTERVAL expr unit)`
- `SUBDATE(expr, INTERVAL expr unit)`
- `ADDDATE(expr, days)`
- `SUBDATE(expr, days)`

The supported interval units in this slice are:

- `DAY`
- `WEEK`
- `MONTH`
- `YEAR`
- `QUARTER`
- `HOUR`
- `MINUTE`
- `SECOND`
- `MICROSECOND`

Combined units such as `YEAR_MONTH`, `DAY_SECOND`, `HOUR_MINUTE`, and
`SECOND_MICROSECOND` are deferred in this slice so that the implementation does
not guess interval-string parsing behavior beyond the units tested here.

References used for this independently authored specification:

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- MySQL 8.4 Reference Manual, Temporal Intervals:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html#temporal-intervals>

## MySQL 8.4.9 behavior verified

The following probes were run against the local `mylite-mysql-849` container,
using `mysql:8.4.9`.

```sql
SELECT @@version, @@sql_mode;
SET time_zone = '+00:00';
SET timestamp = 1700000000.987654;

SELECT
  DATE_ADD('2024-02-29', INTERVAL 1 DAY),
  DATE_SUB('2024-03-01', INTERVAL 1 DAY),
  ADDDATE('2024-02-29', INTERVAL 1 DAY),
  SUBDATE('2024-03-01', INTERVAL 1 DAY),
  DATE_ADD('2024-01-31', INTERVAL 1 MONTH),
  DATE_ADD('2024-02-29', INTERVAL 1 YEAR),
  DATE_SUB('2024-03-31', INTERVAL 1 MONTH),
  DATE_ADD('2024-01-01', INTERVAL -1 DAY),
  DATE_SUB('2024-01-01', INTERVAL -1 DAY),
  DATE_ADD('2024-01-01', INTERVAL 1 WEEK),
  DATE_ADD('2024-01-01', INTERVAL 1 HOUR),
  DATE_ADD('2024-01-01 23:59:59', INTERVAL 1 SECOND),
  DATE_ADD('2024-01-01', INTERVAL 1 QUARTER),
  ADDDATE('2024-01-01', INTERVAL 1 QUARTER),
  DATE_ADD('2024-01-01', INTERVAL 1 MICROSECOND),
  SUBDATE('2024-01-01 00:00:00.000001', INTERVAL 2 MICROSECOND),
  ADDDATE('2024-01-01', 1),
  SUBDATE('2024-01-01', 1),
  ADDDATE('2024-01-01', '2x');
```

Verified results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `DATE_ADD('2024-02-29', INTERVAL 1 DAY)` | `2024-03-01` | none |
| `DATE_SUB('2024-03-01', INTERVAL 1 DAY)` | `2024-02-29` | none |
| `ADDDATE('2024-02-29', INTERVAL 1 DAY)` | `2024-03-01` | none |
| `SUBDATE('2024-03-01', INTERVAL 1 DAY)` | `2024-02-29` | none |
| `DATE_ADD('2024-01-31', INTERVAL 1 MONTH)` | `2024-02-29` | none |
| `DATE_ADD('2024-02-29', INTERVAL 1 YEAR)` | `2025-02-28` | none |
| `DATE_SUB('2024-03-31', INTERVAL 1 MONTH)` | `2024-02-29` | none |
| `DATE_ADD('2024-01-01', INTERVAL -1 DAY)` | `2023-12-31` | none |
| `DATE_SUB('2024-01-01', INTERVAL -1 DAY)` | `2024-01-02` | none |
| `DATE_ADD('2024-01-01', INTERVAL 1 WEEK)` | `2024-01-08` | none |
| `DATE_ADD('2024-01-01', INTERVAL 1 HOUR)` | `2024-01-01 01:00:00` | none |
| `DATE_ADD('2024-01-01 23:59:59', INTERVAL 1 SECOND)` | `2024-01-02 00:00:00` | none |
| `DATE_ADD('2024-01-01', INTERVAL 1 QUARTER)` | `2024-04-01` | none |
| `ADDDATE('2024-01-01', INTERVAL 1 QUARTER)` | `2024-04-01` | none |
| `DATE_ADD('2024-01-01', INTERVAL 1 MICROSECOND)` | `2024-01-01 00:00:00.000001` | none |
| `SUBDATE('2024-01-01 00:00:00.000001', INTERVAL 2 MICROSECOND)` | `2023-12-31 23:59:59.999999` | none |
| `ADDDATE('2024-01-01', 1)` | `2024-01-02` | none |
| `SUBDATE('2024-01-01', 1)` | `2023-12-31` | none |
| `ADDDATE('2024-01-01', '2x')` | `2024-01-03` | 1292, truncated incorrect integer |
| `DATE_ADD('2024-02-29 23:59:59.1234', INTERVAL 1 SECOND)` | `2024-03-01 00:00:00.123400` | none |
| `DATE_ADD('2024-02-29 23:59:59.0000005', INTERVAL 0 SECOND)` | `2024-02-29 23:59:59.000001` | none |
| `DATE_ADD('2024-02-29 23:59:59.9999995', INTERVAL 0 SECOND)` | `2024-03-01 00:00:00` | none |

String-like fractional datetime inputs use six fractional digits when the
rounded microsecond value is nonzero, including values read from character
columns. Temporal-column fractional seconds keep the column value's fractional
display precision. Verified `VARCHAR`, `DATETIME(3)`, and `DATETIME(6)` columns
produce `.123400`, `.123` / `.000`, and `.123456` / `.000000` respectively
after second arithmetic.

For string-like operands, `MICROSECOND` interval arithmetic emits a fractional
part only when the resulting microsecond value is nonzero. For typed `DATE`,
`DATETIME`, and `TIMESTAMP` operands, `MICROSECOND` arithmetic returns typed
`DATETIME(6)` metadata and display behavior, including `.000000` when the
result lands on a whole second.

Invalid and NULL behavior:

| Expression | Result | Warnings |
| --- | --- | --- |
| `DATE_ADD(NULL, INTERVAL 1 DAY)` | `NULL` | none |
| `DATE_ADD('2024-01-01', INTERVAL NULL DAY)` | `NULL` | none |
| `DATE_ADD('2006-05-00', INTERVAL 1 DAY)` | `NULL` | 1292, incorrect datetime value |
| `DATE_ADD('bad', INTERVAL 1 DAY)` | `NULL` | 1292, incorrect datetime value |
| `DATE_ADD('2024-01-01', INTERVAL 'bad' DAY)` | `2024-01-01` | 1292, truncated incorrect integer |
| `DATE_ADD(NULL, INTERVAL 'bad' DAY)` | `NULL` | none |
| `DATE_ADD('bad', INTERVAL 'bad' DAY)` | `NULL` | only the bad-date warning |

Syntax probes verified that `DATE_ADD('2024-01-01', 1)`,
`DATE_ADD('2024-01-01')`, `DATE_ADD('2024-01-01', INTERVAL 1 DAY, 3)`, and
`DATE_ADD('2024-01-01', INTERVAL 1 BOGUS)` are rejected by MySQL. MyLite rejects
unsupported non-`INTERVAL` date-arithmetic shapes in the parser for this slice,
including two-argument `DATE_ADD()` and `DATE_SUB()` calls.

Metadata probes with `mysql --column-type-info -vvv` verified:

| Expression | Type | Length | Decimals | Collation | Nullable |
| --- | --- | ---: | ---: | --- | --- |
| `DATE_ADD('2024-02-29', INTERVAL 1 DAY)` after `SET NAMES utf8mb4` | `STRING` | 116 | 31 | `utf8mb4_0900_ai_ci` | yes |
| `DATE_ADD(CURDATE(), INTERVAL 1 DAY)` | `DATE` | 10 | 0 | `binary` | yes |
| `DATE_ADD(CURDATE(), INTERVAL 1 HOUR)` | `DATETIME` | 19 | 0 | `binary` | yes |
| `DATE_ADD(CURDATE(), INTERVAL 1 MICROSECOND)` | `DATETIME` | 26 | 6 | `binary` | yes |
| `ADDDATE(CURDATE(), 1)` | `DATE` | 10 | 0 | `binary` | yes |

Strict DML probes verified that warnings from invalid temporal operands are
promoted by MyLite's existing DML warning-promotion path: an update assigning
`DATE_ADD('bad', INTERVAL 1 DAY)` to a `DATE` column should fail and leave the
row unchanged.

## Semantics

`DATE_ADD()` and `ADDDATE()` add the interval to the first operand.
`DATE_SUB()` and `SUBDATE()` subtract it. Negative interval values invert the
operation in the same way as MySQL; for example, subtracting `INTERVAL -1 DAY`
adds one day.

The two-argument `ADDDATE(expr, days)` and `SUBDATE(expr, days)` overloads are
day arithmetic. The second argument is converted through the same MySQL-style
signed-integer path used by simple interval amounts, so text such as `'2x'`
contributes warning 1292 and uses the truncated value `2`.

The first operand is evaluated first. If it is `NULL`, the result is `NULL` and
the interval expression is not evaluated. If the first operand is non-`NULL`
but cannot be converted to a complete MySQL date or datetime value, the result
is `NULL`, warning 1292 is appended, and the interval expression is not
evaluated.

The interval expression is evaluated only after the first operand has produced
a non-`NULL`, valid temporal value. If the interval expression is `NULL`, the
result is `NULL`. For the supported simple units, MyLite converts the interval
expression to a signed integer using MyLite's existing MySQL-style integer
conversion path. Invalid text therefore contributes warning 1292 and uses zero
as the interval value.

`DAY`, `WEEK`, `MONTH`, `QUARTER`, and `YEAR` preserve a date result when the
first operand has no time part. `HOUR`, `MINUTE`, `SECOND`, and `MICROSECOND`
produce a datetime result even when the first operand is a date. Date/datetime
inputs with an explicit time part return datetime values.

Month and year arithmetic clips the day to the final valid day of the target
month. This covers leap-day and month-end behavior such as January 31 plus one
month and February 29 plus one year.

## Parser and AST design

This slice does not implement general `INTERVAL` expressions or interval
operator arithmetic. It adds only the function-call-specific grammar needed by
the supported functions.

The MyLite AST remains a `MYLITE_SQL_AST_FUNCTION_CALL`. The argument list
contains exactly the evaluatable arguments:

1. the temporal expression
2. the interval amount expression

The interval unit is stored as metadata on the function call node, not as a
child of `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`, so recursive expression
binding for `SELECT`, `UPDATE`, and `DELETE` continues to visit only
evaluable expressions.

For the `ADDDATE(expr, days)` and `SUBDATE(expr, days)` overloads, the parser
stores `DAY` as interval-unit metadata and reuses the same two evaluatable
arguments.

Independently authored Lemon-style grammar shape:

```lemon
scalar_function_call ::= function_name LPAREN expression COMMA INTERVAL expression interval_unit RPAREN.
scalar_function_call ::= ADDDATE LPAREN expression COMMA expression RPAREN.
scalar_function_call ::= SUBDATE LPAREN expression COMMA expression RPAREN.

interval_unit ::= DAY.
interval_unit ::= WEEK.
interval_unit ::= MONTH.
interval_unit ::= YEAR.
interval_unit ::= QUARTER.
interval_unit ::= HOUR.
interval_unit ::= MINUTE.
interval_unit ::= SECOND.
interval_unit ::= MICROSECOND.
```

The parser helper accepts this special form only for `DATE_ADD`, `DATE_SUB`,
`ADDDATE`, and `SUBDATE`.

## Runtime design

The runtime extends the committed `DATE()` / `DATEDIFF()` temporal parsing code
so date arithmetic uses MyLite-owned MySQL semantics rather than SQLite date
helpers.

The runtime steps are:

1. Evaluate the first argument.
2. Return `NULL` immediately for a `NULL` first argument.
3. Convert the first argument through the existing temporal source parser,
   extended to preserve optional time fields.
4. Return `NULL` and append the same temporal warning family used by
   `DATE()`/`DATEDIFF()` when the value is malformed or incomplete.
5. Evaluate the interval amount.
6. Return `NULL` for a `NULL` interval amount.
7. Convert the interval amount to a signed integer.
8. Apply year/month/quarter components first, with month-end clipping.
9. Apply day/week, second-scale time, and microsecond components using MyLite's
   day-number helpers and deterministic carry/borrow normalization.
10. Format `YYYY-MM-DD`, `YYYY-MM-DD HH:MM:SS`, or fractional datetime text
    according to the result shape.

The runtime should keep `NOW()`, `CURDATE()`, and related current temporal
inputs statement-stable by relying on the existing expression evaluation
context. `DATE_ADD(NOW(), INTERVAL 1 SECOND)` must evaluate `NOW()` through
the same callback as any other nested current temporal function.

## Metadata

MyLite should infer result metadata without evaluating expressions:

- known `DATE` first operand plus date-only unit: nullable `DATE`, length 10,
  decimals 0, binary charset/collation, `BINARY` flag
- known `DATE` first operand plus time unit: nullable `DATETIME`, length 19,
  decimals 0, binary charset/collation, `BINARY` flag
- known `DATE` first operand plus `MICROSECOND`: nullable `DATETIME`, length
  26, decimals 6, binary charset/collation, `BINARY` flag
- known `DATETIME` or `TIMESTAMP` first operand: nullable `DATETIME`, length
  19 plus fractional precision if already present on the first operand, or
  length 26 and decimals 6 for `MICROSECOND`, binary charset/collation,
  `BINARY` flag
- unknown/string first operand: nullable connection-character-set `STRING`,
  length 29 characters times connection max bytes per character, decimals 31

The first slice does not implement exact prepared-parameter metadata because
MyLite does not yet expose prepared parameters.

## Tests

Parser tests should cover:

- `DATE_ADD`, `DATE_SUB`, `ADDDATE`, and `SUBDATE` interval form
- `ADDDATE(expr, days)` and `SUBDATE(expr, days)` day-overload form
- supported simple units, including `QUARTER` and `MICROSECOND`
- nested temporal expressions such as `DATE_ADD(CURDATE(), INTERVAL 1 DAY)`
- malformed interval units and unsupported arities/forms

Runtime tests should cover:

- literal date arithmetic for day, week, month, quarter, year, hour, minute,
  second, and microsecond
- alias behavior for the shared interval form
- compact and numeric temporal inputs reused from the `DATE()` / `DATEDIFF()`
  parser
- fractional seconds padding, rounding, carry, string-column padding, and typed
  temporal-column fractional precision
- leap-day and month-end clipping
- negative intervals
- `NULL` propagation and short-circuiting
- invalid dates/times and warning counts
- invalid interval text using warning 1292 and MySQL-style truncated integer
  values
- non-`INTERVAL` day overloads, including negative days, `NULL`, and truncated
  integer conversion warnings
- statement-stable current temporal inputs
- strict DML warning promotion and rollback
- parser rejection for unsupported non-`INTERVAL` `DATE_ADD`/`DATE_SUB` shapes
- result metadata for string, date, and datetime result shapes

## Compatibility status

This feature moves `DATE_ADD()`, `DATE_SUB()`, `ADDDATE()`, and `SUBDATE()` to
partial compatibility for the supported `INTERVAL expr unit` forms, simple
units listed above, and the two-argument `ADDDATE`/`SUBDATE` day overloads.

Remaining gaps:

- combined interval units such as `YEAR_MONTH`, `DAY_SECOND`, and
  `SECOND_MICROSECOND`
- general interval expressions outside the four function calls
- exact parser diagnostic parity for unsupported non-`INTERVAL` shapes
- overflow diagnostics at the edges of MySQL's temporal range

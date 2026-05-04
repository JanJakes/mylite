# Temporal part scalar functions

## Scope

This feature implements the first scalar temporal part extraction surface over
the expression contexts MyLite already executes:

- `YEAR(expr)`
- `MONTH(expr)`
- `DAY(expr)` and `DAYOFMONTH(expr)`
- `HOUR(expr)`
- `MINUTE(expr)`
- `SECOND(expr)`
- `EXTRACT(unit FROM expr)` for `YEAR`, `MONTH`, `DAY`, `HOUR`, `MINUTE`, and
  `SECOND`

The functions are supported in no-table `SELECT`, one-table `SELECT`
projection, `WHERE`, and `ORDER BY`, and supported single-table `UPDATE` and
`DELETE` expression paths.

Out of scope:

- `WEEK`, `YEARWEEK`, `DAYOFYEAR`, `DAYOFWEEK`, `WEEKDAY`, `QUARTER`,
  `MONTHNAME`, `DAYNAME`, `MICROSECOND`, and combined `EXTRACT()` units such as
  `YEAR_MONTH`
- standalone time-only parsing beyond the existing MyLite temporal parser; for
  example `HOUR('10:05:03')` is intentionally deferred even though MySQL
  supports it
- SQL-mode-specific temporal edge behavior beyond MyLite's current default
  strict-mode warning policy
- exact native error-code parity for runtime-validated unsupported arities

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- MySQL 8.4 Reference Manual, Date and Time Literals:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/date-and-datediff-functions/specs.md`
  - `docs/specs/date-add-sub-functions/specs.md`
  - `docs/specs/current-temporal-functions/specs.md`

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

`DAY(expr)` is a synonym for `DAYOFMONTH(expr)`. All supported one-argument
part functions return `NULL` when the argument is `NULL`.

Verified no-table results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `YEAR('2024-02-29 12:34:56')` | `2024` | none |
| `MONTH('2024-02-29 12:34:56')` | `2` | none |
| `DAY('2024-02-29 12:34:56')` | `29` | none |
| `DAYOFMONTH('2024-02-29 12:34:56')` | `29` | none |
| `HOUR('2024-02-29 12:34:56')` | `12` | none |
| `MINUTE('2024-02-29 12:34:56')` | `34` | none |
| `SECOND('2024-02-29 12:34:56')` | `56` | none |
| `EXTRACT(YEAR FROM '2024-02-29 12:34:56')` | `2024` | none |
| `EXTRACT(MONTH FROM '2024-02-29 12:34:56')` | `2` | none |
| `EXTRACT(DAY FROM '2024-02-29 12:34:56')` | `29` | none |
| `EXTRACT(HOUR FROM '2024-02-29 12:34:56')` | `12` | none |
| `EXTRACT(MINUTE FROM '2024-02-29 12:34:56')` | `34` | none |
| `EXTRACT(SECOND FROM '2024-02-29 12:34:56')` | `56` | none |

Temporal string and numeric conversion follows the same relaxed input surface
used by MyLite's current `DATE()`, `DATEDIFF()`, and date-arithmetic slices:
dashed date/datetime strings, compact strings, integer compact temporal values,
approximate numeric values truncated to their integer temporal text, two-digit
year normalization, fractional seconds, and current temporal function outputs.

Observed edge behavior:

| Expression | Result | Warnings |
| --- | --- | --- |
| `YEAR(NULL)` / `EXTRACT(YEAR FROM NULL)` | `NULL` | none |
| `YEAR('2008-00-00')`, `MONTH('2008-00-00')`, `DAY('2008-00-00')` | `2008`, `0`, `0` | none |
| `YEAR('0000-01-00')`, `MONTH('0000-01-00')`, `DAY('0000-01-00')` | `0`, `1`, `0` | none |
| `YEAR('0000-00-00')` | `NULL` | 1292 |
| `YEAR('0000-00-00 12:34:56')`, `HOUR('0000-00-00 12:34:56')` | `0`, `12` | none |
| `YEAR('2023-02-31')` | `NULL` | 1292 |
| `YEAR('bad')` | `NULL` | 1292 |
| `YEAR('2024-02-29foo')` | `2024` | 1292 |
| `HOUR('2024-02-29 12:34:56foo')` | `12` | 1292 |
| `YEAR(240229)`, `MONTH(240229)`, `DAY(240229)` | `2024`, `2`, `29` | none |
| `HOUR(20240229123456)`, `MINUTE(20240229123456)`, `SECOND(20240229123456)` | `12`, `34`, `56` | none |
| `YEAR(20240229.9)`, `EXTRACT(DAY FROM 20240229.9)` | `2024`, `29` | one 1292 warning each |

MySQL supports standalone time strings for time parts, including large `TIME`
hours. That conversion is broader than MyLite's current date/datetime parser
and is deferred for this slice.

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| `YEAR(...)` | `YEAR` | 4 | 0 | binary | `UNSIGNED BINARY NUM` | yes |
| `MONTH(...)` | `LONGLONG` | 3 | 0 | binary | `BINARY NUM` | yes |
| `DAY(...)`, `DAYOFMONTH(...)` | `LONGLONG` | 3 | 0 | binary | `BINARY NUM` | yes |
| `HOUR(...)` | `LONGLONG` | 4 | 0 | binary | `BINARY NUM` | yes |
| `MINUTE(...)`, `SECOND(...)` | `LONGLONG` | 3 | 0 | binary | `BINARY NUM` | yes |
| `EXTRACT(YEAR FROM ...)` | `LONGLONG` | 5 | 0 | binary | `BINARY NUM` | yes |
| `EXTRACT(MONTH|DAY|HOUR|MINUTE|SECOND FROM ...)` | `LONGLONG` | `3` or `4` | 0 | binary | `BINARY NUM` | yes |

`EXTRACT(WEEK FROM ...)` and `EXTRACT(YEAR_MONTH FROM ...)` are valid MySQL
forms, but they are deliberately deferred in MyLite until those units are
specified and tested.

`EXTRACT(unit)` without `FROM expr` is malformed syntax in MySQL 8.4.9, so
MyLite should reject that shape in the parser rather than reporting a runtime
unsupported function.

## MyLite compatibility decisions

MyLite should reuse the existing MyLite-owned temporal parser used by `DATE()`,
`DATEDIFF()`, and date arithmetic, extended only as needed to allow incomplete
date values for date-part extraction. `2008-00-00` and `0000-01-00` therefore
return zero month/day parts. Delimited `0000-00-00` with a valid time suffix
returns the extracted zero date and time parts. Date-only `0000-00-00`, invalid
month values, impossible complete dates, and malformed non-`NULL` inputs return
`NULL` with warning 1292 under the current default strict-mode policy.

`HOUR`, `MINUTE`, and `SECOND` return the parsed time fields when the input is a
date/datetime value with a time part, and return `0` for date-only values
through the current parser. MySQL's standalone time-string conversion remains a
known gap for this feature.

`EXTRACT(unit FROM expr)` should be represented as a function call whose
argument list contains only `expr`. The unit is AST metadata, not an
evaluable child, so binder/evaluator tree walks continue to visit only
expressions that can produce values.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon grammar snippets

These snippets describe the intended MyLite grammar shape; they are not copied
from MySQL grammar.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
scalar_function_call ::= function_name LPAREN expression FROM expression RPAREN.

function_name ::= identifier.

interval_unit ::= YEAR.
interval_unit ::= MONTH.
interval_unit ::= DAY.
interval_unit ::= HOUR.
interval_unit ::= MINUTE.
interval_unit ::= SECOND.
```

`YEAR`, `MONTH`, `DAY`, `HOUR`, `MINUTE`, `SECOND`, and `DAYOFMONTH` remain
ordinary identifier-style function names outside the `EXTRACT` unit position.
`EXTRACT` uses the existing `function(expr FROM expr)` grammar shape; the
parser constructor validates that the function name is `EXTRACT`, converts the
first expression to interval-unit metadata, and leaves only the second
expression as an evaluatable function argument. `EXTRACT` remains usable as a
nonreserved identifier when it is not part of that call shape.

## Runtime semantics

For `YEAR`, `MONTH`, `DAY`, `DAYOFMONTH`, `HOUR`, `MINUTE`, `SECOND`, and
supported `EXTRACT` units:

1. Evaluate the temporal expression.
2. Return `NULL` without warnings when the value is `NULL`.
3. Convert the value through the supported date/datetime temporal parser.
4. Return `NULL` and append warning 1292 for malformed or unsupported
   non-`NULL` temporal values.
5. Return the requested integer part.

`EXTRACT(WEEK FROM ...)` should parse because `WEEK` is already part of the
shared interval-unit grammar, but it must remain unsupported by the scalar
function registry for this slice.

## Tests

Parser tests should cover:

- keyword function-name parsing for `YEAR`, `MONTH`, `DAY`, `HOUR`, `MINUTE`,
  and `SECOND`
- ordinary identifier parsing for `DAYOFMONTH`
- `EXTRACT(YEAR|MONTH|DAY|HOUR|MINUTE|SECOND FROM expr)` with only one
  evaluatable child in the argument list
- nested current temporal and date-arithmetic inputs
- rejected arities, deferred `EXTRACT` units, and malformed `EXTRACT` syntax

Runtime tests should cover:

- no-table scalar results for all supported function names and `EXTRACT`
- `NULL` propagation
- invalid temporal inputs, truncation warnings, and warning counts
- incomplete date part extraction with zero month/day values
- compact string, integer, approximate numeric, fractional datetime, current
  temporal, and nested `DATE_ADD()` inputs
- result metadata for direct part functions and `EXTRACT`
- one-table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignments/predicates/order keys and `DELETE` predicates
- strict DML warning promotion and rollback for invalid temporal inputs
- unsupported units and malformed `EXTRACT` call forms

## Compatibility status

After implementation, the listed temporal part functions have partial
compatibility for the supported scalar expression paths and date/datetime input
surface. Time-only parsing, combined `EXTRACT` units, microseconds, week and
calendar-name functions, exact native diagnostics, and broader SQL-mode
temporal variants remain deferred.

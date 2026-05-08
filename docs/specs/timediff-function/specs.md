# TIMEDIFF function

## Scope

This feature implements `TIMEDIFF(expr1, expr2)` for the scalar expression
contexts MyLite currently executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- exact native error-code parity for unsupported arities
- exact prepared-statement metadata for every `NULL` or unresolved text argument
  shape
- time-zone-sensitive `TIMESTAMP` conversion
- broader permissive temporal literal variants beyond MyLite's current time and
  datetime parsers
- broader SQL-mode variants beyond MyLite's current strict-mode warning policy

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/time-function/specs.md`

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
| `TIMEDIFF('12:00:00','11:30:00')` | `00:30:00` | none |
| `TIMEDIFF('11:30:00','12:00:00')` | `-00:30:00` | none |
| `TIMEDIFF('12:00:00.123456','11:59:59.654321')` | `00:00:00.469135` | none |
| `TIMEDIFF('100:00:00','01:00:00')` | `99:00:00` | none |
| `TIMEDIFF('-12:00:00','01:00:00')` | `-13:00:00` | none |
| `TIMEDIFF('2024-01-02 03:04:05.123456','2024-01-01 01:02:03.654321')` | `26:02:01.469135` | none |
| `TIMEDIFF('2024-01-01 00:00:00','2024-01-02 00:00:00')` | `-24:00:00` | none |
| `TIMEDIFF('2024-01-02','2024-01-01')` | `00:00:00` | two 1292 truncated time warnings |
| `TIMEDIFF('2024-01-02 00:00:00','00:00:00')` | `NULL` | none |
| `TIMEDIFF(NULL,'11:30:00')` | `NULL` | none |
| `TIMEDIFF('bad','00:00:00')` | `NULL` | 1292 truncated time |
| `TIMEDIFF('839:00:00','00:00:00')` | `838:59:59` | 1292 truncated time |

Typed temporal values differ from untyped strings:

| Source expression | Result | Warnings |
| --- | --- | --- |
| `TIMEDIFF(date_col_2, date_col_1)` for `DATE` values one day apart | `24:00:00` | none |
| `TIMEDIFF(time_col_2, time_col_1)` for `TIME(6)` values | fractional `TIME` difference | none |
| `TIMEDIFF(datetime_col_2, datetime_col_1)` for `DATETIME(6)` values | fractional elapsed `TIME` difference | none |
| `TIMEDIFF(datetime_col, time_col)` | `NULL` | none |
| `TIMEDIFF(date_col, datetime_col)` | `NULL` | none |
| `TIMEDIFF(date_col, time_col)` | `NULL` | none |

Observed overflow behavior:

| Expression | Result | Warnings |
| --- | --- | --- |
| `TIMEDIFF('838:00:00','-838:00:00')` | `838:59:59` | 1292 truncated time for the result |
| `TIMEDIFF('2024-02-10 00:00:00.123456','2024-01-01 00:00:00')` | `838:59:59.000000` | 1292 truncated time for the result |
| `TIMEDIFF('-839:00:00.1','00:00:00')` | `-838:59:59.0` | 1292 truncated time |

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| whole-second arguments | `TIME` | 10 | 0 | binary | `BINARY` | yes |
| fractional arguments with max scale `N` | `TIME` | `11 + N` | `N` | binary | `BINARY` | yes |
| table-backed `TIME(6)` with `TIME` or `DATETIME(6)` with `DATETIME(3)` | `TIME` | 17 | 6 | binary | `BINARY` | yes |
| table-backed `DATE` operands | `TIME` | 10 | 0 | binary | `BINARY` | yes |
| unresolved textual precision | `TIME` | 17 | 6 | binary | `BINARY` | yes |

## MyLite Compatibility Decisions

`TIMEDIFF()` evaluates both arguments, returns `NULL` when either argument is
`NULL`, and accepts exactly two arguments. Each argument is classified as either
a time operand or a datetime operand. The subtraction is valid only when both
operands have the same class after conversion.

Typed temporal values use their declared type:

- `TIME` values are time operands.
- `DATE`, `DATETIME`, and `TIMESTAMP` values are datetime operands. A typed
  `DATE` contributes midnight for the time-of-day component.

Untyped text and numeric values use MyLite's existing temporal coercion rules.
Untyped values that parse as datetimes and include an explicit time part become
datetime operands. Other untyped values fall back to the time parser, which
matches MySQL's warning behavior for date-shaped strings such as
`'2024-01-02'`.

The result is a signed `TIME` value. Fractional precision is the maximum
fractional precision of the successfully converted operands. Results outside the
MySQL `TIME` range `-838:59:59` through `838:59:59` are clipped and emit warning
1292 with time-value wording. Fractional overflow clips to
`838:59:59.000000`, matching observed MySQL behavior.

The first metadata slice reports exact precision for constant non-`NULL`
results and typed descriptors. When the result is `NULL` or text precision
cannot be resolved statically, MyLite reports conservative `TIME(6)` metadata.
Exact prepared-statement metadata for every unresolved expression shape remains
deferred.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

`TIMEDIFF()` uses the ordinary scalar function call shape.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression COMMA expression RPAREN.

function_name ::= identifier.
```

`TIMEDIFF` remains an ordinary nonreserved function name and may be used as an
identifier when it is not called.

## Runtime Semantics

1. Evaluate `expr1`, then `expr2`, preserving conversion warnings in evaluation
   order.
2. Return `NULL` without additional warnings when either expression is `NULL`.
3. Convert each non-`NULL` value to a typed time or datetime operand.
4. Return `NULL` when either conversion fails or when the two operand classes do
   not match.
5. For time operands, subtract signed total microseconds.
6. For datetime operands, subtract day-number plus time-of-day microseconds.
7. Clip out-of-range results to MySQL's maximum `TIME` magnitude and emit
   warning 1292 for the result value.
8. Format the signed `TIME` result with the selected fractional precision.

## Tests

Parser tests should cover:

- ordinary two-argument function-call parsing
- use of `timediff` as an identifier when not called
- rejected unsupported arities

Runtime tests should cover:

- result metadata for whole-second, fractional, and `NULL` expressions
- table-backed fractional `TIME` and `DATETIME` result metadata
- positive, negative, over-24-hour, fractional, and `NULL` time inputs
- datetime differences across dates and negative datetime differences
- typed `DATE`, `TIME(6)`, and `DATETIME(6)` column behavior
- mixed time/datetime operands returning `NULL`
- date-shaped untyped strings, invalid strings, input clipping, and result
  clipping with warning 1292
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment, `DELETE` predicate, and strict DML warning promotion

## Compatibility Status

After implementation, `TIMEDIFF()` has partial compatibility for supported
scalar expression paths and MyLite's current temporal coercion surface. Exact
native diagnostics, time-zone-sensitive `TIMESTAMP` behavior, exact unresolved
prepared metadata, broader temporal literal variants, and protocol metadata
remain deferred.

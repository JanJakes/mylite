# TIME_TO_SEC function

## Scope

This feature implements `TIME_TO_SEC(expr)` for the scalar expression contexts
MyLite currently executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- exact native error-code parity for unsupported arities
- broader SQL-mode variants beyond MyLite's current strict-mode warning policy
- temporal literal forms not already handled by MyLite's time parser

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/time-function/specs.md`
  - `docs/specs/scalar-built-in-functions/specs.md`

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

Verified results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `TIME_TO_SEC('22:23:00')` | `80580` | none |
| `TIME_TO_SEC('00:39:38')` | `2378` | none |
| `TIME_TO_SEC('-12:34:56')` | `-45296` | none |
| `TIME_TO_SEC('838:59:59')` | `3020399` | none |
| `TIME_TO_SEC('-838:59:59')` | `-3020399` | none |
| `TIME_TO_SEC(NULL)` | `NULL` | none |
| `TIME_TO_SEC('12:34:56.123456')` | `45296` | none |
| `TIME_TO_SEC('12:34:56.9999995')` | `45297` | none |
| `TIME_TO_SEC('2024-02-29 12:34:56')` | `45296` | none |
| `TIME_TO_SEC('20240229123456')` | `45296` | none |
| `TIME_TO_SEC(20240229123456)` | `45296` | none |
| `TIME_TO_SEC('2024-02-29')` | `1224` | 1292 truncated time |
| `TIME_TO_SEC('20240229')` | `3020399` | 1292 truncated time |
| `TIME_TO_SEC('bad')` | `NULL` | 1292 truncated time |
| `TIME_TO_SEC('')` | `NULL` | 1292 truncated time |
| `TIME_TO_SEC('839:00:00')` | `3020399` | 1292 truncated time |
| `TIME_TO_SEC('-839:00:00')` | `-3020399` | 1292 truncated time |
| `TIME_TO_SEC(1234567)` | `NULL` | 1292 truncated time |
| `TIME_TO_SEC(123456)` | `45296` | none |
| `TIME_TO_SEC(1234)` | `754` | none |
| `TIME_TO_SEC(12)` | `12` | none |

Observed metadata:

| Expression family | Field type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | ---: | ---: | --- | --- | --- |
| `TIME_TO_SEC(...)` | `LONGLONG` | 10 | 0 | binary | `BINARY NUM` | yes |

## MyLite Compatibility Decisions

`TIME_TO_SEC()` should reuse MyLite's `TIME()` coercion path so typed temporal
values, datetime strings, compact numeric temporal values, fractional rounding,
range clipping, and warning text stay aligned with the existing time function.
The returned value is a signed integer number of whole seconds:

```text
hours * 3600 + minutes * 60 + seconds
```

The sign is applied after the absolute time components are converted. Fractional
seconds affect the result only when parsing rounds them into the next second.
The returned metadata is nullable signed `LONGLONG` with MySQL's observed display
length `10`.

The feature does not change the `.mylite` file format, schema catalog, or
SQLite storage layout.

## MyLite Lemon Grammar Snippets

`TIME_TO_SEC()` uses the ordinary scalar function call shape.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN expression RPAREN.

function_name ::= identifier.
```

`TIME_TO_SEC` remains an ordinary nonreserved function name and may be used as
an identifier when it is not called.

## Runtime Semantics

1. Evaluate the argument expression.
2. Return `NULL` without warnings when the argument is `NULL`.
3. Convert the value through the same time coercion path as `TIME()`.
4. Return `NULL` and preserve parser warnings when conversion fails.
5. Convert the parsed time to whole seconds and apply the sign.
6. Return a signed integer.

## Tests

Parser tests should cover:

- ordinary one-argument function-call parsing
- use of `time_to_sec` as an identifier when not called

Runtime tests should cover:

- result metadata for non-`NULL` and `NULL` expressions
- positive, negative, min, max, and `NULL` inputs
- fractional rounding into the next second
- datetime text, compact datetime text, and compact numeric datetime inputs
- invalid, truncated, clipped, and malformed values with warning 1292
- compact integer and approximate numeric inputs
- table projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignment, `DELETE` predicate, and strict DML warning promotion
- rejected unsupported arities

## Compatibility Status

After implementation, `TIME_TO_SEC()` has partial compatibility for supported
scalar expression paths and MyLite's current time input surface. Exact native
diagnostics, full SQL-mode variants, broader temporal conversion, and protocol
metadata remain deferred.

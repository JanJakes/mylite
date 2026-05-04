# TIME() Function

## Scope

This slice implements MySQL 8.4.9-compatible `TIME(expr)` for the scalar
expression paths MyLite already executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

The function extracts or coerces one expression to a `TIME` value. It returns
`NULL` for `NULL` input and for invalid uncoercible time input.

Out of scope for this slice:

- prepared-statement parameter metadata
- complete MySQL warning catalog coverage for deprecated datetime delimiters
- time-zone-sensitive `TIMESTAMP` conversion
- every permissive temporal delimiter variant accepted by the server
- unsupported insert-expression, generated-column, view, stored-program, and
  protocol contexts

## Sources

- MySQL 8.4 Reference Manual, Date and Time Functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4.9 runtime probes against Docker container `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

Runtime probes used MySQL 8.4.9 with the default strict SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

Observed scalar results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `TIME(NULL)` | `NULL` | none |
| `TIME('12:34:56')` | `12:34:56` | none |
| `TIME('12:34:56.123456')` | `12:34:56.123456` | none |
| `TIME('2024-02-29 12:34:56.123456')` | `12:34:56.123456` | none |
| `TIME('20240229123456')` | `12:34:56` | none |
| `TIME(20240229123456)` | `12:34:56` | none |
| `TIME('20240229123456.789')` | `12:34:56.789` | none |
| `TIME(20240229123456.789)` | `12:34:56.789` | none |
| `TIME(123456)` | `12:34:56` | none |
| `TIME(1234)` | `00:12:34` | none |
| `TIME(12)` | `00:00:12` | none |
| `TIME(123456.789)` | `12:34:56.789` | none |
| `TIME(123456E0)` | `12:34:56.000000` | none |
| `TIME(12E0)` | `00:00:12.000000` | none |
| `TIME(20240229123456E0)` | `12:34:56.000000` | none |
| `TIME('1:2:3')` | `01:02:03` | none |
| `TIME('25:00:00')` | `25:00:00` | none |
| `TIME('-12:34:56')` | `-12:34:56` | none |
| `TIME('2024-02-29')` | `00:20:24` | 1292 `Truncated incorrect time value: '2024-02-29'` |
| `TIME('2024-02-29 12:34:56x')` | `12:34:56` | 1292 `Truncated incorrect time value: '2024-02-29 12:34:56x'` |
| `TIME('bad')` | `NULL` | 1292 `Truncated incorrect time value: 'bad'` |
| `TIME('')` | `NULL` | 1292 `Truncated incorrect time value: ''` |
| `TIME('20240229')` | `838:59:59` | 1292 `Truncated incorrect time value: '20240229'` |
| `TIME('123456789')` | `NULL` | 1292 `Truncated incorrect time value: '123456789'` |
| `TIME('202402291234')` | `NULL` | 1292 `Truncated incorrect time value: '202402291234'` |
| `TIME('839:00:00')` | `838:59:59` | 1292 `Truncated incorrect time value: '839:00:00'` |
| `TIME('-839:00:00')` | `-838:59:59` | 1292 `Truncated incorrect time value: '-839:00:00'` |
| `TIME(1234567)` | `NULL` | 1292 `Truncated incorrect time value: '1234567'` |
| `TIME(1e20)` | `NULL` | 1292 `Truncated incorrect time value: '1e20'` |
| `TIME('12:34:')` | `12:34:00` | 1292 `Truncated incorrect time value: '12:34:'` |
| `TIME('12:34:56.1234567')` | `12:34:56.123457` | none |
| `TIME('12:34:56.9999995')` | `12:34:57.000000` | none |
| `TIME('838:59:59.9999999')` | `838:59:59.000000` | 1292 `Truncated incorrect time value: '838:59:59.9999999'` |

Typed temporal values differ from untyped strings:

| Source expression | Result | Warnings |
| --- | --- | --- |
| `TIME(date_column)` for `DATE '2024-02-29'` | `00:00:00` | none |
| `TIME(datetime_column)` for `DATETIME(6)` | time part with column precision | none |
| `TIME(time_column)` for `TIME(6)` | same time with column precision | none |
| `TIME(varchar_column)` for text `2024-02-29` | coerced text time | 1292 warning |

Native arity diagnostics:

- `TIME()` -> parser syntax error
- `TIME(1,2)` -> parser syntax error

MyLite currently exposes unsupported arity as parse/prepare failure for scalar
functions. This slice keeps that project-local behavior and tests exact
one-argument acceptance.

Observed metadata:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `TIME('12:34:56') AS time_value` | `TIME` | `10` | `0` | `binary` | `BINARY` |
| `TIME('12:34:56.123456') AS time_fraction` | `TIME` | `17` | `6` | `binary` | `BINARY` |
| `TIME(NULL) AS time_null` | `TIME` | `10` | `0` | `binary` | `BINARY` |
| `TIME('bad') AS time_bad` | `TIME` | `17` | `6` | `binary` | `BINARY` |
| `TIME(123456.789) AS numeric_fraction` | `TIME` | `14` | `3` | `binary` | `BINARY` |
| `TIME(123456.789E0) AS approx_fraction` | `TIME` | `17` | `6` | `binary` | `BINARY` |
| `TIME(CAST(123456.789 AS DECIMAL(9,3)))` | `TIME` | `14` | `3` | `binary` | `BINARY` |
| `TIME(time_col_3)` | `TIME` | `14` | `3` | `binary` | `BINARY` |
| `TIME(datetime_col_3)` | `TIME` | `14` | `3` | `binary` | `BINARY` |
| `TIME(varchar_col)` | `TIME` | `17` | `6` | `binary` | `BINARY` |

## Syntax

`TIME` is parsed as a scalar function name with exactly one argument:

```lemon
nonreserved_identifier_keyword ::= TIME.
function_name ::= identifier.
function_call ::= function_name LPAREN expression RPAREN.
```

The grammar must still allow `TIME` and `TIME(fsp)` temporal type syntax in
column definitions. Temporal cast target syntax, such as `CAST(expr AS TIME)`,
remains governed by the separate CAST expression surface and is not changed by
this slice.

## Runtime Semantics

Evaluation order:

1. Evaluate the single argument.
2. If the argument is `NULL`, return `NULL` without a warning.
3. If the argument is a typed `DATE`, return `00:00:00`.
4. If the argument is a typed `DATETIME` or `TIMESTAMP`, parse it as a datetime
   and return the time part.
5. If the argument is a typed `TIME`, normalize and return the time value.
6. Otherwise coerce text or numeric input as a MySQL time literal:
   - `HH:MM[:SS[.fraction]]` accepts one- or two-digit minute/second fields;
   - digit strings up to six digits are interpreted as right-aligned
     `HHMMSS`;
   - untyped eight-digit date-shaped strings follow MySQL's time coercion path
     and can clip with warning 1292;
   - valid twelve- and fourteen-digit compact datetimes return their
     `HH:MM:SS` part, while malformed compact-datetime-shaped inputs return
     `NULL` with warning 1292;
   - exact decimal numeric values preserve their literal or declared scale up
     to microseconds;
   - approximate numeric values use microsecond display precision, including
     whole-second values such as `123456E0`;
   - invalid text returns `NULL` with warning 1292;
   - out-of-range time values are clipped to `838:59:59` or `-838:59:59` with
     warning 1292.
7. Fractional seconds round to microsecond precision. Rounding that overflows
   seconds carries into the next second. Rounding beyond the supported time
   range clips and warns.

Warning text uses MySQL error code 1292 with message form:

```text
Truncated incorrect time value: '<input>'
```

## Metadata

`TIME()` result metadata is binary `TIME` metadata and remains nullable because
invalid coercions can produce `NULL`.

| Property | Value |
| --- | --- |
| type | `TIME` |
| display length | `10` for no fractional precision, otherwise `11 + fsp` |
| decimals | argument temporal precision, literal/result precision, or `6` for non-temporal text columns |
| charset | binary (`63`) |
| flags | `BINARY` |
| nullability | nullable |

The evaluator carries the temporal source type alongside text values so typed
`DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` inputs are not confused with plain
strings.

## Warnings And DML Promotion

Scalar `SELECT` keeps time-conversion warnings in the statement warning list.

In supported strict `UPDATE` and `DELETE` expression paths, existing MyLite
warning promotion turns invalid `TIME()` conversion warnings into execution
errors and rolls back the statement. Ordinary typed temporal extraction remains
usable in table predicates, ordering, assignments, and deletion predicates.

## Test Plan

Parser tests:

- `TIME('12:34:56')` parses as a one-argument scalar function call.
- Nested arguments parse.
- `time` remains usable as an uncalled identifier where identifiers are valid.
- zero-argument and two-argument calls fail deterministically.

Runtime tests:

- metadata for plain, fractional, `NULL`, and invalid literal calls.
- scalar extraction from time strings, datetime strings, compact datetime
  strings/numbers, short numeric time forms, exact decimal and approximate
  numeric values, negative time, and over-24-hour values.
- fractional rounding and clipping at the time range boundary.
- invalid text, empty text, malformed partial time, out-of-range values, and
  warning message order.
- typed `DATE`, `DATETIME(6)`, and `TIME(6)` column behavior.
- table projection, `WHERE`, and `ORDER BY`.
- supported `UPDATE` assignment and predicate use.
- supported `DELETE` predicate use.
- strict `UPDATE` and `DELETE` warning promotion and rollback for invalid text.

## Compatibility Notes

This slice intentionally accepts a focused set of MySQL's permissive temporal
input grammar. Broader delimiter deprecation warnings, time-zone-sensitive
`TIMESTAMP` conversion, and unsupported expression contexts remain deferred.

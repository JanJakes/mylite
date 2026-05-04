# FROM_DAYS() Function

## Scope

This slice implements MySQL 8.4.9-compatible `FROM_DAYS(expr)` for the scalar
expression paths MyLite already executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

The function converts one day number to a date string. It is the practical
inverse of `TO_DAYS()` for supported dates after MySQL's year-zero boundary.
`NULL` input returns `NULL`.

Out of scope for this slice:

- extending the expression value model to represent MySQL's overflow
  displayed-null-but-not-`IS NULL` date sentinel
- exact date-comparison diagnostics for zero dates in every predicate context
- native MySQL 1582 Gregorian-calendar caveat beyond matching MySQL's numeric
  proleptic calculation for supported inputs
- stored-program, prepared-statement, view, generated-column, and insert
  expression paths outside the currently executable scalar surface

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
| `FROM_DAYS(NULL)` | `NULL` | none |
| `FROM_DAYS(0)` | `0000-00-00` | none |
| `FROM_DAYS(1)` | `0000-00-00` | none |
| `FROM_DAYS(365)` | `0000-00-00` | none |
| `FROM_DAYS(366)` | `0001-01-01` | none |
| `FROM_DAYS(739310)` | `2024-02-29` | none |
| `FROM_DAYS(3652424)` | `9999-12-31` | none |
| `FROM_DAYS(3652425)` | `NULL` | 1441 `Datetime function: from_days field overflow` |
| `FROM_DAYS(-1)` | `0000-00-00` | none |
| `FROM_DAYS('739310')` | `2024-02-29` | none |
| `FROM_DAYS(739310.4)` | `2024-02-29` | none |
| `FROM_DAYS(739310.5)` | `2024-03-01` | none |
| `FROM_DAYS(739310.9)` | `2024-03-01` | none |
| `FROM_DAYS('739310.9')` | `2024-02-29` | 1292 `Truncated incorrect INTEGER value: '739310.9'` |
| `FROM_DAYS('bad')` | `0000-00-00` | 1292 `Truncated incorrect INTEGER value: 'bad'` |
| `FROM_DAYS('739310x')` | `2024-02-29` | 1292 `Truncated incorrect INTEGER value: '739310x'` |
| `FROM_DAYS('')` | `0000-00-00` | 1292 `Truncated incorrect INTEGER value: ''` |

Native arity diagnostics:

- `FROM_DAYS()` -> 1582 `Incorrect parameter count in the call to native function 'FROM_DAYS'`
- `FROM_DAYS(1,2)` -> 1582 with the same message form

MyLite currently exposes unsupported arity as parse/prepare failure for scalar
functions. This slice keeps that project-local behavior and tests exact
one-argument acceptance.

Observed metadata:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `FROM_DAYS(739310) AS from_days_value` | `DATE` | `10` | `0` | `binary` | `NOT_NULL BINARY` |
| `FROM_DAYS(NULL) AS from_days_null` | `DATE` | `10` | `0` | `binary` | `BINARY` |
| `FROM_DAYS(3652425) AS from_days_overflow` | `DATE` | `10` | `0` | `binary` | `NOT_NULL BINARY` |

`FROM_DAYS(3652425)` displays `NULL`, but `FROM_DAYS(3652425) IS NULL`
returns `0` in MySQL. MyLite's current expression value model has ordinary
`NULL` and text values only, so this slice documents the sentinel distinction as
deferred and returns ordinary `NULL` for overflow.

## Syntax

`FROM_DAYS` is parsed as an ordinary scalar function name with exactly one
argument:

```lemon
function_call ::= identifier LPAREN expression RPAREN.
```

The function name remains nonreserved. An uncalled identifier named
`from_days` must continue to parse as an identifier in table-backed expressions
where an identifier is valid.

## Runtime Semantics

Evaluation order:

1. Evaluate the single argument.
2. If the argument is `NULL`, return `NULL` without a warning.
3. Convert the argument to a signed integer day number:
   - integer values are used directly;
   - approximate numeric values are rounded to the nearest integer with halves
     away from zero;
   - string values use MySQL integer parsing and append warning 1292 when the
     text is empty, nonnumeric, fractional, or has trailing non-space garbage.
4. If the converted day number is `365` or lower, return `0000-00-00`.
5. Otherwise convert `day_number - 1` through MyLite's zero-based
   `temporal_date_from_day_number()` helper and return the formatted date.
6. If the day number exceeds the supported range, append warning 1441 and
   return `NULL`.

The `day_number - 1` adjustment mirrors `TO_DAYS()`:

- `TO_DAYS('0001-01-01')` -> `366`, so `FROM_DAYS(366)` -> `0001-01-01`
- `TO_DAYS('2024-02-29')` -> `739310`, so `FROM_DAYS(739310)` -> `2024-02-29`
- `TO_DAYS('9999-12-31')` -> `3652424`, so `FROM_DAYS(3652424)` -> `9999-12-31`

## Metadata

`FROM_DAYS()` result metadata is date metadata:

| Property | Value |
| --- | --- |
| type | `DATE` |
| display length | `10` |
| decimals | `0` |
| charset | binary (`63`) |
| flags | `BINARY`, plus `NOT_NULL` when the argument is known nonnullable |
| nullability | follows argument nullability |

The overflow sentinel remains marked `NOT_NULL` in MySQL for nonnullable
arguments even though it displays as `NULL`. MyLite keeps metadata based on
argument nullability for this slice.

## Warnings And DML Promotion

Scalar `SELECT` keeps integer-conversion and overflow warnings in the statement
warning list.

In supported strict `UPDATE` and `DELETE` expression paths, existing MyLite
warning promotion turns `FROM_DAYS('bad')` conversion warning 1292 into an
execution error and rolls back the statement. Supported strict `UPDATE`
assignment paths also promote `FROM_DAYS(3652425)` overflow warning 1441.
MySQL also rejects assigning the `0000-00-00` result to a strict `DATE` column;
MyLite's warning-promotion surface catches the root conversion warning for the
covered cases.

`DELETE` predicates are covered for ordinary supported `FROM_DAYS()` values. A
MySQL-specific overflow predicate such as `FROM_DAYS(3652425) IS NULL` depends
on the deferred displayed-null sentinel and is not claimed in this slice.

## Test Plan

Parser tests:

- `FROM_DAYS(739310)` parses as a one-argument scalar function call.
- Nested arguments parse.
- `from_days` remains a nonreserved identifier when used without a call.
- zero-argument and two-argument calls fail deterministically.

Runtime tests:

- metadata for `FROM_DAYS(739310)` and `FROM_DAYS(NULL)`.
- valid day-number conversion for `366`, leap day, and maximum date.
- zero, low positive, and negative day numbers return `0000-00-00`.
- overflow returns `NULL` with warning 1441.
- integer, approximate numeric rounding, string, fractional string, empty text,
  and trailing-garbage text conversion.
- `NULL` propagation without warning.
- nested call with `TO_DAYS()`.
- table projection, `WHERE`, and `ORDER BY`.
- supported `UPDATE` assignment and predicate use.
- supported `DELETE` predicate use.
- strict `UPDATE` warning promotion and rollback for invalid text and overflow
  inputs.
- strict `DELETE` warning promotion and rollback for invalid text input.

## Compatibility Notes

This slice does not broaden date comparison semantics, zero-date strict-mode
diagnostics, native MySQL error-code exposure for arity, or unsupported
statement contexts. The displayed-null non-`IS NULL` overflow sentinel is
explicitly deferred until MyLite has a richer temporal value representation.

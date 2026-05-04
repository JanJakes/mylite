# TO_SECONDS() Function

## Scope

This slice implements MySQL 8.4.9-compatible `TO_SECONDS(expr)` for the scalar
expression paths MyLite already executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

The function converts one date or datetime-like expression to MySQL's seconds
count since year zero. Valid datetime inputs include the hour, minute, and
second fields. Fractional seconds are ignored after MySQL-compatible temporal
normalization, so overlong fractional input can still round into the next
second before the integer result is produced. `NULL` input returns `NULL`.

Out of scope for this slice:

- `TIME_TO_SEC()` and `SEC_TO_TIME()`
- relaxed-delimiter warning parity beyond MyLite's existing temporal parser
- time-zone-sensitive `TIMESTAMP` column conversion
- stored-program, prepared-statement, view, generated-column, and insert
  expression paths outside the currently executable scalar surface
- native MySQL 1582 Gregorian-calendar caveat beyond matching MySQL's numeric
  proleptic calculation for supported inputs

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
| --- | ---: | --- |
| `TO_SECONDS('0000-00-00')` | `NULL` | 1292 `Incorrect datetime value: '0000-00-00'` |
| `TO_SECONDS('0000-01-01')` | `86400` | none |
| `TO_SECONDS('0001-01-01')` | `31622400` | none |
| `TO_SECONDS('2006-05-00')` | `NULL` | 1292 `Incorrect datetime value: '2006-05-00'` |
| `TO_SECONDS('0000-02-29')` | `NULL` | 1292 `Incorrect datetime value: '0000-02-29'` |
| `TO_SECONDS('2024-02-29')` | `63876384000` | none |
| `TO_SECONDS('2024-02-29 23:59:59')` | `63876470399` | none |
| `TO_SECONDS('2024-02-29 00:00:00.999999')` | `63876384000` | none |
| `TO_SECONDS('2024-02-29 23:59:59.9999999')` | `63876470400` | none |
| `TO_SECONDS('9999-12-31 23:59:59')` | `315569519999` | none |
| `TO_SECONDS('9999-12-31 23:59:59.9999999')` | `NULL` | 1441 `Datetime function: datetime field overflow`; 1292 `Truncated incorrect datetime value: '9999-12-31 23:59:59.9999999'` |
| `TO_SECONDS(NULL)` | `NULL` | none |
| `TO_SECONDS('bad')` | `NULL` | 1292 `Incorrect datetime value: 'bad'` |
| `TO_SECONDS(20240229)` | `63876384000` | none |
| `TO_SECONDS(20240229235959)` | `63876470399` | none |
| `TO_SECONDS(20240229.9)` | `63876384000` | 1292 `Truncated incorrect date value: '20240229.9'` |
| `TO_SECONDS('950501')` | `62966505600` | none |
| `TO_SECONDS('20091129134332')` | `63426721412` | none |
| `TO_SECONDS(DATE_ADD('2024-02-28 23:59:59', INTERVAL 1 SECOND))` | `63876384000` | none |

`TO_SECONDS(NOW())` returns the current statement's datetime seconds with no
warning. MyLite tests must compare it to same-statement temporal expressions
rather than hard-coding the current wall clock.

Invalid argument counts are native function errors in MySQL:

- `TO_SECONDS()` -> 1582 `Incorrect parameter count in the call to native function 'TO_SECONDS'`
- `TO_SECONDS('2024-01-01','x')` -> 1582 with the same message form

MyLite currently exposes unsupported arity as parse/prepare failure for scalar
functions. This slice keeps that project-local behavior and tests exact
one-argument acceptance.

## Syntax

`TO_SECONDS` is parsed as an ordinary scalar function name with exactly one
argument:

```lemon
function_call ::= identifier LPAREN expression RPAREN.
```

The function name remains nonreserved. An uncalled identifier named
`to_seconds` must continue to parse as an identifier in table-backed
expressions where an identifier is valid.

## Runtime Semantics

Evaluation order:

1. Evaluate the single argument.
2. If the argument is `NULL`, return `NULL` without a warning.
3. Convert the argument using the same strict temporal conversion rules used by
   `TO_DAYS()`, `DATE()`, `DATEDIFF()`, and `TIMESTAMPDIFF()`.
4. If conversion fails, return `NULL` and append warning 1292.
5. If conversion succeeds, return
   `(temporal_day_number(date) + 1) * 86400 + hour * 3600 + minute * 60 + second`.

The `+ 1` day adjustment is required. MyLite's internal `temporal_day_number()`
is zero-based, while MySQL's exposed seconds count starts after the first
year-zero day:

- internal `0000-01-01` -> `0`, `TO_SECONDS()` -> `86400`
- internal `0001-01-01` -> `365`, `TO_SECONDS()` -> `31622400`
- internal `2024-02-29 23:59:59` -> `(739309 + 1) * 86400 + 86399`

Fractional seconds are accepted with valid datetime input. Fractions within
MySQL's stored six-digit precision do not change the integer second count.
Overlong fractional datetime text follows MyLite's shared MySQL-compatible
temporal normalization and can round into the next second before `TO_SECONDS()`
computes the result. If that rounding would overflow MySQL's supported
datetime range, the scalar result is `NULL` and warnings 1441 and 1292 are
emitted. Fractional numeric inputs are converted through the existing
approximate-number temporal conversion path: the temporal value can still be
accepted after truncation, and warning 1292 uses `Truncated incorrect date
value`.

Zero and incomplete dates are strict for `TO_SECONDS()` in this slice:

- all-zero date (`0000-00-00`) returns `NULL` with warning 1292
- zero day or zero month inputs return `NULL` with warning 1292
- impossible year-zero leap day returns `NULL` with warning 1292

## Metadata

`TO_SECONDS()` result metadata is nullable signed integer metadata:

| Property | Value |
| --- | --- |
| type | `LONGLONG` |
| display length | `21` |
| decimals | `0` |
| charset | binary (`63`) |
| flags | `BINARY NUM` |
| nullability | nullable |

The metadata is nullable even when the argument is a non-`NULL` literal because
invalid temporal conversion can produce `NULL`.

## Warnings And DML Promotion

Scalar `SELECT` keeps invalid temporal conversion as a warning and returns
`NULL`.

In supported strict `UPDATE` and `DELETE` expression paths, existing MyLite
warning promotion turns warning 1292 into an execution error and rolls back the
statement. The row set must remain unchanged for:

```sql
UPDATE t SET n = TO_SECONDS('bad') WHERE id = 1;
DELETE FROM t WHERE TO_SECONDS('bad') IS NULL;
```

## Test Plan

Parser tests:

- `TO_SECONDS('2024-02-29')` parses as a one-argument scalar function call.
- Nested/current temporal arguments parse.
- `to_seconds` remains a nonreserved identifier when used without a call.
- zero-argument and two-argument calls fail deterministically.

Runtime tests:

- metadata for `TO_SECONDS('2024-02-29')` and `TO_SECONDS(NULL)`.
- valid date and datetime inputs, including time-of-day inclusion,
  fractional-second truncation, and overlong fractional-second rounding.
- year-zero and boundary values: `0000-01-01`, `0001-01-01`,
  `9999-12-31 23:59:59`.
- compact string, integer numeric date, and integer numeric datetime inputs.
- fractional numeric truncation with warning 1292.
- `NULL` propagation without warning.
- invalid text, all-zero date, zero day, and impossible year-zero leap day
  warnings.
- maximum datetime overlong fractional overflow warnings.
- nested temporal call using `DATE_ADD`.
- stable current temporal call using same-statement relational comparison.
- table projection, `WHERE`, and `ORDER BY`.
- supported `UPDATE` assignment and predicate use.
- supported `DELETE` predicate use.
- strict DML warning promotion and rollback for invalid temporal input.

## Compatibility Notes

This slice shares MyLite's current temporal parser limits. It does not broaden
relaxed delimiter handling, time-zone offsets, native MySQL error-code exposure
for arity, or unsupported statement contexts. Those gaps remain documented in
the scalar built-in function family until the shared temporal conversion and
function diagnostics layers are expanded.

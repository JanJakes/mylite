# TO_DAYS() Function

## Scope

This slice implements MySQL 8.4.9-compatible `TO_DAYS(expr)` for the scalar
expression paths that MyLite already executes:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

The function converts one date or datetime-like expression to the MySQL day
number used by `TO_DAYS()`: a one-based day count from year zero. The time part
is ignored. `NULL` input returns `NULL`.

Out of scope for this slice:

- `TO_SECONDS()`
- `FROM_DAYS()`
- exact warning ordering for expression forms not yet supported by MyLite
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
| `TO_DAYS('0000-00-00')` | `NULL` | 1292 `Incorrect datetime value: '0000-00-00'` |
| `TO_DAYS('0000-01-01')` | `1` | none |
| `TO_DAYS('0001-01-01')` | `366` | none |
| `TO_DAYS('2006-05-00')` | `NULL` | 1292 `Incorrect datetime value: '2006-05-00'` |
| `TO_DAYS('0000-02-29')` | `NULL` | 1292 `Incorrect datetime value: '0000-02-29'` |
| `TO_DAYS('2024-02-29')` | `739310` | none |
| `TO_DAYS('2024-02-29 23:59:59')` | `739310` | none |
| `TO_DAYS('9999-12-31')` | `3652424` | none |
| `TO_DAYS(NULL)` | `NULL` | none |
| `TO_DAYS('bad')` | `NULL` | 1292 `Incorrect datetime value: 'bad'` |
| `TO_DAYS(20240229)` | `739310` | none |
| `TO_DAYS(20240229.9)` | `739310` | 1292 `Truncated incorrect date value: '20240229.9'` |
| `TO_DAYS(DATE_ADD('2024-02-28', INTERVAL 1 DAY))` | `739310` | none |

`TO_DAYS(NOW(6))` returns the current statement's day number with no warning.
MyLite tests must compare it relationally to stable same-statement temporal
expressions rather than hard-coding the current date.

Invalid argument counts are native function errors in MySQL:

- `TO_DAYS()` -> 1582 `Incorrect parameter count in the call to native function 'TO_DAYS'`
- `TO_DAYS('2024-01-01','x')` -> 1582 with the same message form

MyLite currently exposes unsupported arity as parse/prepare failure for scalar
functions. This slice keeps that project-local behavior and tests exact
one-argument acceptance.

## Syntax

`TO_DAYS` is parsed as an ordinary scalar function name with exactly one
argument:

```lemon
function_call ::= identifier LPAREN expression RPAREN.
```

The function name remains nonreserved. An uncalled identifier named `to_days`
must continue to parse as an identifier in table-backed expressions where an
identifier is valid.

## Runtime Semantics

Evaluation order:

1. Evaluate the single argument.
2. If the argument is `NULL`, return `NULL` without a warning.
3. Convert the argument using the same strict temporal conversion rules used by
   `DATE()`, `DATEDIFF()`, and `TIMESTAMPDIFF()`.
4. If conversion fails, return `NULL` and append warning 1292.
5. If conversion succeeds, compute `temporal_day_number(date) + 1` and return
   that signed 64-bit integer.

The internal `temporal_day_number()` helper is intentionally zero-based because
it is used for differences. `TO_DAYS()` must add one to match MySQL's exposed
day numbering:

- internal `0000-01-01` -> `0`, `TO_DAYS()` -> `1`
- internal `0001-01-01` -> `365`, `TO_DAYS()` -> `366`
- internal `2024-02-29` -> `739309`, `TO_DAYS()` -> `739310`

The time portion of valid datetime inputs is ignored. Fractional numeric inputs
are converted through the existing approximate-number temporal conversion path:
the date can still be accepted after truncation, and warning 1292 uses
`Truncated incorrect date value`.

Zero and incomplete dates are strict for `TO_DAYS()` in this slice:

- all-zero date (`0000-00-00`) returns `NULL` with warning 1292
- zero day or zero month inputs return `NULL` with warning 1292
- impossible year-zero leap day returns `NULL` with warning 1292

## Metadata

`TO_DAYS()` result metadata is nullable signed integer metadata:

| Property | Value |
| --- | --- |
| type | `LONGLONG` |
| display length | `8` |
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
UPDATE t SET n = TO_DAYS('bad') WHERE id = 1;
DELETE FROM t WHERE TO_DAYS('bad') IS NULL;
```

## Test Plan

Parser tests:

- `TO_DAYS('2024-02-29')` parses as a one-argument scalar function call.
- Nested/current temporal arguments parse.
- `to_days` remains a nonreserved identifier when used without a call.
- zero-argument and two-argument calls fail deterministically.

Runtime tests:

- metadata for `TO_DAYS('2024-02-29')` and `TO_DAYS(NULL)`.
- valid date and datetime inputs, including time-part ignoring.
- year-zero and boundary values: `0000-01-01`, `0001-01-01`, `9999-12-31`.
- compact and numeric date inputs.
- fractional numeric truncation with warning 1292.
- `NULL` propagation without warning.
- invalid text, all-zero date, zero day, and impossible year-zero leap day
  warnings.
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

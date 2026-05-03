# Angle conversion scalar functions

## Scope

This feature implements MySQL-compatible angle conversion scalar functions for
the scalar expression surfaces that already execute MyLite built-ins:

- `DEGREES(X)`
- `RADIANS(X)`

The supported call sites are no-table `SELECT`, one-table `SELECT`
projection, `WHERE`, and `ORDER BY`, and the supported single-table `UPDATE`
and `DELETE` expression paths. This feature does not add aggregate, window,
generated-column, default-expression, stored-function, prepared-statement, or
`INSERT` expression support beyond the current scalar evaluator.

## Sources

- MySQL 8.4 Reference Manual, Mathematical Functions:
  https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4.9 runtime behavior observed in Docker container
  `mylite-mysql-849` using:
  `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- Result metadata observed with:
  `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv`

This specification is independently authored from official MySQL
documentation and observed MySQL 8.4.9 behavior. It does not copy MySQL
grammar, documentation prose, or implementation source.

## Syntax

The angle conversion functions use ordinary scalar function-call grammar. They
are not special syntactic forms.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
```

Binding and scalar function validation recognize `DEGREES` and `RADIANS`
case-insensitively. Each function requires exactly one argument. MySQL reports
native error 1582 for zero-argument and two-argument calls; MyLite may continue
to use the existing unsupported scalar arity path until native scalar
diagnostic exposure is generalized.

## Semantics

`DEGREES(X)` returns `X * 180 / PI`, converting an angle from radians to
degrees. `RADIANS(X)` returns `X * PI / 180`, converting an angle from degrees
to radians.

If the argument is `NULL`, the result is `NULL` and no numeric conversion is
attempted.

Non-`NULL` arguments are converted through the current MyLite
DOUBLE-compatible numeric conversion path:

- signed and unsigned integer values convert to double values
- real and approximate values are used as double values
- decimal-looking literals currently enter the evaluator as real values
- numeric strings convert with MySQL-like leading numeric parsing
- empty and whitespace-only strings convert to `0` without warning
- nonnumeric strings convert to `0` with warning 1292
- trailing garbage after a numeric prefix produces warning 1292,
  `Truncated incorrect DOUBLE value: '...'`
- text forms that overflow string-to-double conversion clamp to the largest
  finite positive or negative DOUBLE value and produce warning 1292 before the
  angle conversion is applied

The result is a DOUBLE-style value. Public result metadata for both functions
is:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM`, plus `NOT_NULL` when the argument is known non-null |
| Nullability | follows the argument's nullability |

Observed MySQL 8.4.9 metadata marks `DEGREES(1)`, `DEGREES('1x')`,
`RADIANS(1)`, `RADIANS('1x')`, and calls over `NOT NULL` columns as
`NOT_NULL`; `DEGREES(NULL)`, `RADIANS(NULL)`, and calls over nullable columns
are nullable.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `DEGREES(PI())` | `180` |
| `DEGREES(PI()/2)` | `90` |
| `DEGREES(-PI())` | `-180` |
| `DEGREES(1)` | `57.29577951308232` |
| `RADIANS(180)` | `3.141592653589793` |
| `RADIANS(90)` | `1.5707963267948966` |
| `RADIANS(-180)` | `-3.141592653589793` |
| `RADIANS(1)` | `0.017453292519943295` |
| `DEGREES(0)`, `DEGREES(-0.0)` | `0` |
| `RADIANS(0)`, `RADIANS(-0.0)` | `0` |
| `DEGREES(NULL)`, `RADIANS(NULL)` | `NULL` |
| `DEGREES(1e-9999)`, `RADIANS(1e-9999)` | `0`, no warning |
| `DEGREES(1e308)` | error 1690 / `22003` |
| `DEGREES(-1e308)` | error 1690 / `22003` |
| `RADIANS(1e308)` | `1.7453292519943295e306` |
| `RADIANS(-1e308)` | `-1.7453292519943295e306` |
| `DEGREES('1x')` | `57.29577951308232`, warning 1292 |
| `RADIANS('1x')` | `0.017453292519943295`, warning 1292 |
| `DEGREES('foo')`, `RADIANS('foo')` | `0`, warning 1292 |
| `DEGREES('')`, `DEGREES(' ')` | `0`, no warning |
| `RADIANS('')`, `RADIANS(' ')` | `0`, no warning |
| `DEGREES('.')`, `DEGREES('+')`, `DEGREES('-')` | `0`, warning 1292 |
| `RADIANS('.')`, `RADIANS('+')`, `RADIANS('-')` | `0`, warning 1292 |
| `DEGREES('0x10')`, `RADIANS('0x10')` | `0`, warning 1292 |
| `DEGREES('  2.5e1 ')` | `1432.3944878270581`, no warning |
| `RADIANS('  2.5e1 ')` | `0.4363323129985824`, no warning |
| `DEGREES('1e309')` | warning 1292, then error 1690 / `22003` |
| `DEGREES('-1e309')` | warning 1292, then error 1690 / `22003` |
| `RADIANS('1e309')` | `3.1375664143845866e306`, warning 1292 |
| `RADIANS('-1e309')` | `-3.1375664143845866e306`, warning 1292 |

`DEGREES()` range failures are errors, not warnings. MySQL's diagnostic text
includes the lower-case native function name and the offending expression, for
example `DOUBLE value is out of range in 'degrees(1e308)'`. MyLite should
raise an expression error with code 1690 when the converted result is NaN or
infinite. `RADIANS()` over the current SQL input surface does not overflow for
finite DOUBLE inputs; if a future input path can produce a non-finite
conversion result, MyLite must not return NaN or infinity.

For `SELECT`, DOUBLE conversion truncation remains a warning. In default
strict mode, existing MyLite DML warning-promotion rules apply: conversion
warnings from `UPDATE` assignments or `DELETE` predicates become execution
errors and roll back the statement. `DEGREES()` overflow errors also roll back
DML statements. Runtime probes showed:

- `UPDATE t SET x = RADIANS('1x')` promotes 1292 and rolls back.
- `UPDATE t SET x = DEGREES(x) WHERE id = 4` with `x = 1e308` errors with
  1690 and rolls back.
- `DELETE FROM t WHERE DEGREES(x) > 0` with `x = 1e308` errors with 1690
  and rolls back.
- `DELETE FROM t WHERE DEGREES(s) > 0` promotes the first 1292 and rolls back.

## Interactions

The functions participate in the same expression evaluator as other scalar
math functions:

- projection values in no-table and one-table `SELECT`
- `WHERE DEGREES(col) > 90`
- `ORDER BY RADIANS(col)`
- `UPDATE t SET col = RADIANS(col) WHERE DEGREES(col) > 0`
- `DELETE FROM t WHERE DEGREES(col) > 0`

Results remain numeric expression values with compact DOUBLE-style display
text, consistent with `EXP()`, `POW()`, `SQRT()`, and the trigonometric
functions.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires scalar
evaluator additions, metadata inference entries, and tests. The implementation
can reuse the existing DOUBLE coercion helpers and the standard C finite-result
checks used by other math functions. No new dependency is needed.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for both names and mixed-case names
- binding rejection through the current unsupported path for arity mismatches
- no-table scalar results for zero, negative zero, common finite values,
  `NULL`, common angles using `PI()`, huge finite values, tiny underflowing
  literals, and string inputs
- warning 1292 behavior for nonnumeric strings, trailing-garbage strings,
  hex-like strings, sign-only strings, dot-only strings, named nonnumeric
  strings, and positive/negative text overflow
- absence of warnings for empty strings, whitespace-only strings, exponent
  text that remains finite, and text underflow to zero
- overflow error 1690 behavior for `DEGREES()` in `SELECT`, `UPDATE`, and
  `DELETE`
- metadata for non-null constants, `NULL` constants, warning-producing string
  expressions, non-null table columns, nullable table columns, and string
  table columns
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML promotion and rollback for conversion warnings

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- exact expression rendering inside 1690 diagnostics may remain less detailed
  than MySQL's expression-specific message until expression diagnostic
  rendering is generalized
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

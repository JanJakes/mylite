# Basic trigonometric scalar functions

## Scope

This feature implements MySQL-compatible basic trigonometric scalar functions
for the scalar expression surfaces that already execute MyLite built-ins:

- `SIN(X)`
- `COS(X)`
- `TAN(X)`

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

The basic trigonometric functions use ordinary scalar function-call grammar.
They are not special syntactic forms.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
```

Binding and scalar function validation recognize `SIN`, `COS`, and `TAN`
case-insensitively. Each function requires exactly one argument. MySQL reports
native error 1582 for zero-argument and two-argument calls; MyLite may continue
to use the existing unsupported scalar arity path until native scalar
diagnostic exposure is generalized.

## Semantics

`SIN(X)` returns the sine of `X`, `COS(X)` returns the cosine of `X`, and
`TAN(X)` returns the tangent of `X`. The argument is interpreted as radians.

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
  trigonometric function is applied

The result is a DOUBLE-style value. Public result metadata for all three
functions is:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark even constant non-`NULL` trigonometric results as
`NOT_NULL` in the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `SIN(0)`, `TAN(0)` | `0` |
| `COS(0)` | `1` |
| `SIN(-0.0)`, `TAN(-0.0)` | `0` |
| `COS(-0.0)` | `1` |
| `SIN(1)` | `0.8414709848078965` |
| `COS(1)` | `0.5403023058681398` |
| `TAN(1)` | `1.5574077246549023` |
| `SIN(NULL)`, `COS(NULL)`, `TAN(NULL)` | `NULL` |
| `SIN(PI()/2)` | `1` |
| `COS(PI())` | `-1` |
| `TAN(PI()/4)` | `0.9999999999999999` |
| `TAN(PI()/2)` | `1.633123935319537e16` |
| `SIN(1e308)` | `0.4533964905016491` |
| `COS(1e308)` | `-0.8913089376870335` |
| `TAN(1e308)` | `-0.5086861259107568` |
| `SIN(1e-9999)` | `0`, no warning |
| `COS(1e-9999)` | `1`, no warning |
| `TAN(1e-9999)` | `0`, no warning |
| `SIN('1x')` | `0.8414709848078965`, warning 1292 |
| `COS('foo')` | `1`, warning 1292 |
| `TAN('')`, `SIN(' ')` | `0`, no warning |
| `COS('0x10')` | `1`, warning 1292 |
| `TAN('1e309')` | `-0.004962015874444895`, warning 1292 |
| `SIN('-1e309')` | `-0.004961954789184062`, warning 1292 |
| `SIN('nan')`, `SIN('.')` | `0`, warning 1292 |
| `COS('inf')`, `COS('+')` | `1`, warning 1292 |
| `TAN('-inf')`, `TAN('-')` | `0`, warning 1292 |
| `COS('  2.5e1 ')` | `0.9912028118634736`, no warning |
| `TAN('1e2')` | `-0.5872139151569291`, no warning |

Unlike logarithm functions, ordinary finite inputs have no invalid-domain
warning path. Values such as `TAN(PI()/2)` return the host finite tangent
result; they are not divide-by-zero errors.

For `SELECT`, DOUBLE conversion truncation remains a warning. In default
strict mode, existing MyLite DML warning-promotion rules apply: conversion
warnings from `UPDATE` assignments or `DELETE` predicates can become execution
errors and roll back the statement.

Runtime probes showed:

- `UPDATE t SET x = SIN('1x')` promotes 1292 and rolls back.
- `UPDATE t SET x = COS('foo')` promotes 1292 and rolls back.
- `DELETE FROM t WHERE TAN(s) > 0` promotes the first 1292 encountered and
  rolls back.

## Interactions

The functions participate in the same expression evaluator as other scalar
math functions:

- projection values in no-table and one-table `SELECT`
- `WHERE SIN(col) > 0`
- `ORDER BY COS(col)`
- `UPDATE t SET col = TAN(col) WHERE COS(col) < 0`
- `DELETE FROM t WHERE TAN(col) > 0`

Results remain numeric expression values with compact DOUBLE-style display
text, consistent with `POW()`, `SQRT()`, `EXP()`, and the logarithm functions.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires scalar
evaluator additions, metadata inference entries, and tests. Using the C
library's `sin()`, `cos()`, and `tan()` is acceptable because MyLite already
links the math library for `POW()`, `SQRT()`, `EXP()`, and logarithm functions
on platforms that require it.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for all three spellings and mixed-case names
- binding rejection through the current unsupported path for arity mismatches
- no-table scalar results for zero, negative zero, common finite values,
  `NULL`, common angles using `PI()`, huge finite values, tiny underflowing
  literals, and string inputs
- warning 1292 behavior for nonnumeric strings, trailing-garbage strings,
  hex-like strings, sign-only strings, dot-only strings, named nonnumeric
  strings, and positive/negative text overflow
- absence of warnings for empty strings, whitespace-only strings, exponent
  text that remains finite, and text underflow to zero
- metadata for `SIN(1)`, `COS(NULL)`, and warning-producing `TAN('1x')`
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML promotion and rollback for conversion warnings

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- `TAN()` delegates to the host math library, so a small number of ordinary
  finite results can differ from the verified MySQL 8.4.9 display text by the
  last digit on platforms whose `tan()` returns a different final bit
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

# SQRT() scalar function

## Scope

This feature implements MySQL-compatible `SQRT(X)` for the scalar expression
surfaces that already execute MyLite built-in functions:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

`SQRT()` requires exactly one argument. The feature does not add aggregate,
window, generated-column, default-expression, stored-function,
prepared-statement, or `INSERT` expression support beyond the current scalar
evaluator.

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

`SQRT` uses the ordinary scalar function-call grammar. It is not a special
syntactic form.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
```

The parser may continue to accept any identifier as a function name. Binding
and scalar function validation must recognize `SQRT` case-insensitively and
reject any arity other than one. Exact arity belongs to the scalar function
registry rather than parser-specific syntax checks.

## Semantics

`SQRT(X)` evaluates its argument, converts non-`NULL` input to the current
MyLite DOUBLE-compatible numeric representation, and returns the square root
of nonnegative values.

If the argument is `NULL`, the result is `NULL` and no numeric conversion is
attempted. If the converted numeric value is less than zero, the result is
`NULL`; this is not an error and does not add a warning unless conversion
itself added one.

Numeric conversion follows the current scalar evaluator's MySQL-like DOUBLE
coercion:

- signed and unsigned integer values convert to double values
- real and approximate values are used as double values
- decimal-looking literals currently enter the evaluator as real values
- numeric strings convert with MySQL-like leading numeric parsing; host-only
  numeric forms such as C hexadecimal floating-point notation are not accepted
- nonnumeric strings convert to `0`
- trailing garbage after a numeric string produces warning 1292,
  `Truncated incorrect DOUBLE value: '...'`

The result is a DOUBLE-style value. For public result metadata, `SQRT()` must
report:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark even constant non-`NULL` `SQRT()` results as `NOT_NULL` in
the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `SQRT(4)` | `2` |
| `SQRT(20)` | `4.47213595499958` |
| `SQRT(0)` | `0` |
| `SQRT(-1)` | `NULL`, no warning |
| `SQRT(NULL)` | `NULL` |
| `SQRT(-0.0)` | `0` |
| `SQRT('-0')` | `-0` |
| `SQRT(1e308)` | `1e154` |
| `SQRT(1e-308)` | `1e-154` |
| `SQRT('1e-9999')` | `0`, no warning |
| `SQRT('2x')` | `1.4142135623730951`, warning 1292 |
| `SQRT('foo')` | `0`, warning 1292 |
| `SQRT('')`, `SQRT(' ')` | `0`, no warning |
| `SQRT('.')`, `SQRT('+')`, `SQRT('-')` | `0`, warning 1292 |
| `SQRT('0x10')` | `0`, warning 1292 |
| `SQRT('  2.5e1 ')` | `5`, no warning |
| `SQRT('-1x')` | `NULL`, warning 1292 |
| `SQRT('1e309')` | `1.3407807929942596e154`, warning 1292 |
| `SQRT('-1e309')` | `NULL`, warning 1292 |
| `SQRT('nan')`, `SQRT('inf')` | `0`, warning 1292 |

Incorrect arity is rejected during function validation. MySQL reports native
error 1582 for arity failures; MyLite's current scalar registry reports the
existing unsupported-function status for arity mismatches until native error
code exposure is generalized.

For `SELECT`, DOUBLE conversion truncation remains a warning. In default strict
mode, existing MyLite DML warning-promotion rules apply: conversion warnings
from `UPDATE` assignments or `DELETE` predicates can become execution errors
where the corresponding statement layer already promotes expression warnings.

## Edge cases

- Negative finite values return `NULL`, not an out-of-range error.
- `-0` can remain negative zero when it reaches the math operation as a
  negative signed zero.
- Empty and whitespace-only strings convert to `0` without warnings.
- Text forms that overflow the host string-to-double conversion clamp to the
  largest finite DOUBLE value with warning 1292 before `SQRT()` applies domain
  handling.
- Text forms that underflow to `0` do not add a truncation warning unless
  there is trailing garbage.
- Host NaN and infinity values should not be returned to callers. Current SQL
  input paths do not create native NaN or infinity values directly.

## Interactions

`SQRT()` participates in the same expression evaluator as other Task 24 scalar
functions. It must work inside supported scalar expressions, including
comparisons and boolean predicates:

- projection values in no-table and one-table `SELECT`
- `WHERE SQRT(col) >= 2`
- `ORDER BY SQRT(col)`
- `UPDATE t SET col = SQRT(col) WHERE SQRT(col) > 1`
- `DELETE FROM t WHERE SQRT(col) IS NULL`

The result remains a numeric expression value with compact DOUBLE-style display
text, which keeps comparisons, predicates, ordering, and result metadata
consistent with `POW()`.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires only a
small scalar evaluator addition and metadata inference entry. Using the C
library's `sqrt()` is acceptable because MyLite already links the math library
for `POW()` on platforms that require it.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for `SQRT(9)` and mixed-case names
- parser or binding rejection for zero and two arguments
- no-table scalar results for positive values, zero, `NULL`, negative values,
  negative zero, very large values, very small values, and text inputs
- warning 1292 behavior for nonnumeric and trailing-garbage strings
- absence of warnings for negative-domain numeric inputs
- metadata for `SQRT(4)`, `SQRT(NULL)`, `SQRT(-1)`, and warning cases
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML behavior for conversion warnings

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

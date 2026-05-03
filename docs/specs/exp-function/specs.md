# EXP() scalar function

## Scope

This feature implements MySQL-compatible `EXP(X)` for the scalar expression
surfaces that already execute MyLite built-in functions:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

`EXP()` requires exactly one argument. The feature does not add aggregate,
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

`EXP` uses the ordinary scalar function-call grammar. It is not a special
syntactic form.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
```

The parser may continue to accept any identifier as a function name. Binding
and scalar function validation must recognize `EXP` case-insensitively and
reject any arity other than one. Exact arity belongs to the scalar function
registry rather than parser-specific syntax checks.

## Semantics

`EXP(X)` evaluates its argument, converts non-`NULL` input to the current
MyLite DOUBLE-compatible numeric representation, and returns `e` raised to the
converted argument value.

If the argument is `NULL`, the result is `NULL` and no numeric conversion is
attempted.

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
- text forms that overflow string-to-double conversion clamp to the largest
  finite positive or negative DOUBLE value and produce warning 1292 before
  `EXP()` applies range handling

The result is a DOUBLE-style value. For public result metadata, `EXP()` must
report:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark even constant non-`NULL` `EXP()` results as `NOT_NULL` in
the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `EXP(2)` | `7.38905609893065` |
| `EXP(-2)` | `0.1353352832366127` |
| `EXP(0)` | `1` |
| `EXP(NULL)` | `NULL` |
| `EXP(1)` | `2.718281828459045` |
| `EXP(-0.0)` | `1` |
| `EXP(1e-308)` | `1` |
| `EXP(709)` | `8.218407461554972e307` |
| `EXP(710)` | error 1690 / `22003` |
| `EXP(-745)` | `5e-324` |
| `EXP(-746)` | `0` |
| `EXP('2x')` | `7.38905609893065`, warning 1292 |
| `EXP('foo')` | `1`, warning 1292 |
| `EXP('')`, `EXP(' ')` | `1`, no warning |
| `EXP('.')`, `EXP('+')`, `EXP('-')` | `1`, warning 1292 |
| `EXP('0x10')` | `1`, warning 1292 |
| `EXP('  2.5e1 ')` | `72004899337.38588`, no warning |
| `EXP('1e2')` | `2.6881171418161356e43`, no warning |
| `EXP('1e-9999')` | `1`, no warning |
| `EXP('1e309')` | warning 1292, then error 1690 / `22003` |
| `EXP('-1e309')` | `0`, warning 1292 |
| `EXP('nan')`, `EXP('inf')` | `1`, warning 1292 |

Incorrect arity is rejected during function validation. MySQL reports native
error 1582 for arity failures; MyLite's current scalar registry reports the
existing unsupported-function status for arity mismatches until native error
code exposure is generalized.

For `SELECT`, DOUBLE conversion truncation remains a warning. In default strict
mode, existing MyLite DML warning-promotion rules apply: conversion warnings
from `UPDATE` assignments or `DELETE` predicates can become execution errors
where the corresponding statement layer already promotes expression warnings.

`EXP()` range failures are errors, not warnings. MyLite should raise an
expression error with code 1690 when the host exponential result is infinity,
including finite numeric inputs large enough to overflow and positive text
overflow after conversion clamps to the largest finite DOUBLE. Underflow to
`0` or the smallest positive subnormal DOUBLE is not an error.

## Edge cases

- `EXP()` has no negative-domain `NULL` behavior; negative values are valid and
  approach zero as their magnitude grows.
- Small positive values such as `1e-308` return `1` because they round to a
  DOUBLE value indistinguishable from one at MySQL's display precision.
- Empty and whitespace-only strings convert to `0` without warnings.
- Text forms that underflow to `0` do not add a truncation warning unless
  there is trailing garbage.
- Host NaN and infinity values must never be returned to callers. Current SQL
  input paths do not create native NaN or infinity values directly.

## Interactions

`EXP()` participates in the same expression evaluator as other Task 24 scalar
functions. It must work inside supported scalar expressions, including
comparisons and boolean predicates:

- projection values in no-table and one-table `SELECT`
- `WHERE EXP(col) > 1`
- `ORDER BY EXP(col)`
- `UPDATE t SET col = EXP(col) WHERE EXP(col) > 1`
- `DELETE FROM t WHERE EXP(col) = 1`

The result remains a numeric expression value with compact DOUBLE-style display
text, which keeps comparisons, predicates, ordering, and result metadata
consistent with `POW()` and `SQRT()`.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires only a
small scalar evaluator addition and metadata inference entry. Using the C
library's `exp()` is acceptable because MyLite already links the math library
for `POW()` and `SQRT()` on platforms that require it.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for `EXP(2)` and mixed-case names
- parser or binding rejection for zero and two arguments
- no-table scalar results for positive values, negative values, zero, `NULL`,
  very large values, very small values, underflow, overflow, and text inputs
- warning 1292 behavior for nonnumeric and trailing-garbage strings
- metadata for `EXP(2)`, `EXP(NULL)`, `EXP('2x')`, and underflow cases
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML behavior for conversion warnings
- error 1690 behavior and rollback for overflow in `SELECT`, `UPDATE`, and
  `DELETE`

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

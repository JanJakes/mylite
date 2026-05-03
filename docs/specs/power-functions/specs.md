# POW() and POWER()

## Scope

This feature implements MySQL-compatible `POW(X,Y)` and `POWER(X,Y)` for the
scalar expression surfaces that already execute MyLite built-in functions:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

`POWER()` is an exact alias of `POW()`. Both names require exactly two
arguments. The feature does not add aggregate, window, generated-column,
default-expression, stored-function, prepared-statement, or `INSERT`
expression support beyond the current scalar evaluator.

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

`POW` and `POWER` use the ordinary scalar function-call grammar. They are not
special syntactic forms.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression COMMA expression.
```

The parser may continue to accept any identifier as a function name. Binding
and scalar function validation must recognize `POW` and `POWER`
case-insensitively and reject any arity other than two. Because `POW` and
`POWER` are ordinary scalar function calls without special grammar, exact
arity belongs to the scalar function registry rather than parser-specific
syntax checks.

## Semantics

`POW(X,Y)` evaluates both arguments left to right, converts each non-`NULL`
argument to the current MyLite DOUBLE-compatible numeric representation, and
returns `X` raised to `Y`. `POWER(X,Y)` follows the same code path and differs
only in the original expression label when no alias is supplied.

If either argument is `NULL`, the result is `NULL` and no numeric conversion
is attempted for the `NULL` argument.

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

The result is a DOUBLE-style value. For public result metadata, both functions
must report:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark even constant non-`NULL` `POW()` results as `NOT_NULL` in
the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `POW(2,10)` | `1024` |
| `POWER(2,-2)` | `0.25` |
| `POW(3,-1)` | `0.3333333333333333` |
| `POW(10,22)` | `1e22` |
| `POW(NULL,2)`, `POW(2,NULL)` | `NULL` |
| `POW(-2,2)` | `4` |
| `POW(-2,3)` | `-8` |
| `POW(-2,-3)` | `-0.125` |
| `POW(-2,0.5)` | error 1690 / `22003` |
| `POW(-2,1.0)` | `-2` |
| `POW(-2,1.0000000000000002)` | error 1690 / `22003` |
| `POW(-2,1.0000000000000001)` | `-2` |
| `POW(0,0)` | `1` |
| `POW(0,2)` | `0` |
| `POW(0,-1)` | error 1690 / `22003` |
| `POW(2,-1074)` | `5e-324` |
| `POW(2,-1075)` | `0` |
| `POW(-2,-1075)` | `-0` |
| `POW(1e-308,2)` | `0` |
| `POW(1e308,2)` | error 1690 / `22003` |
| `POW('2x','3y')` | `8`, two warning 1292 records |
| `POW('foo',2)` | `0`, warning 1292 |
| `POW(2,'bar')` | `1`, warning 1292 |
| `POW('','')`, `POW(' ',2)` | `1`, `0`; no warnings for empty or whitespace-only strings |
| `POW('.',2)`, `POW('+',2)`, `POW('-',2)` | `0`, warning 1292 |

Domain and range failures are errors, not warnings. MyLite should raise an
expression error with code 1690 when the host math result is NaN or infinity,
including negative finite bases with non-integer effective exponents, zero
bases with negative exponents, and overflow. Underflow to signed or unsigned
zero is not an error.

For `SELECT`, DOUBLE conversion truncation remains a warning. In default strict
mode, existing MyLite DML warning-promotion rules apply: conversion warnings
from `UPDATE` assignments or `DELETE` predicates can become execution errors
where the corresponding statement layer already promotes expression warnings.

## Edge cases

- Negative bases are valid when the converted exponent is an integer-valued
  double. They are invalid when the exponent has a fractional double value.
- `0^0` returns `1`.
- `0` raised to a negative exponent is out of range.
- Overflow to positive or negative infinity is out of range.
- Underflow to `0` or `-0` is allowed.
- Host NaN and infinity results must never be returned to callers.
- `POWER` and `POW` must produce identical values, warnings, errors, and
  metadata.

## Interactions

`POW()` participates in the same expression evaluator as other Task 24 scalar
functions. It must work inside supported scalar expressions, including
comparisons and boolean predicates:

- projection values in no-table and one-table `SELECT`
- `WHERE POW(col, 2) > 4`
- `ORDER BY POW(col, 2)`
- `UPDATE t SET col = POW(col, 2) WHERE POW(col, 1) > 0`
- `DELETE FROM t WHERE POW(col, 2) = 4`

Because result ordering and predicates are numeric, the runtime value should
remain a numeric expression value even when its text representation is compact
DOUBLE-style display text.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires only a
small scalar evaluator addition and metadata inference entry. Using the C
library's `pow()` is acceptable because MyLite already depends on the standard
C runtime; platforms that require an explicit math library link may add that
link narrowly to `libmylite`.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for `POW(2,10)`, `POWER(2,10)`, and mixed-case names
- parser or binding rejection for zero, one, and three arguments
- no-table scalar results for positive, negative, fractional, zero, `NULL`,
  overflow, underflow, and invalid-domain inputs
- warning 1292 behavior for nonnumeric and trailing-garbage strings
- metadata for `POW(2,10)`, `POWER(2,-2)`, `POW(NULL,2)`, and warning cases
- alias parity between `POW` and `POWER`
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML behavior for conversion warnings and error 1690 behavior for
  invalid domains or overflow

Known current MyLite limitations:

- result formatting is limited by the current expression value text model and
  should use compact DOUBLE-style text for power results without changing
  DECIMAL-style formatting used by existing arithmetic operators
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

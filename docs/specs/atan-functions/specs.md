# ATAN() and ATAN2() scalar functions

## Scope

This feature implements MySQL-compatible arctangent scalar functions for the
scalar expression surfaces that already execute MyLite built-ins:

- `ATAN(X)`
- `ATAN(Y,X)`
- `ATAN2(X)`
- `ATAN2(Y,X)`

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

`ATAN` and `ATAN2` use the ordinary scalar function-call grammar. They are not
special syntactic forms.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
function_argument_list ::= expression COMMA expression.
```

Binding and scalar function validation recognize `ATAN` and `ATAN2`
case-insensitively. Both names accept one or two arguments. MySQL reports
native error 1582 for zero-argument and three-argument calls. MyLite may
continue to use the existing unsupported scalar arity path until native scalar
diagnostic exposure is generalized.

## Semantics

`ATAN(X)` and `ATAN2(X)` return the arctangent of `X`, in radians. `ATAN(Y,X)`
and `ATAN2(Y,X)` return the quadrant-aware arctangent for coordinate `Y,X`.
The argument order is `Y` first and `X` second for both two-argument spellings.

Arguments are evaluated left to right:

- If the first argument is `NULL`, the result is `NULL` and later arguments
  are not evaluated.
- For one-argument calls, a non-`NULL` first argument is converted to DOUBLE
  and evaluated with arctangent.
- For two-argument calls, after a non-`NULL` first argument is converted, the
  second argument is evaluated. If it is `NULL`, the result is `NULL`.
- If both two-argument inputs are non-`NULL`, both are converted to DOUBLE and
  evaluated with the quadrant-aware arctangent.

Numeric conversion follows the current MyLite DOUBLE-compatible coercion path:

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
  arctangent is applied

The result is a DOUBLE-style value. Public result metadata for all supported
arctangent calls is:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark constant non-`NULL` calls or calls over `NOT NULL` columns
as `NOT_NULL` in the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `ATAN(0)`, `ATAN(-0.0)` | `0` |
| `ATAN(1)`, `ATAN2(1)` | `0.7853981633974483` |
| `ATAN(-1)`, `ATAN2(-1)` | `-0.7853981633974483` |
| `ATAN(2)` | `1.1071487177940904` |
| `ATAN(-2)` | `-1.1071487177940904` |
| `ATAN(NULL)`, `ATAN2(NULL)` | `NULL` |
| `ATAN(1e-9999)` | `0`, no warning |
| `ATAN(1,2)`, `ATAN2(1,2)` | `0.4636476090008061` |
| `ATAN(2,1)`, `ATAN2(2,1)` | `1.1071487177940904` |
| `ATAN(-1,2)`, `ATAN2(-1,2)` | `-0.4636476090008061` |
| `ATAN(1,-2)`, `ATAN2(1,-2)` | `2.677945044588987` |
| `ATAN(-1,-2)`, `ATAN2(-1,-2)` | `-2.677945044588987` |
| `ATAN(0,1)`, `ATAN2(0,1)` | `0` |
| `ATAN(0,-1)`, `ATAN2(0,-1)` | `3.141592653589793` |
| `ATAN(1,0)`, `ATAN2(1,0)` | `1.5707963267948966` |
| `ATAN(-1,0)`, `ATAN2(-1,0)` | `-1.5707963267948966` |
| `ATAN(0,0)`, `ATAN2(0,0)` | `0` |
| `ATAN(-0.0,-1)`, `ATAN2(-0.0,-1)` | `3.141592653589793` |
| `ATAN(0,-0.0)`, `ATAN2(0,-0.0)` | `0` |
| `ATAN('1x')`, `ATAN2('1x')` | `0.7853981633974483`, warning 1292 |
| `ATAN('foo')`, `ATAN2('foo')` | `0`, warning 1292 |
| `ATAN('')`, `ATAN(' ')` | `0`, no warning |
| `ATAN('.')`, `ATAN('+')`, `ATAN('-')` | `0`, warning 1292 |
| `ATAN('0x10')` | `0`, warning 1292 |
| `ATAN('  2.5e1 ')` | `1.5308176396716067`, no warning |
| `ATAN('1e309')` | `1.5707963267948966`, warning 1292 |
| `ATAN('-1e309')` | `-1.5707963267948966`, warning 1292 |
| `ATAN('nan')`, `ATAN('inf')`, `ATAN('-inf')` | `0`, warning 1292 |
| `ATAN('1x','2x')`, `ATAN2('1x','2x')` | `0.4636476090008061`, warning 1292 for `1x` then `2x` |
| `ATAN('1x',NULL)`, `ATAN2('1x',NULL)` | `NULL`, warning 1292 for `1x` |
| `ATAN(NULL,'2x')`, `ATAN2(NULL,'2x')` | `NULL`, no warning |

`ATAN()` and `ATAN2()` do not have logarithm-style domain warnings or
cotangent-style range errors for the verified finite input surface. For
`SELECT`, DOUBLE conversion truncation remains a warning.

In default strict mode, existing MyLite DML warning-promotion rules apply:

- `UPDATE t SET x = ATAN('1x')` promotes 1292 and rolls back.
- `UPDATE t SET n = ATAN2(s,x)` promotes the first 1292 encountered and rolls
  back when `s` contains a truncated numeric string.
- `DELETE FROM t WHERE ATAN(s) > 0` promotes 1292 and rolls back when a
  scanned row reaches a truncated numeric string.
- `DELETE FROM t WHERE ATAN2(NULL,s) IS NULL` does not evaluate `s` and does
  not produce a warning.

## Interactions

The functions participate in the same expression evaluator as other scalar
math functions:

- projection values in no-table and one-table `SELECT`
- `WHERE ATAN(col) > 0`
- `ORDER BY ATAN2(y,x)`
- `UPDATE t SET col = ATAN(col) WHERE ATAN2(y,x) > 0`
- `DELETE FROM t WHERE ATAN2(col, other_col) < 0`

Results remain numeric expression values with compact DOUBLE-style display
text, consistent with `SIN()`, `COS()`, `TAN()`, `COT()`, and `ACOS()` /
`ASIN()`. Statement errors and promoted warnings must preserve existing DML
atomicity and rollback behavior.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires scalar
evaluator additions, metadata inference entries, compatibility docs, and
runtime tests. The implementation can reuse the existing DOUBLE coercion
helpers, compact real formatting, PI-expression normalization used by the
trigonometric functions, and standard C math functions already linked by
MyLite.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for `ATAN`, `ATAN2`, mixed-case names, and one- and
  two-argument calls
- binding rejection through the current unsupported path for zero- and
  three-argument calls
- no-table scalar results for positive, negative, zero, negative zero,
  `NULL`, tiny underflowing literals, and string inputs
- two-argument quadrant and axis behavior, including positive and negative
  axes, origin, and negative-zero axis inputs
- one-argument `ATAN2(X)` alias behavior
- warning 1292 behavior for trailing-garbage, nonnumeric, hex-like,
  sign-only, dot-only, named nonnumeric, and positive/negative overflow text
- absence of warnings for empty strings, whitespace-only strings, exponent
  text that remains finite, and text underflow to zero
- left-to-right warning ordering for two non-`NULL` text arguments
- `NULL` short-circuit behavior for two-argument calls
- metadata for `ATAN(1)`, `ATAN(NULL)`, warning-producing `ATAN('1x')`,
  `ATAN(1,2)`, `ATAN2(1)`, `ATAN2(NULL)`, warning-producing `ATAN2('1x')`,
  `ATAN2(1,2)`, non-null table columns, nullable table columns, and string
  table columns
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML promotion and rollback for conversion warnings

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- `ATAN()` and `ATAN2()` delegate to the host math library, so a small number
  of finite results can differ from the verified MySQL 8.4.9 display text by
  the final digit, matching the existing trigonometric implementation tradeoff
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

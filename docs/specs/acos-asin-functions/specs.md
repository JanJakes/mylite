# ACOS() and ASIN() scalar functions

## Scope

This feature implements MySQL-compatible inverse trigonometric scalar
functions for the scalar expression surfaces that already execute MyLite
built-ins:

- `ACOS(X)`
- `ASIN(X)`

The supported call sites are no-table `SELECT`, one-table `SELECT`
projection, `WHERE`, and `ORDER BY`, and the supported single-table `UPDATE`
and `DELETE` expression paths. This feature does not implement `ATAN()`,
`ATAN2()`, aggregate/window behavior, generated-column or default-expression
execution, stored-function resolution, prepared-statement support, or `INSERT`
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

`ACOS` and `ASIN` use the ordinary scalar function-call grammar. They are not
special syntactic forms.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
```

Binding and scalar function validation recognize `ACOS` and `ASIN`
case-insensitively and require exactly one argument. MySQL reports native
error 1582 for zero-argument and two-argument calls. MyLite may continue to
use the existing unsupported scalar arity path until native scalar diagnostic
exposure is generalized.

## Semantics

`ACOS(X)` returns the inverse cosine of `X`, in radians. `ASIN(X)` returns the
inverse sine of `X`, in radians.

If the argument is `NULL`, the result is `NULL` and no numeric conversion is
attempted. Non-`NULL` arguments are converted through the current MyLite
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
  finite positive or negative DOUBLE value and produce warning 1292 before
  inverse-trigonometric domain handling

After conversion, values outside the inclusive `[-1, 1]` domain return `NULL`
without an additional domain warning. Domain `NULL` is not a range error.

The result is a DOUBLE-style value. Public result metadata for both functions
is:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark even constant non-`NULL` `ACOS()` or `ASIN()` results, nor
calls over `NOT NULL` columns, as `NOT_NULL` in the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `ACOS(1)` | `0` |
| `ACOS(0)` | `1.5707963267948966` |
| `ACOS(-1)` | `3.141592653589793` |
| `ACOS(0.5)` | `1.0471975511965979` |
| `ACOS(-0.5)` | `2.0943951023931957` |
| `ACOS(1.0001)`, `ACOS(-1.0001)`, `ACOS(2)` | `NULL`, no warning |
| `ACOS(NULL)` | `NULL` |
| `ACOS(-0.0)` | `1.5707963267948966` |
| `ASIN(1)` | `1.5707963267948966` |
| `ASIN(0)` | `0` |
| `ASIN(-1)` | `-1.5707963267948966` |
| `ASIN(0.2)` | `0.2013579207903308` |
| `ASIN(0.5)` | `0.5235987755982989` |
| `ASIN(-0.5)` | `-0.5235987755982989` |
| `ASIN(1.0001)`, `ASIN(-1.0001)`, `ASIN(-2)` | `NULL`, no warning |
| `ASIN(NULL)` | `NULL` |
| `ASIN(-0.0)` | `0` |
| `ACOS(1e-9999)` | `1.5707963267948966`, no warning |
| `ASIN(1e-9999)` | `0`, no warning |
| `ACOS('1x')` | `0`, warning 1292 |
| `ASIN('1x')` | `1.5707963267948966`, warning 1292 |
| `ACOS('foo')` | `1.5707963267948966`, warning 1292 |
| `ASIN('foo')` | `0`, warning 1292 |
| `ACOS('')`, `ACOS(' ')` | `1.5707963267948966`, no warning |
| `ASIN('')`, `ASIN(' ')` | `0`, no warning |
| `ACOS('.')`, `ACOS('+')`, `ACOS('0x10')` | `1.5707963267948966`, warning 1292 |
| `ASIN('.')`, `ASIN('-')`, `ASIN('0x10')` | `0`, warning 1292 |
| `ACOS('  2.5e1 ')`, `ASIN('  2.5e1 ')` | `NULL`, no warning |
| `ACOS('1e309')`, `ASIN('1e309')` | `NULL`, warning 1292 |
| `ACOS('-1e309')`, `ASIN('-1e309')` | `NULL`, warning 1292 |
| `ACOS('nan')` | `1.5707963267948966`, warning 1292 |
| `ASIN('inf')` | `0`, warning 1292 |

For `SELECT`, DOUBLE conversion truncation remains a warning. Invalid
inverse-trigonometric domains do not append warnings. In default strict mode,
existing MyLite DML warning-promotion rules apply before row mutation:

- `UPDATE t SET x = ACOS('1x')` promotes 1292 and rolls back.
- `DELETE FROM t WHERE ASIN(s) > 0` promotes the first 1292 encountered and
  rolls back.
- `UPDATE t SET n = ACOS(x)` may store `NULL` for out-of-domain rows when the
  target column is nullable.
- `UPDATE t SET n = ACOS(2)` fails when `n` is `NOT NULL`, because the domain
  result is `NULL`.
- `DELETE FROM t WHERE ACOS(x) IS NULL` deletes out-of-domain rows without a
  domain warning.

## Interactions

The functions participate in the same expression evaluator as other scalar
math functions:

- projection values in no-table and one-table `SELECT`
- `WHERE ACOS(col) IS NULL`
- `ORDER BY ASIN(col)`
- `UPDATE t SET col = ASIN(col) WHERE ACOS(col) < 2`
- `DELETE FROM t WHERE ACOS(col) IS NULL`

Results remain numeric expression values with compact DOUBLE-style display
text, consistent with `SIN()`, `COS()`, `TAN()`, `SQRT()`, and the logarithm
functions. Statement errors and promoted warnings must preserve existing DML
atomicity and rollback behavior.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires scalar
evaluator additions, metadata inference entries, compatibility docs, and
runtime tests. The implementation can reuse the existing DOUBLE coercion
helpers and standard C math functions already linked by MyLite.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for `ACOS(1)`, `ASIN(1)`, and mixed-case names
- binding rejection through the current unsupported path for arity mismatches
- no-table scalar results for positive, negative, zero, negative zero,
  fractional, `NULL`, boundary, and out-of-domain values
- invalid-domain `NULL` behavior without warnings
- warning 1292 behavior for trailing-garbage, nonnumeric, hex-like,
  sign-only, dot-only, named nonnumeric, and positive/negative overflow text
- absence of warnings for empty strings, whitespace-only strings, finite
  out-of-domain text, and text underflow to zero
- metadata for constant values, `NULL`, invalid-domain calls,
  warning-producing calls, non-null table columns, nullable table columns, and
  string table columns
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML warning promotion and rollback for conversion warnings
- `NOT NULL` assignment failure when an invalid domain produces `NULL`
- out-of-domain DML behavior without warning promotion

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- `ACOS()` and `ASIN()` delegate to the host math library, so a small number
  of finite results can differ from the verified MySQL 8.4.9 display text by
  the final digit, matching the existing basic trigonometric implementation
  tradeoff
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

# COT() scalar function

## Scope

This feature implements MySQL-compatible `COT(X)` for the scalar expression
surfaces that already execute MyLite built-ins:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

`COT()` requires exactly one argument. This feature does not add inverse
trigonometric functions, aggregate/window behavior, generated-column or
default-expression execution, stored-function resolution, prepared-statement
support, or `INSERT` expression support beyond the current scalar evaluator.

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

`COT` uses the ordinary scalar function-call grammar. It is not a special
syntactic form.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
```

Binding and scalar function validation recognize `COT` case-insensitively and
require exactly one argument. MySQL reports native error 1582 for zero-argument
and two-argument calls. MyLite may continue to use the existing unsupported
scalar arity path until native scalar diagnostic exposure is generalized.

## Semantics

`COT(X)` returns the cotangent of `X`, where `X` is interpreted as radians. If
the argument is `NULL`, the result is `NULL` and no numeric conversion is
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
  finite positive or negative DOUBLE value and produce warning 1292 before
  `COT()` applies range handling

The result is a DOUBLE-style value. Public result metadata is:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark even constant non-`NULL` `COT()` results or calls over
`NOT NULL` columns as `NOT_NULL` in the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `COT(1)` | `0.6420926159343306` |
| `COT(12)` | `-1.5726734063976893` |
| `COT(-1)` | `-0.6420926159343306` |
| `COT(NULL)` | `NULL` |
| `COT(PI()/2)` | `6.123233995736766e-17` |
| `COT(PI()/4)` | `1.0000000000000002` |
| `COT(PI())` | `-8.165619676597685e15` |
| `COT(-PI())` | `8.165619676597685e15` |
| `COT(1e308)` | `-1.9658487799516644` |
| `COT(1e-308)` | `1e308` |
| `COT(-1e-308)` | `-1e308` |
| `COT(0)`, `COT(-0.0)` | error 1690 / `22003` |
| `COT(5e-324)`, `COT(-5e-324)` | error 1690 / `22003` |
| `COT(1e-9999)` | error 1690 / `22003` |
| `COT('1x')` | `0.6420926159343306`, warning 1292 |
| `COT('foo')` | warning 1292, then error 1690 / `22003` |
| `COT('')`, `COT(' ')` | error 1690 / `22003` without 1292 |
| `COT('.')`, `COT('+')`, `COT('-')` | warning 1292, then error 1690 / `22003` |
| `COT('0x10')` | warning 1292, then error 1690 / `22003` |
| `COT('  2.5e1 ')` | `-7.489155308722675` |
| `COT('1e2')` | `-1.702956919426469` |
| `COT('1e309')` | `-201.53099572900314`, warning 1292 |
| `COT('-1e309')` | `201.53099572900314`, warning 1292 |
| `COT('1e-9999')`, `COT('-1e-9999')` | error 1690 / `22003` |
| `COT('nan')`, `COT('inf')`, `COT('-inf')` | warning 1292, then error 1690 / `22003` |

`COT()` range failures are errors, not warnings. They happen when the converted
argument or the host cotangent calculation would produce a NaN or infinity,
including exact zero, underflow-to-zero, and finite inputs whose reciprocal
tangent overflows. MyLite should raise expression error code 1690 for these
cases. Empty and whitespace-only strings still convert to `0` without a 1292
warning, then fail with 1690.

For `SELECT`, DOUBLE conversion truncation remains a warning when the cotangent
result is finite. In default strict mode, existing MyLite DML warning-promotion
rules apply before row mutation:

- `UPDATE t SET x = COT('1x')` promotes 1292 and rolls back.
- `UPDATE t SET x = COT(0)` errors with 1690 and rolls back.
- `DELETE FROM t WHERE COT(0) IS NULL` errors with 1690 and rolls back.
- `DELETE FROM t WHERE COT(s) > 0` promotes the first 1292 encountered when
  `s` contains a truncated numeric string and rolls back.

## Interactions

`COT()` participates in the same expression evaluator as other scalar math
functions:

- projection values in no-table and one-table `SELECT`
- `WHERE COT(col) > 0`
- `ORDER BY COT(col)`
- `UPDATE t SET col = COT(col) WHERE COT(col) > 0`
- `DELETE FROM t WHERE COT(col) < 0`

Results remain numeric expression values with compact DOUBLE-style display
text, consistent with `SIN()`, `COS()`, `TAN()`, `EXP()`, and the logarithm
functions. Statement errors and promoted warnings must preserve existing DML
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

- parser acceptance for `COT(1)` and mixed-case names
- binding rejection through the current unsupported path for arity mismatches
- no-table scalar results for positive, negative, `NULL`, common PI-derived
  angles, huge finite inputs, tiny finite inputs, and string inputs
- error 1690 behavior for zero, negative zero, underflow-to-zero, subnormal
  overflow, and string inputs that convert to zero
- warning 1292 behavior for trailing-garbage, nonnumeric, hex-like,
  sign-only, dot-only, named nonnumeric, and positive/negative overflow text
- absence of warning 1292 for empty and whitespace-only strings, while still
  returning the 1690 cotangent range error
- metadata for `COT(1)`, `COT(NULL)`, warning-producing `COT('1x')`, non-null
  table columns, nullable table columns, and string table columns
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML warning promotion and rollback for conversion warnings
- DML rollback for 1690 range errors

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- exact expression rendering inside 1690 diagnostics may remain less detailed
  than MySQL's expression-specific message until expression diagnostic
  rendering is generalized
- `COT()` delegates to the host math library, so a small number of finite
  results can differ from the verified MySQL 8.4.9 display text by the final
  digit, matching the existing `TAN()` implementation tradeoff
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

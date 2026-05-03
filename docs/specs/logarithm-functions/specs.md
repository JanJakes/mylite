# Logarithm scalar functions

## Scope

This feature implements MySQL-compatible logarithm scalar functions for the
scalar expression surfaces that already execute MyLite built-in functions:

- `LN(X)`
- `LOG(X)`
- `LOG(B,X)`
- `LOG2(X)`
- `LOG10(X)`

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

The logarithm functions use ordinary scalar function-call grammar. They are
not special syntactic forms.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression.
function_argument_list ::= expression COMMA expression.
```

Binding and scalar function validation recognize names case-insensitively:

- `LN`, `LOG2`, and `LOG10` require exactly one argument.
- `LOG` accepts either one or two arguments.

MySQL reports native error 1582 for `LN`, `LOG2`, and `LOG10` arity failures.
`LOG()` and `LOG(1,2,3)` are syntax errors in MySQL. MyLite may continue to use
the existing unsupported scalar arity path until native scalar diagnostic
exposure is generalized.

## Semantics

`LN(X)` and one-argument `LOG(X)` return the natural logarithm of `X`.
`LOG(B,X)` returns the logarithm of `X` to base `B`.
`LOG2(X)` returns the base-2 logarithm of `X`.
`LOG10(X)` returns the base-10 logarithm of `X`.

Arguments are evaluated left to right. If a one-argument logarithm input is
`NULL`, the result is `NULL` and no numeric conversion is attempted.

Two-argument `LOG(B,X)` has MySQL-specific ordering:

- `B` is evaluated first.
- If `B` is `NULL`, the result is `NULL` and `X` is not evaluated.
- If `B` converts to a value less than or equal to `0`, the result is `NULL`,
  warning 3020 is appended, and `X` is not evaluated.
- Otherwise, `X` is evaluated. If `X` is `NULL`, the result is `NULL` and no
  logarithm-domain warning is appended for `B = 1`.
- Non-`NULL` `X` is converted before final domain checking. If `B = 1` or
  `X <= 0`, the result is `NULL` with warning 3020.
- Bases between `0` and `1` are valid and produce negative results for
  arguments greater than `1`.

Numeric conversion follows the current scalar evaluator's MySQL-like DOUBLE
coercion:

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
  logarithm-domain handling

Invalid logarithm domains are not range errors. They return `NULL` and append
warning 3020, `Invalid argument for logarithm`:

- `LN(X)`, `LOG(X)`, `LOG2(X)`, and `LOG10(X)` are invalid when `X <= 0`
- `LOG(B,X)` is invalid when `B <= 0`, `B = 1`, or `X <= 0`

The result is a DOUBLE-style value. Public result metadata for all logarithm
functions is:

| Property | Value |
| --- | --- |
| Type | `DOUBLE` |
| Length | `23` |
| Decimals | `31` |
| Collation / charset | binary / `63` |
| Flags | `BINARY NUM` |
| Nullability | nullable |

MySQL does not mark even constant non-`NULL` logarithm results as `NOT_NULL` in
the verified metadata.

## Errors and warnings

Observed MySQL 8.4.9 behavior:

| Expression | Result |
| --- | --- |
| `LN(2)`, `LOG(2)` | `0.6931471805599453` |
| `LOG(10,100)` | `2` |
| `LOG2(8)` | `3` |
| `LOG10(1000)` | `3` |
| `LN(NULL)`, `LOG(NULL)`, `LOG(NULL,8)`, `LOG(2,NULL)` | `NULL`, no warning |
| `LN(-2)`, `LN(0)`, `LOG(-2)`, `LOG(0)` | `NULL`, warning 3020 |
| `LOG(1,100)`, `LOG(0,100)`, `LOG(2,-4)` | `NULL`, warning 3020 |
| `LOG(0.5,8)` | `-3` |
| `LOG2(0)`, `LOG10(-1)` | `NULL`, warning 3020 |
| `LN('2x')`, `LOG(2,'8x')` | natural log of `2`, base-2 log of `8`; warning 1292 |
| `LN('')`, `LN(' ')` | `NULL`, warning 3020 only |
| `LN('.')`, `LN('+')`, `LN('-')`, `LN('0x10')` | `NULL`, warning 1292 then 3020 |
| `LN('1e309')` | `709.782712893384`, warning 1292 |
| `LN('-1e309')` | `NULL`, warning 1292 then 3020 |
| `LOG(2,'')`, `LOG('',8)` | `NULL`, warning 3020 only |
| `LOG('2x','8y')` | `3`, two warning 1292 records |
| `LOG(1,'8x')` | `NULL`, warning 1292 for `8x`, then warning 3020 |
| `LOG('1x','8y')` | `NULL`, warning 1292 for `1x`, warning 1292 for `8y`, then warning 3020 |
| `LOG(0.5,'8x')`, `LOG('0.5x','8y')` | valid base logarithm; conversion warnings for nonnumeric suffixes |
| `LOG('foo','bar')` | `NULL`, warning 1292 for `foo`, then warning 3020 |
| `LOG(NULL,'8x')`, `LOG(1,NULL)` | `NULL`, no warning |
| `LOG('1x',NULL)` | `NULL`, warning 1292 for `1x` only |
| `LOG(0,NULL)` | `NULL`, warning 3020 |

For `SELECT`, DOUBLE conversion truncation and invalid logarithm domains remain
warnings. In default strict mode, existing MyLite DML warning-promotion rules
apply: conversion warnings and logarithm-domain warnings from `UPDATE`
assignments or `DELETE` predicates can become execution errors and roll back
the statement.

Runtime probes showed:

- `UPDATE t SET x = LN('8x')` promotes 1292 and rolls back.
- `UPDATE t SET x = LN('bad')` promotes 1292 first, includes 3020 after it,
  and rolls back.
- `DELETE FROM t WHERE LN(s) IS NULL` promotes 3020 and rolls back when a
  scanned row reaches a converted zero domain.

## Interactions

The functions participate in the same expression evaluator as other scalar
math functions:

- projection values in no-table and one-table `SELECT`
- `WHERE LOG2(col) > 1`
- `ORDER BY LOG10(col)`
- `UPDATE t SET col = LN(col) WHERE LOG(col) > 0`
- `DELETE FROM t WHERE LOG(col) IS NULL`

Results remain numeric expression values with compact DOUBLE-style display
text, consistent with `POW()`, `SQRT()`, and `EXP()`.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires scalar
evaluator additions, metadata inference entries, and warning-code support.
Using the C library's `log()`, `log2()`, and `log10()` is acceptable because
MyLite already links the math library for `POW()`, `SQRT()`, and `EXP()` on
platforms that require it.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance for all five spellings, mixed-case names, and one- and
  two-argument `LOG`
- binding rejection through the current unsupported path for arity mismatches
- no-table scalar results for positive values, exact integer-looking results,
  `NULL`, zero and negative domains, invalid base values, and string inputs
- warning order for conversion followed by logarithm-domain warning
- two-argument `LOG` evaluation ordering for `NULL`, `<= 0`, `= 1`, and
  fractional bases
- metadata for every function family and warning-producing expressions
- table projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and predicate paths
- supported `DELETE` predicate paths
- strict DML promotion and rollback for conversion warnings and 3020 domain
  warnings

Known current MyLite limitations:

- exact native arity diagnostics are deferred with the broader scalar-function
  diagnostic surface
- exact fixed-point decimal storage and exhaustive DECIMAL-to-DOUBLE edge
  behavior remain part of broader numeric compatibility work

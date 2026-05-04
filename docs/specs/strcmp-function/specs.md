# STRCMP() Scalar Function

## Scope

This feature implements MySQL-compatible scalar `STRCMP()` calls for the
expression surfaces that already execute MyLite scalar built-ins:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` and `DELETE` expression paths

The first implementation slice compares values using MyLite's current
collation registry and expression descriptor inference. It supports ASCII case
sensitivity and PAD SPACE / NO PAD trailing-space rules for ordinary text,
column, cast, and numeric-to-string arguments. Numeric arguments use the
connection collation after string coercion. Full MySQL collation weight tables,
illegal-mix diagnostics, character-set introducers, and `BINARY expr`
operator behavior are deferred.

## Sources

- MySQL 8.4 Reference Manual, built-in function and operator reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, string functions and operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, string comparison functions and operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html
- MySQL 8.4 Reference Manual, type conversion in expression evaluation:
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

`STRCMP` uses the ordinary scalar function-call grammar. It is not a special
syntactic form.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression COMMA expression.
```

Binding recognizes `STRCMP` case-insensitively. MySQL reports native error
1582 for zero, one, and three-or-more arguments. MyLite rejects unsupported
arities through the existing scalar arity path until native scalar diagnostic
exposure is generalized.

## Semantics

`STRCMP(expr1, expr2)` evaluates arguments from left to right. If the first
argument evaluates to `NULL`, the result is `NULL` and the second argument is
not evaluated. If the second evaluated argument is `NULL`, the result is
`NULL`. This short-circuit rule avoids warnings from later expressions such as
division by zero.

For non-`NULL` arguments, both operands compare as strings. Numeric arguments
are converted to their string form for the comparison and do not emit numeric
truncation warnings.

The result is:

- `-1` when the first string sorts before the second
- `0` when the strings compare equal
- `1` when the first string sorts after the second
- `NULL` when either argument is `NULL`

Verified examples:

| Expression | Result | Warnings |
| --- | ---: | ---: |
| `STRCMP('text','text2')` | `-1` | 0 |
| `STRCMP('text2','text')` | `1` | 0 |
| `STRCMP('text','text')` | `0` | 0 |
| `STRCMP(NULL,'a')` | `NULL` | 0 |
| `STRCMP('a',NULL)` | `NULL` | 0 |
| `STRCMP(NULL,1/0)` | `NULL` | 0 |
| `STRCMP(10,'2')` | `-1` | 0 |
| `STRCMP('10',2)` | `-1` | 0 |
| `STRCMP(10,2)` | `-1` | 0 |
| `STRCMP(2,'10')` | `1` | 0 |
| `STRCMP('foo',0)` | `1` | 0 |
| `STRCMP(0,'foo')` | `-1` | 0 |
| `STRCMP(1.5,'1.50')` | `-1` | 0 |
| `STRCMP(1.50,'1.50')` | `0` | 0 |
| `STRCMP(CAST(1.50 AS DECIMAL(5,2)),'1.50')` | `0` | 0 |

## Collation Behavior

MySQL compares using the operands' effective collations. MyLite infers the
comparison collation from argument literals and descriptors with the existing
collation inference helper. Numeric arguments are treated as connection-
collation strings for this function, matching MySQL's numeric-to-string
comparison behavior without truncation warnings. If no non-`NULL` text or
numeric argument contributes a collation, MyLite falls back to the connection
collation.

Implemented first-slice behavior:

- `utf8mb4_0900_ai_ci`: ASCII case-insensitive; trailing ASCII spaces remain
  significant because the collation is `NO PAD`
- `latin1_swedish_ci`, `latin1_bin`, `utf8mb4_bin`, `utf8mb3_general_ci`, and
  `utf8mb3_bin`: trailing ASCII spaces are ignored because these supported
  registry entries are `PAD SPACE`
- `*_bin` and `binary`: ASCII byte values are compared case-sensitively
- other supported nonbinary collations use ASCII case-insensitive comparison
- column descriptors participate in collation selection when MyLite has table
  metadata for the expression path

Observed MySQL behavior:

| Connection or operand collation | Expression | Result |
| --- | --- | ---: |
| initial CLI `latin1_swedish_ci` | `STRCMP('a','A')` | `0` |
| initial CLI `latin1_swedish_ci` | `STRCMP('a','B')` | `-1` |
| initial CLI `latin1_swedish_ci` | `STRCMP('B','a')` | `1` |
| initial CLI `latin1_swedish_ci` | `STRCMP('a','a ')` | `0` |
| initial CLI `latin1_swedish_ci` | `STRCMP('a ','a')` | `0` |
| after `SET NAMES utf8mb4` | `STRCMP('a','A')` | `0` |
| after `SET NAMES utf8mb4` | `STRCMP('a','a ')` | `-1` |
| after `SET NAMES utf8mb4` | `STRCMP('a ','a')` | `1` |
| explicit `utf8mb4_bin` | `STRCMP(_utf8mb4'a' COLLATE utf8mb4_bin, _utf8mb4'a ' COLLATE utf8mb4_bin)` | `0` |
| binary operator | `STRCMP(BINARY 'a','A')` | `1` |

The `BINARY expr` operator itself emits MySQL warning 1287. MyLite currently
does not implement that operator's warning or per-operand binary comparison
state in this scalar path, so binary-operand behavior remains deferred.

## Metadata

`STRCMP()` result metadata is numeric:

- type: `LONGLONG`
- length: `2`
- decimals: `0`
- collation: `binary`
- flags: `BINARY NUM`
- `NOT_NULL` is set only when both arguments are statically non-nullable

Observed MySQL 8.4.9 metadata:

| Expression alias | Type | Length | Decimals | Collation | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `STRCMP('text','text2') AS cmp_text` | `LONGLONG` | `2` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `STRCMP(NULL,'a') AS cmp_null` | `LONGLONG` | `2` | `0` | `binary` | `BINARY NUM` |
| `STRCMP(10,'2') AS cmp_mixed` | `LONGLONG` | `2` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `STRCMP(s,'a') AS cmp_nullable` where `s` is nullable | `LONGLONG` | `2` | `0` | `binary` | `BINARY NUM` |
| `STRCMP(nn,'a') AS cmp_not_null` where `nn` is `NOT NULL` | `LONGLONG` | `2` | `0` | `binary` | `NOT_NULL BINARY NUM` |

## Errors and warnings

Verified MySQL 8.4.9 behavior:

- `STRCMP()`, `STRCMP('a')`, and `STRCMP('a','b','c')` raise error 1582.
- `NULL` propagation produces no warnings, including `STRCMP(NULL,1/0)`.
- Numeric-to-string conversion for comparison produces no warnings.
- `BINARY expr` operands produce warning 1287 from the `BINARY` operator.
- Assigning a `NULL` result into an `INT NOT NULL` target in default strict
  mode raises error 1048 and rolls back the row change.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. Runtime comparison
must remain MyLite-owned. The implementation must not delegate comparison to
SQLite because MySQL string comparison rules, numeric-to-string conversion,
`NULL` short-circuiting, and PAD SPACE / NO PAD behavior differ from SQLite's
default comparison behavior.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance of mixed-case `STRCMP` with exactly two arguments
- binding rejection through the current unsupported path for zero, one, and
  three-or-more arguments
- core `-1`, `0`, and `1` comparison results
- `NULL` propagation and `STRCMP(NULL,1/0)` warning suppression
- default `utf8mb4_0900_ai_ci` ASCII case-insensitive comparison and NO PAD
  trailing-space behavior
- `SET NAMES latin1` PAD SPACE trailing-space behavior
- numeric-to-string conversion without warnings
- metadata for non-nullable, nullable, and table-column arguments
- projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and rollback for `NOT NULL` failure
- supported `DELETE` predicate paths
- source hygiene check proving no restrictive source references were introduced

Known current MyLite limitations:

- exact native 1582 arity diagnostics remain deferred with the broader
  scalar-function diagnostic surface
- full Unicode collation weights, explicit operand collations, character-set
  introducers, and mixed-collation illegal-mix diagnostics are deferred
- binary-string-sensitive operand behavior and the `BINARY expr` deprecation
  warning are deferred
- supported behavior is limited to the collations currently present in
  `mylite_charset.{h,c}` and their ASCII case/PAD attributes

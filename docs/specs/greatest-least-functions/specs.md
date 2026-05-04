# GREATEST() and LEAST() Scalar Functions

## Scope

This feature implements MySQL-compatible scalar `GREATEST()` and `LEAST()`
calls for the expression surfaces that already execute MyLite scalar built-ins:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` and `DELETE` expression paths

The feature does not add aggregate, window, generated-column,
default-expression, stored-function, prepared-statement parameter, broad
temporal comparison, binary-string collation, or full collation repertoire
support beyond the current scalar evaluator.

## Sources

- MySQL 8.4 Reference Manual, comparison functions and operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
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

`GREATEST` and `LEAST` use the ordinary scalar function-call grammar. They are
not special syntactic forms.

```lemon
primary_expression ::= scalar_function_call.

scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.

function_name ::= identifier.

function_argument_list ::= expression COMMA expression.
function_argument_list ::= function_argument_list COMMA expression.
```

Binding recognizes `GREATEST` and `LEAST` case-insensitively. MySQL reports
native error 1582 for zero-argument and one-argument calls. MyLite rejects those
arities through the existing unsupported scalar arity path until native scalar
diagnostic exposure is generalized.

## Semantics

Arguments are evaluated left to right. If any evaluated argument is `NULL`, the
result is `NULL`. MySQL short-circuits at the first `NULL` for this function
family: `GREATEST(NULL, 1/0)` and `LEAST(NULL, 1/0)` return `NULL` without a
division warning.

For non-`NULL` arguments, MySQL chooses the comparison domain before comparing:

- if any ordinary string argument is present, all arguments compare in the
  string domain and the result metadata is string metadata
- otherwise, numeric arguments compare in the numeric domain

The string-domain rule is important for mixed calls. These calls compare
lexicographically, not numerically, and do not produce numeric conversion
warnings:

| Expression | Result |
| --- | --- |
| `GREATEST(1,'2')` | `2` |
| `LEAST(1,'2')` | `1` |
| `GREATEST(2,'9')` | `9` |
| `LEAST(10,'2')` | `10` |
| `GREATEST(10,'2')` | `2` |
| `GREATEST('1x',2)` | `2` |
| `LEAST('1x',2)` | `1x` |
| `GREATEST('foo',0)` | `foo` |
| `LEAST('foo',0)` | `0` |

The supported string subset follows MySQL's default collation behavior for
simple ASCII values:

- case-insensitive comparison
- trailing ASCII spaces ignored for comparison
- when strings compare equal, `GREATEST()` chooses the later equal argument
- when strings compare equal, `LEAST()` keeps the first equal argument

Verified examples:

| Expression | Result |
| --- | --- |
| `GREATEST('B','A','C')` | `C` |
| `LEAST('B','A','C')` | `A` |
| `GREATEST('11','45','2')` | `45` |
| `LEAST('11','45','2')` | `11` |
| `GREATEST('a','B')` | `B` |
| `LEAST('a','B')` | `a` |
| `GREATEST('a','A')` | `A` |
| `GREATEST('A','a')` | `a` |
| `LEAST('a','A')` | `a` |
| `LEAST('A','a')` | `A` |
| `HEX(GREATEST('a','a '))` | `6120` |
| `HEX(LEAST('a','a '))` | `61` |
| `HEX(GREATEST('a ','a'))` | `61` |
| `HEX(LEAST('a ','a'))` | `6120` |

Numeric-domain calls compare using the existing MyLite numeric value model.
Verified MySQL examples:

| Expression | Result |
| --- | --- |
| `GREATEST(11,45,2)` | `45` |
| `LEAST(11,45,2)` | `2` |
| `GREATEST(34.0,3,5)` | `34.0` |
| `LEAST(34.0,3,5)` | `3.0` |
| `GREATEST(CAST(-1 AS SIGNED), CAST(18446744073709551615 AS UNSIGNED))` | `18446744073709551615` |
| `LEAST(CAST(-1 AS SIGNED), CAST(18446744073709551615 AS UNSIGNED))` | `-1` |

## Metadata

Metadata follows the selected comparison domain:

- all-numeric integer calls return `LONGLONG`, binary collation, numeric flags,
  and `NOT_NULL` only when all arguments are non-nullable
- decimal-looking numeric literals return `NEWDECIMAL` metadata in MySQL; MyLite
  should preserve existing decimal metadata fidelity where the current
  descriptor system can infer it
- string and mixed string/numeric calls return `VAR_STRING` with the connection
  result collation and `NOT_NULL` only when all arguments are non-nullable
- `NULL` propagation makes the result nullable

Observed MySQL 8.4.9 metadata:

| Expression alias | Type | Length | Decimals | Collation | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `GREATEST(11,45,2) AS g` | `LONGLONG` | `3` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `LEAST(11,45,2) AS l` | `LONGLONG` | `3` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `GREATEST(34.0,3,5) AS g` | `NEWDECIMAL` | `5` | `1` | `binary` | `NOT_NULL BINARY NUM` |
| `LEAST(34.0,3,5) AS l` | `NEWDECIMAL` | `5` | `1` | `binary` | `NOT_NULL BINARY NUM` |
| `GREATEST(2,'9') AS g` after `SET NAMES utf8mb4` | `VAR_STRING` | `8` | `31` | `utf8mb4_0900_ai_ci` | `NOT_NULL` |
| `LEAST(10,'2') AS l` after `SET NAMES utf8mb4` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | `NOT_NULL` |
| `GREATEST(1,NULL,2) AS g` | `LONGLONG` | `2` | `0` | `binary` | `BINARY NUM` |
| `GREATEST(NULL,NULL) AS g` | `NULL` | `0` | `0` | `binary` | `BINARY NUM` |

## Errors and warnings

Verified MySQL 8.4.9 behavior:

- `GREATEST()` and `LEAST()` raise error 1582.
- `GREATEST(1)` and `LEAST(1)` raise error 1582.
- `GREATEST(1,NULL,2)` and `LEAST(1,NULL,2)` return `NULL` with no warnings.
- Mixed string/numeric calls listed above produce no warnings.
- `GREATEST('1x',2)+0` returns `2` with no warnings.
- `LEAST('1x',2)+0` returns `1` with no warnings.
- `GREATEST('foo',0)=0` and `LEAST('foo',0)=0` predicates produce no warnings
  on the verified runtime.
- `BINARY 'a'` changes the comparison domain to binary-string behavior, but the
  `BINARY` operator itself produces MySQL deprecation warnings. Full binary
  string collation behavior is deferred in this MyLite slice.

For supported DML paths in default strict mode:

- `UPDATE t SET n = GREATEST(s,7) WHERE id=2` with `s='20x'` stores `7` and
  produces no warnings.
- A string-domain result used directly by an outer numeric expression keeps its
  warning suppression, but once assigned into an `UPDATE` row value it behaves
  like an ordinary column value for later assignments in the same statement.
- `UPDATE t SET n = GREATEST(NULL,7)` for an `INT NOT NULL` column raises 1048
  and leaves the row unchanged.
- `DELETE FROM t WHERE LEAST(s,0)=0` deletes all non-`NULL` string rows in the
  verified fixture and produces no warnings.

## Storage and runtime implications

The feature has no file-format or catalog storage impact. It requires scalar
function registry entries, evaluator support, result metadata inference,
compatibility documentation, and runtime tests.

Runtime comparison must remain MyLite-owned. The implementation must not
delegate comparison to SQLite because MySQL's mixed string/numeric rule for
these functions differs from both SQLite comparison and MyLite's general
expression comparison helper.

## MySQL-runtime-verified tests

Implementation tests should cover:

- parser acceptance of mixed-case names and variadic two-or-more argument calls
- binding rejection through the current unsupported path for zero and one
  argument
- numeric integer results
- decimal-looking numeric metadata at the current MyLite fidelity level
- signed/unsigned edge values where feasible
- string-only results, including lexicographic numeric-looking strings
- mixed string/numeric results with no warnings
- `NULL` propagation and `NULL` short-circuit warning suppression
- default ASCII case-insensitive comparisons
- trailing-space comparison equality and tie-breaking
- metadata for numeric, string, mixed, and nullable calls
- projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and `WHERE` paths
- supported `DELETE` predicate paths
- `NOT NULL` assignment failure rollback
- source hygiene check proving no restrictive source references were introduced

Known current MyLite limitations:

- exact native 1582 arity diagnostics remain deferred with the broader
  scalar-function diagnostic surface
- full collation repertoire, accent sensitivity, character-set introducers,
  `COLLATE`, and binary-string aggregation are deferred
- temporal argument aggregation and comparison are deferred
- exact fixed-point runtime storage and exhaustive DECIMAL metadata aggregation
  remain part of broader numeric compatibility work

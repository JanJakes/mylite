# Expression operator foundation

## Scope

This feature specifies Task 16, the reusable expression operator foundation for
later `WHERE`, `UPDATE`, `DELETE`, scalar functions, grouping, joins, and richer
projection/default handling. It expands MyLite from the current narrow
arithmetic expression subset to a MySQL-compatible expression parser, AST,
value model, evaluator, and SQLite translation boundary for common scalar
operators.

In scope:

- comparison operators: `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, `>=`
- logical operators: `NOT`, `!`, `AND`, `&&`, `XOR`, `OR`, `||`
- bitwise operators: `~`, `&`, `|`, `^`, `<<`, `>>`
- null and boolean tests: `IS NULL`, `IS NOT NULL`, `IS TRUE`,
  `IS NOT TRUE`, `IS FALSE`, `IS NOT FALSE`, `IS UNKNOWN`,
  `IS NOT UNKNOWN`, and MySQL's `IS` / `IS NOT` boolean test forms
- range, pattern, and membership operators: `BETWEEN`, `NOT BETWEEN`, `LIKE`,
  `NOT LIKE`, optional `LIKE ... ESCAPE`, `IN`, and `NOT IN` over scalar
  expression lists
- arithmetic operators: unary `+`, unary `-`, binary `+`, `-`, `*`, `/`,
  `DIV`, `%`, and `MOD`
- MySQL operator precedence and left-to-right associativity for the in-scope
  non-assignment operators
- scalar value coercion rules needed by these operators, including three-valued
  logic, truthiness, numeric conversion warnings, decimal division scale,
  integer division, and unsigned 64-bit bitwise behavior
- reusable expression evaluation for existing expression call sites where the
  surrounding statement already has execution support, such as `SELECT` without
  table scans, projection expressions that do not require `WHERE`, default
  expressions, and `INSERT ... SET` assignment expressions
- deterministic unsupported diagnostics for expression shapes that remain
  outside this foundation

Out of scope:

- `WHERE` clause execution, row filtering, join predicates, `HAVING`, and
  `ON`; those features consume this expression foundation later
- table-backed arbitrary projection expressions that require broader result
  metadata than this task can expose safely, except for reusable literal and
  scalar expression support where current execution paths can handle it
- assignment operators `:=` and assignment-form `=`
- function calls, aggregate calls, window functions, `CASE`, `CAST`,
  `INTERVAL`, `BINARY`, `REGEXP`, `RLIKE`, `SOUNDS LIKE`,
  JSON operators, `MEMBER OF`, subqueries, quantified comparisons, `EXISTS`,
  variables, parameters, prepared statement marker binding, and stored-program
  local variables
- row constructors for comparison or `IN`
- full character set conversion, collation coercibility, PAD SPACE / NO PAD
  comparison details, and non-ASCII pattern matching beyond the current
  character-set foundation
- SQL-mode variants other than the explicitly documented parser behavior for
  default `||`, `PIPES_AS_CONCAT`, and `HIGH_NOT_PRECEDENCE`
- optimizer rewrites, constant folding that changes warning timing, and
  short-circuit elision that would suppress MySQL-observable warnings

Task 16 is not a statement feature. It should not mark `WHERE`, `UPDATE`,
`DELETE`, grouping, joins, or full result metadata as supported.

## Sources

- MySQL 8.4 Reference Manual, Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, Operator Precedence:
  https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, Logical Operators:
  https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html
- MySQL 8.4 Reference Manual, Arithmetic Operators:
  https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html
- MySQL 8.4 Reference Manual, Bit Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html
- MySQL 8.4 Reference Manual, String Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html
- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- Existing MyLite specs:
  - `docs/specs/mysql-lexer/specs.md`
  - `docs/specs/mysql-parser-scaffold/specs.md`
  - `docs/specs/column-attributes/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/character-set-collation-foundation/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.
  Metadata observations used `mysql --column-type-info -vvv`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Precedence and associativity

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT 1 + 2 * 3` | `7` |
| `SELECT (1 + 2) * 3` | `9` |
| `SELECT 1 | 2 & 0` | `1` |
| `SELECT 1 OR 0 AND 0` | `1` |
| `SELECT NOT 1 BETWEEN 0 AND 2` | `0` |
| `SELECT 1 + 2 << 1` | `6` |
| `SELECT 1 BETWEEN 0 AND 2 AND 0` | `0` |
| `SELECT 1 XOR 1 OR 1` | `1` |

The parser must encode MySQL's in-scope precedence, high to low:

1. `!` by default
2. unary `-`, unary `+`, `~`
3. `^`
4. `*`, `/`, `DIV`, `%`, `MOD`
5. binary `-`, binary `+`
6. `<<`, `>>`
7. `&`
8. `|`
9. comparisons, `IS`, `LIKE`, and `IN`
10. `BETWEEN`
11. `NOT`
12. `AND`, `&&`
13. `XOR`
14. `OR`, `||`

Operators at the same in-scope precedence evaluate left to right. Assignment
precedence is not part of this task.

### NULL, truthiness, and boolean tests

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT NULL = NULL` | `NULL` |
| `SELECT NULL <=> NULL` | `1` |
| `SELECT 1 <=> NULL` | `0` |
| `SELECT NULL <> 1` | `NULL` |
| `SELECT NULL IS NULL` | `1` |
| `SELECT NULL IS NOT NULL` | `0` |
| `SELECT 0 IS FALSE` | `1` |
| `SELECT 2 IS TRUE` | `1` |
| `SELECT NULL IS UNKNOWN` | `1` |

Logical operators use MySQL truthiness: zero is false, nonzero numeric values
are true, and `NULL` is unknown. `AND`, `OR`, `XOR`, and `NOT` return integer
truth values or `NULL` according to MySQL's three-valued logic. MyLite must not
rewrite expressions in a way that changes conversion warnings from operands
whose truthiness is inspected.

`<=>` is the null-safe equality operator. It returns `1` when both operands are
`NULL`, `0` when exactly one operand is `NULL`, and otherwise compares like
`=`.

### Temporal comparisons

When at least one operand is a typed `DATE`, `DATETIME`, or `TIMESTAMP` value,
MySQL compares valid date-compatible operands in the temporal domain rather
than as raw strings. A `DATE` value compares as midnight on that date. Plain
string-vs-string comparisons continue to use string ordering.

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT DATE '2024-02-29' = '2024-02-29 00:00:00'` | `1` |
| `SELECT DATE '2024-02-29' < '2024-02-29 12:00:00'` | `1` |
| `SELECT TIMESTAMP '2024-02-29 12:34:56.123456' > '2024-02-29 12:34:56.123455'` | `1` |
| `SELECT '2024-02-29' = '2024-02-29 00:00:00'` | `0` |
| `SELECT '2024-02-29' < '2024-02-29 00:00:00'` | `1` |

### `BETWEEN`

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT 2 BETWEEN 1 AND 3` | `1` |
| `SELECT 2 BETWEEN 3 AND 1` | `0` |
| `SELECT NULL BETWEEN 1 AND 3` | `NULL` |
| `SELECT 2 BETWEEN NULL AND 3` | `NULL` |
| `SELECT 2 BETWEEN 1 AND NULL` | `NULL` |
| `SELECT 2 BETWEEN 3 AND NULL` | `0` |
| `SELECT 2 BETWEEN NULL AND 1` | `0` |
| `SELECT 2 NOT BETWEEN 3 AND 1` | `1` |
| `SELECT 2 NOT BETWEEN 3 AND NULL` | `1` |
| `SELECT 2 NOT BETWEEN NULL AND 1` | `1` |

`BETWEEN` is inclusive and follows the three-valued result of
`low <= value AND value <= high`. `NOT BETWEEN` is the logical negation of
that result, preserving unknown results as `NULL`.

### `LIKE`

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT 'abc' LIKE 'a%'` | `1` |
| `SELECT 'abc' LIKE 'A%'` | `1` |
| `SELECT 'abc' LIKE BINARY 'A%'` | `0`, plus deprecation warning 1287 for `BINARY expr` |
| `SELECT 'a_c' LIKE 'a\\_c'` | `1` |
| `SELECT 'a\\c' LIKE 'a\\\\c'` | `1` |
| `SELECT 'abc' NOT LIKE 'a%'` | `0` |
| `SELECT 'a\\0b' LIKE 'a'` | `0` |
| `SELECT 'a\\0b' LIKE 'a%'` | `1` |
| `SELECT 'a\\0b' LIKE 'a_b'` | `1` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT HEX('a\\0b')` | `615C3062` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT 'a_c' LIKE 'a\\_c'` | `0` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT 'a\\_c' LIKE 'a\\_c'` | `1` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT 'a_c' LIKE 'a\\_c' ESCAPE CHAR(92)` | `1` |

Task 16 should implement `%`, `_`, default backslash escaping, and
`LIKE ... ESCAPE` for ASCII-compatible strings under the current default
connection collation. It must preserve `NULL` propagation when either operand
is `NULL`. Decoded NUL bytes are ordinary bytes for this slice's matcher:
they do not terminate the value or pattern.

When `NO_BACKSLASH_ESCAPES` is active, backslash remains ordinary string
content and `LIKE` does not use a default backslash escape. Explicit
`LIKE ... ESCAPE` still supplies an escape character.

`BINARY expr` is accepted as a deprecated binary-string cast and forces
case-sensitive `LIKE` evaluation for the covered ASCII-compatible string
subset. The cast preserves the source byte length, including embedded NUL
bytes, in the shared value model. Full collation-aware pattern semantics,
`COLLATE`, character set
introducers, and multibyte collation equivalence are deferred. Until those are
implemented, non-ASCII and explicit-collation pattern tests should remain
outside supported coverage.

### `IN`

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT 2 IN (1,2,3)` | `1` |
| `SELECT 4 IN (1,2,NULL)` | `NULL` |
| `SELECT 2 IN (NULL,2)` | `1` |
| `SELECT NULL IN (1,2)` | `NULL` |
| `SELECT 4 NOT IN (1,2,NULL)` | `NULL` |

An empty expression list is a syntax error:

- `SELECT 1 IN ()` fails with 1064 / SQLSTATE `42000`

Task 16 supports scalar expression-list `IN` only. Row-value `IN` is deferred
even though MySQL accepts forms such as `(1,2) IN ((1,2),(3,4))`. Row arity
diagnostics are also deferred; MySQL reports 1241 / SQLSTATE `21000` when row
operand cardinalities do not match.

### Arithmetic and bitwise operators

Representative runtime results:

| SQL | Result |
| --- | --- |
| `SELECT 5 DIV 2` | `2` |
| `SELECT 5 / 2` | `2.5000` |
| `SELECT 5 % 2` | `1` |
| `SELECT 5 MOD 2` | `1` |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) - 1` | `18446744073709551614` |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) DIV 2` | `9223372036854775807` |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) / 2` | `9223372036854775807.5000` |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) % 2` | `1` |
| `SELECT -9223372036854775808` | `-9223372036854775808` |
| `SELECT ~0` | `18446744073709551615` |
| `SELECT 1 << 63` | `9223372036854775808` |
| `SELECT 1 >> 1` | `0` |
| `SELECT '1x' | 0` | `1`, warning 1292 `Truncated incorrect INTEGER value: '1x'` |
| `SELECT '1.9' | 0, 1.9 | 0` | `1`, `2` |

Unsigned integer arithmetic uses MySQL's unsigned `BIGINT` range for exact
integer operands. Addition, subtraction, multiplication, and integer division
that overflow or underflow that range fail with error/warning 1690. `/` keeps
the MySQL four-decimal exact-division display for covered unsigned values.
Bitwise operators coerce operands to unsigned integers: string operands parse
an integer prefix and warn 1292 on trailing garbage, while approximate numeric
operands round half away from zero before applying the unsigned 64-bit operator.

Division by zero returns `NULL` and records warning 1365 for `/`, `DIV`, and
`MOD`/`%` under the verified SQL mode:

```sql
SELECT 1/0, 1 DIV 0, 1 % 0;
SHOW WARNINGS;
```

All three expressions return `NULL`; `SHOW WARNINGS` reports three
`Division by 0` warnings.

Bitwise operators use unsigned 64-bit integer behavior for numeric operands in
Task 16. Binary-string bit operations are deferred until MyLite has enough
binary string type and result metadata support to match MySQL's direct binary
string evaluation.

### Type conversion and warnings

Representative runtime results:

| SQL | Result | Warnings |
| --- | --- | --- |
| `SELECT '2' = 2` | `1` | none |
| `SELECT '2a' = 2` | `1` | 1292 truncated incorrect double value |
| `SELECT 'a' = 0` | `1` | 1292 truncated incorrect double value |
| `SELECT '10' < '2'` | `1` | none |
| `SELECT '10' < 2` | `0` | none |

Task 16 must introduce a reusable value model that can represent at least:

- `NULL`
- signed 64-bit integer
- unsigned 64-bit integer
- exact decimal sufficient for MySQL-style `/` display scale in the supported
  numeric cases, including unsigned integer division above signed 64-bit range
- approximate double for numeric conversion and warnings
- byte strings with charset/collation metadata placeholders
- boolean result values as integer `0` or `1`

Numeric conversion of strings should preserve MySQL's prefix-number behavior
and warning records for truncated nonnumeric suffixes. Full range clipping,
invalid temporal comparison diagnostics, JSON comparison order, and complete
decimal precision math are deferred unless directly needed by the verified
Task 16 tests. Hexadecimal and bit literals retain binary-string results in
string contexts but use MySQL's literal numeric value in covered numeric
operator contexts; hexadecimal literals enter unsigned integer arithmetic,
while bit literals enter signed integer arithmetic using their packed byte
value.

### SQL modes

By default, `||` behaves as logical `OR` and emits deprecation warning 1287.
With `PIPES_AS_CONCAT`, `||` is string concatenation and has different
precedence. Task 16 should parse and evaluate default `||` as logical `OR`.
If MyLite's parser already carries SQL-mode flags at prepare time, it may
recognize `PIPES_AS_CONCAT` as an unsupported expression operator with a clear
diagnostic. Full concatenation semantics wait for string functions/operators.

By default, `!` has higher precedence than `NOT`. `HIGH_NOT_PRECEDENCE` makes
their precedence equal in MySQL. Task 16 should implement the default
precedence and either reject `HIGH_NOT_PRECEDENCE`-dependent parsing as
unsupported or delay mode-sensitive parser changes until SQL mode management is
broader.

## MyLite behavior

### Parser and AST

The parser should extend the existing `expression` grammar in one place so all
statement grammars reuse the same operator tree. AST nodes should remain
statement-independent and source-span preserving.

Expression AST requirements:

- keep existing literal, qualified identifier, parenthesized, unary, and binary
  node shapes where they still fit
- add operator enum values for every in-scope operator
- add ternary or dedicated nodes for `BETWEEN` and `NOT BETWEEN`
- add dedicated nodes for `IS` tests so `IS NULL`, `IS TRUE`, and related
  forms do not need to masquerade as binary comparisons
- add `LIKE` nodes with optional escape expression
- add `IN` nodes that store the left expression plus an ordered nonempty
  expression list
- preserve source order for warning timing and later diagnostics
- leave row constructors, subqueries, variables, parameters, functions,
  collations, casts, and assignment operators outside the accepted grammar

The lexer already has symbolic operator tokens for the required symbolic
operators. Word operators such as `AND`, `OR`, `XOR`, `DIV`, `MOD`, `LIKE`,
`BETWEEN`, `IN`, `IS`, and `NOT` are keyword tokens and need parser-token
mapping in expression contexts.

### Lemon grammar snippets

These snippets describe MyLite's intended Task 16 grammar shape. They are not
copied from MySQL grammar.

```lemon
expression ::= logical_or_expression.

logical_or_expression ::= logical_xor_expression.
logical_or_expression ::= logical_or_expression OR logical_xor_expression.
logical_or_expression ::= logical_or_expression LOGICAL_OR logical_xor_expression.

logical_xor_expression ::= logical_and_expression.
logical_xor_expression ::= logical_xor_expression XOR logical_and_expression.

logical_and_expression ::= logical_not_expression.
logical_and_expression ::= logical_and_expression AND logical_not_expression.
logical_and_expression ::= logical_and_expression LOGICAL_AND logical_not_expression.

logical_not_expression ::= between_expression.
logical_not_expression ::= NOT logical_not_expression.

between_expression ::= comparison_expression.
between_expression ::= comparison_expression BETWEEN comparison_expression AND comparison_expression.
between_expression ::= comparison_expression NOT BETWEEN comparison_expression AND comparison_expression.

comparison_expression ::= bit_or_expression.
comparison_expression ::= comparison_expression comparison_operator bit_or_expression.
comparison_expression ::= comparison_expression IS is_test.
comparison_expression ::= comparison_expression IS NOT is_test.
comparison_expression ::= comparison_expression LIKE bit_or_expression opt_like_escape.
comparison_expression ::= comparison_expression NOT LIKE bit_or_expression opt_like_escape.
comparison_expression ::= comparison_expression IN LPAREN expression_list RPAREN.
comparison_expression ::= comparison_expression NOT IN LPAREN expression_list RPAREN.

comparison_operator ::= EQ.
comparison_operator ::= NULL_SAFE_EQ.
comparison_operator ::= NE.
comparison_operator ::= LT.
comparison_operator ::= LE.
comparison_operator ::= GT.
comparison_operator ::= GE.

is_test ::= NULL.
is_test ::= TRUE.
is_test ::= FALSE.
is_test ::= UNKNOWN.

opt_like_escape ::= .
opt_like_escape ::= ESCAPE expression.

expression_list ::= expression.
expression_list ::= expression_list COMMA expression.

bit_or_expression ::= bit_and_expression.
bit_or_expression ::= bit_or_expression BIT_OR bit_and_expression.

bit_and_expression ::= bit_shift_expression.
bit_and_expression ::= bit_and_expression BIT_AND bit_shift_expression.

bit_shift_expression ::= additive_expression.
bit_shift_expression ::= bit_shift_expression SHIFT_LEFT additive_expression.
bit_shift_expression ::= bit_shift_expression SHIFT_RIGHT additive_expression.

additive_expression ::= multiplicative_expression.
additive_expression ::= additive_expression PLUS multiplicative_expression.
additive_expression ::= additive_expression MINUS multiplicative_expression.

multiplicative_expression ::= bit_xor_expression.
multiplicative_expression ::= multiplicative_expression STAR bit_xor_expression.
multiplicative_expression ::= multiplicative_expression SLASH bit_xor_expression.
multiplicative_expression ::= multiplicative_expression DIV bit_xor_expression.
multiplicative_expression ::= multiplicative_expression PERCENT bit_xor_expression.
multiplicative_expression ::= multiplicative_expression MOD bit_xor_expression.

bit_xor_expression ::= unary_expression.
bit_xor_expression ::= bit_xor_expression BIT_XOR unary_expression.

unary_expression ::= primary_expression.
unary_expression ::= PLUS unary_expression.
unary_expression ::= MINUS unary_expression.
unary_expression ::= BIT_NOT unary_expression.
unary_expression ::= LOGICAL_NOT unary_expression.

primary_expression ::= literal.
primary_expression ::= qualified_identifier.
primary_expression ::= LPAREN expression RPAREN.
```

The snippet puts `^` above multiplication by routing multiplicative operands
through `bit_xor_expression`, matching the verified MySQL precedence. The
actual Lemon file may use precedence declarations instead, but tests must prove
the same parse tree.

Deferred grammar should remain rejected:

```lemon
/* Deferred: row constructors and row comparisons. */
primary_expression ::= ROW LPAREN expression_list RPAREN.
primary_expression ::= LPAREN expression COMMA expression_list RPAREN.

/* Deferred: subqueries and quantified comparisons. */
comparison_expression ::= comparison_expression IN LPAREN select_statement RPAREN.
comparison_expression ::= comparison_expression comparison_operator ANY LPAREN select_statement RPAREN.

/* Deferred: mode-sensitive concatenation and broader cast targets. */
primary_expression ::= CAST LPAREN expression AS cast_type RPAREN.
```

### Analyzer and evaluator

Task 16 should introduce a reusable expression API instead of continuing to
grow statement-local ad hoc evaluators. A statement runtime should be able to:

1. bind an expression to a name-resolution context, or to a no-table context
   for standalone scalar expressions
2. evaluate it against zero or one current row, depending on caller support
3. retrieve a `mylite_value` result plus warnings
4. ask for conservative result metadata where the caller needs it
5. request SQLite translation only when the expression is known safe to lower

Expression evaluation should be deterministic and should append warnings in
left-to-right operand evaluation order. Do not fold constants at prepare time
unless warning timing, SQL mode, and diagnostics match MySQL for every folded
operator.

### SQLite translation

SQLite may be used as an execution substrate only behind a MySQL-semantic
wrapper. Direct SQLite operators are unsafe for several in-scope cases:

- `NULL` truth tables and `<=>`
- string-to-number conversion warnings
- `LIKE` escaping and collation-sensitive matching
- unsigned 64-bit bitwise results
- decimal division scale and division-by-zero warnings

The first implementation should prefer a MyLite-owned evaluator for expression
semantics. Later optimizer work may lower safe subexpressions to SQLite when a
proof exists that MySQL-visible results, warnings, and metadata are unchanged.

### Diagnostics and warnings

Task 16 must support warning records for expression evaluation paths that can
currently expose warnings. Required warning cases include:

- 1292 truncated numeric conversion for string-to-number conversion
- 1365 division by zero for `/`, `DIV`, `%`, and `MOD`
- 1287 default `||` logical-OR deprecation when that warning surface is wired

If MyLite's current diagnostics layer cannot expose warning result sets yet,
the implementation must still store warnings in the statement/session warning
model designed for Task 40 and document temporary exposure gaps in
`COMPATIBILITY.md`.

Unsupported expression shapes should fail deterministically with
`MYLITE_UNSUPPORTED` or a MySQL-compatible syntax/semantic diagnostic depending
on whether MySQL accepts the syntax.

### Metadata

Task 16 should infer enough metadata for supported scalar expressions to avoid
mislabeling basic result columns:

- result labels should continue to follow select-item alias rules from Task 15
- comparison, logical, `IS`, `BETWEEN`, `LIKE`, and `IN` return integer
  truth values with MySQL-like `LONGLONG` metadata
- `/` returns decimal metadata for exact numeric operands in the verified
  simple cases
- `DIV`, modulo, integer arithmetic, and numeric bitwise operators return
  integer metadata, with bitwise numeric results treated as unsigned where
  MySQL does so. Verified scalar bitwise results use unsigned `LONGLONG`
  metadata with length `21`, binary collation id `63`, decimals `0`, and
  `NOT_NULL UNSIGNED BINARY NUM` flags for non-null operands.
- expression origins are empty for computed expressions

Full expression metadata remains Task 23. Task 16 may expose conservative
metadata internally, but public metadata claims should remain limited to
verified behavior.

### Storage and runtime implications

This feature does not change the `.mylite` file format or persistent catalog.
It may affect stored expression text indirectly because default expressions,
generated columns, indexes on expressions, checks, and future views will reuse
the same parser and AST.

Runtime state additions should be handle- or statement-owned:

- SQL mode bits that affect parsing or evaluation
- warning records
- expression evaluation scratch allocations
- no mutable process-global expression state

The evaluator should avoid per-row heap churn for common scalar paths. Where
strings or decimals require allocation, use caller-owned or statement-owned
scratch storage with clear lifetime rules.

## MySQL-runtime-verified test expectations

Implementation tests should compare MyLite against MySQL 8.4.9 for at least
these cases.

### Precedence

| SQL | Expected result |
| --- | --- |
| `SELECT 1 + 2 * 3` | `7` |
| `SELECT (1 + 2) * 3` | `9` |
| `SELECT 1 | 2 & 0` | `1` |
| `SELECT 1 OR 0 AND 0` | `1` |
| `SELECT NOT 1 BETWEEN 0 AND 2` | `0` |
| `SELECT 1 + 2 << 1` | `6` |
| `SELECT 1 BETWEEN 0 AND 2 AND 0` | `0` |
| `SELECT 1 XOR 1 OR 1` | `1` |

### NULL, comparisons, and truthiness

| SQL | Expected result |
| --- | --- |
| `SELECT NULL = NULL` | `NULL` |
| `SELECT NULL <=> NULL` | `1` |
| `SELECT 1 <=> NULL` | `0` |
| `SELECT NULL <> 1` | `NULL` |
| `SELECT NULL IS NULL` | `1` |
| `SELECT NULL IS NOT NULL` | `0` |
| `SELECT 0 IS FALSE` | `1` |
| `SELECT 2 IS TRUE` | `1` |
| `SELECT NULL IS UNKNOWN` | `1` |

### Arithmetic and bitwise

| SQL | Expected result | Expected warnings |
| --- | --- | --- |
| `SELECT 5 DIV 2` | `2` | none |
| `SELECT 5 / 2` | `2.5000` | none |
| `SELECT 5 % 2` | `1` | none |
| `SELECT 5 MOD 2` | `1` | none |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) - 1` | `18446744073709551614` | none |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) DIV 2` | `9223372036854775807` | none |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) / 2` | `9223372036854775807.5000` | none |
| `SELECT CAST(18446744073709551615 AS UNSIGNED) + 1` | error | 1690 |
| `SELECT CAST(0 AS UNSIGNED) - 1` | error | 1690 |
| `SELECT CAST(9223372036854775808 AS UNSIGNED) * 2` | error | 1690 |
| `SELECT ~0` | `18446744073709551615` | none |
| `SELECT 1 << 63` | `9223372036854775808` | none |
| `SELECT 1/0, 1 DIV 0, 1 % 0` | `NULL`, `NULL`, `NULL` | three 1365 warnings |

### `BETWEEN`, `LIKE`, and `IN`

| SQL | Expected result |
| --- | --- |
| `SELECT 2 BETWEEN 1 AND 3` | `1` |
| `SELECT 2 BETWEEN 3 AND 1` | `0` |
| `SELECT NULL BETWEEN 1 AND 3` | `NULL` |
| `SELECT 2 BETWEEN NULL AND 3` | `NULL` |
| `SELECT 2 BETWEEN 3 AND NULL` | `0` |
| `SELECT 2 BETWEEN NULL AND 1` | `0` |
| `SELECT 2 NOT BETWEEN 3 AND 1` | `1` |
| `SELECT 2 NOT BETWEEN 3 AND NULL` | `1` |
| `SELECT 2 NOT BETWEEN NULL AND 1` | `1` |
| `SELECT 'abc' LIKE 'a%'` | `1` |
| `SELECT 'abc' LIKE 'A%'` | `1` |
| `SELECT 'a_c' LIKE 'a\\_c'` | `1` |
| `SELECT 'abc' NOT LIKE 'a%'` | `0` |
| `SELECT 'a\\0b' LIKE 'a'` | `0` |
| `SELECT 'a\\0b' LIKE 'a%'` | `1` |
| `SELECT 'a\\0b' LIKE 'a_b'` | `1` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT HEX('a\\0b')` | `615C3062` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT 'a_c' LIKE 'a\\_c'` | `0` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT 'a\\_c' LIKE 'a\\_c'` | `1` |
| `SET sql_mode='NO_BACKSLASH_ESCAPES'; SELECT 'a_c' LIKE 'a\\_c' ESCAPE CHAR(92)` | `1` |
| `SELECT 2 IN (1,2,3)` | `1` |
| `SELECT 4 IN (1,2,NULL)` | `NULL` |
| `SELECT 2 IN (NULL,2)` | `1` |
| `SELECT NULL IN (1,2)` | `NULL` |
| `SELECT 4 NOT IN (1,2,NULL)` | `NULL` |

### Type conversion warnings

| SQL | Expected result | Expected warnings |
| --- | --- | --- |
| `SELECT '2' = 2` | `1` | none |
| `SELECT '2a' = 2` | `1` | one 1292 warning |
| `SELECT 'a' = 0` | `1` | one 1292 warning |
| `SELECT '12\\03' + 0` | `12` | one 1292 warning |
| `SELECT '10' < '2'` | `1` | none |
| `SELECT '10' < 2` | `0` | none |

### Metadata spot checks

Using `mysql --column-type-info -vvv`, MySQL reports:

| Expression | Type | Flags summary |
| --- | --- | --- |
| `1 + 2 AS sum_expr` | `LONGLONG` | `NOT_NULL BINARY NUM` |
| `5 / 2 AS div_expr` | `NEWDECIMAL` | `BINARY NUM`, decimals `4` |
| `5 DIV 2 AS div_int` | `LONGLONG` | `BINARY NUM` |
| `1 = 1 AS eq_expr` | `LONGLONG` | `NOT_NULL BINARY NUM` |
| `NULL <=> NULL AS nullsafe_expr` | `LONGLONG` | `NOT_NULL BINARY NUM` |
| `'abc' LIKE 'a%' AS like_expr` | `LONGLONG` | `NOT_NULL BINARY NUM` |
| `1 & 3 AS bit_expr` | `LONGLONG` | `NOT_NULL UNSIGNED BINARY NUM` |

Task 16 tests should assert exact metadata only where MyLite exposes the
relevant public metadata. Internal analyzer tests should still record expected
metadata so Task 23 has a verified handoff.

### Deferred edge cases

These MySQL-observed cases should be test fixtures marked unsupported until
their later tasks:

| SQL | MySQL behavior | MyLite Task 16 behavior |
| --- | --- | --- |
| `SELECT 1 IN ()` | syntax error 1064 / `42000` | parse error |
| `SELECT (1,2) IN ((1,2),(3,4))` | returns `1` | unsupported row constructor |
| `SELECT (1,2) = (1,2)` | returns `1` | unsupported row constructor |
| `SELECT (1,2) = (1,2,3)` | error 1241 / `21000` | unsupported row constructor |
| `SELECT 'abc' LIKE BINARY 'A%'` | returns `0`, warning 1287 | supported |
| `SET sql_mode = CONCAT(@@sql_mode, ',PIPES_AS_CONCAT'); SELECT 'a' || 'b'` | returns `ab` | unsupported mode-sensitive concatenation |

## Known incompatibilities and deferred behavior

- `WHERE` and other predicate consumers remain unsupported even after the
  expression evaluator exists.
- Full collation and character-set coercion are deferred; Task 16 covers only
  the verified default-collation ASCII-compatible cases.
- Binary-string bit operations are deferred.
- Function calls, casts, `CASE`, `INTERVAL`, regular expressions, JSON,
  variables, parameters, subqueries, and row constructors are deferred.
- SQL-mode-sensitive `||` concatenation and `HIGH_NOT_PRECEDENCE` parsing are
  deferred unless MyLite already has a complete SQL-mode parse context when
  implementation starts.
- Warning storage may be implemented before `SHOW WARNINGS` can expose it.
  Compatibility docs must make that exposure gap explicit if it exists.

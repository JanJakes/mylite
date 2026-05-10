# Baseline Scalar Logical Projection

This phase extends MyLite's no-source and `FROM DUAL` scalar projection lane
with a small keyword logical-expression subset:

```sql
SELECT logical_scalar[, logical_scalar ...]
SELECT ALL logical_scalar[, logical_scalar ...]
SELECT logical_scalar[, logical_scalar ...] FROM DUAL
SELECT ALL logical_scalar[, logical_scalar ...] FROM DUAL
```

The admitted logical operators are keyword `NOT`, keyword `AND`, keyword
`XOR`, and keyword `OR`. Operands use the current MyLite-owned signed-64 scalar
expression domain:

- decimal integer, `TRUE`, `FALSE`, and `NULL` literals;
- supported scalar `IF()` / `IFNULL()` / `COALESCE()` / `NULLIF()` /
  `ISNULL()` calls;
- parenthesized admitted expressions;
- signed-64 unary `+` / `-`, binary `+` / `-`, `*`, `%`, infix `MOD`,
  `MOD(left, right)`, and infix `DIV`;
- signed-64 scalar comparison operators `=`, `<=>`, `<>`, `!=`, `<`, `<=`,
  `>`, and `>=`; and
- nested admitted logical expressions.

Arithmetic operands remain the current arithmetic subset. Logical results are
not admitted as operands to arithmetic operators in this phase.

This is still not a general expression engine. This phase does not admit
table-backed expression projection, bare `WHERE`-style predicates in scalar
projection, symbolic `!`, deprecated scalar `&&` or `||`, `PIPES_AS_CONCAT`,
`IS`, `LIKE`, `REGEXP`, `IN`, `BETWEEN`, bitwise operators, `/`, row
constructors, strings, decimals, floats, hex, bit, temporal values, collation or
string truth conversion, user variables, parameters, subqueries, CTEs, DML
assignment expressions, expression metadata, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Logical operators:
    <https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - Expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_scalar_logical_projection_expectations.sh`.

The MySQL 8.4 manual describes logical operators as returning `1`, `0`, or
`NULL`, and describes nonzero non-`NULL` operands as true. It places keyword
`NOT` below comparison operators and above `AND`; `AND` binds above `XOR`, and
`XOR` binds above `OR`. Runtime probes against MySQL 8.4.9 establish these
expectations for this slice:

- keyword `NOT` returns `1` for zero operands, `0` for nonzero operands, and
  `NULL` for `NULL`;
- keyword `AND` returns `0` if either evaluated operand is false, returns `1`
  if both evaluated operands are true, and otherwise returns `NULL`;
- keyword `OR` returns `1` if either evaluated operand is true, returns `0` if
  both evaluated operands are false, and otherwise returns `NULL`;
- keyword `XOR` returns `NULL` if either evaluated operand is `NULL`, otherwise
  returns `1` when exactly one operand is true and `0` when both operands have
  the same truth value;
- operands are evaluated left to right, with MySQL-observed short-circuiting:
  - `left AND right` skips `right` when `left` evaluates to false;
  - `left OR right` skips `right` when `left` evaluates to true;
  - `left XOR right` skips `right` when `left` evaluates to `NULL`;
  - `NOT operand` evaluates its operand;
- evaluated arithmetic operands raise overflow diagnostics before logical
  evaluation can return a value;
- evaluated `DIV` or modulo-by-zero children stage one warning per evaluated
  child expression and return `NULL` into logical evaluation;
- logical expressions in a scalar `SELECT` do not let child division-by-zero
  warnings affect `@@warning_count` or `ROW_COUNT()` values evaluated inside
  the same select list; those reads use the previous statement snapshot;
- after a successful logical select with evaluated child division-by-zero
  warnings, the public result warning count and following diagnostics snapshot
  count those warnings;
- MySQL accepts broader forms such as scalar `&&`, scalar `||`, symbolic `!`,
  table-backed expression projection, string and numeric coercions outside the
  signed-64 domain, and SQL-mode-dependent `||`. Those forms remain outside
  this phase.

## Ownership Boundaries

- Public API: unchanged. Successful supported statements return one synthesized
  row through existing `mylite_execute()` / `mylite_result` text-result
  conventions.
- Statement context: owns previous row-count and diagnostics-count snapshots.
  Logical evaluation must preserve the existing rule that diagnostics count
  variables read the previous statement snapshot while current statement child
  warnings are still staged.
- Lexer/parser/AST: keyword logical operators become expression AST nodes for
  the no-source scalar-expression lane. Existing predicate grammar remains
  separate and unchanged for descriptor-backed `WHERE`.
- Analyzer/runtime: scalar projection admission accepts logical expressions
  only when every child is admitted by this feature's scalar expression domain.
  Runtime evaluation is MyLite-owned and checked for signed-64 operand
  conversion hazards inherited from children.
- Diagnostics: logical operators themselves are warning-free for the admitted
  keyword subset. Warnings staged by evaluated child arithmetic are preserved
  and appended at the normal scalar projection statement boundary.
- Catalog: not involved. The feature must not read or mutate schema/table
  descriptors, descriptor versions, descriptor caches, catalog generation, or
  SQLite schema generation.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite: not involved. This is wrapper/runtime behavior and must not depend on
  SQLite expression evaluation or SQLite fork changes.

## Syntax

MyLite admits these source forms:

```sql
SELECT logical_item[, logical_item ...]
SELECT ALL logical_item[, logical_item ...]
SELECT logical_item[, logical_item ...] FROM DUAL
SELECT ALL logical_item[, logical_item ...] FROM DUAL

logical_item:
    logical_scalar
  | logical_scalar AS alias
  | logical_scalar alias

logical_scalar:
    logical_or

logical_or:
    logical_xor
  | logical_or OR logical_xor

logical_xor:
    logical_and
  | logical_xor XOR logical_and

logical_and:
    logical_not
  | logical_and AND logical_not

logical_not:
    comparison_scalar
  | NOT logical_not

comparison_scalar:
    current scalar comparison and arithmetic expression subset
  | ( logical_scalar )
```

Keyword logical results are integer scalar values (`1` or `0`) or `NULL`, so
they may participate in later admitted comparison or logical operations. They
remain excluded from arithmetic operands in this slice.

## MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
%left OR.
%left XOR.
%left AND.
%right NOT.
%left EQUAL NULL_SAFE_EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%right UPLUS UMINUS.

expression(A) ::= NOT(T) expression(B). [NOT] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, B);
}
expression(A) ::= expression(B) AND(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, C);
}
expression(A) ::= expression(B) XOR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, C);
}
expression(A) ::= expression(B) OR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, C);
}
```

The same operator enum values remain used by descriptor-backed predicates, but
this phase does not change predicate planning or table-backed SQL generation.
Deprecated `LOGICAL_AND` (`&&`), deprecated `LOGICAL_OR` (`||`), and symbolic
`!` are intentionally absent from the scalar expression grammar for this phase.

## Runtime Semantics

Runtime evaluation uses an iterative expression stack shaped like the existing
scalar arithmetic and comparison evaluators.

1. Admit the statement only when every select item is an existing session
   scalar item, an existing scalar value expression, or a supported scalar
   arithmetic/comparison/logical expression.
2. Preserve existing scalar function wrong-arity diagnostics before generic
   logical unsupported diagnostics.
3. Evaluate operands left to right, applying the short-circuit rules verified
   above.
4. Convert supported non-`NULL` operands to signed 64-bit integer values.
   `TRUE` and `FALSE` become `1` and `0` through the existing scalar operand
   conversion path. A value is true when nonzero and false when zero.
5. Accumulate staged division-by-zero warnings only from evaluated operands.
6. For `NOT`, return `NULL` when the operand is `NULL`; otherwise return `1`
   for zero and `0` for nonzero.
7. For `AND`, return `0` immediately if the left operand is non-`NULL` zero.
   Otherwise evaluate the right operand. Return `0` if the right operand is
   non-`NULL` zero, `1` if both operands are nonzero, and `NULL` otherwise.
8. For `OR`, return `1` immediately if the left operand is nonzero. Otherwise
   evaluate the right operand. Return `1` if the right operand is nonzero, `0`
   if both operands are non-`NULL` zero, and `NULL` otherwise.
9. For `XOR`, return `NULL` immediately if the left operand is `NULL`.
   Otherwise evaluate the right operand. Return `NULL` if the right operand is
   `NULL`, otherwise return `1` when exactly one operand is true and `0` when
   both operands have the same truth value.
10. Format non-`NULL` logical results as `1` or `0`.
11. Append staged division-by-zero warnings only after all scalar select-item
    values have been evaluated, before completing the result.

Successful supported logical `SELECT` statements return one synthesized row,
`affected_rows == 0`, and a result `warning_count` equal to staged child
division-by-zero warnings. Keyword logical operators record no warnings.

Default result-column labels use the existing source-span convention. Explicit
aliases override the default label.

## Diagnostics

| Condition | Result |
| --- | --- |
| Lexer or parser error | MySQL-style syntax error `1064` / SQLSTATE `42000` |
| Unsupported scalar logical expression shape | MyLite deterministic unsupported error `1064` / `42000` |
| Unsupported operand literal or expression | MyLite deterministic unsupported scalar logical or scalar projection error |
| Wrong scalar function arity in an admitted child | Existing function-specific MySQL-compatible error |
| Child arithmetic overflow | Existing `1690` / `22003` overflow diagnostic |
| Child division/modulo by zero in an evaluated operand | Successful statement with warning `1365` / `22012`; child value is `NULL` |
| Deprecated scalar `&&`, `||`, or symbolic `!` | Unsupported in this phase; no deprecation-warning support is claimed |
| Table-backed expression projection | Existing table-backed projection unsupported diagnostic |
| Allocation failure | `MYLITE_NOMEM` and handle-owned allocation diagnostic |
| Public API misuse | Existing public API misuse behavior; no public surface changes |

## Performance And Storage

Logical evaluation is O(number of evaluated AST nodes) and uses bounded dynamic
stacks like the current scalar evaluators. Short-circuiting avoids evaluating
right-hand subtrees when MySQL 8.4.9 behavior proves they are not evaluated.

The feature does not read or write SQLite tables, does not generate SQLite SQL,
does not touch catalog rows, and does not require SQLite fork patches.

## Test Plan

Add a MySQL-runtime expectation script and fast C tests covering:

- keyword `NOT`, `AND`, `OR`, and `XOR` truth tables over integer, boolean, and
  `NULL` operands;
- nonzero truthiness for positive and negative integers;
- precedence and associativity with arithmetic and comparison operands;
- short-circuit behavior with warning-producing `DIV` children, including
  `0 AND 5 DIV 0`, `1 OR 5 DIV 0`, `NULL XOR 5 DIV 0`, and evaluated
  warning-producing right operands;
- preservation of `@@warning_count`, `ROW_COUNT()`, result warning count, and
  following `SHOW WARNINGS` behavior;
- scalar function operands and explicit aliases;
- signed boundary operands already admitted by the arithmetic/comparison
  slices;
- deterministic rejection of unsupported scalar logical forms: `&&`, `||`,
  `!`, strings, decimals, hex, bit literals, session variables as logical
  operands, table-backed expression projection, row constructors, parameters,
  subqueries, `IS`, `BETWEEN`, and `IN`;
- parser AST shape and precedence for keyword logical operators;
- file preamble, catalog generation, SQLite schema generation, and independent
  handle invariants; and
- zero-initialized cleanup for any new evaluator stack.

Focused verification:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'libmylite\.(parser|runtime\.scalar_logical_projection|runtime\.scalar_comparison_projection|runtime\.scalar_div_projection|runtime\.scalar_modulo_projection|runtime\.scalar_arithmetic_projection|runtime\.scalar_expression_projection|runtime\.session_value_scalar_projection)'
./packages/libmylite/tests/mysql_baseline_scalar_logical_projection_expectations.sh
cmake --workflow --preset check
```

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/operators.md`, and
`docs/compatibility/sql-query-expressions.md` only for the exact keyword
logical scalar projection subset.

Do not claim full logical expressions, scalar `&&`, scalar `||`, symbolic `!`,
SQL-mode-dependent `PIPES_AS_CONCAT`, table-backed expression projection,
general truth conversion, row constructors, subqueries, full predicate reuse,
or arbitrary expression evaluation.

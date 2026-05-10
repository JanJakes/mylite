# Baseline Scalar IS Projection

This phase extends MyLite's no-source and `FROM DUAL` scalar projection lane
with a deliberately small scalar `IS` subset:

```sql
SELECT is_scalar[, is_scalar ...]
SELECT ALL is_scalar[, is_scalar ...]
SELECT is_scalar[, is_scalar ...] FROM DUAL
SELECT ALL is_scalar[, is_scalar ...] FROM DUAL
```

The admitted operators are `IS NULL`, `IS NOT NULL`, `IS TRUE`,
`IS NOT TRUE`, `IS FALSE`, `IS NOT FALSE`, `IS UNKNOWN`, and
`IS NOT UNKNOWN`. Operands use the current MyLite-owned signed-64 scalar
expression domain:

- decimal integer, `TRUE`, `FALSE`, and `NULL` literals;
- supported scalar `IF()` / `IFNULL()` / `COALESCE()` / `NULLIF()` /
  `ISNULL()` calls;
- parenthesized admitted expressions;
- signed-64 unary `+` / `-`, binary `+` / `-`, `*`, `%`, infix `MOD`,
  `MOD(left, right)`, and infix `DIV`;
- signed-64 scalar comparison operators `=`, `<=>`, `<>`, `!=`, `<`, `<=`,
  `>`, and `>=`;
- keyword logical `NOT`, `AND`, `XOR`, and `OR`; and
- parenthesized nested scalar `IS` expressions where the nested result is used
  as an operand of another admitted scalar expression.

This is still not a general expression engine. This phase does not admit
table-backed expression projection, descriptor-column scalar `IS` projection,
unparenthesized direct chaining after a scalar `IS` expression, symbolic `!`,
deprecated scalar `&&` or `||`, `PIPES_AS_CONCAT`, `LIKE`, `REGEXP`,
expression-level `IN` or `BETWEEN`, bitwise operators, `/`, row constructors,
strings, decimals, floats, hex, bit, temporal values, collation or string truth
conversion, user variables, parameters, subqueries, CTEs, DML assignment
expressions, expression metadata, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Comparison operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - Expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_scalar_is_projection_expectations.sh`.

The MySQL 8.4 manual documents `IS boolean_value` and
`IS NOT boolean_value` for `TRUE`, `FALSE`, and `UNKNOWN`, and documents
`IS NULL` / `IS NOT NULL` as null tests. It places `IS` with comparison
operators, below arithmetic and above `BETWEEN`, `NOT`, `AND`, `XOR`, and
`OR`.

Runtime probes against MySQL 8.4.9 establish these expectations for this
slice:

- `expr IS NULL` returns `1` only when `expr` evaluates to `NULL`;
- `expr IS NOT NULL` returns the inverse of `IS NULL`;
- `expr IS TRUE` returns `1` for non-`NULL` nonzero operands and `0` for zero
  or `NULL`;
- `expr IS FALSE` returns `1` for non-`NULL` zero operands and `0` for nonzero
  or `NULL`;
- `expr IS UNKNOWN` is equivalent to `expr IS NULL`;
- `IS NOT TRUE`, `IS NOT FALSE`, and `IS NOT UNKNOWN` return the inverse of the
  corresponding non-`NOT` test;
- scalar `IS` results are integer `1` or `0`, never `NULL`;
- evaluated arithmetic operands raise overflow diagnostics before scalar `IS`
  can return a value;
- evaluated `DIV` or modulo-by-zero children stage one warning per evaluated
  child expression and feed `NULL` into scalar `IS` evaluation;
- scalar `IS` expressions in a scalar `SELECT` do not let child division-by-zero
  warnings affect `@@warning_count` or `ROW_COUNT()` values evaluated inside
  the same select list; those reads use the previous statement snapshot;
- after a successful scalar `IS` select with evaluated child division-by-zero
  warnings, the public result warning count and following diagnostics snapshot
  count those warnings;
- logical operands preserve the previously implemented MySQL-observed
  short-circuit behavior before scalar `IS` tests their result;
- `NOT 1 IS TRUE` is parsed as `NOT (1 IS TRUE)`, so scalar `IS` binds tighter
  than keyword `NOT`;
- `1 = 1 IS TRUE` is parsed as `(1 = 1) IS TRUE`;
- `1 IS TRUE AND 0`, `1 AND 0 IS FALSE`, `1 IS TRUE XOR 0`, and
  `0 OR NULL IS UNKNOWN` follow the documented precedence ordering;
- MySQL rejects direct `IS TRUE` chaining such as `1 IS TRUE IS TRUE` and
  direct comparison after `IS TRUE` such as `1 IS TRUE = 1`, while accepting
  the corresponding parenthesized forms;
- MySQL accepts some broader `IS NULL` chaining and comparison forms such as
  `1 IS NULL IS TRUE` and `1 IS NULL = 0`; those grammar distinctions are
  deferred by this phase unless the first scalar `IS` result is explicitly
  parenthesized; and
- MySQL accepts broader operands such as strings, decimals, floats, hex, bit,
  and table-backed expressions. Those forms remain outside this phase.

## Ownership Boundaries

- Public API: unchanged. Successful supported statements return one synthesized
  row through existing `mylite_execute()` / `mylite_result` text-result
  conventions.
- Statement context: owns previous row-count and diagnostics-count snapshots.
  Scalar `IS` evaluation must preserve the existing rule that diagnostics
  count variables read the previous statement snapshot while current statement
  child warnings are still staged.
- Lexer/parser/AST: scalar `IS` operators become expression AST nodes for the
  no-source scalar-expression lane. Existing descriptor-backed predicate
  grammar remains separate and unchanged.
- Analyzer/runtime: scalar projection admission accepts scalar `IS`
  expressions only when every child is admitted by this feature's scalar
  expression domain. Runtime evaluation is MyLite-owned and checked for
  signed-64 operand conversion hazards inherited from children.
- Diagnostics: scalar `IS` operators themselves are warning-free for the
  admitted subset. Warnings staged by evaluated child arithmetic are preserved
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
SELECT is_item[, is_item ...]
SELECT ALL is_item[, is_item ...]
SELECT is_item[, is_item ...] FROM DUAL
SELECT ALL is_item[, is_item ...] FROM DUAL

is_item:
    is_scalar
  | is_scalar AS alias
  | is_scalar alias

is_scalar:
    is_operand IS NULL
  | is_operand IS NOT NULL
  | is_operand IS TRUE
  | is_operand IS NOT TRUE
  | is_operand IS FALSE
  | is_operand IS NOT FALSE
  | is_operand IS UNKNOWN
  | is_operand IS NOT UNKNOWN

is_operand:
    current scalar value, function, arithmetic, comparison, or logical subset
  | ( is_scalar )
```

Parenthesized scalar `IS` results are integer scalar values (`1` or `0`), so
they may participate in later admitted comparison, logical, or scalar `IS`
operations. Unparenthesized direct chaining after a scalar `IS` result remains
outside this phase, including the broader `IS NULL` chaining forms that MySQL
accepts.

## MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
%left OR.
%left XOR.
%left AND.
%right NOT.
%left EQUAL NULL_SAFE_EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL IS.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%right UPLUS UMINUS.

expression(A) ::= expression(B) IS(T) NULL(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NULL, C);
}
expression(A) ::= expression(B) IS(T) NOT NULL(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, C);
}
expression(A) ::= expression(B) IS(T) TRUE(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_TRUE, C);
}
expression(A) ::= expression(B) IS(T) NOT TRUE(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE, C);
}
expression(A) ::= expression(B) IS(T) FALSE(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_FALSE, C);
}
expression(A) ::= expression(B) IS(T) NOT FALSE(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE, C);
}
expression(A) ::= expression(B) IS(T) UNKNOWN(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN, C);
}
expression(A) ::= expression(B) IS(T) NOT UNKNOWN(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN, C);
}
```

The same operator enum values remain used by descriptor-backed predicates, but
this phase does not change predicate planning or table-backed SQL generation.

## Runtime Semantics

Runtime evaluation uses the existing scalar logical/comparison/arithmetic
operand evaluators and adds scalar `IS` truth tests over their results.

1. Admit the statement only when every select item is an existing session
   scalar item, an existing scalar value expression, or a supported scalar
   arithmetic/comparison/logical/`IS` expression.
2. Preserve existing scalar function wrong-arity diagnostics before generic
   scalar `IS` unsupported diagnostics.
3. Evaluate the operand expression using the same left-to-right and
   short-circuit rules already specified for admitted child expressions.
4. Convert supported non-`NULL` operands to signed 64-bit integer values.
   `TRUE` and `FALSE` become `1` and `0` through the existing scalar operand
   conversion path.
5. Accumulate staged division-by-zero warnings only from evaluated child
   expressions.
6. For `IS NULL`, return `1` when the operand is `NULL`, otherwise `0`.
7. For `IS NOT NULL`, return `0` when the operand is `NULL`, otherwise `1`.
8. For `IS TRUE`, return `1` when the operand is non-`NULL` and nonzero,
   otherwise `0`.
9. For `IS NOT TRUE`, return `0` when the operand is non-`NULL` and nonzero,
   otherwise `1`.
10. For `IS FALSE`, return `1` when the operand is non-`NULL` zero, otherwise
    `0`.
11. For `IS NOT FALSE`, return `0` when the operand is non-`NULL` zero,
    otherwise `1`.
12. For `IS UNKNOWN`, return the same result as `IS NULL`.
13. For `IS NOT UNKNOWN`, return the same result as `IS NOT NULL`.
14. Format results as `1` or `0`.
15. Append staged division-by-zero warnings only after all scalar select-item
    values have been evaluated, before completing the result.

Successful supported scalar `IS` `SELECT` statements return one synthesized
row, `affected_rows == 0`, and a result `warning_count` equal to staged child
division-by-zero warnings. Scalar `IS` operators record no warnings.

Default result-column labels use the existing source-span convention. Explicit
aliases override the default label.

## Diagnostics

| Condition | Result |
| --- | --- |
| Lexer or parser error | MySQL-style syntax error `1064` / SQLSTATE `42000` |
| Unsupported scalar `IS` expression shape | MyLite deterministic unsupported error `1064` / `42000` |
| Unsupported direct unparenthesized scalar `IS` chaining | MyLite deterministic unsupported scalar `IS` error |
| Unsupported operand literal or expression | MyLite deterministic unsupported scalar `IS` or scalar projection error |
| Wrong scalar function arity in an admitted child | Existing function-specific MySQL-compatible error |
| Child arithmetic overflow | Existing `1690` / `22003` overflow diagnostic |
| Child division/modulo by zero in an evaluated operand | Successful statement with warning `1365` / `22012`; child value is `NULL` |
| Deprecated scalar `&&`, `||`, or symbolic `!` | Unsupported in this phase; no deprecation-warning support is claimed |
| Table-backed expression projection | Existing table-backed projection unsupported diagnostic |
| Allocation failure | `MYLITE_NOMEM` and handle-owned allocation diagnostic |
| Public API misuse | Existing public API misuse behavior; no public surface changes |

## Performance And Storage

Scalar `IS` evaluation is O(number of evaluated AST nodes) and reuses the
bounded dynamic stacks already used by scalar arithmetic/comparison/logical
evaluation. Logical child expressions retain MySQL-observed short-circuiting,
so right-hand subtrees are not evaluated when an admitted logical operand proves
they should be skipped.

The feature does not read or write SQLite tables, does not generate SQLite SQL,
does not touch catalog rows, and does not require SQLite fork patches.

## Test Plan

Add a MySQL-runtime expectation script and fast C tests covering:

- `IS NULL`, `IS NOT NULL`, `IS TRUE`, `IS NOT TRUE`, `IS FALSE`,
  `IS NOT FALSE`, `IS UNKNOWN`, and `IS NOT UNKNOWN` over integer, boolean,
  and `NULL` operands;
- nonzero signed integer truthiness, zero falsehood, and signed 64-bit boundary
  operands;
- arithmetic, comparison, scalar function, logical, and parenthesized nested
  scalar `IS` operands;
- precedence with arithmetic, comparison, keyword `NOT`, `AND`, `XOR`, and
  `OR`;
- parenthesized nested scalar `IS` results used by comparison, logical, and
  scalar `IS` operators;
- direct unparenthesized scalar `IS` chaining and direct comparison after
  scalar `IS` rejected deterministically by MyLite for this phase;
- child arithmetic overflow diagnostics;
- division/modulo-by-zero child warnings, in-statement `@@warning_count` /
  `ROW_COUNT()` snapshot behavior, and final warning counts;
- default labels and explicit aliases;
- no-source and `FROM DUAL` forms with mixed scalar select items;
- deterministic syntax errors for malformed `IS` forms such as missing
  operands or unsupported right-hand tokens;
- deterministic rejection of strings, decimals, floats, hex, bit literals,
  parameters, table-backed expression projection, descriptor-column scalar
  projection, symbolic `!`, deprecated scalar `&&` / `||`, expression `IN`,
  expression `BETWEEN`, functions outside the admitted scalar set, subqueries,
  and CTEs;
- no catalog mutation, no storage mutation, no `.mylite` preamble changes, and
  independent handle behavior for file-backed databases;
- zero-initialized cleanup for any new evaluator objects; and
- existing lexer, parser, runtime handle, diagnostics, statement context,
  scalar value, arithmetic, comparison, logical, catalog, storage, lifecycle,
  and workflow tests still passing.

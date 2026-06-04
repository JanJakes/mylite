# Baseline Scalar Comparison Projection

This phase extends MyLite's no-source and `FROM DUAL` scalar projection lane
with a deliberately small comparison-expression subset:

```sql
SELECT comparison_scalar[, comparison_scalar ...]
SELECT ALL comparison_scalar[, comparison_scalar ...]
SELECT comparison_scalar[, comparison_scalar ...] FROM DUAL
SELECT ALL comparison_scalar[, comparison_scalar ...] FROM DUAL
```

The admitted comparison operators are `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`,
and `>=`. Operands use the current MyLite-owned scalar arithmetic domain plus
the narrow numeric-string/decimal projection forms needed by WordPress:
signed-64 decimal integer/boolean/`NULL` values, decimal and float literals,
ordinary string literals coerced through MySQL-like leading numeric prefixes,
supported scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` calls,
parenthesized admitted values, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`,
infix `MOD`, `MOD(left, right)`, and infix `DIV`. Arithmetic over decimal,
float, or numeric-string operands is limited to unary signs, `+`, `-`, and `*`.

This is still not a general expression engine. This phase does not admit
table-backed expression projection, row comparisons, hex/bit/temporal operands,
full comparison type coercion outside the documented numeric-string/decimal
subset, `IS`, `LIKE`, `REGEXP`, `IN`, `BETWEEN`, logical scalar operators,
bitwise operators, `/`, predicates around no-source scalar projection, DML
assignment expressions, parameters, subqueries, CTEs, expression metadata, or
arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Comparison operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - Expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - `NULL` behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/problems-with-null.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_scalar_comparison_projection_expectations.sh`.

The MySQL 8.4 manual documents comparison operations as producing `1`, `0`, or
`NULL`, and documents `<=>` as NULL-safe equality. It places comparison
operators below arithmetic operators and above `BETWEEN`, `NOT`, `AND`, `XOR`,
and `OR`; same-precedence operators evaluate left to right.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- `=`, `<>`, `!=`, `<`, `<=`, `>`, and `>=` over non-`NULL` signed integer
  operands return `1` for true and `0` for false;
- ordinary comparison operators return `NULL` if either operand is `NULL`;
- `<=>` returns `1` if both operands are `NULL`, `0` if exactly one operand is
  `NULL`, and otherwise follows equality over the supported integer domain;
- `TRUE` and `FALSE` behave as `1` and `0`;
- comparison operands are evaluated left to right; for ordinary comparisons, a
  left operand that evaluates to `NULL` makes the comparison `NULL` without
  evaluating the right operand, while `<=>` still evaluates both operands;
- evaluated arithmetic operands are evaluated before comparison, so child
  overflow raises error `1690` / SQLSTATE `22003` before comparison can return
  a value;
- warning-producing child arithmetic, such as `5 DIV 0`, produces `NULL`
  operand values and stages one warning per evaluated child expression;
- string literals in scalar comparison projection use MySQL-like numeric
  conversion of leading numeric prefixes, so `'00.42' = 0.4200` and
  `0 + '1234abcd' = 1234` return `1` in this subset;
- decimal and float literals compare as approximate numeric values in this
  no-source scalar projection lane;
- comparison expressions in a scalar `SELECT` do not affect `@@warning_count`
  or `ROW_COUNT()` values evaluated inside the same select list; those reads use
  the previous statement snapshot;
- after a successful comparison select with child division-by-zero warnings,
  the public result warning count and following diagnostics snapshot count
  those warnings;
- comparison operators bind looser than arithmetic, and comparison operators at
  the same precedence associate left to right; and
- MySQL accepts broader forms such as hex/bit coercions, table-backed
  expression projection, row comparisons, and warning-complete string
  conversion diagnostics. Those forms remain outside this phase.

## Ownership Boundaries

- Public API: unchanged. Successful supported statements return one row through
  existing `mylite_execute()` / `mylite_result` text-result conventions.
- Statement context: owns previous row-count and diagnostics-count snapshots.
  Comparison evaluation must preserve the existing rule that diagnostics count
  variables read the previous statement snapshot while the current statement is
  still being evaluated.
- Lexer/parser/AST: existing comparison operator tokens are reused. The parser
  adds comparison binary expressions to MyLite's expression grammar using the
  existing `MYLITE_SQL_AST_BINARY_EXPRESSION` node and operator enum values.
  No MySQL grammar text is copied.
- Analyzer/runtime: scalar projection admission accepts comparison expressions
  only when both operands are admitted by this feature's scalar numeric domain,
  or are nested admitted comparison expressions produced by the same subset.
  Runtime evaluation is MyLite-owned and checked for signed-64 operand
  conversion hazards.
- Diagnostics: comparison itself is warning-free for the admitted in-range
  subset. Warnings staged by child arithmetic are preserved and appended at the
  normal scalar projection statement boundary.
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
SELECT comparison_item[, comparison_item ...]
SELECT ALL comparison_item[, comparison_item ...]
SELECT comparison_item[, comparison_item ...] FROM DUAL
SELECT ALL comparison_item[, comparison_item ...] FROM DUAL

comparison_item:
    comparison_scalar
  | comparison_scalar AS alias
  | comparison_scalar alias

comparison_scalar:
    comparison_operand
  | comparison_scalar = comparison_scalar
  | comparison_scalar <=> comparison_scalar
  | comparison_scalar <> comparison_scalar
  | comparison_scalar != comparison_scalar
  | comparison_scalar < comparison_scalar
  | comparison_scalar <= comparison_scalar
  | comparison_scalar > comparison_scalar
  | comparison_scalar >= comparison_scalar
  | ( comparison_scalar )

comparison_operand:
    scalar_numeric_operand
```

`comparison_operand` is the current scalar arithmetic expression domain from
the arithmetic, modulo, and `DIV` slices plus direct decimal/float/string
numeric operands and `+`, `-`, and `*` arithmetic over those approximate
numeric operands. Comparison results themselves are integer scalar values (`1`
or `0`) or `NULL`, so admitted comparisons may appear as operands of later
same-slice comparisons where MySQL's left-to-right precedence makes that
visible, for example `1 < 2 = 1`.

## MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
%left EQUAL NULL_SAFE_EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%right UPLUS UMINUS.

expression(A) ::= expression(B) EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_EQUAL, C);
}
expression(A) ::= expression(B) NULL_SAFE_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, C);
}
expression(A) ::= expression(B) NOT_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, C);
}
expression(A) ::= expression(B) LESS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS, C);
}
expression(A) ::= expression(B) LESS_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, C);
}
expression(A) ::= expression(B) GREATER(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER, C);
}
expression(A) ::= expression(B) GREATER_EQUAL(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, C);
}
```

The same AST operators remain used for descriptor-backed predicates, but this
slice does not change predicate planning or table-backed SQL generation.

## Runtime Semantics

Runtime evaluation uses an iterative expression stack shaped like the current
scalar arithmetic evaluator.

1. Admit the statement only when every select item is an existing scalar
   projection item, a supported scalar arithmetic expression, or a supported
   comparison expression.
2. Preserve existing scalar function wrong-arity diagnostics before generic
   comparison unsupported diagnostics.
3. Evaluate comparison operands left to right. For `=`, `<>`, `!=`, `<`, `<=`,
   `>`, and `>=`, stop after a left operand that evaluates to `NULL` and return
   `NULL` with only the left operand's staged warnings. `<=>` evaluates both
   operands because it needs to distinguish one-`NULL` and two-`NULL` cases.
   Evaluated child arithmetic overflow wins over comparison result evaluation.
4. Convert supported non-`NULL` operands to numeric values. Integer-domain
   operands remain signed 64-bit values. Decimal/float literals and string
   literals admitted by this slice are evaluated as approximate numeric values;
   string literals use the leading numeric prefix and fall back to `0` when no
   prefix is present. `TRUE` and `FALSE` become `1` and `0` through the
   existing scalar arithmetic operand conversion path.
5. Accumulate staged division-by-zero warnings from evaluated operands.
6. For `=`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`, return `NULL` if either
   operand is `NULL`.
7. For `<=>`, return `1` if both operands are `NULL`, `0` if exactly one
   operand is `NULL`, and otherwise return the equality result.
8. For non-`NULL` operands, compare signed integer values unless either side is
   an approximate numeric value; mixed or approximate operands compare as
   approximate numeric values and return `1` or `0`.
9. Format non-`NULL` comparison results as `1` or `0`.
10. Append staged division-by-zero warnings only after all scalar select-item
    values have been evaluated, before completing the result.

Successful supported comparison `SELECT` statements return one synthesized row,
`affected_rows == 0`, and a result `warning_count` equal to staged child
division-by-zero warnings.

Default result-column labels use the existing source-span convention. Explicit
aliases override the default label.

## Diagnostics

Comparison over the admitted in-range subset is warning-free. Child `DIV` by
zero warnings are preserved exactly as in the `DIV` slice: warning `1365` /
SQLSTATE `22012`, message `Division by 0`.

Unsupported or invalid forms use deterministic MyLite diagnostics in the
current scalar projection style:

- unsupported scalar comparison shape: MyLite unsupported-feature parse-class
  diagnostic describing the admitted scalar arithmetic and comparison subset;
- hex, bit, temporal, parameter, subquery, CTE, system-variable arithmetic
  operand, unsupported string literal form, or table-backed comparison
  expression: same unsupported scalar comparison diagnostic;
- row comparisons: unsupported scalar comparison diagnostic or syntax error,
  depending on parser acceptance;
- signed-64 operand literal outside the admitted envelope: existing
  signed-64-operand diagnostic;
- child arithmetic overflow: MySQL-compatible `1690` / SQLSTATE `22003`
  `BIGINT value is out of range...`;
- malformed comparison syntax such as `1 =`, `= 1`, or `1 <=>`: syntax-class
  parse error;
- allocation failure while building AST/runtime stacks/result rows/warnings:
  `MYLITE_NOMEM` with `HY001`;
- public API misuse: unchanged existing API misuse diagnostics.

## Unsupported In This Phase

- table-backed comparison projection, including `SELECT id = 1 FROM t`;
- row comparisons, including `(1, 2) = (1, 2)`;
- hex, bit, temporal, JSON, and binary operands;
- MySQL's broader comparison coercion rules and warning-complete string
  conversion diagnostics;
- `IS`, `IS NOT`, `LIKE`, `REGEXP`, `RLIKE`, `IN`, `BETWEEN`, `CASE`, and
  logical scalar operators;
- comparison expressions in predicates, ordering, grouping, aggregate
  arguments, `HAVING`, or DML assignments beyond the already implemented
  descriptor-backed predicate subset;
- system/session values as comparison operands, including
  `@@warning_count = 0`;
- parameters, variables, casts, collations, subqueries, CTEs, window functions,
  and arbitrary expression evaluation; and
- SQLite expression pass-through or SQLite fork hooks.

## Performance And Storage

The runtime evaluator remains iterative and proportional to AST size. It stores
only the select-item cell array, expression/value stacks, and staged warning
counts. It does not materialize table data, read descriptors, generate SQLite
SQL, or touch storage.

## Tests

Add or extend fast plain C runtime and parser tests under
`packages/libmylite/tests/`. The MySQL expectation script must cover:

- all admitted comparison operators over positive, negative, and equal integer
  values;
- `TRUE`, `FALSE`, and `NULL` operands;
- `<=>` with both operands `NULL`, one operand `NULL`, and no operands `NULL`;
- ordinary comparison `NULL` propagation;
- precedence with arithmetic and left-to-right same-precedence comparisons;
- aliases and generated column labels;
- `FROM DUAL` comparison expressions and scalar-function operands;
- numeric string, decimal, and float scalar comparison operands, including
  leading-zero decimal strings and leading-prefix string coercion;
- child `DIV` by zero warnings, result values, `SHOW WARNINGS`, in-select
  diagnostics-count snapshot behavior, result warning counts, and following
  diagnostics snapshot visibility;
- signed-64 boundary cases, including comparison of arithmetic-produced
  `INT64_MIN`;
- child overflow under comparison;
- syntax errors for invalid comparison forms; and
- MySQL-accepted but deferred broader forms, documented without admitting them.

Implementation tests should also cover:

- catalog and SQLite schema generation are unchanged;
- `.mylite` preamble is unchanged;
- independent handles keep diagnostics independent;
- unsupported table-backed comparison projection is rejected deterministically;
- unsupported hex/bit/row comparisons are rejected
  deterministically; and
- no regressions in existing parser, scalar value, scalar arithmetic, modulo,
  `DIV`, session scalar, diagnostics, result, storage, runtime lifecycle, and
  compatibility tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/operators.md`, and
`docs/compatibility/sql-query-expressions.md` for only the limited no-source
and `FROM DUAL` scalar comparison subset. Do not overclaim table-backed
expression projection, general comparison coercion, row comparisons, collations,
`IS`, `LIKE`, `IN`, `BETWEEN`, logical scalar expressions, expression metadata,
subqueries, or arbitrary SQLite expression execution.

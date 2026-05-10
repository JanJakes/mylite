# Baseline Scalar Modulo Projection

This phase extends the no-source and `FROM DUAL` scalar arithmetic projection
surface with MyLite-owned integer remainder evaluation:

```sql
SELECT modulo_scalar[, modulo_scalar ...]
SELECT ALL modulo_scalar[, modulo_scalar ...]
SELECT modulo_scalar[, modulo_scalar ...] FROM DUAL
SELECT ALL modulo_scalar[, modulo_scalar ...] FROM DUAL
```

The admitted forms are binary `%`, infix `MOD`, and the two-argument
`MOD(left, right)` function. Operands use the current scalar arithmetic domain:
signed-64 decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, and binary `+`, binary `-`, `*`, `%`,
infix `MOD`, and `MOD(left, right)` over the same domain.

This phase does not admit `/`, `DIV`, table-backed expression projection,
modulo in predicates, DML assignments, string/decimal/float/hex/bit operands,
system/session values as arithmetic operands, unsigned-width expression
results, parameters, subqueries, CTEs, date/interval arithmetic, arbitrary
expression evaluation, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Mathematical functions:
    <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - `NULL` behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/problems-with-null.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_scalar_modulo_projection_expectations.sh`.

The MySQL 8.4 manual documents `%`, infix `MOD`, and `MOD()` as modulo forms
that return the remainder of the left operand divided by the right operand. It
places `*`, `/`, `DIV`, `%`, and infix `MOD` at the same precedence level and
evaluates same-precedence operators left to right.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- integer `%`, infix `MOD`, and `MOD(left, right)` over the supported signed-64
  scalar domain return integer text;
- the remainder sign follows the dividend for tested signed integer values:
  `-5 % 2` returns `-1`, `5 % -2` returns `1`, and `-5 % -2` returns `-1`;
- unary signs bind tighter than modulo, and modulo binds with `*` before binary
  `+` and binary `-`;
- same-precedence `*` and modulo operators associate left to right;
- `TRUE` and `FALSE` behave as `1` and `0`;
- if either modulo operand is `NULL`, the result is `NULL` and no division by
  zero warning is produced, even when the other operand is zero;
- if both operands are non-`NULL` and the right operand is zero, the modulo
  expression result is `NULL` and MySQL records warning `1365` / SQLSTATE
  `22012`, message `Division by 0`;
- warning-producing modulo expressions in a scalar `SELECT` do not affect
  `@@warning_count` or `ROW_COUNT()` values evaluated inside the same select
  list; those scalar reads use the previous statement snapshot;
- after a successful modulo select with zero-divisor warnings, the public result
  warning count and the following diagnostics snapshot count those warnings;
- supported non-warning modulo expressions produce `warning_count == 0`;
- child arithmetic overflow is reported before the parent modulo expression can
  return `NULL` or record warnings;
- `MOD()`, `MOD(value)`, and `MOD(left, right, extra)` are MySQL syntax errors
  `1064` / SQLSTATE `42000`; and
- MySQL accepts broader forms such as decimal/string numeric conversion and
  table-backed modulo expressions. Those forms remain intentionally outside
  this phase.

## Ownership Boundaries

- Public API: unchanged. Successful supported statements return one row through
  the existing `mylite_execute()`/`mylite_result` text-result conventions.
- Statement context: owns the previous row-count snapshot used by
  `ROW_COUNT()`. Modulo evaluation must preserve the existing rule that
  diagnostics count variables read the previous statement snapshot while the
  current statement is still being evaluated.
- Lexer/parser/AST: `%` already exists as an operator token, while `MOD` is an
  existing reserved keyword. The grammar adds expression-level binary modulo
  nodes for `%` and infix `MOD`, using a new AST operator value. The grammar
  also adds a two-argument `MOD()` function AST node. No grammar text is copied
  from MySQL.
- Analyzer/runtime: scalar projection admission accepts `%`, infix `MOD`, and
  `MOD()` only when operands are admitted by this feature's scalar arithmetic
  domain. Runtime evaluation is MyLite-owned, iterative, and checked for
  signed-64 arithmetic hazards.
- Diagnostics: modulo-by-zero warnings are MyLite-owned diagnostic records,
  appended after scalar select-item values are evaluated so in-statement
  diagnostics count variables see the previous statement snapshot.
- Catalog: not involved. The feature must not read or mutate schema/table
  descriptors, descriptor versions, catalog generation, or SQLite schema
  generation.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite: not involved. This is wrapper/runtime behavior and must not depend on
  SQLite expression evaluation or SQLite fork changes.

## Syntax

MyLite admits these source forms:

```sql
SELECT modulo_item[, modulo_item ...]
SELECT ALL modulo_item[, modulo_item ...]
SELECT modulo_item[, modulo_item ...] FROM DUAL
SELECT ALL modulo_item[, modulo_item ...] FROM DUAL

modulo_item:
    modulo_scalar
  | modulo_scalar AS alias
  | modulo_scalar alias

modulo_scalar:
    scalar_arithmetic_operand
  | + modulo_scalar
  | - modulo_scalar
  | modulo_scalar + modulo_scalar
  | modulo_scalar - modulo_scalar
  | modulo_scalar * modulo_scalar
  | modulo_scalar % modulo_scalar
  | modulo_scalar MOD modulo_scalar
  | MOD ( modulo_scalar , modulo_scalar )
  | ( modulo_scalar )
```

`scalar_arithmetic_operand` is the existing scalar arithmetic operand domain:

- decimal integer literals with optional unary `+` or `-`, where source values
  fit the signed-64 operand envelope;
- `TRUE`, `FALSE`, and `NULL`;
- supported scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values
  from the current no-source/`DUAL` scalar function slices; and
- admitted nested scalar arithmetic expression results.

`MOD()` is admitted only with exactly two arguments. MySQL 8.4.9 reports syntax
error `1064` / SQLSTATE `42000` for `MOD()`, `MOD(value)`, and
`MOD(left, right, extra)`, so MyLite should leave those wrong-arity forms as
parse errors in this phase rather than inventing a function-arity diagnostic.

## MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's existing
expression grammar:

```lemon
%left PLUS MINUS.
%left STAR SLASH PERCENT MOD.
%right UPLUS UMINUS.

expression(A) ::= expression(B) PERCENT(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}
expression(A) ::= expression(B) MOD(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}
expression(A) ::= MOD(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_MOD_FUNCTION, B, C, R);
}
```

`SLASH` remains parsed but unsupported by this runtime slice. `MOD` remains a
reserved keyword and is not admitted as a bare identifier.

## Runtime Semantics

Runtime evaluation uses the same iterative scalar arithmetic stacks as the
current `+`, binary `-`, `*`, and unary arithmetic slices.

1. Admit the statement only when every select item is either an existing scalar
   projection item or a modulo scalar expression/function.
2. Preserve existing scalar function wrong-arity diagnostics before generic
   arithmetic unsupported diagnostics.
3. Evaluate nested child expressions before applying their parent operator.
   Child overflow wins over parent modulo `NULL` propagation or modulo-by-zero
   warnings.
4. Convert `TRUE` and `FALSE` to `1` and `0` for arithmetic operands.
5. If either modulo operand is `NULL`, return `NULL` without recording a
   division-by-zero warning.
6. If both operands are non-`NULL` and the right operand is zero, return `NULL`
   and stage one warning `1365` / `22012`, `Division by 0`.
7. Otherwise return the signed remainder. The `INT64_MIN % -1` case must return
   `0` without relying on C undefined behavior.
8. Format non-`NULL` results as canonical signed decimal integer text.
9. Append staged division-by-zero warnings only after all scalar select-item
   values have been evaluated, before completing the result.

Successful supported modulo `SELECT` statements return one synthesized row,
`affected_rows == 0`, and a result `warning_count` equal to the number of
staged modulo-by-zero warnings.

## Diagnostics

Supported zero-divisor modulo expressions produce warning `1365` / SQLSTATE
`22012`, `Division by 0`. They do not fail the statement.

Unsupported or invalid forms use deterministic MyLite diagnostics in the
current scalar projection style:

- unsupported scalar arithmetic shape: MyLite unsupported-feature parse-class
  diagnostic describing the admitted scalar arithmetic subset;
- non-decimal, string, decimal, float, hex, bit, parameter, subquery, CTE,
  system-variable arithmetic operand, or table-backed arithmetic expression:
  same unsupported scalar arithmetic diagnostic;
- signed-64 operand literal outside the admitted envelope: MyLite
  signed-64-operand diagnostic;
- child arithmetic overflow: MySQL-compatible `1690` / SQLSTATE `22003`
  `BIGINT value is out of range...`;
- wrong arity for existing scalar functions inside a modulo expression:
  existing function parameter-count diagnostics;
- `MOD()` wrong-arity forms: MySQL-compatible syntax-class parse error;
- allocation failure while building AST/runtime stacks/result rows/warnings:
  `MYLITE_NOMEM` with `HY001`;
- public API misuse: unchanged existing API misuse diagnostics.

## Unsupported In This Phase

- `/` division and `DIV` integer division;
- decimal, float, string, hex, bit, temporal, JSON, and binary operands;
- table-backed modulo projection, including `SELECT id % 2 FROM t`;
- modulo in predicates, ordering, grouping, aggregate arguments, or DML
  assignment expressions;
- system/session values as arithmetic operands, including
  `@@warning_count % 2`;
- expression-level unsigned result semantics and results above signed-64;
- parameters, variables, casts, collations, subqueries, CTEs, window functions,
  and arbitrary expression evaluation; and
- SQLite expression pass-through or SQLite fork hooks.

## Performance And Storage

The runtime evaluator remains iterative and proportional to AST size. It stores
only the select-item cell array, value array, existing expression stacks, and a
warning counter for staged division-by-zero warnings. It does not materialize
table data, read descriptors, generate SQLite SQL, or touch storage.

## Tests

Add or extend fast plain C runtime and parser tests under
`packages/libmylite/tests/`. The MySQL expectation script must cover:

- `%`, infix `MOD`, and `MOD()` values over positive and negative signed
  integers;
- `TRUE`, `FALSE`, and `NULL` operands;
- precedence with unary signs, `*`, binary `+`, binary `-`, and left-to-right
  same-precedence evaluation;
- aliases and generated column labels;
- `FROM DUAL` modulo expressions and scalar-function operands;
- `NULL` operands with zero on the other side and no warnings;
- non-`NULL` zero divisors, result `NULL`, warning `1365` / `22012`, in-select
  `@@warning_count`/`ROW_COUNT()` ordering, result warning counts, and
  following diagnostics snapshot visibility;
- signed-64 boundary cases, especially `INT64_MIN % -1`;
- child overflow under modulo;
- deterministic rejection of `/`, `DIV`, `MOD()` wrong arity, strings,
  decimals, floats, hex, bit, parameters, system-variable operands,
  table-backed modulo projection, subqueries, CTEs, and unsupported clauses; and
- file-backed preamble/catalog-generation/schema-generation safety plus
  independent handles.

Verification commands:

1. build parser/runtime targets touched by the implementation;
2. run focused parser/runtime CTest entries;
3. run
   `packages/libmylite/tests/mysql_baseline_scalar_modulo_projection_expectations.sh`;
4. run `cmake --workflow --preset check`.

## Compatibility Documentation

`COMPATIBILITY.md`, `docs/compatibility/operators.md`, and
`docs/compatibility/sql-query-expressions.md` should mark `%`, infix `MOD`, and
`MOD(left, right)` as specified, then implemented, only for the limited
no-source/`DUAL` signed-64 scalar modulo projection subset.
`docs/compatibility/functions-numeric-math.md` should mark `MOD()` with that
same limited scope. The docs must not imply support for `/`, `DIV`, decimal
modulo, string conversion, table-backed expression projection, full expression
metadata, modulo predicates, DML assignment expressions, or SQLite pass-through.

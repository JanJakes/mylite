# Baseline Scalar DIV Projection

This phase extends the no-source and `FROM DUAL` scalar arithmetic projection
surface with MyLite-owned integer division evaluation:

```sql
SELECT div_scalar[, div_scalar ...]
SELECT ALL div_scalar[, div_scalar ...]
SELECT div_scalar[, div_scalar ...] FROM DUAL
SELECT ALL div_scalar[, div_scalar ...] FROM DUAL
```

The admitted new operator is infix `DIV`. Operands use the current scalar
arithmetic domain: signed-64 decimal integer/boolean/`NULL` values, supported
scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values,
parenthesized admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`,
`%`, infix `MOD`, `MOD(left, right)`, and infix `DIV` over the same domain.

This phase does not admit `/`, table-backed expression projection, `DIV` in
predicates, DML assignments, string/decimal/float/hex/bit operands,
system/session values as arithmetic operands, unsigned-width expression
results, parameters, subqueries, CTEs, date/interval arithmetic, arbitrary
expression evaluation, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - `NULL` behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/problems-with-null.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_scalar_div_projection_expectations.sh`.

The MySQL 8.4 manual documents `DIV` as integer division and places `*`, `/`,
`DIV`, `%`, and infix `MOD` at the same precedence level, with same-precedence
operators evaluated left to right.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- integer `DIV` over the supported signed-64 scalar domain returns integer
  text;
- fractional remainders are discarded toward zero: `5 DIV 2` returns `2`,
  `-5 DIV 2` returns `-2`, `5 DIV -2` returns `-2`, and `-5 DIV -2` returns
  `2`;
- unary signs bind tighter than `DIV`, and `DIV` binds with `*`, `%`, and
  infix `MOD` before binary `+` and binary `-`;
- same-precedence `*`, `%`, infix `MOD`, and `DIV` operators associate left to
  right;
- `TRUE` and `FALSE` behave as `1` and `0`;
- if either `DIV` operand is `NULL`, the result is `NULL` and no division by
  zero warning is produced, even when the other operand is zero;
- if both operands are non-`NULL` and the right operand is zero, the `DIV`
  expression result is `NULL` and MySQL records warning `1365` / SQLSTATE
  `22012`, message `Division by 0`;
- warning-producing `DIV` expressions in a scalar `SELECT` do not affect
  `@@warning_count` or `ROW_COUNT()` values evaluated inside the same select
  list; those scalar reads use the previous statement snapshot;
- after a successful `DIV` select with zero-divisor warnings, the public result
  warning count and the following diagnostics snapshot count those warnings;
- child arithmetic overflow is reported before the parent `DIV` expression can
  return `NULL` or record warnings;
- `(-9223372036854775807 - 1) DIV 1` and
  `(-9223372036854775807 - 1) DIV -1` raise MySQL error `1690` / SQLSTATE
  `22003`, even though ordinary arithmetic can produce the signed minimum
  value in other contexts;
- `DIV(5, 2)`, `5 DIV`, and `DIV 2` are MySQL syntax errors `1064` /
  SQLSTATE `42000`; and
- MySQL accepts broader forms such as decimal/string numeric conversion and
  table-backed `DIV` expressions. Those forms remain intentionally outside
  this phase.

## Ownership Boundaries

- Public API: unchanged. Successful supported statements return one row through
  the existing `mylite_execute()`/`mylite_result` text-result conventions.
- Statement context: owns the previous row-count snapshot used by
  `ROW_COUNT()`. `DIV` evaluation must preserve the existing rule that
  diagnostics count variables read the previous statement snapshot while the
  current statement is still being evaluated.
- Lexer/parser/AST: `DIV` is already a reserved keyword token from the lexer.
  The parser maps that keyword to a new expression-level binary integer
  division AST operator. No grammar text is copied from MySQL.
- Analyzer/runtime: scalar projection admission accepts infix `DIV` only when
  operands are admitted by this feature's scalar arithmetic domain. Runtime
  evaluation is MyLite-owned, iterative, and checked for signed-64 arithmetic
  hazards.
- Diagnostics: `DIV` by zero warnings are MyLite-owned diagnostic records,
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
SELECT div_item[, div_item ...]
SELECT ALL div_item[, div_item ...]
SELECT div_item[, div_item ...] FROM DUAL
SELECT ALL div_item[, div_item ...] FROM DUAL

div_item:
    div_scalar
  | div_scalar AS alias
  | div_scalar alias

div_scalar:
    scalar_arithmetic_operand
  | + div_scalar
  | - div_scalar
  | div_scalar + div_scalar
  | div_scalar - div_scalar
  | div_scalar * div_scalar
  | div_scalar % div_scalar
  | div_scalar MOD div_scalar
  | MOD ( div_scalar , div_scalar )
  | div_scalar DIV div_scalar
  | ( div_scalar )
```

`scalar_arithmetic_operand` is the existing scalar arithmetic operand domain:

- decimal integer literals with optional unary `+` or `-`, where source values
  fit the signed-64 operand envelope;
- `TRUE`, `FALSE`, and `NULL`;
- supported scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values
  from the current no-source/`DUAL` scalar function slices; and
- admitted nested scalar arithmetic expression results.

`DIV` is an infix operator only. MySQL 8.4.9 reports syntax errors for
`DIV(5, 2)`, `5 DIV`, and `DIV 2`, so MyLite should leave those as parse errors
in this phase rather than inventing a function-arity diagnostic.

## MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's existing
expression grammar:

```lemon
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%right UPLUS UMINUS.

expression(A) ::= expression(B) DIV(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE, C);
}
```

`SLASH` remains parsed but unsupported by this runtime slice. `DIV` remains a
reserved keyword and is not admitted as a bare identifier.

## Runtime Semantics

Runtime evaluation uses the same iterative scalar arithmetic stacks as the
current `+`, binary `-`, `*`, unary arithmetic, and modulo slices.

1. Admit the statement only when every select item is either an existing scalar
   projection item or a `DIV` scalar expression.
2. Preserve existing scalar function wrong-arity diagnostics before generic
   arithmetic unsupported diagnostics.
3. Evaluate nested child expressions before applying their parent operator.
   Child overflow wins over parent `DIV` `NULL` propagation or division-by-zero
   warnings.
4. Convert `TRUE` and `FALSE` to `1` and `0` for arithmetic operands.
5. If either `DIV` operand is `NULL`, return `NULL` without recording a
   division-by-zero warning.
6. If both operands are non-`NULL` and the right operand is zero, return `NULL`
   and stage one warning `1365` / `22012`, `Division by 0`.
7. Otherwise return the signed integer quotient truncated toward zero.
8. Match the observed MySQL 8.4.9 signed-minimum boundary for this supported
   subset: `INT64_MIN DIV 1` and `INT64_MIN DIV -1` return error `1690` /
   `22003`.
9. Format non-`NULL` results as canonical signed decimal integer text.
10. Append staged division-by-zero warnings only after all scalar select-item
    values have been evaluated, before completing the result.

Successful supported `DIV` `SELECT` statements return one synthesized row,
`affected_rows == 0`, and a result `warning_count` equal to the number of
staged division-by-zero warnings.

## Diagnostics

Supported zero-divisor `DIV` expressions produce warning `1365` / SQLSTATE
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
- `INT64_MIN DIV 1` and `INT64_MIN DIV -1`: MySQL-compatible `1690` /
  SQLSTATE `22003`;
- wrong arities for existing scalar functions inside a `DIV` expression:
  existing function parameter-count diagnostics;
- invalid `DIV` syntax such as `DIV(5, 2)`, `5 DIV`, or `DIV 2`:
  MySQL-compatible syntax-class parse error;
- allocation failure while building AST/runtime stacks/result rows/warnings:
  `MYLITE_NOMEM` with `HY001`;
- public API misuse: unchanged existing API misuse diagnostics.

## Unsupported In This Phase

- `/` division;
- decimal, float, string, hex, bit, temporal, JSON, and binary operands;
- table-backed `DIV` projection, including `SELECT id DIV 2 FROM t`;
- `DIV` in predicates, ordering, grouping, aggregate arguments, or DML
  assignment expressions;
- system/session values as arithmetic operands, including
  `@@warning_count DIV 2`;
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

- `DIV` values over positive and negative signed integers;
- `TRUE`, `FALSE`, and `NULL` operands;
- precedence with unary signs, `*`, `%`, infix `MOD`, binary `+`, binary `-`,
  and left-to-right same-precedence evaluation;
- aliases and generated column labels;
- `FROM DUAL` `DIV` expressions and scalar-function operands;
- `NULL` operands with zero on the other side and no warnings;
- non-`NULL` zero divisors, result `NULL`, warning `1365` / `22012`,
  in-select `@@warning_count`/`ROW_COUNT()` ordering, result warning counts,
  and following diagnostics snapshot visibility;
- signed-64 boundary cases, including `INT64_MIN DIV 2`, `INT64_MIN DIV -2`,
  `INT64_MIN DIV 1`, and `INT64_MIN DIV -1`;
- child overflow under `DIV`;
- syntax errors for invalid `DIV` forms; and
- MySQL-accepted but deferred broader forms, documented without admitting them.

Implementation tests should also cover:

- no-source and `FROM DUAL` execution;
- nested `DIV` with existing modulo/arithmetic nodes;
- file-backed `.mylite` preamble and catalog/sqlite generation invariants;
- independent handle diagnostics state; and
- deterministic rejection of table-backed and unsupported operand forms.

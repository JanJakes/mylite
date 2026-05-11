# Baseline Scalar Division Projection

## Summary

This phase admits the next narrow MyLite-owned scalar arithmetic slice for
no-source, `FROM DUAL`, and `DO` execution:

```sql
SELECT division_scalar[, division_scalar ...]
SELECT ALL division_scalar[, division_scalar ...]
SELECT division_scalar[, division_scalar ...] FROM DUAL
SELECT ALL division_scalar[, division_scalar ...] FROM DUAL
DO division_scalar[, division_scalar ...]
```

The admitted new operator is `/`, but only as a top-level division expression.
Each side of `/` must be an existing signed-64 scalar arithmetic expression
without `/`: decimal integer, `TRUE`, `FALSE`, `NULL`, supported scalar
control-flow values, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

For this integer-only exact-value subset, MySQL 8.4.9 returns a decimal value
with four fractional digits. MyLite mirrors that exact visible result for the
admitted top-level `/` forms. This phase does not introduce a general decimal
expression engine, nested slash arithmetic, `/` as an operand to arithmetic,
comparison, logical, `CASE`, functions, predicates, ordering, grouping, DML
assignments, table-backed expressions, strings, floats, decimal literals, hex,
bit literals, parameters, subqueries, CTEs, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - Precision math expression handling:
    <https://dev.mysql.com/doc/refman/8.4/en/precision-math-expressions.html>
  - `NULL` behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/problems-with-null.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_scalar_division_projection_expectations.sh`.

The MySQL 8.4 manual documents `/` as division. For exact-value operands, the
result scale is the scale of the first operand plus `@@div_precision_increment`,
which is `4` by default. Runtime probes against MySQL 8.4.9 establish these
expectations for this slice:

- integer `/` returns four fractional digits: `1/2` returns `0.5000`,
  `1/3` returns `0.3333`, and `10/4` returns `2.5000`;
- results are rounded to the displayed four-digit scale for the admitted
  integer/integer forms, including half cases such as `1/20000` returning
  `0.0001`;
- negative results use a leading `-` only when the rounded result is nonzero:
  `-1/20000` returns `-0.0001`, while `-1/200000` returns `0.0000`;
- `TRUE` and `FALSE` behave as `1` and `0`;
- if either operand is already `NULL`, the result is `NULL`;
- if both operands are non-`NULL` and the right operand is zero, the result is
  `NULL` and MySQL records warning `1365` / SQLSTATE `22012`, `Division by 0`;
- warning-producing `/` expressions in scalar `SELECT` do not affect
  `@@warning_count` or `ROW_COUNT()` values read inside the same select list;
- child arithmetic warnings and overflow keep their existing behavior before
  the parent `/` result is applied;
- `/` shares MySQL's multiplicative precedence tier with `*`, `%`, `MOD`, and
  `DIV`, but this phase admits only top-level `/`; nested composition with
  other multiplicative operators remains deferred;
- `DO 1/0` produces no rows, affected rows `0`, and one division-by-zero
  warning;
- syntax edges such as `SELECT /2`, `SELECT 1/`, and `DO /2` are syntax errors;
  and
- MySQL accepts broader forms such as strings, decimals, floats, hex, bit
  literals, table-backed columns, nested `/` composition, and expression
  metadata. Those remain intentionally outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing text result conventions; successful supported `DO`
  statements return a non-row result.
- Statement context: preserves existing row-count and warning-count snapshot
  behavior. Division-by-zero warnings are staged during scalar evaluation and
  appended after all select items or `DO` expressions are evaluated.
- Lexer/parser/AST: `/` is already tokenized and parsed as
  `MYLITE_SQL_AST_OPERATOR_DIVIDE`. Parser acceptance is wider than runtime
  admission, as with the existing scalar expression slices.
- Analyzer/runtime: admits top-level `/` only when both operands are current
  signed-64 scalar arithmetic expressions that do not contain `/`. Runtime
  evaluation is MyLite-owned and formats an exact decimal result with four
  fractional digits.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL and no SQLite fork patch. This is MyLite
  wrapper/runtime behavior.

## Syntax

MyLite admits these source forms:

```sql
SELECT division_item[, division_item ...]
SELECT ALL division_item[, division_item ...]
SELECT division_item[, division_item ...] FROM DUAL
SELECT ALL division_item[, division_item ...] FROM DUAL
DO division_scalar[, division_scalar ...]

division_item:
    division_scalar
  | division_scalar AS alias
  | division_scalar alias

division_scalar:
    division_operand / division_operand
  | ( division_scalar )

division_operand:
    scalar_arithmetic_expression_without_slash
```

`scalar_arithmetic_expression_without_slash` is the existing signed-64 scalar
arithmetic domain:

- decimal integer literals with optional unary `+` or `-`, where source values
  fit the signed-64 operand envelope;
- `TRUE`, `FALSE`, and `NULL`;
- supported scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values;
- parenthesized admitted arithmetic;
- unary `+`/`-`;
- binary `+`, binary `-`, `*`, `%`, infix `MOD`, `MOD(left, right)`, and infix
  `DIV`.

`/` is not admitted inside either operand in this phase. Therefore forms such
as `1+5/2`, `5/2*3`, `5/2/2`, `5/2%2`, `(5/2) DIV 1`, `ABS(5/2)`,
`CASE WHEN TRUE THEN 5/2 END`, `5/2 = 2.5`, and `SELECT id/2 FROM t` are
deterministically rejected by MyLite even though MySQL accepts them.

### MyLite Lemon Snippet

The parser grammar already contains the independently authored expression
production:

```lemon
%left STAR SLASH DIV PERCENT MOD.

expression(A) ::= expression(B) SLASH(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_DIVIDE, C);
}
```

The runtime admission grammar for this phase is narrower:

```lemon
division_scalar(A) ::= division_operand(B) SLASH division_operand(C).
division_scalar(A) ::= LPAREN division_scalar(B) RPAREN.

division_operand(A) ::= scalar_arithmetic_expression_without_slash(B).
```

These snippets describe MyLite's admitted subset and are not copied from MySQL
grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` expression as division only when the top-level
   unwrapped expression is `/` and both operands are current signed-64 scalar
   arithmetic expressions without `/`.
2. Preserve existing function wrong-arity diagnostics and child arithmetic
   diagnostics before generic unsupported diagnostics.
3. Evaluate the left operand and right operand using the existing signed-64
   scalar arithmetic evaluator. This preserves `NULL`, child
   division-by-zero warnings from `%`, `MOD`, and `DIV`, and child overflow
   behavior.
4. If either evaluated operand is `NULL`, return `NULL`. Child warnings
   accumulated while reaching that `NULL` are preserved.
5. If both operands are non-`NULL` and the right operand is zero, return `NULL`
   and stage one warning `1365` / SQLSTATE `22012`, `Division by 0`.
6. Otherwise format `left / right` as exact decimal text with four fractional
   digits:
   - use unsigned magnitudes internally so `INT64_MIN / -1` can produce
     `9223372036854775808.0000`;
   - compute the integer part, four fractional digits, and one rounding digit
     using integer arithmetic;
   - round the fourth fractional digit up when the next digit is at least `5`;
   - carry from `9999` fractional text into the integer part; and
   - suppress a negative sign when the rounded value is exactly `0.0000`.
7. Append staged division-by-zero warnings after scalar select-item or `DO`
   expression evaluation through the existing scalar warning path.

Supported in-range division statements report `warning_count == 0` unless an
evaluated child arithmetic expression or the parent `/` stages an existing
division-by-zero warning. They do not touch catalog state, SQLite schema
generation, physical tables, or the `.mylite` preamble.

## Diagnostics

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- unsupported operands or nested `/` composition use a deterministic MyLite
  unsupported-feature diagnostic describing the admitted scalar division
  subset;
- signed-64 operand literals outside the admitted scalar arithmetic envelope
  use the existing scalar arithmetic operand diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE `22003`;
- evaluated child or parent division by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- `/` nested inside arithmetic, comparison, logical, scalar `IS`, `CASE`,
  control-flow functions, numeric functions, predicates, `ORDER BY`,
  `GROUP BY`, aggregate arguments, defaults, generated columns, and DML
  assignment expressions;
- table-backed `column / value` or `value / column`;
- strings, decimal literals, float literals, hex literals, bit literals,
  binary strings, temporal values, JSON, parameters, user variables, system
  variables, session functions, casts, collations, subqueries, and CTEs;
- direct exact decimal result scales other than the integer/integer default
  scale of four fractional digits; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser AST coverage for `/` as `MYLITE_SQL_AST_OPERATOR_DIVIDE` and
  precedence with existing multiplicative operators;
- no-source and `FROM DUAL` division over integers, booleans, `NULL`, unary
  signed operands, signed boundaries, current scalar control-flow operands,
  and current scalar arithmetic operands without slash;
- exact four-fractional-digit formatting, rounding, negative results, and
  zero-result sign suppression;
- `DO` division with no result rows and correct affected-row behavior;
- parent and child division-by-zero warning staging, including in-statement
  `@@warning_count` and `ROW_COUNT()` snapshots;
- child arithmetic overflow diagnostics;
- explicit aliases and generated column labels;
- deterministic rejection for nested/general `/` composition, table-backed
  columns, strings, decimals, floats, hex, bit literals, parameters, system
  variables, session functions, subqueries, and use inside larger scalar
  expressions;
- file-backed preamble/catalog-generation/schema-generation safety;
- independent handles; and
- existing lexer, parser, scalar projection, arithmetic, modulo, `DIV`,
  bitwise, comparison, logical, `CASE`, `DO`, runtime, storage, and catalog
  tests.

Verification commands:

1. `packages/libmylite/tests/mysql_baseline_scalar_division_projection_expectations.sh`
2. `cmake --build --preset dev`
3. focused parser/runtime CTest entries for division and adjacent scalar
   expression surfaces
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/operators.md`,
`docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-stored-programs.md` only for this limited top-level
no-source/`DUAL`/`DO` integer-operand `/` subset. Do not imply support for a
general decimal expression engine, nested slash composition, table-backed
division, decimal/float/string conversion, expression metadata, or arbitrary
SQLite expression execution.

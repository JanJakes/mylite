# Baseline Scalar Arithmetic Projection

## Summary

This phase adds the next narrow MyLite-owned expression step for no-source and
`FROM DUAL` scalar `SELECT` statements:

```sql
SELECT arithmetic_scalar[, arithmetic_scalar ...]
SELECT ALL arithmetic_scalar[, arithmetic_scalar ...]
SELECT arithmetic_scalar[, arithmetic_scalar ...] FROM DUAL
SELECT ALL arithmetic_scalar[, arithmetic_scalar ...] FROM DUAL
```

The admitted arithmetic operators are signed 64-bit exact integer `+`, binary
`-`, and `*` over the existing scalar value domain. Operands may be decimal
integer/boolean/`NULL` values and scalar `IF()`/`IFNULL()`/`COALESCE()`/
`NULLIF()`/`ISNULL()` calls that are already admitted by
`baseline-scalar-expression-projection`. Parentheses may group admitted
arithmetic expressions. The result is one synthesized row, evaluated by MyLite,
with no table access and no SQLite expression delegation.

This is not yet the general expression engine. It deliberately excludes table
columns, table-backed expression projection, string/decimal/float/hex/bit
literals, system variables and session functions inside arithmetic,
comparison/logical/bitwise operators, `/`, `DIV`, `%`, `MOD`, unary arithmetic
over arbitrary expressions, casts, parameters, user variables, subqueries, CTEs,
expression metadata, expression assignments, expression predicates, and
arbitrary SQLite pass-through.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - Expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - Precision math expression handling:
    <https://dev.mysql.com/doc/refman/8.4/en/precision-math-expressions.html>
  - `NULL` behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/problems-with-null.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_scalar_arithmetic_projection_expectations.sh`
  and verified against MySQL 8.4.9.

Runtime probes against MySQL 8.4.9 confirm:

- `*` binds tighter than binary `+` and `-`, and parentheses override
  precedence;
- same-precedence arithmetic operators evaluate left to right;
- integer `+`, binary `-`, and `*` over signed integer operands return exact
  integer results when the result fits MySQL's integer expression range;
- `TRUE` and `FALSE` behave as `1` and `0` in arithmetic;
- arithmetic with any `NULL` operand returns `NULL`, but overflow in a
  subexpression still raises the overflow error before an outer `NULL`
  operator could hide it;
- in-range supported expressions produce no warnings;
- `ROW_COUNT()` inside the scalar select observes the previous statement's row
  count, while a following `ROW_COUNT()` returns `-1`;
- signed integer overflow raises MySQL error 1690 / SQLSTATE `22003`; and
- MySQL accepts much broader forms such as unsigned integer arithmetic,
  expression unary signs, division, modulo, string and decimal numeric
  conversion, session/system values as numeric operands, and table-backed
  expression projection. Those remain deferred by this MyLite slice.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle lifetime, diagnostics, and statement-boundary behavior.
- Statement context: successful supported arithmetic scalar `SELECT` statements
  use the existing row-returning result conventions: one row, zero affected
  rows, statement warning count, and following `ROW_COUNT()` state `-1`.
- Lexer/parser/AST: no new tokens are required. The existing binary-expression,
  parenthesized-expression, literal, function, `FROM DUAL`, `ALL`, and alias AST
  nodes are reused. Parser acceptance remains broader than runtime admission.
- Analyzer/runtime: the scalar projection analyzer admits no-source or
  `FROM DUAL` selects only when every item is an existing scalar projection
  expression or the arithmetic subset defined here. Runtime evaluation is
  MyLite-owned and uses checked signed 64-bit arithmetic plus SQL `NULL`
  propagation.
- Catalog: not involved. The feature must not read or mutate schema/table
  descriptors, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: appends one column per select item and one row through
  existing result helpers. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  This is wrapper/runtime behavior.

## Supported SQL

Supported statement shapes:

```sql
SELECT arithmetic_item[, arithmetic_item ...]
SELECT ALL arithmetic_item[, arithmetic_item ...]
SELECT arithmetic_item[, arithmetic_item ...] FROM DUAL
SELECT ALL arithmetic_item[, arithmetic_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
arithmetic_item:
    arithmetic_scalar
  | arithmetic_scalar AS alias
  | arithmetic_scalar alias
```

The admitted value grammar is:

```sql
arithmetic_scalar:
    arithmetic_operand
  | arithmetic_scalar + arithmetic_scalar
  | arithmetic_scalar - arithmetic_scalar
  | arithmetic_scalar * arithmetic_scalar
  | ( arithmetic_scalar )

arithmetic_operand:
    baseline_scalar_expression_projection.scalar_value
```

The operand domain is intentionally the existing warning-free scalar value
domain with a signed-64 arithmetic envelope:

- decimal integer literals with optional unary `+` or `-`, where the magnitude
  is no greater than `9223372036854775807`;
- `TRUE`, `FALSE`, and `NULL`;
- parenthesized operands; and
- `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values over the same
  admitted scalar-value domain.

Arithmetic expressions may produce `-9223372036854775808`, but a source literal
with magnitude `9223372036854775808` remains deferred because MySQL's unsigned
and exact numeric expression behavior is broader than this slice.

### MyLite Lemon-Syntax Snippet

No new parser production is required for this phase. The parser already accepts
the relevant expression forms. The analyzer/runtime acceptance grammar is:

```lemon
arithmetic_scalar(A) ::= arithmetic_additive(B).

arithmetic_additive(A) ::= arithmetic_additive(B) PLUS arithmetic_multiplicative(C).
arithmetic_additive(A) ::= arithmetic_additive(B) MINUS arithmetic_multiplicative(C).
arithmetic_additive(A) ::= arithmetic_multiplicative(B).

arithmetic_multiplicative(A) ::= arithmetic_multiplicative(B) STAR arithmetic_primary(C).
arithmetic_multiplicative(A) ::= arithmetic_primary(B).

arithmetic_primary(A) ::= scalar_value(B).
arithmetic_primary(A) ::= LPAREN arithmetic_scalar(B) RPAREN.
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

Evaluation is one row wide:

1. Validate every select item against the admitted session scalar, scalar value,
   or arithmetic scalar subset.
2. Preserve function-specific wrong-arity diagnostics before generic arithmetic
   diagnostics.
3. Evaluate each arithmetic expression using MyLite-owned signed 64-bit checked
   arithmetic.
4. Preserve existing scalar value function semantics.
5. Convert `TRUE` and `FALSE` to `1` and `0` for arithmetic operands.
6. If either operand of an admitted arithmetic operator is `NULL`, the operator
   result is `NULL`, after any already-required child expression validation.
7. If a non-`NULL` arithmetic result is outside `[-9223372036854775808,
   9223372036854775807]`, return deterministic MySQL-compatible overflow
   diagnostics for the supported subset.
8. Render non-`NULL` results in canonical decimal text.
9. Return one result row with one value per select item.

Supported arithmetic expressions produce `warning_count == 0`.

Default result-column labels continue to use the source-span convention already
used by scalar projection. Runtime probes confirm MySQL labels such as:

- `1+2*3`;
- `(1+2)*3`;
- `TRUE+2`;
- `IFNULL(NULL,5)*2`; and
- explicit aliases overriding the default label.

## Unsupported And Diagnostics

Unsupported forms must fail deterministically without falling through to
SQLite:

- table-backed arithmetic projection, including `SELECT 1 + id FROM t`;
- no-source or `DUAL` arithmetic projection with `WHERE`, `ORDER BY`, `GROUP BY`,
  `HAVING`, or `LIMIT`;
- arithmetic operators other than `+`, binary `-`, and `*`;
- unary `+` or `-` over arbitrary arithmetic expressions, including
  `-(1+2)`;
- string, decimal, float, hex, bit, temporal, JSON, parameter, subquery,
  column-reference, aggregate, window, user-variable, session-function, or
  system-variable operands;
- session/system variables inside scalar value functions or arithmetic;
- expression use in DML assignments, defaults, predicates, table `ORDER BY`,
  `GROUP BY`, `HAVING`, or aggregate arguments;
- source integer literals outside the admitted signed-64 operand envelope; and
- arithmetic results outside the signed-64 result envelope.

Wrong arities for existing native functions retain the existing
function-specific MySQL diagnostics. Unsupported expressions that look like a
scalar projection but exceed the admitted domain should use a MyLite-specific
diagnostic that mentions supported signed-64 arithmetic and scalar values.
Arithmetic overflow should use MySQL-compatible error code 1690 and SQLSTATE
`22003` where the supported subset can identify the overflowing operator.

Unknown system variables, disallowed scopes, allocation failures, parse errors,
and public API misuse preserve existing diagnostics.

## Tests

Fast C tests should cover:

- no-source and `FROM DUAL` arithmetic projection over integer, boolean,
  `NULL`, and current scalar value function operands;
- `+`, binary `-`, and `*`;
- precedence (`*` before `+`/`-`), left-to-right behavior for same-precedence
  operators, and parentheses;
- explicit `ALL`, aliases, duplicate labels, and default expression labels;
- `NULL` propagation;
- in-range signed boundaries, including a result of `-9223372036854775808`;
- deterministic overflow diagnostics for addition, subtraction, multiplication,
  and arithmetic nested below a `NULL` operator;
- `ROW_COUNT()`, `@@warning_count`, affected rows, absence of result mutation,
  and file-backed preamble/catalog-generation safety;
- independent handles;
- deterministic rejection of table-backed arithmetic projection, unsupported
  operators, expression unary signs, strings, decimals, floats, hex, bit,
  parameters, system/session values as operands, subqueries, and clauses around
  no-source arithmetic projection; and
- preservation of existing lexer, parser, scalar value projection,
  session-value scalar projection, result, statement-context, storage, and
  lifecycle tests.

Focused verification:

1. build parser/runtime test targets touched by the implementation;
2. run focused parser/runtime CTest entries;
3. run
   `packages/libmylite/tests/mysql_baseline_scalar_arithmetic_projection_expectations.sh`;
4. run `cmake --workflow --preset check`.

## Compatibility Notes

This phase is a bridge toward a general expression evaluator, but it is still
runtime-only and no-source only. Table-backed expression pushdown, expression
metadata, unsigned integer arithmetic, decimal and approximate math, division
and modulo warning behavior, implicit string conversion, subqueries, and
optimizer-grade expression planning remain future work. Some of those future
steps may need SQLite extension APIs or targeted SQLite fork hooks where public
SQLite interfaces cannot expose MySQL-compatible semantics or avoidable
overhead.

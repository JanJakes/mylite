# Baseline Scalar Unary Arithmetic Projection

## Summary

This phase extends the no-source and `FROM DUAL` scalar arithmetic projection
surface with MyLite-owned unary arithmetic over the existing signed-64 scalar
arithmetic domain:

```sql
SELECT unary_arithmetic_scalar[, unary_arithmetic_scalar ...]
SELECT ALL unary_arithmetic_scalar[, unary_arithmetic_scalar ...]
SELECT unary_arithmetic_scalar[, unary_arithmetic_scalar ...] FROM DUAL
SELECT ALL unary_arithmetic_scalar[, unary_arithmetic_scalar ...] FROM DUAL
```

The admitted unary operators are unary `+` and unary `-`. Operands may be the
current `baseline-scalar-arithmetic-projection` domain: signed-64 decimal
integer/boolean/`NULL` values, supported scalar `IF()`/`IFNULL()`/`COALESCE()`/
`NULLIF()`/`ISNULL()` values, parenthesized admitted arithmetic, and binary
`+`, binary `-`, and `*` arithmetic over the same domain.

This remains a scalar projection slice, not the general expression engine. It
excludes table-backed expression projection, expression predicates,
expression assignments, unary operators inside scalar function argument
domains beyond the already admitted literal signs, string/decimal/float/hex/bit
operands, system/session values as numeric operands, bitwise inversion, logical
negation, casts, parameters, subqueries, CTEs, date/interval arithmetic, and
arbitrary SQLite pass-through.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - Expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - `NULL` behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/problems-with-null.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_scalar_unary_arithmetic_projection_expectations.sh`
  and verified against MySQL 8.4.9.

Runtime probes against MySQL 8.4.9 confirm:

- unary `-` changes the sign of numeric operands;
- unary `+` returns the numeric operand unchanged;
- unary operators bind tighter than binary `*`, `+`, and `-`;
- parentheses may make the unary operator apply to a full arithmetic
  expression, such as `-(1+2)`;
- repeated unary signs are accepted when tokenized as separate operators, such
  as `- -1`, `+ -1`, `- +1`, and `+ +1`;
- `-NULL` and `+NULL` return `NULL`;
- `TRUE` and `FALSE` behave as `1` and `0`;
- overflow inside a child expression raises the child overflow error before an
  outer unary operator or outer `NULL` arithmetic could hide it;
- supported in-range unary expressions produce no warnings; and
- MySQL also accepts broader unary operands and results, including strings,
  decimals, system variables, table columns, and unsigned-width outcomes such
  as `-(-9223372036854775807 - 1) = 9223372036854775808`. Those remain
  deferred by this MyLite slice.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle lifetime, diagnostics, and statement-boundary behavior.
- Statement context: successful supported unary scalar `SELECT` statements use
  the existing row-returning result conventions: one row, zero affected rows,
  statement warning count, and following `ROW_COUNT()` state `-1`.
- Lexer/parser/AST: no new tokens are required. The existing unary-expression,
  binary-expression, parenthesized-expression, literal, function, `FROM DUAL`,
  `ALL`, and alias AST nodes are reused.
- Analyzer/runtime: scalar projection admission accepts unary `+` and unary `-`
  only when the operand is admitted by this feature's scalar arithmetic domain.
  Runtime evaluation remains MyLite-owned, iterative, and checked against the
  signed-64 result envelope.
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
SELECT unary_item[, unary_item ...]
SELECT ALL unary_item[, unary_item ...]
SELECT unary_item[, unary_item ...] FROM DUAL
SELECT ALL unary_item[, unary_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
unary_item:
    unary_arithmetic_scalar
  | unary_arithmetic_scalar AS alias
  | unary_arithmetic_scalar alias
```

The admitted value grammar is:

```sql
unary_arithmetic_scalar:
    scalar_arithmetic_projection.arithmetic_operand
  | + unary_arithmetic_scalar
  | - unary_arithmetic_scalar
  | unary_arithmetic_scalar + unary_arithmetic_scalar
  | unary_arithmetic_scalar - unary_arithmetic_scalar
  | unary_arithmetic_scalar * unary_arithmetic_scalar
  | ( unary_arithmetic_scalar )
```

The operand and result domain is intentionally signed 64-bit:

- decimal integer literals with optional unary `+` or `-`, where source
  literal magnitude is no greater than `9223372036854775807`;
- `TRUE`, `FALSE`, and `NULL`;
- supported scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values
  over their existing scalar-value domain; and
- arithmetic expression results within
  `[-9223372036854775808, 9223372036854775807]`.

Unary `-` of `-9223372036854775808` is not admitted in this slice because MySQL
returns `9223372036854775808`, which is outside the current signed-64 scalar
carrier. MyLite must return a deterministic signed-64 arithmetic diagnostic for
that deferred case rather than silently wrapping or changing type semantics.

### MyLite Lemon-Syntax Snippet

No new parser production is required for this phase. The parser already accepts
the relevant unary expression forms. The analyzer/runtime acceptance grammar is:

```lemon
unary_arithmetic_scalar(A) ::= arithmetic_additive(B).

arithmetic_additive(A) ::= arithmetic_additive(B) PLUS arithmetic_multiplicative(C).
arithmetic_additive(A) ::= arithmetic_additive(B) MINUS arithmetic_multiplicative(C).
arithmetic_additive(A) ::= arithmetic_multiplicative(B).

arithmetic_multiplicative(A) ::= arithmetic_multiplicative(B) STAR arithmetic_unary(C).
arithmetic_multiplicative(A) ::= arithmetic_unary(B).

arithmetic_unary(A) ::= PLUS arithmetic_unary(B).
arithmetic_unary(A) ::= MINUS arithmetic_unary(B).
arithmetic_unary(A) ::= arithmetic_primary(B).

arithmetic_primary(A) ::= scalar_value(B).
arithmetic_primary(A) ::= LPAREN unary_arithmetic_scalar(B) RPAREN.
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

Evaluation is one row wide:

1. Validate every select item against the admitted session scalar, scalar value,
   or unary arithmetic scalar subset.
2. Preserve function-specific wrong-arity diagnostics before generic arithmetic
   diagnostics.
3. Evaluate each unary arithmetic expression using MyLite-owned checked
   signed-64 arithmetic.
4. Preserve existing scalar value function semantics.
5. Convert `TRUE` and `FALSE` to `1` and `0` for arithmetic operands.
6. If the unary operand is `NULL`, the unary result is `NULL`, after any
   already-required child expression validation.
7. Unary `+` returns the operand unchanged.
8. Unary `-` returns the sign-negated operand unless doing so would leave the
   signed-64 result envelope.
9. If a child expression overflows, report the child expression's
   MySQL-compatible overflow diagnostic rather than a generic unary diagnostic.
10. Render non-`NULL` results in canonical decimal text.
11. Return one result row with one value per select item.

Supported unary arithmetic expressions produce `warning_count == 0`.

Default result-column labels continue to use the source-span convention already
used by scalar projection. Runtime probes confirm MySQL labels such as:

- `-(1+2)`;
- `+(3*4)`;
- `- -1`;
- `-NULL`; and
- explicit aliases overriding default labels.

## Unsupported And Diagnostics

Unsupported forms must fail deterministically without falling through to
SQLite:

- table-backed unary arithmetic projection, including `SELECT -id FROM t`;
- no-source or `DUAL` unary arithmetic projection with `WHERE`, `ORDER BY`,
  `GROUP BY`, `HAVING`, or `LIMIT`;
- unary operators other than `+` and `-`;
- unary `+` or unary `-` inside scalar function argument domains when the
  operand is an arithmetic expression, such as `IF(-(1+2), 1, 0)`;
- unary `-` of the current signed-64 minimum value;
- string, decimal, float, hex, bit, temporal, JSON, parameter, subquery,
  column-reference, aggregate, window, user-variable, session-function, or
  system-variable operands;
- expression use in DML assignments, defaults, predicates, table `ORDER BY`,
  `GROUP BY`, `HAVING`, or aggregate arguments;
- source integer literals outside the admitted signed-64 operand envelope; and
- arithmetic results outside the signed-64 result envelope.

Wrong arities for existing native functions retain existing function-specific
MySQL diagnostics. Unsupported expressions that look like scalar projection but
exceed the admitted domain should use a MyLite-specific diagnostic that mentions
supported signed-64 arithmetic and scalar values. Binary arithmetic overflow
continues to use MySQL-compatible error code 1690 and SQLSTATE `22003` for the
supported subset. Unary sign overflow should use the same error family when the
supported subset can identify the overflowing unary operator.

Unknown system variables, disallowed scopes, allocation failures, parse errors,
and public API misuse preserve existing diagnostics.

## Tests

Fast C tests should cover:

- no-source and `FROM DUAL` unary arithmetic projection over integer, boolean,
  `NULL`, and current scalar value function operands;
- unary `+` and unary `-`;
- repeated unary signs, precedence with `*`, binary `+`, and binary `-`, and
  parentheses;
- explicit `ALL`, aliases, duplicate labels, and default expression labels;
- `NULL` propagation;
- in-range signed boundaries, including `+(-9223372036854775807 - 1)` and
  rejected unary `-` of `-9223372036854775808`;
- deterministic overflow diagnostics for unary negation and child binary
  overflow under unary operators;
- `ROW_COUNT()`, `@@warning_count`, affected rows, absence of result mutation,
  and file-backed preamble/catalog-generation safety;
- independent handles;
- deterministic rejection of table-backed unary arithmetic projection,
  unsupported unary operators, strings, decimals, floats, hex, bit, parameters,
  system/session values as operands, subqueries, scalar-function arithmetic
  arguments if deferred, and clauses around no-source unary arithmetic
  projection; and
- preservation of existing lexer, parser, scalar value projection, scalar
  arithmetic projection, session-value scalar projection, result,
  statement-context, storage, and lifecycle tests.

Focused verification:

1. build parser/runtime test targets touched by the implementation;
2. run focused parser/runtime CTest entries;
3. run
   `packages/libmylite/tests/mysql_baseline_scalar_unary_arithmetic_projection_expectations.sh`;
4. run `cmake --workflow --preset check`.

## Compatibility Notes

Compatibility tables should mark unary `-` as a limited no-source/`DUAL`
signed-64 scalar projection feature once implemented. Binary `+`, binary `-`,
and `*` remain scoped to the existing scalar arithmetic slice. The detail docs
must not claim general expression evaluation, table-backed expression
projection, unsigned expression results, unary bit inversion, logical
negation, casts, string/decimal/float conversion, date arithmetic, or table
column unary operators.

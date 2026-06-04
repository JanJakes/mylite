# Baseline Scalar Expression Projection

## Summary

This phase turns the existing no-source and `FROM DUAL` scalar `SELECT` path
from several homogeneous one-function lanes into one deliberately small
MyLite-owned scalar value projection lane:

```sql
SELECT scalar_value[, scalar_value ...]
SELECT ALL scalar_value[, scalar_value ...]
SELECT scalar_value[, scalar_value ...] FROM DUAL
SELECT ALL scalar_value[, scalar_value ...] FROM DUAL
```

The admitted values are the already-supported warning-free scalar leaves and
functions:

- top-level decimal integer literals with optional unary `+` or `-` in the
  current literal-projection envelope;
- top-level hexadecimal and bit literals as byte-safe binary scalar values;
- `TRUE`, `FALSE`, and `NULL`;
- parentheses around admitted scalar values; and
- nested supported `IF()`, `IFNULL()`, `COALESCE()`, `NULLIF()`, and
  `ISNULL()` over the same value domain.

The user-visible addition is mixed scalar value select lists such as
`SELECT 1, IF(1,2,3), ISNULL(NULL) FROM DUAL`, plus parenthesized admitted
values inside and outside the supported scalar functions. The architectural
addition is a shared scalar-value projection classifier and validation path,
so new scalar functions do not require another homogeneous projection lane.

This is still not a general expression engine. It does not add table-backed
expression projection, arithmetic, comparison or logical operators as scalar
values, string/decimal/float/temporal operands, hex or bit literals as numeric
operands, user or system
variables inside scalar value functions, subqueries, CTEs, aliases in
expressions, query clauses around no-source scalar value projection, predicates,
DML assignment expressions, expression metadata, or arbitrary SQLite
pass-through.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - Expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - Functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
  - Flow-control functions:
    <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
  - Comparison functions and `ISNULL()`:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - Numeric literals:
    <https://dev.mysql.com/doc/refman/8.4/en/number-literals.html>
  - Boolean literals:
    <https://dev.mysql.com/doc/refman/8.4/en/boolean-literals.html>
  - `NULL` values:
    <https://dev.mysql.com/doc/refman/8.4/en/null-values.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_scalar_expression_projection_expectations.sh`
  and verified against MySQL 8.4.9.

The MySQL manual permits `SELECT` results computed without a table and permits
`DUAL` as a dummy table for no-table expressions. It also documents the broad
MySQL expression grammar, including literals, function calls, unary operators,
parentheses, arithmetic, predicates, subqueries, variables, and more. MyLite's
baseline admits only the scalar values it already owns precisely.

Runtime probes against MySQL 8.4.9 confirm:

- mixed no-source scalar value select lists return one row with one column per
  expression;
- `FROM DUAL` has the same result for expressions that reference no table;
- explicit `ALL` is accepted and remains the duplicate-preserving default;
- explicit aliases override default expression labels, including bare aliases;
- default labels preserve expression source text for supported scalar
  functions and most parenthesized expressions, with MySQL-observed literal
  exceptions such as string literals labeling as their decoded string value,
  `+2` and `(+2)` labeling as `2`, `(1)` labeling as `1`, and `(NULL)`
  labeling as `NULL`;
- parenthesized top-level scalar values and parenthesized scalar function
  operands are accepted;
- `TRUE` and `FALSE` render as `1` and `0`;
- `NULL` renders as SQL `NULL`;
- successful admitted projections report `@@warning_count = 0` and make a
  following `ROW_COUNT()` return `-1`;
- wrong function arities retain the function-specific MySQL diagnostics or
  syntax errors from the existing function slices; and
- MySQL accepts much broader forms such as arithmetic, table-backed expression
  projection, no-source `WHERE`, `ORDER BY`, and `LIMIT`, all of which remain
  deferred in this MyLite slice.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle lifetime, diagnostics, and statement-boundary behavior.
- Statement context: successful scalar value `SELECT` statements use the
  existing row-returning `SELECT` result conventions: one result row, zero
  affected rows, statement warning count, and previous row-count state `-1`.
- Lexer/parser/AST: no new tokens are required. Existing expression AST nodes,
  function nodes, parenthesized expressions, source spans, and select-item
  aliases are reused.
- Analyzer/runtime: the scalar projection analyzer accepts a no-source or
  `FROM DUAL` select only when every select item is an admitted scalar value
  expression. Evaluation remains MyLite-owned and reuses the existing
  nonrecursive scalar function evaluator.
- Catalog: not involved. The feature must not read or mutate schema/table
  descriptors, descriptor versions, catalog generation, descriptor caches, or
  `sqlite_schema_generation`.
- Result builder: appends one column per select item and one row through
  existing result helpers. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  This is MyLite wrapper/runtime behavior, not a SQLite extension point.

## Supported SQL

Supported statement shapes:

```sql
SELECT scalar_value_item[, scalar_value_item ...]
SELECT ALL scalar_value_item[, scalar_value_item ...]
SELECT scalar_value_item[, scalar_value_item ...] FROM DUAL
SELECT ALL scalar_value_item[, scalar_value_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
scalar_value_item:
    scalar_value
  | scalar_value AS alias
  | scalar_value alias
```

The admitted scalar value expression subset is:

```sql
scalar_value:
    scalar_integer
  | TRUE
  | FALSE
  | NULL
  | ( scalar_value )
  | IF ( scalar_value , scalar_value , scalar_value )
  | IFNULL ( scalar_value , scalar_value )
  | COALESCE ( scalar_value [, scalar_value ...] )
  | NULLIF ( scalar_value , scalar_value )
  | ISNULL ( scalar_value )

scalar_integer:
    unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
```

Top-level scalar integer select items keep the existing literal-projection
envelope and diagnostics, including the current 81-significant-digit exact
integer limit. Scalar function operands remain limited to the warning-free
signed-64 baseline envelope: `-9223372036854775807` through
`9223372036854775807`. Larger MySQL unsigned and exact numeric function
behavior is deferred until MyLite owns expression numeric types, warnings, and
metadata more generally.

### MyLite Lemon-Syntax Snippet

No new grammar production is required for this phase. The parser already
accepts the relevant expression forms. The analyzer/runtime acceptance grammar
is:

```lemon
scalar_value(A) ::= INTEGER(T).
scalar_value(A) ::= PLUS(P) INTEGER(T).
scalar_value(A) ::= MINUS(M) INTEGER(T).
scalar_value(A) ::= TRUE(T).
scalar_value(A) ::= FALSE(T).
scalar_value(A) ::= NULL(T).
scalar_value(A) ::= LPAREN scalar_value(B) RPAREN(R).
scalar_value(A) ::= IF(T) LPAREN scalar_value(B) COMMA scalar_value(C) COMMA scalar_value(D) RPAREN(R).
scalar_value(A) ::= IFNULL(T) LPAREN scalar_value(B) COMMA scalar_value(C) RPAREN(R).
scalar_value(A) ::= COALESCE(T) LPAREN scalar_value_list(B) RPAREN(R).
scalar_value(A) ::= NULLIF(T) LPAREN scalar_value(B) COMMA scalar_value(C) RPAREN(R).
scalar_value(A) ::= ISNULL(T) LPAREN scalar_value(B) RPAREN(R).
scalar_value_list(A) ::= scalar_value(B).
scalar_value_list(A) ::= scalar_value_list(B) COMMA scalar_value(C).
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

Evaluation is one row wide:

1. Validate every select item against the admitted scalar value subset.
2. Preserve function-specific wrong-arity diagnostics when an admitted function
   name has an invalid argument count.
3. Evaluate each select item independently from left to right.
4. Preserve existing scalar function semantics for `IF()`, `IFNULL()`,
   `COALESCE()`, `NULLIF()`, and `ISNULL()`.
5. Preserve SQL `NULL`.
6. Render booleans as `1` and `0`.
7. Render admitted integers in canonical decimal text.
8. Return one result row with one value per select item.

Default result-column labels use the existing source-span convention where it
matches MySQL's observed labels:

- `SELECT 1` labels the column `1`;
- `SELECT +2` labels the column `2`;
- `SELECT (1)` labels the column `1`;
- `SELECT (+2)` labels the column `2`;
- `SELECT 'abc'` labels the column `abc`;
- `SELECT ('abc')` labels the column `abc`;
- `SELECT (NULL)` labels the column `NULL`;
- `SELECT (TRUE)` labels the column `(TRUE)`;
- `SELECT (-3)` labels the column `(-3)`;
- `SELECT IFNULL((NULL),(6))` labels the column `IFNULL((NULL),(6))`; and
- explicit aliases override the default label.

Successful supported statements return:

- one row;
- one column per select item;
- `affected_rows == 0`;
- `warning_count == 0` for the admitted warning-free subset; and
- a following `ROW_COUNT()` result of `-1`.

## Unsupported And Diagnostics

Unsupported forms must fail deterministically without falling through to
SQLite:

- table-backed scalar value projection, including `SELECT 1 FROM t`;
- scalar value projection with no-source `WHERE`, `ORDER BY`, `GROUP BY`,
  `HAVING`, or `LIMIT`;
- arithmetic, comparison, logical, bitwise, cast, string, decimal, float, hex,
  bit, temporal, JSON, variable, parameter, subquery, column-reference,
  aggregate, window, or arbitrary function values;
- `CASE`, `GREATEST()`, `LEAST()`, `INTERVAL()`, `VALUES()`, `DEFAULT(col)`,
  and general function calls;
- user or system variables as operands inside scalar value functions;
- function values in DML assignments, defaults, predicates, table `ORDER BY`,
  `GROUP BY`, `HAVING`, or aggregate arguments; and
- integers outside the admitted signed-64 warning-free envelope.

Wrong arities for `IFNULL()`, `COALESCE()`, `NULLIF()`, and `ISNULL()` use the
existing native function parameter-count diagnostics. Wrong `IF()` arities
remain syntax errors in the current parser.

The preferred MyLite-specific unsupported diagnostic for expressions that look
like this scalar value lane but exceed the admitted value domain is:

```text
SELECT scalar expression projection supports only signed 64-bit integer,
boolean, NULL, and nested IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() values
```

Existing lower-level function-specific diagnostics may remain where they are
more precise for a recognized top-level function call.

## Tests

Fast C tests should cover:

- mixed no-source scalar value projection with literals and all supported
  scalar functions, preserving the existing top-level literal integer envelope;
- mixed `FROM DUAL` projection and explicit `ALL`;
- explicit aliases and default labels, including unary `+` and parentheses;
- parenthesized top-level values and parenthesized nested function operands;
- nested combinations across `IF()`, `IFNULL()`, `COALESCE()`, `NULLIF()`, and
  `ISNULL()`;
- row count, warning count, affected rows, absence of catalog generation
  changes, and `.mylite` preamble preservation;
- deterministic rejection of table-backed scalar value projection, arithmetic,
  clauses around no-source scalar value projection, parameters, variables,
  subqueries, column references, and unsupported functions;
- wrong arity propagation in a mixed projection list;
- independent handles; and
- zero-initialized cleanup for any new helper objects.

Focused verification:

1. build parser/runtime test targets touched by the implementation;
2. run focused parser/runtime CTest entries;
3. run
   `packages/libmylite/tests/mysql_baseline_scalar_expression_projection_expectations.sh`;
4. run `cmake --workflow --preset check`.

## Compatibility Notes

This phase moves MyLite closer to a real expression subsystem but intentionally
does not claim general expression support. General expressions, table-backed
expression projection, subqueries, and expression metadata still require a
larger planner/evaluator design and may need SQLite extension hooks or targeted
fork hooks where public SQLite APIs cannot expose MySQL-compatible semantics or
performance.

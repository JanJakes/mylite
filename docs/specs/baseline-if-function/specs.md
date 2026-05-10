# Baseline IF Function

## Summary

This phase admits a deliberately small MySQL `IF(expr1, expr2, expr3)` scalar
function surface in the existing no-source and `FROM DUAL` scalar `SELECT`
path:

```sql
SELECT IF(condition, true_value, false_value)[, ...]
SELECT IF(condition, true_value, false_value)[, ...] FROM DUAL
```

The admitted values are MyLite-owned integer/boolean/`NULL` scalar values, plus
nested `IF()` calls over the same admitted value domain. `IF()` returns the
second argument when the condition is nonzero and not `NULL`; otherwise it
returns the third argument.

This is a step toward a real expression subsystem, not a general expression
engine. The slice does not add table-backed `IF()`, `WHERE IF(...)`, arithmetic
arguments, string/decimal/float/hex/bit operands, casts, collations, subqueries,
prepared parameters, `CASE`, `IFNULL`, `NULLIF`, or arbitrary SQLite
pass-through.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - Flow control functions:
    <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
  - Functions and operators overview:
    <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
  - Numeric literals:
    <https://dev.mysql.com/doc/refman/8.4/en/number-literals.html>
  - Boolean literals:
    <https://dev.mysql.com/doc/refman/8.4/en/boolean-literals.html>
  - `NULL` values:
    <https://dev.mysql.com/doc/refman/8.4/en/null-values.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_if_function_expectations.sh` and
  verified against MySQL 8.4.9.

The MySQL manual defines `IF()` as a flow-control function with three
expressions. MySQL returns the second expression when the first expression is
true, where true means nonzero and not `NULL`; otherwise it returns the third
expression. MySQL's general expression system also drives result-type
aggregation. MyLite's baseline slice intentionally admits only values it can
own today without string, decimal, collation, or protocol-grade expression
metadata.

Runtime probes against MySQL 8.4.9 confirm:

- `IF(1,2,3)`, `IF(-1,2,3)`, and `IF(TRUE,2,3)` return `2`.
- `IF(0,2,3)`, `IF(+0,2,3)`, `IF(NULL,2,3)`, and `IF(FALSE,2,3)` return `3`.
- `IF()` returns SQL `NULL` when the selected branch is explicit `NULL`.
- `TRUE` and `FALSE` branch values render as `1` and `0`.
- Decimal integer branch values render in canonical decimal text for the
  supported warning-free subset.
- `IF (1,2,3)` with a space before `(` is accepted.
- `(IF(1,2,3))` is accepted and uses the parenthesized source text as the
  default column label.
- nested `IF()` calls are accepted.
- `SELECT IF(1,2,3) FROM DUAL` behaves like no-source scalar projection.
- successful scalar `IF()` projection reports `@@warning_count = 0` for the
  admitted in-range values and makes a following `ROW_COUNT()` return `-1`.
- wrong arities such as `IF()`, `IF(1)`, `IF(1,2)`, and `IF(1,2,3,4)` are
  syntax errors in MySQL 8.4.9.
- MySQL accepts broader forms such as string operands, arithmetic operands,
  table-backed `IF()`, and no-source `ORDER BY` / `LIMIT`. Those are deferred
  by this MyLite slice.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle ownership, diagnostics, and statement-boundary behavior.
- Statement context: scalar `IF()` `SELECT` statements use existing
  row-returning `SELECT` result conventions, including previous row-count state
  `-1` and warning count storage.
- Lexer/parser/AST: the parser admits a new `IF()` expression node only for an
  exact three-argument argument list. It preserves source spans for default
  result-column labels and syntax diagnostics.
- Analyzer/runtime: the scalar projection analyzer accepts `IF()` only in the
  existing no-source and `FROM DUAL` scalar select path. Evaluation is
  MyLite-owned and walks nested admitted `IF()` expressions without SQLite SQL.
- Catalog: not involved. This feature must not read or mutate schema/table
  descriptors, descriptor versions, catalog generation, descriptor caches, or
  `sqlite_schema_generation`.
- Result builder: appends columns and one row through existing public result
  helpers. Successful scalar `IF()` returns no affected rows and one result row.
- Storage/VFS/file format: no file-format or VFS changes. Scalar `IF()` must
  not touch user-table storage or the `.mylite` preamble.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  The feature is a small MyLite expression evaluator in front of the existing
  public result API.

## Supported SQL

Supported statement shapes:

```sql
SELECT scalar_if_item[, scalar_if_item ...]
SELECT ALL scalar_if_item[, scalar_if_item ...]
SELECT scalar_if_item[, scalar_if_item ...] FROM DUAL
SELECT ALL scalar_if_item[, scalar_if_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
scalar_if_item:
    if_expression
  | if_expression AS alias
  | if_expression alias
```

The admitted expression subset is:

```sql
if_expression:
    IF ( if_condition , if_result , if_result )

if_condition:
    scalar_integer
  | TRUE
  | FALSE
  | NULL
  | if_expression

if_result:
    scalar_integer
  | TRUE
  | FALSE
  | NULL
  | if_expression

scalar_integer:
    unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
```

Integer operands are limited to the warning-free signed-64 baseline envelope:
`-9223372036854775807` through `9223372036854775807`. MySQL has additional
large integer and unsigned behavior, including warnings in some `IF()` numeric
contexts; that behavior is deferred until MyLite owns expression numeric types
and warnings more generally.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar, not MySQL's full grammar:

```lemon
expression(A) ::= IF(T) LPAREN if_argument(B) COMMA if_argument(C) COMMA if_argument(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_IF_FUNCTION, B, C, D, R);
}

if_argument(A) ::= INTEGER(T).
if_argument(A) ::= PLUS(P) INTEGER(T).
if_argument(A) ::= MINUS(M) INTEGER(T).
if_argument(A) ::= TRUE(T).
if_argument(A) ::= FALSE(T).
if_argument(A) ::= NULL(T).
if_argument(A) ::= IF(T) LPAREN if_argument(B) COMMA if_argument(C) COMMA if_argument(D) RPAREN(R).
```

The actual grammar may reuse the existing `expression` nonterminal for
arguments, but runtime acceptance must remain bounded to the admitted
`if_argument` expression subset.

## Semantics

Evaluation walks the admitted expression tree:

1. Evaluate the condition.
2. The condition is true when it is not `NULL` and its integer text is not
   zero.
3. Evaluate and return only the selected branch value.
4. Returned `NULL` values remain SQL `NULL`.
5. Returned boolean values render as `1` or `0`.
6. Returned integer values render in canonical decimal text, matching the
   existing scalar literal projection convention for the admitted signed-64
   subset.

MyLite may validate all nested `IF()` syntax and admitted operand kinds before
execution, but it must not evaluate unselected branch values in a way that
changes results or warnings for supported in-range operands. Since all admitted
operands are constants with no side effects, this distinction has no visible
effect inside the slice.

Default result-column labels use the existing source-span label convention:

- `SELECT IF(1,2,3)` labels the column `IF(1,2,3)`;
- `SELECT IF (1,2,3)` labels the column `IF (1,2,3)`;
- `SELECT (IF(1,2,3))` labels the column `(IF(1,2,3))`;
- explicit select-item aliases override the default label.

Successful supported statements return:

- one row;
- one column per select item;
- `affected_rows == 0`;
- `warning_count == 0`; and
- a following `ROW_COUNT()` result of `-1`.

## Unsupported And Diagnostics

Unsupported forms must fail deterministically without falling through to
SQLite:

- wrong `IF()` arity;
- table-backed `IF()` projection;
- `IF()` in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments,
  defaults, or predicates;
- no-source `ORDER BY` / `LIMIT` around scalar `IF()` projection;
- arithmetic, comparison, logical, bitwise, cast, string, decimal, float, hex,
  bit, temporal, JSON, variable, parameter, subquery, column-reference, or
  aggregate arguments;
- `CASE`, `IFNULL`, `NULLIF`, `COALESCE`, `DEFAULT(col_name)`, and general
  function calls; and
- integers outside the admitted signed-64 warning-free `IF()` envelope.

Wrong arity may surface as a parser syntax error because MySQL 8.4.9 reports
`IF()` arity mistakes as syntax errors, not as native-function
parameter-count errors. Unsupported but parseable forms should use the existing
MyLite unsupported-feature diagnostic style.

## Physical Handling And Performance

Scalar `IF()` projection is constant per statement. MyLite should evaluate it
directly into the result object without opening catalog descriptors, scanning
SQLite tables, or generating SQLite SQL. Nested `IF()` depth is bounded by
parser input and AST allocation; validation and evaluation should be linear in
the admitted expression tree size and may use small explicit stacks for nested
`IF()` traversal, plus the result-cell scratch storage already used by scalar
projection.

No SQLite extension API or fork patch is needed. A future general expression
engine may introduce shared expression planning or SQLite hooks, but this slice
keeps `IF()` under the current MyLite scalar path.

## Tests

The implementation must add fast C tests under `packages/libmylite/tests/`,
preferably a new `runtime_if_function` test binary. Coverage must include:

- no-source and `FROM DUAL` `IF()` projections;
- `SELECT ALL`;
- multiple select items and aliases;
- true, false, zero, negative, `NULL`, `TRUE`, and `FALSE` conditions;
- integer, boolean, and `NULL` branch values;
- nested `IF()` calls;
- source-span labels including spaced `IF (` and parenthesized `IF()`;
- warning count, affected rows, row count after scalar select, and no catalog
  mutation;
- file-backed preamble preservation and independent handles;
- deterministic rejection for wrong arity, table-backed `IF()`, no-source
  `ORDER BY` / `LIMIT`, arithmetic/string/decimal/float/hex/bit arguments,
  parameters, variables, subqueries, column arguments, `IF()` in predicates or
  DML assignment values, and out-of-range integer operands; and
- no regression in existing parser, scalar select, literal projection, system
  function, result metadata, statement-context, storage, and lifecycle tests.

The MySQL runtime expectation script must be run before implementation and
kept as the evidence artifact for visible behavior.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` `IF()` row from unsupported to limited support;
- `docs/compatibility/functions-control-flow.md` for the exact limited
  `IF()` subset;
- `docs/compatibility/sql-query-expressions.md` only if scalar expression
  wording needs to mention `IF()`; and
- avoid claiming `CASE`, `IFNULL`, `NULLIF`, `COALESCE`, general expressions,
  table-backed expressions, subqueries, or MySQL result metadata fidelity.

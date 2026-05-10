# Baseline COALESCE Function

## Summary

This phase admits a narrow MySQL `COALESCE(value, ...)` scalar function surface
in the existing no-source and `FROM DUAL` scalar `SELECT` path:

```sql
SELECT COALESCE(value[, value ...])[, ...]
SELECT COALESCE(value[, value ...])[, ...] FROM DUAL
```

The admitted values are MyLite-owned signed-64 decimal integer, boolean, and
`NULL` scalar values, plus nested supported scalar `COALESCE()`, `IFNULL()`,
and `IF()` calls over the same value domain. `COALESCE()` returns the first
non-`NULL` argument, or `NULL` when every argument is `NULL`.

This is still a baseline scalar path, not a general expression subsystem. It
does not add table-backed `COALESCE()`, predicates, DML assignment values,
arithmetic/string arguments, subqueries, expression metadata, or arbitrary
SQLite pass-through.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - Comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - Built-in function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html>
  - Functions and operators overview:
    <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
  - Numeric literals:
    <https://dev.mysql.com/doc/refman/8.4/en/number-literals.html>
  - Boolean literals:
    <https://dev.mysql.com/doc/refman/8.4/en/boolean-literals.html>
  - `NULL` values:
    <https://dev.mysql.com/doc/refman/8.4/en/null-values.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_coalesce_function_expectations.sh`
  and verified against MySQL 8.4.9.

The MySQL manual describes `COALESCE()` as returning the first non-`NULL` value
from its argument list, or `NULL` when no argument is non-`NULL`. MySQL's full
expression system also owns type aggregation, collations, warnings, broader
operand forms, and table-backed evaluation. This MyLite slice admits only the
constant scalar values currently owned by the baseline runtime.

Runtime probes against MySQL 8.4.9 confirm:

- `COALESCE(1)`, `COALESCE(1,2)`, `COALESCE(0,2)`,
  `COALESCE(FALSE,2)`, `COALESCE(TRUE,2)`, `COALESCE(+0,2)`, and
  `COALESCE(-1,2)` return the first argument because it is not `NULL`.
- `COALESCE(NULL)` returns SQL `NULL`; `COALESCE(NULL,10)` returns `10`;
  `COALESCE(NULL,NULL,NULL)` returns SQL `NULL`.
- `TRUE` and `FALSE` render as `1` and `0`.
- Decimal integer values render in canonical decimal text for the supported
  warning-free signed-64 subset.
- `COALESCE (NULL,10)` with a space before `(` is accepted.
- `(COALESCE(NULL,10))` is accepted and uses the parenthesized source text as
  the default column label.
- nested `COALESCE()`, supported nested `IFNULL()`, and supported nested `IF()`
  calls are accepted.
- `SELECT COALESCE(NULL,10) FROM DUAL` behaves like no-source scalar
  projection.
- successful scalar `COALESCE()` projection reports `@@warning_count = 0` for
  admitted in-range values and makes a following `ROW_COUNT()` return `-1`.
- `COALESCE()` with no arguments reports MySQL syntax error 1064 / SQLSTATE
  `42000`, rather than native-function argument-count error 1582.
- `COALESCE` remains usable as an unquoted nonreserved identifier.
- MySQL accepts broader forms such as string operands, arithmetic operands,
  user/system variables, subqueries, table-backed `COALESCE()`, and no-source
  `ORDER BY` / `LIMIT`. Those are deferred by this MyLite slice.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle ownership, diagnostics, and statement-boundary behavior.
- Statement context: scalar `COALESCE()` `SELECT` statements use existing
  row-returning `SELECT` result conventions, including previous row-count state
  `-1` and warning count storage.
- Lexer/parser/AST: the parser admits a new `COALESCE()` expression node with a
  one-or-more argument list. It preserves source spans for default result-column
  labels and diagnostics. `COALESCE` remains usable as an unquoted nonreserved
  identifier outside the admitted function-call grammar.
- Analyzer/runtime: the scalar projection analyzer accepts `COALESCE()` only in
  the existing no-source and `FROM DUAL` scalar select path. Evaluation is
  MyLite-owned and walks nested admitted `COALESCE()` / `IFNULL()` / `IF()`
  expressions without SQLite SQL.
- Catalog: not involved. This feature must not read or mutate schema/table
  descriptors, descriptor versions, catalog generation, descriptor caches, or
  `sqlite_schema_generation`.
- Result builder: appends columns and one row through existing public result
  helpers. Successful scalar `COALESCE()` returns no affected rows and one
  result row.
- Storage/VFS/file format: no file-format or VFS changes. Scalar `COALESCE()`
  must not touch user-table storage or the `.mylite` preamble.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  The feature is a MyLite scalar evaluator in front of the existing public
  result API.

## Supported SQL

Supported statement shapes:

```sql
SELECT scalar_coalesce_item[, scalar_coalesce_item ...]
SELECT ALL scalar_coalesce_item[, scalar_coalesce_item ...]
SELECT scalar_coalesce_item[, scalar_coalesce_item ...] FROM DUAL
SELECT ALL scalar_coalesce_item[, scalar_coalesce_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
scalar_coalesce_item:
    coalesce_expression
  | coalesce_expression AS alias
  | coalesce_expression alias
```

The admitted expression subset is:

```sql
coalesce_expression:
    COALESCE ( scalar_value )
  | COALESCE ( scalar_value , scalar_value_list )

scalar_value_list:
    scalar_value
  | scalar_value , scalar_value_list

scalar_value:
    scalar_integer
  | TRUE
  | FALSE
  | NULL
  | coalesce_expression
  | ifnull_expression
  | if_expression

scalar_integer:
    unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
```

Integer operands are limited to the warning-free signed-64 baseline envelope:
`-9223372036854775807` through `9223372036854775807`. MySQL's larger unsigned
and exact numeric behavior is deferred until MyLite owns expression numeric
types and warnings more generally.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar, not MySQL's full grammar:

```lemon
expression(A) ::= COALESCE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_COALESCE_FUNCTION, B, R);
}
identifier(A) ::= COALESCE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

`function_argument_list` already requires one or more expressions. Therefore
`COALESCE()` with no arguments remains a syntax error, matching the observed
MySQL 8.4.9 behavior for this slice.

## Semantics

Evaluation walks the admitted expression tree:

1. Evaluate arguments left to right.
2. Return the first argument whose value is not SQL `NULL`.
3. If every argument is SQL `NULL`, return SQL `NULL`.
4. Returned boolean values render as `1` or `0`.
5. Returned integer values render in canonical decimal text, matching the
   existing scalar literal projection convention for the admitted signed-64
   subset.

MyLite may validate all nested admitted scalar operands before execution, but
it must not evaluate later arguments in a way that changes results or warnings
for supported in-range operands. Since all admitted operands are constants with
no side effects, this distinction has no visible effect inside the slice.

Default result-column labels use the existing source-span label convention:

- `SELECT COALESCE(1,0)` labels the column `COALESCE(1,0)`;
- `SELECT COALESCE (NULL,10)` labels the column `COALESCE (NULL,10)`;
- `SELECT (COALESCE(NULL,10))` labels the column `(COALESCE(NULL,10))`;
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

- `COALESCE()` with no arguments as a syntax error 1064 / SQLSTATE `42000`;
- table-backed `COALESCE()` projection;
- `COALESCE()` in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments,
  defaults, or predicates;
- no-source `ORDER BY` / `LIMIT` around scalar `COALESCE()` projection;
- mixed literal, `IF()`, `IFNULL()`, and `COALESCE()` top-level scalar
  projection items unless the implementation explicitly admits and tests a
  broader scalar expression-list shape;
- arithmetic, comparison, logical, bitwise, cast, string, decimal, float, hex,
  bit, temporal, JSON, variable, parameter, subquery, column-reference, or
  aggregate arguments;
- `CASE`, `NULLIF`, `DEFAULT(col_name)`, and general function calls; and
- integers outside the admitted signed-64 warning-free envelope.

Unsupported but parseable forms should use the existing MyLite
unsupported-feature diagnostic style. Parser-only forms may surface as syntax
errors when MyLite has no admitted AST shape for the unsupported form.

## Physical Handling And Performance

Scalar `COALESCE()` projection is constant per statement. MyLite should
evaluate it directly into the result object without opening catalog
descriptors, scanning SQLite tables, or generating SQLite SQL. Nested
control-flow depth and argument count are bounded by parser input and AST
allocation; validation and evaluation should be linear in the admitted
expression tree size and may use small explicit stacks plus result-cell scratch
storage.

No SQLite extension API or fork patch is needed. A future general expression
engine may introduce shared expression planning or SQLite hooks, but this slice
keeps `COALESCE()` under the current MyLite scalar path.

## Tests

The implementation must add fast C tests under `packages/libmylite/tests/`,
preferably a new `runtime_coalesce_function` test binary. Coverage must
include:

- no-source and `FROM DUAL` `COALESCE()` projections;
- `SELECT ALL`;
- one-argument, two-argument, and three-or-more-argument forms;
- multiple select items and aliases;
- non-`NULL` first values including zero, negative, `TRUE`, and `FALSE`;
- `NULL` fallback values and all-`NULL` results;
- integer, boolean, and `NULL` returned values;
- nested `COALESCE()`, nested supported `IFNULL()`, and nested supported `IF()`
  operands;
- source-span labels including spaced `COALESCE (` and parenthesized
  `COALESCE()`;
- `COALESCE` as an unquoted table and column identifier;
- warning count, affected rows, row count after scalar select, and no catalog
  mutation;
- file-backed preamble preservation and independent handles;
- deterministic rejection for zero arguments, table-backed `COALESCE()`,
  no-source `ORDER BY` / `LIMIT`, mixed top-level scalar forms if deferred,
  arithmetic/string/decimal/float/hex/bit arguments, parameters, variables,
  subqueries, column arguments, `COALESCE()` in predicates or DML assignment
  values, and out-of-range integer operands; and
- no regression in existing parser, scalar select, literal projection, `IF()`,
  `IFNULL()`, system function, result metadata, statement-context, storage,
  and file-format tests.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` `COALESCE()` row from unsupported to limited support;
- `docs/compatibility/functions-control-flow.md`;
- `docs/compatibility/sql-query-expressions.md` projection rows for the exact
  scalar-only subset.

Do not overclaim general `COALESCE`, table-backed expression evaluation, type
aggregation, collations, expression metadata, string or decimal behavior,
subqueries, DML assignment support, or arbitrary expression evaluation.

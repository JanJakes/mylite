# Baseline IFNULL Function

## Summary

This phase admits a narrow MySQL `IFNULL(expr1, expr2)` scalar function surface
in the existing no-source and `FROM DUAL` scalar `SELECT` path:

```sql
SELECT IFNULL(value, fallback)[, ...]
SELECT IFNULL(value, fallback)[, ...] FROM DUAL
```

The admitted values are MyLite-owned signed-64 decimal integer, boolean, and
`NULL` scalar values, plus nested supported scalar `IFNULL()` and `IF()` calls
over the same admitted value domain. `IFNULL()` returns the first argument when
it is not `NULL`; otherwise it returns the second argument.

This is another step toward a real expression subsystem. It does not add
table-backed `IFNULL()`, predicates, DML assignment values, arithmetic/string
arguments, subqueries, expression metadata, or arbitrary SQLite pass-through.

Later row-scalar predicate work admits supported `IFNULL()` expressions as
direct descriptor-column comparison RHS values, such as
`WHERE id = IFNULL(1,0)`. The later
[baseline row-scalar truth predicates](../baseline-row-scalar-truth-predicates/specs.md)
slice also admits supported `WHERE IFNULL(...)` truth predicates. Other
unsupported expression contexts remain outside this baseline scalar slice.

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
  `packages/libmylite/tests/mysql_baseline_ifnull_function_expectations.sh` and
  verified against MySQL 8.4.9.

The MySQL manual defines `IFNULL()` as a two-argument flow-control function:
return the first expression if it is not `NULL`, otherwise return the second
expression. MySQL's full expression system also handles type aggregation,
collations, warnings, and broader operand forms. This MyLite slice admits only
the scalar values already owned by the baseline runtime.

Runtime probes against MySQL 8.4.9 confirm:

- `IFNULL(1,0)`, `IFNULL(0,2)`, `IFNULL(FALSE,2)`, `IFNULL(TRUE,2)`,
  `IFNULL(+0,2)`, and `IFNULL(-1,2)` return the first argument because it is
  not `NULL`.
- `IFNULL(NULL,10)` returns `10`; `IFNULL(NULL,NULL)` returns SQL `NULL`.
- `TRUE` and `FALSE` render as `1` and `0`.
- Decimal integer values render in canonical decimal text for the supported
  warning-free signed-64 subset.
- `IFNULL (NULL,10)` with a space before `(` is accepted.
- `(IFNULL(NULL,10))` is accepted and uses the parenthesized source text as the
  default column label.
- nested `IFNULL()` calls are accepted, and MySQL also accepts nesting with
  `IF()`.
- `SELECT IFNULL(NULL,10) FROM DUAL` behaves like no-source scalar projection.
- successful scalar `IFNULL()` projection reports `@@warning_count = 0` for the
  admitted in-range values and makes a following `ROW_COUNT()` return `-1`.
- wrong arities such as `IFNULL()`, `IFNULL(1)`, and `IFNULL(1,2,3)` report
  MySQL error 1582 / SQLSTATE `42000`, including when the malformed call is
  nested inside an admitted `IFNULL()` or `IF()` expression.
- MySQL accepts broader forms such as string operands, arithmetic operands,
  user/system variables, subqueries, table-backed `IFNULL()`, and no-source
  `ORDER BY` / `LIMIT`. Those are deferred by this MyLite slice.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle ownership, diagnostics, and statement-boundary behavior.
- Statement context: scalar `IFNULL()` `SELECT` statements use existing
  row-returning `SELECT` result conventions, including previous row-count state
  `-1` and warning count storage.
- Lexer/parser/AST: the parser admits a new `IFNULL()` expression node and a
  matching argument-count error node. It preserves source spans for default
  result-column labels and diagnostics. `IFNULL` remains usable as an unquoted
  nonreserved identifier outside the admitted function-call grammar.
- Analyzer/runtime: the scalar projection analyzer accepts `IFNULL()` only in
  the existing no-source and `FROM DUAL` scalar select path. Evaluation is
  MyLite-owned and walks nested admitted `IFNULL()` / `IF()` expressions without
  SQLite SQL.
- Catalog: not involved. This feature must not read or mutate schema/table
  descriptors, descriptor versions, catalog generation, descriptor caches, or
  `sqlite_schema_generation`.
- Result builder: appends columns and one row through existing public result
  helpers. Successful scalar `IFNULL()` returns no affected rows and one result
  row.
- Storage/VFS/file format: no file-format or VFS changes. Scalar `IFNULL()`
  must not touch user-table storage or the `.mylite` preamble.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  The feature is a small MyLite expression evaluator in front of the existing
  public result API.

## Supported SQL

Supported statement shapes:

```sql
SELECT scalar_ifnull_item[, scalar_ifnull_item ...]
SELECT ALL scalar_ifnull_item[, scalar_ifnull_item ...]
SELECT scalar_ifnull_item[, scalar_ifnull_item ...] FROM DUAL
SELECT ALL scalar_ifnull_item[, scalar_ifnull_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
scalar_ifnull_item:
    ifnull_expression
  | ifnull_expression AS alias
  | ifnull_expression alias
```

The admitted expression subset is:

```sql
ifnull_expression:
    IFNULL ( scalar_value , scalar_value )

scalar_value:
    scalar_integer
  | TRUE
  | FALSE
  | NULL
  | ifnull_expression
  | if_expression

if_expression:
    supported baseline IF expression

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
expression(A) ::= IFNULL(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_IFNULL_FUNCTION, B, C, R);
}
expression(A) ::= IFNULL(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= IFNULL(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR, B, R);
}
```

The actual grammar may reuse the existing `expression` nonterminal for
arguments, but runtime acceptance must remain bounded to the admitted
`scalar_value` expression subset. The exact two-argument production must appear
before the catch-all argument-count production.

## Semantics

Evaluation walks the admitted expression tree:

1. Evaluate the first argument.
2. If the first argument is not `NULL`, return it.
3. Otherwise evaluate and return the second argument.
4. Returned `NULL` values remain SQL `NULL`.
5. Returned boolean values render as `1` or `0`.
6. Returned integer values render in canonical decimal text, matching the
   existing scalar literal projection convention for the admitted signed-64
   subset.

MyLite may validate all nested admitted scalar operands before execution, but
it must not evaluate unselected fallback values in a way that changes results
or warnings for supported in-range operands. Since all admitted operands are
constants with no side effects, this distinction has no visible effect inside
the slice.

Default result-column labels use the existing source-span label convention:

- `SELECT IFNULL(1,0)` labels the column `IFNULL(1,0)`;
- `SELECT IFNULL (NULL,10)` labels the column `IFNULL (NULL,10)`;
- `SELECT (IFNULL(NULL,10))` labels the column `(IFNULL(NULL,10))`;
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

- wrong `IFNULL()` arity with MySQL-compatible error 1582 / SQLSTATE `42000`;
- table-backed `IFNULL()` projection;
- `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults, or unsupported
  predicate positions;
- no-source `ORDER BY` / `LIMIT` around scalar `IFNULL()` projection;
- mixed literal, `IF()`, and `IFNULL()` top-level scalar projection items
  unless the implementation explicitly admits and tests a broader scalar
  expression-list shape;
- arithmetic, comparison, logical, bitwise, cast, string, decimal, float, hex,
  bit, temporal, JSON, variable, parameter, subquery, column-reference, or
  aggregate arguments;
- `CASE`, `COALESCE`, `NULLIF`, `DEFAULT(col_name)`, and general function
  calls; and
- integers outside the admitted signed-64 warning-free envelope.

Unsupported but parseable forms should use the existing MyLite
unsupported-feature diagnostic style. Parser-only forms may surface as syntax
errors when MyLite has no admitted AST shape for the unsupported form.

## Physical Handling And Performance

Scalar `IFNULL()` projection is constant per statement. MyLite should evaluate
it directly into the result object without opening catalog descriptors,
scanning SQLite tables, or generating SQLite SQL. Nested control-flow depth is
bounded by parser input and AST allocation; validation and evaluation should be
linear in the admitted expression tree size and may use small explicit stacks
for nested traversal, plus result-cell scratch storage.

No SQLite extension API or fork patch is needed. A future general expression
engine may introduce shared expression planning or SQLite hooks, but this slice
keeps `IFNULL()` under the current MyLite scalar path.

## Tests

The implementation must add fast C tests under `packages/libmylite/tests/`,
preferably a new `runtime_ifnull_function` test binary. Coverage must include:

- no-source and `FROM DUAL` `IFNULL()` projections;
- `SELECT ALL`;
- multiple select items and aliases;
- non-`NULL` first argument values including zero, negative, `TRUE`, and
  `FALSE`;
- `NULL` first argument fallback values;
- integer, boolean, and `NULL` returned values;
- nested `IFNULL()` and nested supported `IF()` operands;
- source-span labels including spaced `IFNULL (` and parenthesized `IFNULL()`;
- warning count, affected rows, row count after scalar select, and no catalog
  mutation;
- file-backed preamble preservation and independent handles;
- deterministic rejection for wrong arity, table-backed `IFNULL()`, no-source
  `ORDER BY` / `LIMIT`, mixed top-level scalar forms if deferred,
  arithmetic/string/decimal/float/hex/bit arguments, parameters, variables,
  subqueries, unsupported column arguments, unsupported predicate positions,
  DML assignment values, and out-of-range integer operands; and
- no regression in existing parser, scalar select, literal projection,
  `IF()` function, system function, result metadata, statement-context,
  storage, and lifecycle tests.

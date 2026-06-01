# Baseline Window Rank And Navigation Functions

## Summary

This slice completes the remaining baseline window-function rows by extending
the existing descriptor-driven `ROW_NUMBER()` window projection path to:

- `RANK()`, `DENSE_RANK()`, `PERCENT_RANK()`, `CUME_DIST()`, and `NTILE()`;
- `LAG()`, `LEAD()`, `FIRST_VALUE()`, `LAST_VALUE()`, and `NTH_VALUE()`.

The support is intentionally narrow. It accepts these functions only in the
current row-scalar projection envelope: no-source or `FROM DUAL` for empty
windows, or one descriptor-backed table source with optional one-column
`PARTITION BY`, optional one-column `ORDER BY`, existing single-table `WHERE`,
outer `ORDER BY`, and `LIMIT`. It does not implement named windows, explicit
frames, expression keys, multi-key windows, grouped selects, joins, predicates,
DML contexts, or general window expression planning.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite window support:
  - `docs/specs/baseline-row-number-window-function/specs.md`
  - `packages/libmylite/tests/mysql_baseline_row_number_window_function_expectations.sh`
  - `packages/libmylite/tests/runtime_row_number_window_function_test.c`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/functions-window.md`
  - `docs/compatibility/sql-query-expressions.md`
- Official MySQL 8.4 Reference Manual:
  - window function descriptions:
    <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
  - window function usage:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_window_rank_navigation_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite behavior, and existing MyLite code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 observations used by this slice:

- `RANK()`, `DENSE_RANK()`, `PERCENT_RANK()`, `CUME_DIST()`, and `NTILE(1)`
  over an empty no-source window return one row: `1`, `1`, `0`, `1`, `1`.
- Ranking starts at `1`. `RANK()` leaves gaps for peer groups, while
  `DENSE_RANK()` does not.
- `PERCENT_RANK()` returns `0` for the first row or a one-row partition and
  `(rank - 1) / (partition_rows - 1)` otherwise.
- `CUME_DIST()` returns the fraction of partition rows whose ordering key is
  less than or equal to the current peer group.
- `NTILE(n)` assigns buckets `1..n` across each partition. `NTILE(0)` returns
  1210 / `HY000`; `NTILE(NULL)` and signed `NTILE(+/-n)` forms are rejected as
  syntax errors by the MySQL parser.
- `LAG(value)` and `LEAD(value)` use offset `1`. A second literal offset may
  be `0` or positive. A third literal default is used when the offset row does
  not exist. Without a third argument, missing rows return `NULL`. `NULL` and
  signed offset forms are rejected as syntax errors by the MySQL parser.
- `FIRST_VALUE(value)`, `LAST_VALUE(value)`, and `NTH_VALUE(value, n)` use the
  default frame. With `ORDER BY id`, `LAST_VALUE()` returns the current row
  value and `NTH_VALUE(value, 2)` returns `NULL` until the frame contains a
  second row.
- `NTH_VALUE(value, 0)`, `NTH_VALUE(value, -1)`, and
  `NTH_VALUE(value, NULL)` return error 1210 / `HY000` with an
  "Incorrect arguments" message.
- Metadata for `RANK()`, `DENSE_RANK()`, and `NTILE()` is non-null unsigned
  `LONGLONG`. Metadata for `PERCENT_RANK()` and `CUME_DIST()` is non-null
  `DOUBLE`. Navigation and frame-value functions expose the first argument's
  value type and are nullable.

## Scope

Supported syntax:

```sql
window_rank_expr ::= RANK LPAREN RPAREN OVER LPAREN window_spec_opt RPAREN
window_rank_expr ::= DENSE_RANK LPAREN RPAREN OVER LPAREN window_spec_opt RPAREN
window_rank_expr ::= PERCENT_RANK LPAREN RPAREN OVER LPAREN window_spec_opt RPAREN
window_rank_expr ::= CUME_DIST LPAREN RPAREN OVER LPAREN window_spec_opt RPAREN
window_rank_expr ::= NTILE LPAREN literal_integer RPAREN OVER LPAREN window_spec_opt RPAREN

window_nav_expr ::= LAG LPAREN value_arg RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= LAG LPAREN value_arg COMMA literal_integer RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= LAG LPAREN value_arg COMMA literal_integer COMMA literal_value RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= LEAD LPAREN value_arg RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= LEAD LPAREN value_arg COMMA literal_integer RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= LEAD LPAREN value_arg COMMA literal_integer COMMA literal_value RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= FIRST_VALUE LPAREN value_arg RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= LAST_VALUE LPAREN value_arg RPAREN OVER LPAREN window_spec_opt RPAREN
window_nav_expr ::= NTH_VALUE LPAREN value_arg COMMA literal_integer RPAREN OVER LPAREN window_spec_opt RPAREN

window_spec_opt ::= empty
window_spec_opt ::= window_partition_clause
window_spec_opt ::= window_order_clause
window_spec_opt ::= window_partition_clause window_order_clause

window_partition_clause ::= PARTITION BY qualified_identifier
window_order_clause ::= ORDER BY qualified_identifier order_direction_opt

order_direction_opt ::= empty
order_direction_opt ::= ASC
order_direction_opt ::= DESC
```

`value_arg` may be a descriptor column from the single source or a scalar
literal. `literal_integer` is an integer literal planned by the row-scalar
literal path. `literal_value` is a string, integer, boolean, or `NULL` literal.
Default and offset arguments do not accept descriptor columns in this phase.

Supported statement envelope:

- no-source and `FROM DUAL` projection for zero-argument rank/distribution
  functions and literal-argument value functions using `OVER ()`;
- one descriptor-backed base-table source using the existing selected/default
  schema policy and temporary-table shadowing behavior;
- explicit projection items mixing descriptor columns and top-level supported
  window functions;
- optional aliases using the existing select-item alias policy;
- existing row-scalar `WHERE`, outer `ORDER BY`, and outer `LIMIT` subsets;
- one unqualified or source-qualified descriptor column in `PARTITION BY`;
- one unqualified or source-qualified descriptor column in window `ORDER BY`;
- optional `ASC` / `DESC` in window `ORDER BY`.

Deferred syntax and behavior:

- named windows, window inheritance, explicit frames, `ROWS`, `RANGE`,
  `GROUPS`, `EXCLUDE`, and `FROM LAST`;
- multi-key `PARTITION BY` or window `ORDER BY`;
- expression, ordinal, parameter, collation, or function window keys;
- joins, derived tables, CTEs, grouped selects, aggregate windows, compound
  selects, and general optimizer planning for windows;
- window functions in predicates, `GROUP BY`, `HAVING`, DML assignments,
  defaults, generated columns, or nested scalar expressions;
- descriptor-column offsets or defaults for `LAG()` and `LEAD()`;
- approximate numeric, JSON, spatial, enum, and set column arguments.

## Ownership Boundaries

- Public API: unchanged.
- Parser/AST: adds MyLite-owned AST nodes for each supported function and
  reuses the compact baseline window spec grammar.
- Analyzer/planner: resolves descriptor columns and validates function argument
  counts, argument domains, and simple window specs before generating SQLite
  SQL.
- Catalog/storage/VFS: unchanged. This slice does not mutate catalog rows,
  file format state, SQLite pager state, or the MyLite file preamble.
- SQLite integration: uses SQLite's native window functions through generated
  SQL. It does not add a virtual table, UDF, extension hook, fork patch, or
  MyLite-side materialized row loop.

## Semantics

For admitted statements, MyLite lowers supported functions to SQLite native
window SQL after descriptor validation. The generated SQL uses quoted
identifiers and existing row-scalar source, predicate, order, and limit
builders.

`PARTITION BY` groups `NULL` values together. Window `ORDER BY` defaults to
ascending order. `WHERE` filters rows before window evaluation. Outer
`ORDER BY` and `LIMIT` are applied after projection and window evaluation.

For tied ordering values, tests must assert only MySQL-observed stable cases
with deterministic outer ordering. Broader tie-breaking is deferred until
multi-key window ordering is supported.

## Result Metadata

- `ROW_NUMBER()`, `RANK()`, `DENSE_RANK()`, and `NTILE()` report non-null
  unsigned `LONGLONG` metadata with MySQL scalar `BIGINT` display length.
- `PERCENT_RANK()` and `CUME_DIST()` report non-null `DOUBLE` metadata with
  MySQL scalar approximate display length and decimals.
- `LAG()`, `LEAD()`, `FIRST_VALUE()`, `LAST_VALUE()`, and `NTH_VALUE()` report
  metadata derived from the first value argument and mark the result nullable.
- Literal first arguments use the existing scalar literal metadata conventions.

Supported statements leave `warning_count == 0`.

## Diagnostics

The implementation must diagnose:

- syntax errors for missing `OVER`, malformed argument lists, named-window
  forms, explicit frame clauses, and unsupported grammar;
- MySQL-shaped 1210 / `HY000` incorrect-argument errors for `NTILE(0)` and
  invalid `NTH_VALUE()` integer indexes;
- MySQL-shaped syntax errors for `NTILE(NULL)`, signed `NTILE()` arguments,
  and `NULL` or signed `LAG()` / `LEAD()` offset arguments;
- unknown value, partition, and window order columns before SQLite SQL
  generation;
- unsupported source shapes, non-projection contexts, expression keys, multiple
  keys, non-literal offsets/defaults, and unsupported descriptor types with
  deterministic MyLite diagnostics;
- allocation and SQLite execution failures through existing runtime error
  handling.

## Tests

Add:

- `packages/libmylite/tests/mysql_baseline_window_rank_navigation_expectations.sh`
  for MySQL 8.4.9 result and error expectations;
- `packages/libmylite/tests/runtime_window_rank_navigation_functions_test.c`
  registered as `libmylite.runtime.window_rank_navigation_functions`.

Coverage must include no-source rank/distribution values, table ranking,
distribution, partitioned ranking, `NTILE`, navigation defaults, `FIRST_VALUE`,
`LAST_VALUE`, `NTH_VALUE`, nullable values, metadata, unknown columns,
incorrect integer arguments, and unsupported argument domains.

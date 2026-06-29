# Baseline Core Aggregate Window Functions

## Summary

This slice makes the core MySQL aggregate window subset executable in MyLite's
existing row-scalar projection window path:

```sql
SELECT id, COUNT(*) OVER (PARTITION BY author_id) AS c
FROM posts;

SELECT id, SUM(score) OVER w AS running
FROM posts
WINDOW w AS (
  PARTITION BY author_id
  ORDER BY created_at
  ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
);
```

The executable subset covers `COUNT()`, `SUM()`, `AVG()`, `MIN()`, and `MAX()`
over the existing single-table/tableless projection window envelope. It does
not add aggregate windows for MyLite-owned custom aggregates such as bitwise,
statistical, JSON, or `GROUP_CONCAT()` aggregate implementations.

## Sources And Evidence

- MyLite project policy and architecture:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/functions-aggregate.md`
  - `docs/compatibility/functions-window.md`
  - `docs/compatibility/sql-query-expressions.md`
- Existing window and aggregate specs:
  - `docs/specs/baseline-row-number-window-function/specs.md`
  - `docs/specs/baseline-window-rank-navigation-functions/specs.md`
  - `docs/specs/baseline-named-window-definitions/specs.md`
  - `docs/specs/baseline-multi-aggregate-select/specs.md`
- Official MySQL 8.4 Reference Manual:
  - aggregate functions:
    <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>
  - window function descriptions:
    <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
  - window concepts and `OVER` syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>
  - named windows:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-named-windows.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_core_aggregate_window_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite behavior, and existing MyLite code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
sources.

## MySQL 8.4.9 Runtime Observations

- Most aggregate functions may appear with an `OVER` clause and then operate
  over the resolved partition/frame instead of reducing the result to one row.
- `COUNT(*) OVER ()` on a source-free `SELECT` returns `1`.
- `COUNT(NULL) OVER (...)` returns `0`; `COUNT(literal)` counts the non-`NULL`
  literal once for each row in the partition.
- Integer `AVG()` aggregate-window results use the same four fractional-digit
  text shape observed for non-window integer `AVG()`, such as `6.0000`.
- Named-window definitions can be referenced by core aggregate windows and use
  the same duplicate, missing, inheritance, and cycle diagnostics already
  covered by the named-window baseline.
- `DISTINCT` aggregate window forms are parsed by MySQL but rejected with
  `1235 / 42000` and message text containing
  `<window function>(DISTINCT ..)`.
- `GROUP_CONCAT(...) OVER (...)` is rejected by MySQL 8.4.9 with
  `1235 / 42000`. MyLite keeps it outside this execution slice.
- MySQL accepts bitwise, statistical, and JSON aggregate windows; this core
  slice kept those as documented follow-up work because their implementations
  are MyLite-owned aggregate callbacks, not SQLite built-ins. Bitwise aggregate
  windows are covered by
  `docs/specs/baseline-bitwise-aggregate-window-functions/specs.md`.

## Scope

Supported grammar shape, matching MyLite's existing parser surface:

```text
aggregate_window_function ::=
    COUNT LPAREN STAR RPAREN over_clause
  | COUNT LPAREN expression RPAREN over_clause
  | MIN LPAREN expression RPAREN over_clause
  | MAX LPAREN expression RPAREN over_clause
  | SUM LPAREN expression RPAREN over_clause
  | AVG LPAREN expression RPAREN over_clause

over_clause ::= OVER identifier
over_clause ::= OVER LPAREN window_spec_opt RPAREN
window_clause ::= WINDOW named_window_definition_list
```

Executable statement envelope:

- source-free and `FROM DUAL` projection windows when the resolved window is
  empty;
- one descriptor-backed table source in the existing row-scalar select path;
- projection-only aggregate windows;
- `COUNT(*)`, `COUNT(column)`, `COUNT(literal)`, and current supported
  `COUNT(row_scalar)` expression arguments;
- integer-domain `SUM()`, `AVG()`, `MIN()`, and `MAX()` arguments: integer
  descriptor columns, integer/boolean/`NULL` literals, and the current
  supported signed-integer row-scalar expression subset;
- direct `OVER (...)`, direct `OVER window_name`, and inherited named windows;
- the existing single-key `PARTITION BY`, single-key `ORDER BY`, and baseline
  `ROWS`/`RANGE` frame clauses already accepted for projection windows.

Deferred behavior:

- `DISTINCT` aggregate windows beyond MySQL-compatible `1235` diagnostics;
- statistical, JSON, `GROUP_CONCAT()`, `ANY_VALUE()`, spatial, and
  user-defined aggregate windows;
- decimal widening beyond MyLite's current signed-64 integer aggregate
  envelope;
- approximate, string, binary, temporal, JSON, and spatial aggregate-window
  arguments for `SUM()`, `AVG()`, `MIN()`, and `MAX()`;
- grouped, joined, derived-table, CTE, compound, and DML window contexts beyond
  the current row-scalar projection envelope;
- expression or multi-key partition/order lists beyond the current inline
  window baseline;
- exact optimizer, cost, or protocol metadata parity.

## Runtime Design

The parser already attaches `WINDOW_SPEC` or `WINDOW_REFERENCE` nodes to
aggregate-function AST nodes when `OVER` is present. Runtime planning now treats
supported core aggregate-window nodes as row-scalar window expressions only when
an `OVER` child exists; non-window aggregate planning remains unchanged.

The existing named-window resolver is reused. After resolving the window
clauses, MyLite lowers supported aggregate windows to inline SQLite window SQL:

- `COUNT(*) OVER (...)`
- `COUNT(argument) OVER (...)`
- `SUM(argument) OVER (...)`
- `MIN(argument) OVER (...)`
- `MAX(argument) OVER (...)`

`AVG(argument) OVER (...)` is lowered as a MyLite scalar formatter over a
matching `SUM(argument) OVER (...)` and `COUNT(argument) OVER (...)`. This keeps
integer-average result text aligned with MySQL's four fractional-digit baseline
instead of exposing SQLite's floating result formatting.

Parameter binding duplicates the `AVG()` argument binding because the generated
SQL evaluates that argument in both the `SUM()` and `COUNT()` window calls.

## Diagnostics

- `DISTINCT` core aggregate windows return `1235 / 42000` with the MySQL-shaped
  message `This version of MySQL doesn't yet support '<window function>(DISTINCT ..)'`.
- Unsupported aggregate-window families retain deterministic MyLite unsupported
  diagnostics until a later slice implements their window callbacks.
- Existing named-window diagnostics from the named-window baseline continue to
  apply.
- Existing inline-window diagnostics for unsupported partition/order/frame
  shapes continue to apply.

## SQLite Integration

This feature uses MyLite-side wrapper/translation and public SQLite execution
only. SQLite built-in aggregate windows execute the core aggregate calls. MyLite
adds one private scalar SQLite function to format integer `AVG()` window
results from `SUM()`/`COUNT()` values. No SQLite fork hook is required.

Future statistical, JSON, or other MyLite-owned aggregate windows should use
`sqlite3_create_window_function()` with `xStep`, `xInverse`, `xValue`, and
`xFinal` callbacks, not a SQLite fork, unless profiling or correctness later
proves that a narrow hook is needed. The bitwise follow-up uses that public
SQLite API.

## Tests

- `packages/libmylite/tests/runtime_core_aggregate_window_functions_test.c`
  verifies source-free, table-backed, partitioned, ordered, framed, named-window,
  literal, column, `NULL`, and unsupported diagnostic behavior.
- `packages/libmylite/tests/mysql_baseline_core_aggregate_window_functions_expectations.sh`
  records equivalent MySQL 8.4.9 result and error expectations.
- The parser-corpus aggregate-window runtime test is updated so `SUM(1) OVER ()`
  is a supported execution path while JSON/statistical aggregate windows remain
  rejected.

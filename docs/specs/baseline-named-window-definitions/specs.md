# Baseline Named Window Definitions

## Summary

This slice makes MySQL named window definitions executable inside MyLite's
existing row-scalar projection window-function envelope:

```sql
SELECT id, ROW_NUMBER() OVER w
FROM posts
WINDOW w AS (PARTITION BY author_id ORDER BY created_at DESC);

SELECT id, ROW_NUMBER() OVER (base ORDER BY created_at DESC)
FROM posts
WINDOW base AS (PARTITION BY author_id);
```

It does not add aggregate window execution, joins, grouped selects, arbitrary
window key expressions, multi-key windows, or window functions outside supported
projection contexts.

## Sources And Evidence

- MyLite project policy and architecture:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/functions-window.md`
  - `docs/compatibility/sql-query-expressions.md`
- Existing executable window specs:
  - `docs/specs/baseline-row-number-window-function/specs.md`
  - `docs/specs/baseline-window-rank-navigation-functions/specs.md`
  - `docs/specs/baseline-window-rank-frame-clauses/specs.md`
  - `docs/specs/baseline-window-value-frame-clauses/specs.md`
- Official MySQL 8.4 Reference Manual:
  - named windows:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-named-windows.html>
  - window concepts and `OVER` syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>
  - window function descriptions:
    <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_named_window_definitions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite behavior, and existing MyLite code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
sources.

## MySQL 8.4.9 Runtime Observations

- `WINDOW w AS (...)` defines a window that can be referenced by `OVER w`.
- `OVER (w ...)` inherits the named definition and may add missing properties
  such as an `ORDER BY` when the named definition provides only `PARTITION BY`.
- A referring window cannot redefine a property already supplied by the named
  window. MySQL returns `3583 / HY000`.
- Window names compare case-insensitively for the observed baseline.
- Duplicate names are rejected eagerly with `3591 / HY000`; MySQL reports the
  first definition's spelling in the diagnostic.
- Missing inherited or referenced names are rejected with `3579 / HY000`.
- Cycles in the window dependency graph are rejected with `3580 / HY000`.
- Forward references are accepted when they resolve without a cycle.
- Named windows can carry `ROWS` / `RANGE` frame clauses for functions whose
  existing MyLite baseline admits those frames.

## Scope

Supported grammar shape, matching MyLite's existing parser surface:

```text
window_clause ::= WINDOW named_window_definition_list
named_window_definition_list ::= named_window_definition
named_window_definition_list ::= named_window_definition_list COMMA named_window_definition
named_window_definition ::= identifier AS LPAREN window_spec_opt RPAREN

over_clause ::= OVER identifier
over_clause ::= OVER LPAREN window_spec_opt RPAREN

window_spec_opt ::= empty
window_spec_opt ::= identifier
window_spec_opt ::= identifier window_partition_clause
window_spec_opt ::= identifier window_order_clause
window_spec_opt ::= identifier window_frame_clause
window_spec_opt ::= identifier window_partition_clause window_order_clause
window_spec_opt ::= identifier window_partition_clause window_frame_clause
window_spec_opt ::= identifier window_order_clause window_frame_clause
window_spec_opt ::= identifier window_partition_clause window_order_clause window_frame_clause
```

Executable statement envelope:

- no-source and `FROM DUAL` projection windows when the resolved named window
  is empty;
- one descriptor-backed table source in the row-scalar select path;
- top-level projection window functions already supported by the row-scalar
  baseline: ranking, distribution, navigation, and frame-value functions;
- direct `OVER window_name` references;
- `OVER (window_name ...)` inheritance that only adds missing
  `PARTITION BY`, `ORDER BY`, or frame clauses;
- chained and forward named-window references when the dependency graph is
  acyclic;
- duplicate-name, missing-name, circularity, and duplicate-property diagnostics
  matching observed MySQL 8.4.9 codes and messages for the baseline cases.

Deferred behavior:

- aggregate window execution such as `SUM(col) OVER w`;
- grouped, joined, derived-table, CTE, compound, and DML window contexts beyond
  the existing row-scalar projection envelope;
- expression or multi-key partition/order lists beyond the existing inline
  window baseline;
- ordinary descriptor selects with unused `WINDOW` clauses outside the
  row-scalar window path;
- unused named-window definitions, because full MySQL-compatible validation of
  unreferenced window clauses is not implemented in this slice;
- broader MySQL collation, privilege, optimizer, or execution-plan behavior.

## Runtime Design

The parser already produces `WINDOW_DEFINITION_LIST`, `WINDOW_DEFINITION`,
`WINDOW_REFERENCE`, and `WINDOW_SPEC` nodes. Runtime planning now builds a
borrowed statement-local table of named window definitions for row-scalar
selects. The table stores each definition name, AST spec pointer, resolution
state, and resolved partition/order/frame clause pointers.

Resolution is eager for the definition list once the row-scalar select planner
collects optional clauses. This mirrors MySQL for duplicate names, missing
inherited names, cycles, and duplicate inherited properties before SQLite SQL is
generated.

After resolution, `plan_row_scalar_window_function_expression()` receives the
same partition/order/frame clause pointers that inline windows use. Existing
per-function validation, descriptor-column resolution, frame validation, and
SQLite SQL rendering remain the authority. The generated SQLite remains inline
`OVER (...)` SQL; MyLite does not emit SQLite `WINDOW` clauses, allocate
temporary tables, or add SQLite fork hooks for this feature.

## Diagnostics

MyLite matches these MySQL 8.4.9 diagnostics in the baseline:

- `3579 / HY000`: `Window name '<name>' is not defined.`
- `3580 / HY000`: `There is a circularity in the window dependency graph.`
- `3583 / HY000`: duplicate inherited `PARTITION BY`, `ORDER BY`, or frame
  properties.
- `3591 / HY000`: `Window '<name>' is defined twice.`

Unsupported resolved window specs still use the existing deterministic MyLite
unsupported diagnostics from the inline window baseline.

## SQLite Integration

This feature uses MyLite-side wrapper/translation only. SQLite's native window
functions execute the already-supported inline window SQL generated by MyLite.
No public SQLite extension API additions, virtual tables, or targeted SQLite
fork hooks are required.

## Tests

- `packages/libmylite/tests/runtime_named_window_definitions_test.c`
  verifies direct references, inheritance, forward references,
  case-insensitive names, frame propagation, and diagnostics.
- `packages/libmylite/tests/mysql_baseline_named_window_definitions_expectations.sh`
  records equivalent MySQL 8.4.9 result and error expectations.

# Baseline Window Rank Frame Clauses

## Goal

This slice admits inline `ROWS` and `RANGE` frame clauses for the supported
ranking and distribution window-function subset:

- `ROW_NUMBER()`
- `RANK()`
- `DENSE_RANK()`
- `PERCENT_RANK()`
- `CUME_DIST()`
- `NTILE()`

It keeps the existing descriptor-backed projection envelope: no-source
`OVER ()`, `FROM DUAL`, or one descriptor-backed table source with optional one
descriptor-column `PARTITION BY`, optional one descriptor-column `ORDER BY`,
and existing `WHERE`, outer `ORDER BY`, and `LIMIT` support.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>

Observed MySQL 8.4.9 behavior for this slice:

- Ranking and distribution functions accept inline `ROWS` and `RANGE` frame
  clauses in the supported table-backed window envelope.
- The frame clause does not change the result for `ROW_NUMBER()`, `RANK()`,
  `DENSE_RANK()`, `PERCENT_RANK()`, `CUME_DIST()`, or `NTILE()`.
- Existing argument-count, argument-domain, unknown-column, and unsupported
  context diagnostics remain unchanged.

The exact expected rows are captured in:

- `packages/libmylite/tests/mysql_baseline_row_number_window_function_expectations.sh`
- `packages/libmylite/tests/mysql_baseline_window_rank_navigation_expectations.sh`

## Semantics

MyLite accepts a parsed inline frame clause only when the function kind is one
of the ranking or distribution functions above. The planner validates the
existing supported `PARTITION BY` and `ORDER BY` keys, then ignores the frame
clause before lowering to SQLite SQL because the frame is not semantically
observed by these functions.

Navigation value frames are handled by the later
[baseline window value frame clauses](../baseline-window-value-frame-clauses/specs.md)
slice. `LAG()` and `LEAD()` follow the same validate-then-ignore policy as the
ranking and distribution functions for the baseline frame subset.

Named windows, inherited window specs, expression keys, multiple keys, grouped
selects, joins, predicates, DML contexts, aggregate windows, and arbitrary
window-frame expression semantics remain outside this slice.

## Syntax

The supported window spec shape becomes:

```lemon
window_spec_opt ::= .
window_spec_opt ::= window_partition_clause.
window_spec_opt ::= window_order_clause.
window_spec_opt ::= window_partition_clause window_order_clause.
window_spec_opt ::= window_frame_clause.
window_spec_opt ::= window_partition_clause window_frame_clause.
window_spec_opt ::= window_order_clause window_frame_clause.
window_spec_opt ::= window_partition_clause window_order_clause window_frame_clause.
```

The new executable forms reuse the already parsed MyLite frame-clause grammar.
They are executable only for the rank/distribution function subset listed in
the goal.

## SQLite Integration

No SQLite fork hook is required. MyLite continues to use SQLite native window
functions through generated SQL over descriptor-owned physical table names and
quoted descriptor value expressions.

## Test Plan

- Extend the MySQL expectation scripts for row-number and rank/distribution
  functions with frame-clause cases verified against MySQL 8.4.9.
- Add matching C runtime coverage to
  `runtime_row_number_window_function_test.c` and
  `runtime_window_rank_navigation_functions_test.c`.
- Keep an explicit runtime rejection for value-function frame clauses.
- Run shell syntax checks, focused MySQL expectation scripts, focused CTest
  targets, diff checks, and the full `cmake --workflow --preset check` gate.

## Compatibility Impact

The rank/distribution-style rows can be marked green for their documented
baseline projection subset. Navigation and frame-value rows stay yellow because
their explicit-frame semantics remain unsupported.

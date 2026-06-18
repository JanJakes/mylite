# Baseline Window Value Frame Clauses

## Goal

This slice admits executable inline window frame clauses for the current
frame-sensitive value-function subset:

- `FIRST_VALUE()`
- `LAST_VALUE()`
- `NTH_VALUE()`

It also validates frame clauses for ranking, distribution, `LAG()`, and
`LEAD()` functions before ignoring them, matching the observed MySQL behavior
that those functions do not observe frame boundaries.

The statement envelope remains the existing row-scalar projection path:
no-source or `FROM DUAL` for empty windows, or one descriptor-backed table
source with optional one descriptor-column `PARTITION BY`, optional one
descriptor-column `ORDER BY`, and the existing `WHERE`, outer `ORDER BY`, and
`LIMIT` support.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>

Observed MySQL 8.4.9 behavior for this slice:

- `FIRST_VALUE()`, `LAST_VALUE()`, and `NTH_VALUE()` observe inline frame
  boundaries.
- `LAG()` and `LEAD()` accept frame clauses but return the same values as their
  unframed forms.
- Invalid frame boundaries are rejected even for functions that ignore the
  frame.
- `ROWS` integer-offset frames work in the supported descriptor-backed
  projection envelope.
- `RANGE` unbounded and current-row frames work in the supported envelope.

The exact expected rows are captured in:

- `packages/libmylite/tests/mysql_baseline_window_rank_navigation_expectations.sh`

## Semantics

MyLite validates every admitted frame clause. The frame start cannot be
`UNBOUNDED FOLLOWING`, the frame end cannot be `UNBOUNDED PRECEDING`, and the
start bound class must not sort after the end bound class. Same-direction
offset ranges such as `ROWS BETWEEN 1 PRECEDING AND 2 PRECEDING` are allowed,
matching MySQL's empty-frame behavior.

For `FIRST_VALUE()`, `LAST_VALUE()`, and `NTH_VALUE()`, MyLite stores the
validated frame and emits it into the generated SQLite window SQL. Supported
frame forms are:

- `ROWS` frames with `UNBOUNDED PRECEDING`, `CURRENT ROW`,
  `UNBOUNDED FOLLOWING`, and nonnegative signed-64-bit integer literal
  `PRECEDING` / `FOLLOWING` offsets.
- `RANGE` frames using only unbounded or current-row bounds.

`RANGE` offset frames and interval-bound frames remain unsupported because
their MySQL type and temporal semantics need a broader typed frame model before
they can be emitted safely.

For ranking, distribution, `LAG()`, and `LEAD()`, MyLite validates the same
bound ordering and integer-offset domains, admits integer `RANGE` offsets, and
then discards the frame because those functions do not observe frame
boundaries.

Named windows, inherited window specs, expression keys, multiple keys, grouped
selects, joins, predicates, DML contexts, aggregate windows, `GROUPS`,
`EXCLUDE`, `FROM LAST`, and true `IGNORE NULLS` execution remain outside this
slice.

## Syntax

The executable window spec remains:

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

The frame clause parser keeps the existing MyLite `ROWS` and `RANGE` grammar.
This slice adds typed AST metadata for the frame unit and bound kind, then
restricts runtime execution to the supported frame subset above.

## SQLite Integration

No SQLite fork hook is required. MyLite lowers supported frame-sensitive value
functions to SQLite native window functions with a validated frame clause in
the generated SQL.

## Test Plan

- Extend MySQL expectation coverage for value-function `ROWS` frames,
  unbounded/current `RANGE` frames, `LAG()` / `LEAD()` ignored frames, and
  invalid frame boundaries.
- Add matching C runtime coverage to
  `runtime_window_rank_navigation_functions_test.c`.
- Run shell syntax checks, focused MySQL expectation scripts, focused CTest
  targets, diff checks, and the full `cmake --workflow --preset check` gate.

## Compatibility Impact

`FIRST_VALUE()`, `LAST_VALUE()`, and `NTH_VALUE()` can move to green for their
documented baseline projection subset. `LAG()` and `LEAD()` remain yellow only
for broader argument and context gaps, but explicit frame syntax is no longer a
separate gap.

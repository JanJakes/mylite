# Baseline Bitwise Aggregate Window Functions

## Summary

This slice makes the numeric `BIT_AND()`, `BIT_OR()`, and `BIT_XOR()`
aggregate-window subset executable in MyLite's existing row-scalar projection
window path:

```sql
SELECT id, BIT_OR(flags) OVER (PARTITION BY group_id) AS group_flags
FROM items;

SELECT id, BIT_XOR(flags) OVER (
  ORDER BY id
  ROWS BETWEEN 1 PRECEDING AND CURRENT ROW
) AS rolling_xor
FROM items;
```

The executable subset covers integer-domain bitwise aggregate windows over the
current single-table/tableless projection window envelope. It does not add
binary-string bitwise evaluation, broader source forms, or full expression
coverage beyond the current row-scalar window argument subset.

## Sources And Evidence

- MyLite project policy and architecture:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/functions-aggregate.md`
  - `docs/compatibility/functions-window.md`
- Existing aggregate and window specs:
  - `docs/specs/baseline-bitwise-aggregates/specs.md`
  - `docs/specs/baseline-core-aggregate-window-functions/specs.md`
  - `docs/specs/baseline-named-window-definitions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - aggregate functions:
    <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>
  - window function usage:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_bitwise_aggregate_window_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite behavior, and existing MyLite code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
sources.

## MySQL 8.4.9 Runtime Observations

- `BIT_AND()`, `BIT_OR()`, and `BIT_XOR()` can appear with an `OVER` clause
  and operate over the resolved partition/frame.
- Numeric bitwise aggregate-window results are unsigned 64-bit values rendered
  as decimal text.
- `BIT_AND()` returns all bits set, `18446744073709551615`, for empty or
  all-`NULL` frames.
- `BIT_OR()` and `BIT_XOR()` return `0` for empty or all-`NULL` frames.
- `NULL` input rows are ignored, including inside moving frames.
- `SHOW COLUMNS` for a view containing these expressions reports
  `bigint unsigned`, non-null, and default `0`.
- `BIT_AND(DISTINCT expr) OVER ()` is a syntax error in MySQL 8.4.9; MyLite
  keeps bitwise `DISTINCT` outside this grammar and runtime slice and rejects
  it through the current parse/unsupported-statement diagnostic.

## Scope

Supported grammar shape, matching MyLite's existing parser surface:

```text
bitwise_aggregate_window_function ::=
    BIT_AND LPAREN expression RPAREN over_clause
  | BIT_OR LPAREN expression RPAREN over_clause
  | BIT_XOR LPAREN expression RPAREN over_clause

over_clause ::= OVER identifier
over_clause ::= OVER LPAREN window_spec_opt RPAREN
window_clause ::= WINDOW named_window_definition_list
```

Executable statement envelope:

- source-free and `FROM DUAL` projection windows when the resolved window is
  empty;
- one descriptor-backed table source in the existing row-scalar select path;
- projection-only bitwise aggregate windows;
- integer/boolean/`NULL` literals, integer descriptor columns, and current
  supported signed-integer row-scalar expressions;
- direct `OVER (...)`, direct `OVER window_name`, and inherited named windows;
- the existing single-key `PARTITION BY`, single-key `ORDER BY`, and baseline
  `ROWS`/`RANGE` frame clauses accepted for projection windows.

Deferred behavior:

- binary-string bitwise aggregate evaluation;
- `DISTINCT` bitwise aggregate syntax beyond the current parser behavior;
- decimal, approximate, string, binary, temporal, JSON, and spatial argument
  coercion beyond the current signed-integer baseline;
- grouped, joined, derived-table, CTE, compound, and DML window contexts beyond
  the current row-scalar projection envelope;
- expression or multi-key partition/order lists beyond the current inline
  window baseline;
- exact optimizer, cost, or protocol metadata parity.

## Runtime Design

The parser already attaches `WINDOW_SPEC` or `WINDOW_REFERENCE` nodes to
bitwise aggregate-function AST nodes when `OVER` is present. Runtime planning
treats those nodes as row-scalar window expressions only when an `OVER` child
exists; non-window bitwise aggregate planning remains unchanged.

The existing named-window resolver and row-scalar window SQL builder are reused.
Supported expressions lower to private SQLite window functions:

- `_mylite_bit_and(argument) OVER (...)`
- `_mylite_bit_or(argument) OVER (...)`
- `_mylite_bit_xor(argument) OVER (...)`

The existing MyLite bitwise aggregate callbacks are extended from ordinary
aggregate callbacks to public SQLite window callbacks. The state tracks a
non-`NULL` row count plus per-bit occurrence counts. That makes `xInverse`
constant-time over 64 bits and keeps moving frames correct without replaying
the frame.

No SQLite fork hook is required. The feature uses the public
`sqlite3_create_window_function()` extension API.

## Diagnostics

- Unsupported argument domains return the existing row-scalar window
  unsupported diagnostic, with text saying aggregate-window arguments support
  only signed 64-bit integer expressions.
- Source-free non-empty window specifications continue to return the existing
  `<function>() without a table source supports only OVER ()` diagnostic.
- Existing named-window diagnostics from the named-window baseline continue to
  apply.
- Existing inline-window diagnostics for unsupported partition/order/frame
  shapes continue to apply.
- `DISTINCT` bitwise aggregate-window syntax remains outside this slice and is
  rejected before row-scalar window planning.

## Tests

- `packages/libmylite/tests/runtime_bitwise_aggregate_window_functions_test.c`
  verifies source-free, table-backed, partitioned, ordered, moving-frame,
  empty-frame, named-window, literal, column, `NULL`, metadata, and unsupported
  diagnostic behavior.
- `packages/libmylite/tests/mysql_baseline_bitwise_aggregate_window_functions_expectations.sh`
  records equivalent MySQL 8.4.9 result, metadata, and syntax expectations.

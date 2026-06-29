# Baseline JSON Aggregate Window Functions

## Summary

This slice makes `JSON_ARRAYAGG()` and `JSON_OBJECTAGG()` executable in
MyLite's existing projection-only row-scalar window path:

```sql
SELECT id, JSON_ARRAYAGG(payload) OVER (PARTITION BY group_id ORDER BY id)
FROM events;

SELECT id, JSON_OBJECTAGG(name, value) OVER (
  ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW
) AS object_window
FROM attributes;
```

The executable subset covers the current no-source, `DUAL`, one-table, named
window, single-key partition/order, and baseline frame row-scalar window
envelope. It reuses MyLite's existing JSON constructor argument subset for
aggregate values and keys.

## Sources And Evidence

- MyLite project policy and architecture:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/functions-aggregate.md`
  - `docs/compatibility/functions-window.md`
  - `docs/compatibility/functions-json.md`
- Existing JSON and window specs:
  - `docs/specs/baseline-json-aggregate-functions/specs.md`
  - `docs/specs/parser-corpus-json-stat-aggregate-window-surfaces/specs.md`
  - `docs/specs/baseline-core-aggregate-window-functions/specs.md`
  - `docs/specs/baseline-named-window-definitions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - aggregate functions:
    <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>
  - window function usage:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_json_aggregate_window_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite behavior, and existing MyLite code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
sources.

## MySQL 8.4.9 Runtime Observations

- `JSON_ARRAYAGG(expr)` and `JSON_OBJECTAGG(key, value)` accept an `OVER`
  clause and execute as aggregate window functions.
- `JSON_ARRAYAGG()` includes SQL `NULL` inputs as JSON `null` elements.
- Empty frames return SQL `NULL`.
- `JSON_OBJECTAGG()` raises `ERROR 3158 (22032)` when any key in the frame is
  SQL `NULL`.
- `JSON_OBJECTAGG()` uses normalized JSON object semantics: duplicate keys in
  a frame keep the value from the last encountered row in that frame.
- Result metadata for both functions is `json` and nullable.
- `GROUP_CONCAT(...) OVER (...)` is not supported by MySQL 8.4.9 and returns
  `ERROR 1235 (42000)` with a `group_concat as window function` message.

## Scope

Supported grammar shape, matching MyLite's current generic aggregate-window
parser surface:

```text
json_aggregate_window_function ::=
    JSON_ARRAYAGG LPAREN expression RPAREN over_clause
  | JSON_OBJECTAGG LPAREN expression COMMA expression RPAREN over_clause

over_clause ::= OVER identifier
over_clause ::= OVER LPAREN window_spec_opt RPAREN
window_clause ::= WINDOW named_window_definition_list
```

Executable statement envelope:

- source-free and `FROM DUAL` projection windows when the resolved window is
  empty;
- one descriptor-backed table source in the existing row-scalar select path;
- direct `OVER (...)`, direct `OVER window_name`, and inherited named windows;
- the existing single-key `PARTITION BY`, single-key `ORDER BY`, and baseline
  `ROWS`/`RANGE` frame clauses accepted for projection windows;
- `JSON_ARRAYAGG()` values and `JSON_OBJECTAGG()` keys/values that fit the
  current `JSON_ARRAY()` / `JSON_OBJECT()` constructor argument subset.

Deferred behavior:

- grouped, joined, derived-table, CTE, compound, predicate, DML, and top-level
  window-order contexts outside the current projection window envelope;
- expression or multi-key partition/order lists beyond the current inline
  window baseline;
- arbitrary JSON aggregate arguments beyond the current constructor argument
  subset;
- `DISTINCT`, aggregate-local ordering, and custom ordering inside the JSON
  aggregate call;
- exact binary JSON storage, binary JSON protocol representation, and full
  collation metadata parity;
- deterministic unordered-source ordering beyond MyLite's documented source
  execution order.

## Runtime Design

The parser admits these calls as generic functions with an attached
`WINDOW_SPEC` or `WINDOW_REFERENCE`. Runtime planning recognizes
case-insensitive `JSON_ARRAYAGG` and `JSON_OBJECTAGG` generic functions as
row-scalar window expressions only when an `OVER` child exists. Non-window JSON
aggregate planning remains owned by the existing aggregate planners.

Argument planning reuses `plan_row_scalar_json_constructor_argument()` so JSON
aggregate windows share the non-window aggregate baseline's value coercion,
JSON descriptor embedding, and key validation rules. Lowering emits private
SQLite window calls with MyLite's existing tag/value convention:

- `_mylite_json_arrayagg(value_kind, value) OVER (...)`
- `_mylite_json_objectagg(key_kind, key, value_kind, value) OVER (...)`

The existing JSON aggregate callbacks are extended from ordinary aggregate
callbacks to public SQLite window callbacks. The state stores ordered key/value
entries. The inverse callback removes the first matching outgoing row from the
ordered state, preserving array order and object duplicate-key last-wins
behavior for moving frames. `xValue` renders the current frame without
destroying the state; `xFinal` renders and deinitializes the state.

No SQLite fork hook is required. The feature uses the public
`sqlite3_create_window_function()` extension API.

## Diagnostics

- `JSON_OBJECTAGG()` with a SQL `NULL` key returns the existing MySQL-shaped
  JSON member-name diagnostic, `3158 / 22032`.
- Unsupported argument domains return the existing row-scalar window
  unsupported diagnostics from the JSON constructor argument planner.
- Source-free non-empty window specifications continue to return the existing
  `<function>() without a table source supports only OVER ()` diagnostic.
- Existing named-window diagnostics from the named-window baseline continue to
  apply.
- `GROUP_CONCAT(...) OVER (...)` returns MySQL's `1235 / 42000` unsupported
  diagnostic rather than executing a function that MySQL itself rejects.

## Tests

- `packages/libmylite/tests/runtime_json_aggregate_window_functions_test.c`
  verifies source-free, table-backed, partitioned, running, moving-frame,
  empty-frame, named-window, JSON descriptor value, duplicate-key, metadata,
  `GROUP_CONCAT()` unsupported, null-key, and unsupported diagnostic behavior.
- `packages/libmylite/tests/mysql_baseline_json_aggregate_window_functions_expectations.sh`
  records equivalent MySQL 8.4.9 result, metadata, and error expectations.

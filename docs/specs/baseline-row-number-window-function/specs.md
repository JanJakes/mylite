# Baseline ROW_NUMBER Window Function

## Summary

This phase adds a narrow descriptor-driven `ROW_NUMBER()` window-function
surface for single-table `SELECT` projections:

```sql
SELECT id, ROW_NUMBER() OVER () AS rn FROM t;
SELECT id, ROW_NUMBER() OVER (PARTITION BY group_id ORDER BY sort_key DESC) AS rn FROM t;
```

The implementation is deliberately limited. It is not a general window
framework and it does not add arbitrary expressions, subqueries, joins, CTEs,
named windows, frame clauses, or window functions outside projection lists.
MyLite resolves all table, column, alias, predicate, order, and limit metadata
through MyLite descriptors, then lowers the verified window expression to
SQLite's native `row_number()` execution path over the generated physical table.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite select/runtime slices:
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
  - `docs/specs/baseline-row-scalar-functions/specs.md`
  - `docs/specs/baseline-result-column-metadata/specs.md`
  - `docs/specs/baseline-column-charset-collation-attributes/specs.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/functions-window.md`
  - `docs/compatibility/sql-query-expressions.md`
- Official MySQL 8.4 Reference Manual:
  - window function descriptions:
    <https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html>
  - window function concepts and syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html>
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_row_number_window_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this slice:

- `ROW_NUMBER() OVER ()` is accepted in a projection and returns `1` for a
  no-source `SELECT`.
- For a table source, row numbers start at `1` for each partition.
- `PARTITION BY column` groups `NULL` values into the same partition.
- `ORDER BY column` inside the window defaults to ascending order.
- `ASC` and `DESC` are accepted inside the window. For the admitted simple
  descriptor columns, `NULL` sorts first for ascending order and last for
  descending order.
- An outer `ORDER BY` controls result display order independently of the window
  ordering used to assign row numbers.
- Existing `WHERE` filtering applies before window numbering. Existing outer
  `LIMIT` applies after window numbering and result ordering.
- `ROW_NUMBER()` without `OVER` and `ROW_NUMBER(1) OVER ()` are syntax errors.
- MySQL supports multi-key partition/order lists, named windows, and frame
  clauses for `ROW_NUMBER()`. MyLite defers those broader forms in this phase.
- MySQL rejects window functions in unsupported contexts such as `WHERE` with
  error 3593. MyLite may use a narrower deterministic unsupported diagnostic
  until broader expression planning admits such syntax.

## Scope

Supported syntax:

```sql
row_number_expr ::= ROW_NUMBER LPAREN RPAREN OVER LPAREN window_spec_opt RPAREN

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

Supported statement envelope:

- no-source and `FROM DUAL` scalar projection for `ROW_NUMBER() OVER ()`;
- one descriptor-backed base-table source using the existing selected/default
  schema policy and current temporary-table shadowing behavior;
- explicit projection items mixing descriptor columns and top-level
  `ROW_NUMBER() OVER (...)` expressions;
- optional aliases using the existing select-item alias policy;
- existing single-table `WHERE`, outer `ORDER BY`, and outer `LIMIT` subsets
  supported by the row-scalar select path;
- one unqualified or source-qualified descriptor column in `PARTITION BY`;
- one unqualified or source-qualified descriptor column in window `ORDER BY`;
- optional `ASC` / `DESC` in window `ORDER BY`.

Deferred syntax and behavior:

- joins, comma sources, derived tables, CTEs, views, grouped selects, aggregate
  windows, `DISTINCT`, `SQL_CALC_FOUND_ROWS`, compound selects, `TABLE`, and
  subqueries as row sources;
- window functions in `WHERE`, `HAVING`, `GROUP BY`, DML assignments,
  generated columns, defaults, predicates, and nested scalar expressions;
- named windows, window inheritance, frame clauses, `RANGE` / `ROWS` / `GROUPS`,
  `EXCLUDE`, multi-key partitioning, multi-key ordering, expression partition
  keys, expression order keys, ordinal order keys, table-qualified keys for
  unrelated sources, collations in the window spec, parameters, and arbitrary
  scalar expression arguments;
- rank, distribution, navigation, and frame-value window functions are covered
  by `docs/specs/baseline-window-rank-navigation-functions/specs.md`;
  aggregate window functions remain deferred.

## Ownership Boundaries

- Public API: unchanged. Successful statements use the existing `mylite_execute`
  and result APIs. No ABI additions are required.
- Statement context: unchanged except for ordinary diagnostics and warnings.
  The statement timestamp, row-count, and warning state continue to be owned by
  the existing execution path.
- Parser/AST: adds MyLite-owned AST nodes for `ROW_NUMBER() OVER (...)` and a
  compact window spec. The grammar is independently authored and intentionally
  smaller than MySQL's full grammar.
- Analyzer/planner: resolves source tables, descriptor columns, select aliases,
  predicate columns, outer order columns, limit values, and window keys through
  MyLite descriptors. SQLite metadata is not used as authority.
- Catalog: read-only for this feature. No catalog rows, descriptor versions,
  generation counters, or cache keys are mutated by `SELECT`.
- Result builder: adds a deterministic result column descriptor for
  `ROW_NUMBER()` expressions. Descriptor column metadata remains sourced from
  MyLite catalog descriptors.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are not touched.
- SQLite physical row storage: SQLite performs row access, filtering, ordering,
  limiting, and native `row_number()` window execution over MyLite-generated
  physical table names and quoted physical column identifiers.

## Name Resolution

Unqualified table names use the existing selected/default schema policy.
Schema-qualified table names bypass the default schema only for the named
schema. Missing default schema, unknown schema, unknown table, and reserved
`_mylite_*` schema/table names use the existing table-resolution diagnostics.

Window `PARTITION BY` and window `ORDER BY` keys must resolve to descriptor
columns from the single source. Unqualified names are resolved with the current
single-source descriptor policy. Source-qualified names may use the table name
or the selected source alias. Unknown or ambiguous names fail before SQLite SQL
is generated. MyLite's current descriptor identifier matching remains
ASCII-case-insensitive for baseline catalog names.

## Semantics

`ROW_NUMBER()` returns a non-null unsigned 64-bit integer-like result. Numbering
starts at `1` in each partition. With no `PARTITION BY`, all rows are in one
partition. With no window `ORDER BY`, this slice does not guarantee a stable
assignment order beyond the observed single-table scan behavior used in tests.
Callers that need deterministic numbering should include a window `ORDER BY`
with a unique or otherwise deterministic key.

For admitted string-family partition/order columns, MyLite emits the existing
MyLite string-key collation around descriptor value references so SQLite uses
the same baseline ASCII case-insensitive order/equality policy as the current
descriptor-backed `ORDER BY` and predicate paths. Numeric, temporal, binary
string, bit, year, and decimal descriptors use their stored physical values and
the current row-scalar column support envelope.

For duplicate window order values without another key, MyLite claims only the
set of row numbers assigned to the tied peer group, not a specific tie order.
Tests must avoid overclaiming tied row choice unless a unique order key is
included.

`WHERE` is evaluated before window numbering. Outer `ORDER BY` and `LIMIT` are
evaluated after projection and window numbering. A window `ORDER BY` without an
outer `ORDER BY` controls row-number assignment but does not make a broader
guarantee about final presentation order beyond MySQL's observed rows for the
tested statements.

## Generated SQLite

The generated SQL shape is:

```sql
SELECT "id", row_number() OVER (PARTITION BY "group_id" ORDER BY "sort_key" DESC)
FROM "_mylite_user_table_<table_id>"
WHERE ...
ORDER BY ...
LIMIT ?
```

Every generated SQLite identifier is quoted. Physical table names come from the
MyLite catalog descriptor and remain stable `_mylite_user_table_<table_id>`
style names. The window spec uses descriptor value SQL builders so string
collations and time ordering follow existing MyLite select semantics. Existing
predicate, outer order, and limit values remain bound parameters.

This phase uses SQLite's public native window-function support through SQL
translation. It does not add a new SQLite UDF, virtual table, VFS hook, fork
patch, temporary materialization table, or MyLite-side row-number loop.

## Result Metadata

Successful `SELECT` returns a row result set through the existing public result
object conventions. `ROW_NUMBER()` result columns are reported as non-null
unsigned `BIGINT`/`LONGLONG`-shaped numeric columns with display length matching
MySQL's scalar `BIGINT` result convention. Aliases label the result column;
without an alias, the result label is the original expression text produced by
the parser's existing scalar-projection label path.

Supported statements leave `warning_count == 0`. `ROW_COUNT()` after a result
set remains the existing SELECT convention.

## Diagnostics

The implementation must diagnose:

- syntax errors for missing `OVER`, arguments to `ROW_NUMBER`, malformed window
  clauses, named windows, frame clauses, and unsupported grammar;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` target names through existing table-resolution diagnostics;
- unsupported source shapes such as joins, comma sources, derived tables, CTEs,
  grouped selects, `DISTINCT`, and compound selects;
- unknown partition columns and unknown window ordering columns before SQLite
  SQL generation;
- unsupported partition/order expressions, ordinals, parameters, functions, and
  multiple keys with deterministic MyLite unsupported diagnostics;
- window functions in unsupported contexts with a deterministic MyLite
  unsupported diagnostic until broader expression planning maps the exact
  MySQL 3593 diagnostic;
- physical SQLite failures and allocation failures through existing runtime
  error handling.

## Tests

Add fast C tests under `packages/libmylite/tests/`, preferably in a new
`runtime_row_number_window_function_test.c`, plus parser coverage if the
existing parser corpus is not enough. Register the test with a dotted CTest
name if a new binary is added.

The test suite must cover:

- no-source `SELECT ROW_NUMBER() OVER ()`;
- table-backed `ROW_NUMBER() OVER ()` mixed with descriptor columns;
- `PARTITION BY` over nullable integer and string columns;
- window `ORDER BY` default direction, `ASC`, and `DESC`;
- `NULL` ordering in ascending and descending window order;
- duplicate window order keys without overclaiming tie order;
- `WHERE`, outer `ORDER BY`, and outer `LIMIT` interaction;
- source alias and qualified window keys;
- unknown partition and order columns;
- unsupported multi-key, ordinal, expression, named-window, frame, join,
  grouped, `DISTINCT`, and context forms;
- result metadata, warning count, result rows, and `ROW_COUNT()` behavior;
- reopen persistence remains unaffected by selecting with a window expression.

Verification before commit:

1. `cmake --build --preset dev`
2. targeted parser/runtime row-number CTests
3. `packages/libmylite/tests/mysql_baseline_row_number_window_function_expectations.sh`
4. `cmake --workflow --preset check`

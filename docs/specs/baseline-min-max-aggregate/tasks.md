# Baseline MIN/MAX Aggregate Tasks

## Goal

Add the next narrow aggregate slice: descriptor-driven
`SELECT MIN(column)` and `SELECT MAX(column)` through `mylite_execute()`, backed
by authoritative MyLite catalog descriptors, stable SQLite physical table
names, and the existing baseline predicate conversion path.

## Tasks

1. Design and documentation
   - [x] Create `docs/specs/baseline-min-max-aggregate/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, no-space function-call behavior, schema/table
     resolution, aggregate argument resolution, predicate reuse, generated
     SQLite SQL, diagnostics, result behavior, row-count behavior, and
     unsupported aggregate forms.
   - [x] Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - [x] Add a reproducible MySQL 8.4.9 expectation script for supported
     `MIN(column)` / `MAX(column)` behavior and intentionally rejected/deferred
     wider forms.
   - [x] Verify empty/nonempty aggregates, nullable/all-`NULL` handling,
     no-match predicates, predicate aggregates, labels, warnings, following
     `ROW_COUNT()`, no-source/`DUAL` behavior, and diagnostics for
     syntax-rejected min/max forms.
   - [x] Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - [x] Add `MIN` and `MAX` keyword recognition without making them reserved.
   - [x] Add an AST node for one-column `MIN()` / `MAX()` aggregate functions.
   - [x] Extend Lemon grammar for no-space `MIN(identifier)` and
     `MAX(identifier)`, including whitespace or comments inside the argument
     list.
   - [x] Preserve source spans for default result labels.
   - [x] Preserve `MIN` and `MAX` as identifiers where the grammar admits
     nonreserved function-name identifiers.
   - [x] Add parser tests for supported min/max syntax and unsupported
     argument/spacing forms.

4. Analyzer/planner/runtime execution
   - [x] Add a min/max aggregate select path distinct from scalar session
     selects, count selects, and regular table projection selects.
   - [x] Accept only one aggregate select item, one descriptor-backed
     persistent base table, and optional baseline `WHERE`.
   - [x] Resolve unqualified and schema-qualified table names against selected
     schema and MyLite catalog descriptors.
   - [x] Reject reserved `_mylite_*` schema/table names before generated SQLite
     SQL.
   - [x] Resolve the aggregate argument column from descriptors, including
     explicitly named invisible columns.
   - [x] Reuse the existing descriptor-driven predicate planner for optional
     `WHERE`.
   - [x] Reject unsupported aggregate arguments, aliases, mixed projections,
     multiple aggregate items, no-source/`DUAL`, `ORDER BY`, `LIMIT`,
     `GROUP BY`, `HAVING`, joins, subqueries, CTEs, and window clauses with
     deterministic diagnostics.
   - [x] Return one row with one integer or `NULL` value, warning count `0`,
     and affected rows `0` for supported forms.

5. Physical SQLite aggregate
   - [x] Generate SQLite `SELECT MIN("physical_column") FROM "physical_table"`
     or `SELECT MAX("physical_column") FROM "physical_table"` with optional
     descriptor-built `WHERE`.
   - [x] Quote every generated SQLite identifier.
   - [x] Bind predicate values through prepared statements.
   - [x] Avoid SQLite fork patches and custom SQLite functions.
   - [x] Keep descriptor rows, catalog generation, descriptor versions,
     descriptor caches, and SQLite schema generation unchanged.

6. Tests
   - [x] Add a fast C test under `packages/libmylite/tests/` and register it
     with a dotted CTest name.
   - [x] Cover supported min/max over existing descriptor integer families,
     nullable/all-`NULL` values, empty tables, no-match predicates, schema
     resolution, predicate reuse, labels, warning count, affected rows,
     following `ROW_COUNT()`, reopen persistence, rename/truncate/drop
     behavior, independent handles, preamble safety, and unsupported syntax.
   - [x] Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - [x] Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - [x] Keep first-party warning and clang-tidy policy enabled.
   - [x] Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run the new CTest entry and relevant parser/count/select lifecycle
     entries.
   - [x] Run
     `./packages/libmylite/tests/mysql_baseline_min_max_aggregate_expectations.sh`.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Review the final diff for parser independence, descriptor authority,
     generated SQL safety, result semantics, row-count semantics,
     file-format safety, cleanup on failure, compatibility docs, and scope
     control.

## Out Of Scope

- Full aggregate support, expression aggregate arguments, `DISTINCT`, aliases,
  multiple aggregate items, mixed projections, grouping, having, order/limit
  aggregate semantics, window functions, joins, CTEs, subqueries, temporary
  tables, views, privileges, protocol metadata, optimizer behavior, SQL modes
  such as `IGNORE_SPACE`, arbitrary SQLite pass-through, and SQLite fork
  patches.

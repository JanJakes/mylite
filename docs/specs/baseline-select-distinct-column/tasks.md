# Baseline SELECT DISTINCT Column Tasks

## Goal

Add the next narrow query duplicate-elimination slice:
descriptor-driven `SELECT DISTINCT column` through `mylite_execute()`, backed by
authoritative MyLite catalog descriptors, stable SQLite physical table and
column names, and the existing baseline predicate, ordering, and limit paths.

## Tasks

1. Design and documentation
   - [x] Create `docs/specs/baseline-select-distinct-column/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, distinct modifier scope, schema/table
     resolution, selected/predicate/order column resolution, predicate reuse,
     ordering and limit semantics, generated SQLite SQL, diagnostics, result
     behavior, row-count behavior, and unsupported query forms.
   - [x] Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - [x] Add a reproducible MySQL 8.4.9 expectation script for supported
     `SELECT DISTINCT column` behavior and intentionally rejected/deferred
     wider forms.
   - [x] Verify duplicate-value elimination, `NULL` handling, ordering,
     limiting, predicate reuse, labels, warnings, following `ROW_COUNT()`,
     schema diagnostics, unknown-name diagnostics, and diagnostics for
     non-selected order columns.
   - [x] Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - [ ] Add AST support for the `DISTINCT` select modifier without changing
     public ABI.
   - [ ] Extend Lemon grammar for `SELECT DISTINCT ... FROM table ...`.
   - [ ] Preserve existing non-distinct select parsing and aggregate parsing.
   - [ ] Add parser tests for supported distinct syntax and unsupported wider
     select shapes.

4. Analyzer/planner/runtime execution
   - [ ] Extend the descriptor-backed select path for one-column
     `SELECT DISTINCT column`.
   - [ ] Accept only one selected unqualified descriptor column, one
     descriptor-backed persistent base table, optional baseline `WHERE`, optional
     `ORDER BY` on the same selected column, and optional existing select
     `LIMIT` forms.
   - [ ] Resolve unqualified and schema-qualified table names against selected
     schema and MyLite catalog descriptors.
   - [ ] Reject reserved `_mylite_*` schema/table names before generated SQLite
     SQL.
   - [ ] Resolve selected, predicate, and ordering columns from descriptors,
     including explicitly named invisible columns.
   - [ ] Reuse the existing descriptor-driven predicate and limit planners.
   - [ ] Reject unsupported distinct wildcard, selected expressions, literal
     selected items, table-qualified selected columns, aliases, multiple select
     items, explicit `ALL`, `DISTINCTROW`, non-selected order columns,
     table-qualified order columns, ordinals, expression order keys, multiple
     sort keys, joins, grouping, subqueries, CTEs, query modifiers, and other
     wider MySQL forms with deterministic diagnostics.
   - [ ] Return descriptor-backed result rows, warning count `0`, affected rows
     `0`, and following `ROW_COUNT() == -1` for supported forms.

5. Physical SQLite select
   - [ ] Generate SQLite `SELECT DISTINCT "column" FROM "physical_table"` with
     optional descriptor-built `WHERE`, `ORDER BY`, and `LIMIT`/`OFFSET`.
   - [ ] Quote every generated SQLite identifier.
   - [ ] Bind predicate, limit, and offset values through prepared statements.
   - [ ] Avoid SQLite fork patches, custom SQLite functions, and MyLite-side
     duplicate elimination.
   - [ ] Keep descriptor rows, catalog generation, descriptor versions,
     descriptor caches, and SQLite schema generation unchanged.

6. Tests
   - [ ] Add or extend fast C tests under `packages/libmylite/tests/`.
   - [ ] Cover supported distinct behavior over existing descriptor integer
     families, duplicate values, nullable values, invisible columns, predicates,
     ordering, limits, schema resolution, labels/result conventions, warning
     count, affected rows, following `ROW_COUNT()`, reopen persistence,
     rename/drop behavior, independent handles, preamble safety, and unsupported
     syntax.
   - [ ] Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - [ ] Keep first-party warning and clang-tidy policy enabled.
   - [ ] Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - [ ] Run `cmake --build --preset dev`.
   - [ ] Run the new CTest entry and relevant parser/select lifecycle entries.
   - [ ] Run
     `./packages/libmylite/tests/mysql_baseline_select_distinct_column_expectations.sh`.
   - [ ] Run `cmake --workflow --preset check`.
   - [ ] Review the final diff for parser independence, descriptor authority,
     generated SQL safety, result semantics, row-count semantics,
     file-format safety, cleanup on failure, compatibility docs, and scope
     control.

## Out Of Scope

Full `SELECT DISTINCT`, `DISTINCTROW`, explicit `ALL`, `DISTINCT *`, multi-item
distinct rows, aliases, table-qualified selected columns, selected expressions,
literal selected items, expression ordering, ordinal ordering, table-qualified
ordering, multiple order keys, collations, joins, grouping, subqueries, CTEs,
set operations, locking clauses, query modifiers, temporary tables, views,
privileges, protocol metadata, arbitrary SQLite pass-through, and SQLite fork
patches.

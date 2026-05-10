# Baseline COUNT DISTINCT Column Aggregate Tasks

## Goal

Add the next narrow aggregate slice: descriptor-driven
`SELECT COUNT(DISTINCT column)` through `mylite_execute()`, backed by
authoritative MyLite catalog descriptors, stable SQLite physical table and
column names, and the existing baseline predicate conversion path.

## Tasks

1. Design and documentation
   - [x] Create `docs/specs/baseline-count-distinct-column-aggregate/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, no-space function-call behavior, schema/table
     resolution, aggregate argument resolution, predicate reuse, generated
     SQLite SQL, diagnostics, result behavior, row-count behavior, and
     unsupported aggregate forms.
   - [x] Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - [x] Add a reproducible MySQL 8.4.9 expectation script for supported
     `COUNT(DISTINCT column)` behavior and intentionally rejected/deferred
     wider forms.
   - [x] Verify duplicate-value counts, `NULL` exclusion, all-`NULL` handling,
     empty tables, no-match predicates, predicate counts, labels, warnings,
     following `ROW_COUNT()`, no-source/`DUAL` diagnostics, and diagnostics for
     syntax-rejected forms.
   - [x] Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - [x] Add an AST node for one-column `COUNT(DISTINCT)` aggregate functions.
   - [x] Extend Lemon grammar for no-space `COUNT(DISTINCT identifier)`,
     including whitespace or comments inside the argument list.
   - [x] Preserve source spans for default result labels.
   - [x] Preserve `COUNT` as an identifier where the grammar admits
     nonreserved function-name identifiers.
   - [x] Add parser tests for supported count-distinct-column syntax and
     unsupported argument/spacing forms.

4. Analyzer/planner/runtime execution
   - [x] Extend the count aggregate select path for descriptor-backed
     `COUNT(DISTINCT column)` while preserving existing `COUNT(*)`,
     `COUNT(column)`, and `COUNT(literal)` behavior.
   - [x] Accept only one aggregate select item, one descriptor-backed
     persistent base table, and optional baseline `WHERE`.
   - [x] Resolve unqualified and schema-qualified table names against selected
     schema and MyLite catalog descriptors.
   - [x] Reject reserved `_mylite_*` schema/table names before generated
     SQLite SQL.
   - [x] Resolve the distinct aggregate argument column from descriptors,
     including explicitly named invisible columns.
   - [x] Reuse the existing descriptor-driven predicate planner for optional
     `WHERE`.
   - [x] Reject unsupported aggregate arguments, aliases, mixed projections,
     multiple aggregate items, no-source/`DUAL`, `ORDER BY`, `LIMIT`,
     `GROUP BY`, `HAVING`, joins, subqueries, CTEs, and window clauses with
     deterministic diagnostics.
   - [x] Return one row with one decimal count value, warning count `0`, and
     affected rows `0` for supported forms.

5. Physical SQLite aggregate
   - [x] Generate SQLite `SELECT COUNT(DISTINCT "physical_column") FROM
     "physical_table"` with optional descriptor-built `WHERE`.
   - [x] Quote every generated SQLite identifier.
   - [x] Bind predicate values through prepared statements.
   - [x] Avoid SQLite fork patches and custom SQLite functions.
   - [x] Keep descriptor rows, catalog generation, descriptor versions,
     descriptor caches, and SQLite schema generation unchanged.

6. Tests
   - [x] Extend the fast count aggregate C test under
     `packages/libmylite/tests/`.
   - [x] Cover supported count-distinct-column behavior over existing
     descriptor integer families, duplicate values, nullable/all-`NULL` values,
     empty tables, no-match predicates, schema resolution, predicate reuse,
     labels, warning count, affected rows, following `ROW_COUNT()`, reopen
     persistence, rename/truncate/drop behavior, independent handles, preamble
     safety, and unsupported syntax.
   - [x] Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - [x] Reuse the existing count aggregate test binary where possible.
   - [x] Keep first-party warning and clang-tidy policy enabled.
   - [x] Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run the count aggregate CTest entry and relevant parser/count
     aggregate lifecycle entries.
   - [x] Run
     `./packages/libmylite/tests/mysql_baseline_count_distinct_column_aggregate_expectations.sh`.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Review the final diff for parser independence, descriptor authority,
     generated SQL safety, result semantics, row-count semantics,
     file-format safety, cleanup on failure, compatibility docs, and scope
     control.

## Out Of Scope

- Full aggregate support, general `COUNT(expr)`, full `COUNT(DISTINCT expr)`,
  aliases, table-qualified aggregate arguments, literal distinct arguments,
  expression arguments, multiple distinct expressions, multiple aggregate
  items, mixed projections, grouping, having, order/limit aggregate semantics,
  window functions, joins, CTEs, subqueries, temporary tables, views,
  privileges, protocol metadata, optimizer behavior, SQL modes such as
  `IGNORE_SPACE`, arbitrary SQLite pass-through, and SQLite fork patches.

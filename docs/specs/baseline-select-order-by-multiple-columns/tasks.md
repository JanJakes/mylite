# Baseline SELECT ORDER BY Multiple Columns Tasks

## Goal

Add descriptor-driven multiple `ORDER BY` keys for supported table-backed
`SELECT` paths without widening DML write ordering, aggregate-local ordering, or
general expression ordering.

## Tasks

1. Design and documentation
   - [x] Create
     `docs/specs/baseline-select-order-by-multiple-columns/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, descriptor resolution, alias behavior,
     ordering semantics, generated SQLite SQL, diagnostics, performance
     expectations, and intentionally deferred contexts.
   - [x] Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - [x] Add a reproducible MySQL 8.4.9 expectation script for multi-key
     `SELECT ORDER BY` behavior.
   - [x] Verify default direction, mixed `ASC`/`DESC`, `NULL` ordering, limit
     interaction, alias shadowing, duplicate alias ambiguity, and unknown order
     columns.
   - [x] Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - [x] Extend the SELECT grammar with a comma-separated order item list.
   - [x] Preserve existing one-key grammar for `UPDATE`, `DELETE`, and
     aggregate-local ordering.
   - [x] Preserve existing one-key select AST shape where practical and add a
     list shape only when multiple keys are present.
   - [x] Add parser tests for multi-key SELECT acceptance and unsupported order
     item forms.

4. Analyzer/planner/runtime execution
   - [x] Store a planned order item list for descriptor-backed selects.
   - [x] Resolve each key through selected aliases first, then descriptor
     source columns.
   - [x] Validate each key against the existing supported descriptor order
     families.
   - [x] Preserve current single-key restrictions for `SELECT DISTINCT`,
     grouped aggregate ordering, `UPDATE`, `DELETE`, and `GROUP_CONCAT`.
   - [x] Generate comma-separated SQLite `ORDER BY` expressions from
     descriptors only.
   - [x] Keep all predicate and limit values bound through prepared
     statements.

5. Tests
   - [x] Cover ordinary table selects with integer, nullable integer, temporal,
     `BIT`, and ASCII nonbinary string descriptor order keys where current
     one-key support already admits those families.
   - [x] Cover joined-source qualified order keys.
   - [x] Cover row-scalar table selects and table-select sources used by CTAS,
     `INSERT ... SELECT`, and `REPLACE ... SELECT`.
   - [x] Cover unknown first and later order columns, duplicate aliases,
     unsupported later-key descriptor families, distinct/grouped rejection, and
     preserved DML/aggregate comma-order rejections.
   - [x] Cover no catalog generation, SQLite schema generation, or file
     preamble mutation from successful ordered reads.
   - [x] Keep tests deterministic and avoid a new test framework.

6. Build integration
   - [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`,
     or extend existing CTest entries if clearer.
   - [x] Keep first-party warning and clang-tidy policy enabled.
   - [x] Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run focused parser and select-order CTest entries.
   - [x] Run
     `./packages/libmylite/tests/mysql_baseline_select_order_by_multiple_columns_expectations.sh`.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Review the diff for parser independence, descriptor authority,
     generated SQL safety, result semantics, performance, file-format safety,
     compatibility docs, and scope control.

## Out Of Scope

Expression order keys, ordinal order keys, collations, non-ASCII string
ordering parity, optimizer behavior, multiple DML order keys, aggregate-local
multiple order keys, grouped aggregate multiple order keys, broader `DISTINCT`
ordering, public API changes, arbitrary SQLite pass-through, and SQLite fork
patches.

# Baseline GROUP_CONCAT Aggregate Tasks

## Goal

Add the first descriptor-driven `GROUP_CONCAT()` aggregate slice: one descriptor
column argument, optional one aggregate-local descriptor `ORDER BY`, optional
string-literal `SEPARATOR`, table-backed and single-column grouped aggregate
forms, MySQL-shaped `NULL` and ordering behavior for the admitted subset, and
deterministic rejection of wider MySQL forms.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-group-concat-aggregate/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, descriptor ownership, separator conversion,
     generated SQLite SQL, diagnostics, result behavior, and unsupported forms.
   - Update compatibility docs only for the exact implemented subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `GROUP_CONCAT()` behavior and selected wider MySQL forms.
   - Verify default separator, explicit separators, aggregate-local ordering,
     `NULL` skipping, empty/all-`NULL` results, grouped output, warning count,
     nullable aggregate-local order-key deferral, and `ROW_COUNT()` after
     aggregate `SELECT`.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for compatibility
     expectation changes.

3. Parser and AST
   - Add `GROUP_CONCAT` keyword mapping and an AST node for the aggregate.
   - Parse one descriptor column argument, optional aggregate-local `ORDER BY`,
     and optional string-literal `SEPARATOR`.
   - Preserve no-space-sensitive function parsing with `IGNORE_SPACE` support.
   - Add parser tests for supported syntax and rejected wider grammar.

4. Analyzer and runtime
   - Recognize `GROUP_CONCAT()` in the existing column aggregate planner.
   - Recognize `GROUP_CONCAT()` in the existing grouped aggregate planner for
     one descriptor group column and one aggregate result.
   - Resolve value and order columns through MyLite descriptors.
   - Decode and validate separator literals before binding.
   - Generate quoted descriptor-safe SQLite SQL and bind separator, predicate,
     having, limit, and offset parameters.
   - Reject unsupported value/order column types and unsupported `HAVING`
     aggregate predicates.

5. Tests
   - Add a focused C runtime test under `packages/libmylite/tests/` and
     register a dotted CTest name.
   - Cover successful ungrouped and grouped aggregates, ordering directions,
     `NOT NULL` order keys, separators, filtering, aliases, schema/source
     resolution, persistence, rename/drop, independent handles, preamble
     preservation, and result status.
   - Cover deterministic diagnostics for unknown columns, unsupported types,
     unsupported syntax, and unsupported `HAVING` aggregate predicates.
   - Keep tests deterministic and avoid a new framework.

6. Build integration
   - Add any new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry plus parser and aggregate lifecycle entries.
   - Run
     `./packages/libmylite/tests/mysql_baseline_group_concat_aggregate_expectations.sh`.
   - Run `cmake --workflow --preset check`.
   - Review architecture boundaries, MySQL evidence, descriptor authority,
     SQLite lowering, separator binding, `NULL`/ordering behavior,
     file-format safety, zero-init cleanup, compatibility docs, and tests.

## Out Of Scope

`DISTINCT`, multi-expression arguments, expression arguments, expression or
ordinal order keys, multiple aggregate-local order keys, nullable order keys,
string collation order keys, binary result metadata, truncation via
`group_concat_max_len`, window forms, joins, subqueries, full grouping, and
SQLite fork patches.

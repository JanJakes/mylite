# Baseline SHOW OPEN TABLES Empty Introspection Tasks

## Goal

Add the next narrow public introspection slice: `SHOW OPEN TABLES` as an
embedded-compatible empty table-cache placeholder, returning MySQL's result
columns with zero rows.

## Tasks

1. Design and documentation
   - Create
     `docs/specs/baseline-show-open-tables-empty-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema behavior, result shape, diagnostics,
     physical SQLite policy, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior, upstream table-cache row behavior, and
     intentionally deferred wider forms.
   - Verify schema behavior, zero-row no-open-table results, result headers,
     `LIKE`, warnings, row-count behavior, and diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for
     `SHOW OPEN TABLES [{FROM|IN} schema] [LIKE 'pattern']`.
   - Add an AST node kind for `SHOW OPEN TABLES` statements.
   - Map `OPEN` in the parser adapter while preserving identifier use where
     allowed.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected show-open-tables syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path through the existing statement context.
   - Do not resolve selected/default schema for bare `SHOW OPEN TABLES`.
   - Accept known and unknown explicit schema names as empty successes, except
     reserved `_mylite_*` names.
   - Return a result set with the 4 MySQL 8.4.9 `SHOW OPEN TABLES` column
     labels and zero rows.

5. Catalog and physical policy
   - Do not iterate table descriptors for this empty table-cache placeholder.
   - Do not inspect SQLite schema, table, lock, or performance metadata.
   - Do not mutate catalog rows, descriptor versions, descriptor caches,
     catalog generation, physical SQLite objects, or
     `sqlite_schema_generation`.
   - Do not add SQLite fork patches.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover all admitted forms, exact result columns, zero rows, row-count
     behavior, unknown explicit schema success, reserved names, no-selected
     default behavior, reopen/drop behavior, preamble preservation, generation
     stability, independent handles, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/show/schema lifecycle
     entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, result
     shape accuracy, unknown-schema behavior, no-selected-schema behavior,
     file-format safety, VFS preservation, zero-init safety, cleanup on
     failure, scope control, compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Table-cache row accounting, `In_use` counts, `Name_locked` state, table
  locks, pending locks, `HANDLER ... OPEN`, temporary tables, performance
  schema, and privileges.
- Rows for any table, even if a MyLite statement has read or written that
  table.
- `SHOW OPEN TABLES ... WHERE`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, and
  combined `LIKE ... WHERE`.
- SQLite table-cache or lock metadata and SQLite fork patches.

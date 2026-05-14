# Baseline INFORMATION_SCHEMA VIEWS Tasks

## Goal

Add a queryable empty `INFORMATION_SCHEMA.VIEWS` system view with MySQL 8.4.9
column metadata, without implementing view DDL or execution.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-information-schema-views/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify query surface, empty-row semantics, system metadata, diagnostics,
     ownership boundaries, and unsupported view behavior.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify `VIEWS` column labels, empty user-schema results, system
     `TABLES` metadata, system `COLUMNS` metadata, warning count, and
     row-count behavior.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Runtime metadata registry
   - Add `INFORMATION_SCHEMA.VIEWS` to the synthetic table registry.
   - Add the ten observed column definitions.
   - Ensure `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` emit
     system-view metadata for `VIEWS`.
   - Emit no user view rows until view descriptors are designed.

4. Tests
   - Add fast C tests under `packages/libmylite/tests/`.
   - Cover wildcard and explicit projection, `COUNT(*)`, aliases, predicates,
     order/limit over the empty row set, system `TABLES` metadata, system
     `COLUMNS` metadata, independent handles, reopen safety, warning count,
     affected rows, and `ROW_COUNT()`.
   - Keep tests deterministic and avoid a new framework.

5. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

6. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused information-schema CTest entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for scope control, metadata accuracy, docs, and
     absence of view overclaiming.

## Out Of Scope

`CREATE VIEW`, `ALTER VIEW`, `DROP VIEW`, `SHOW CREATE VIEW`, view execution,
view descriptors, view dependencies, view privilege filtering, `sys` schema
views, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE`, and
`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE`.

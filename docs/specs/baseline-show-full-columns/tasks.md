# Baseline SHOW FULL COLUMNS Tasks

## Goal

Add descriptor-driven `SHOW FULL COLUMNS` / `SHOW FULL FIELDS` introspection for
the current `SHOW COLUMNS` table subset through `mylite_execute()`, without
changing public ABI or relying on SQLite metadata.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-full-columns/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, schema/table resolution, result mapping, diagnostics,
     physical-storage boundaries, and unsupported forms.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify result columns, collation values, privileges, comments, warning
     count, row-count behavior, name resolution, and diagnostics.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Parser and AST
   - Add a distinct AST kind or equivalent marker for `SHOW FULL COLUMNS`.
   - Extend Lemon grammar for `SHOW FULL {COLUMNS|FIELDS}` with current
     `FROM`/`IN`, schema, and `LIKE` forms.
   - Keep ordinary `SHOW COLUMNS`, `DESCRIBE`, and `DESC` behavior unchanged.
   - Add parser tests for supported full forms and deferred forms.

4. Runtime execution
   - Reuse current `SHOW COLUMNS` target resolution, descriptor loading,
     index/key metadata, default rendering, and `LIKE` filtering.
   - Emit the nine-column MySQL `FULL` result shape.
   - Return the table descriptor's default collation for text/enum/set
     descriptors and SQL `NULL` for numeric, temporal, JSON, binary string,
     and BIT descriptors.
   - Return fixed embedded privileges
     `select,insert,update,references` and empty comments.
   - Preserve result-set warning and row-count behavior.

5. Tests
   - Add or extend fast C tests under `packages/libmylite/tests/`.
   - Cover result labels, row values, schema forms, temporary shadowing,
     persistence, rename/drop, independent handles, preamble safety, and
     diagnostics.
   - Keep tests deterministic and avoid a new framework.

6. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused parser/runtime CTest entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for descriptor authority, MySQL result shape,
     parser scope, file-format safety, cleanup, and compatibility docs.

## Out Of Scope

`SHOW EXTENDED COLUMNS`, hidden storage-engine columns, generated invisible
primary keys, views, privilege filtering, column comments, `WHERE`,
column-filtered `DESCRIBE`, execution-plan `EXPLAIN`, new
`INFORMATION_SCHEMA` tables, SQLite metadata introspection, and SQLite fork
patches.

# Baseline SHOW TRIGGERS Empty Introspection Tasks

## Goal

Add the next narrow public introspection slice: `SHOW [FULL] TRIGGERS` for
schema descriptors that currently have no supported trigger descriptors,
returning MySQL's result columns with zero rows.

## Tasks

1. Design and documentation
   - Create
     `docs/specs/baseline-show-triggers-empty-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, result shape, diagnostics,
     physical SQLite policy, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally deferred wider forms.
   - Verify schema-resolution behavior, zero-row no-trigger results, result
     headers, `FULL`, `LIKE`, warnings, row-count behavior, and diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for
     `SHOW [FULL] TRIGGERS [{FROM|IN} schema] [LIKE 'pattern']`.
   - Add an AST node kind for `SHOW TRIGGERS` statements.
   - Map `FULL` and `TRIGGERS` in the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected show-triggers syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path through the existing statement context.
   - Resolve optional schema names with the same selected/default schema policy
     as `SHOW TABLES` and `SHOW TABLE STATUS`.
   - Reject reserved `_mylite_*` schema names before lookup.
   - Reject unknown schemas and no selected schema with deterministic
     diagnostics.
   - Return a result set with the 11 MySQL 8.4.9 `SHOW TRIGGERS` column labels
     and zero rows.

5. Catalog and physical policy
   - Read only MyLite schema descriptors.
   - Do not inspect SQLite schema/trigger metadata.
   - Do not mutate catalog rows, descriptor versions, descriptor caches,
     catalog generation, physical SQLite objects, or
     `sqlite_schema_generation`.
   - Do not add SQLite fork patches.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover all admitted forms, exact result columns, zero rows, row-count
     behavior, schema resolution, reopen/drop behavior, diagnostics, reserved
     names, preamble preservation, generation stability, independent handles,
     and unsupported syntax.
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
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, result shape accuracy, file-format safety, VFS preservation,
     zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Trigger DDL, trigger descriptors, trigger execution, trigger ordering,
  trigger persistence, definers, privileges, stored-program SQL modes, and
  `INFORMATION_SCHEMA.TRIGGERS`.
- Rows for any trigger kind.
- `SHOW CREATE TRIGGER`.
- `SHOW TRIGGERS ... WHERE`, `EXTENDED`, `ORDER BY`, `LIMIT`, singular
  `SHOW TRIGGER`, and combined `LIKE ... WHERE`.
- SQLite trigger metadata or SQLite fork patches.

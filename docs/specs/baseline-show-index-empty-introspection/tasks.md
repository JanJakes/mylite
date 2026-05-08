# Baseline SHOW INDEX Empty Introspection Tasks

## Goal

Add the next narrow public introspection slice: `SHOW INDEX`, `SHOW INDEXES`,
and `SHOW KEYS` for descriptor-backed persistent base tables that currently
have no supported index descriptors, returning MySQL's result columns with zero
rows.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-index-empty-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, result shape, diagnostics,
     physical SQLite policy, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally deferred wider forms.
   - Verify aliases, schema-resolution behavior, zero-row no-index results,
     result headers, warnings, row-count behavior, and diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW {INDEX|INDEXES|KEYS} {FROM|IN} table_name`
     with optional trailing `{FROM|IN} schema_name`.
   - Add an AST node kind for `SHOW INDEX` statements.
   - Map `INDEX`, `INDEXES`, and `KEYS` in the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected show-index syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path through the existing statement context.
   - Resolve table names with the same policy as `SHOW COLUMNS`, including
     trailing schema precedence.
   - Reject reserved `_mylite_*` source schema/table names.
   - Reject unknown tables, unknown schemas, no selected schema, unsupported
     object kinds, and unsupported parsed scopes with deterministic
     diagnostics.
   - Return a result set with the 15 MySQL 8.4.9 `SHOW INDEX` column labels and
     zero rows.

5. Catalog and physical policy
   - Read only MyLite schema/table descriptors.
   - Do not inspect SQLite schema/index metadata.
   - Do not mutate catalog rows, descriptor versions, descriptor caches,
     catalog generation, physical SQLite objects, or `sqlite_schema_generation`.
   - Do not add SQLite fork patches.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover all admitted aliases/forms, exact result columns, zero rows,
     row-count behavior, schema resolution, rename/drop/reopen behavior,
     diagnostics, reserved names, preamble preservation, generation stability,
     independent handles, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/show/table/schema lifecycle
     entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, result shape accuracy, file-format safety, VFS preservation,
     zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Index DDL or index descriptors.
- Rows for primary, unique, secondary, fulltext, spatial, functional,
  expression, prefix, descending, hidden, invisible, generated-column, or
  internal indexes.
- `SHOW EXTENDED INDEX`, `WHERE` filters, `INFORMATION_SCHEMA.STATISTICS`,
  temporary tables, views, privileges, optimizer metadata, and SQLite index
  metadata.
- SQLite fork patches.

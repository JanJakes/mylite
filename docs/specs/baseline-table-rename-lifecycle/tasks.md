# Baseline Table Rename Lifecycle Tasks

## Goal

Add the next narrow public table lifecycle slice: single-pair persistent
base-table `RENAME TABLE` through `mylite_execute()`, backed by authoritative
MyLite catalog descriptors and stable SQLite physical table names.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-table-rename-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, cross-schema behavior,
     descriptor mutation, physical SQLite policy, transactions, diagnostics,
     and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally rejected wider MySQL forms.
   - Verify success status, affected rows, warnings, `SHOW TABLES` side
     effects, diagnostics, and schema-resolution behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `RENAME TABLE table_name TO table_name`.
   - Add an AST node kind for rename statements.
   - Map `RENAME` and `TO` keywords in the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected rename syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path that uses the existing statement context.
   - Resolve source and target names independently against selected schema and
     MyLite catalog descriptors.
   - Support unqualified, schema-qualified, and cross-schema single-pair
     renames.
   - Reject reserved `_mylite_*` source and target schema/table names.
   - Reject unknown source tables, duplicate target tables, unknown schemas, no
     selected schema, unsupported object kinds, and unsupported parsed scopes
     with deterministic diagnostics.
   - Return an empty DDL result on success.

5. Catalog and physical policy
   - Add catalog mutation helpers that update table `schema_id`, `name`,
     `descriptor_version`, and `updated_catalog_generation` inside the caller's
     active mutation.
   - Preserve stable physical table names such as `_mylite_user_table_<id>`.
   - Do not issue SQLite physical DDL for rename.
   - Advance catalog generation and invalidate descriptor caches only after a
     successful commit.
   - Leave column descriptors and SQLite schema generation unchanged.
   - Roll back catalog changes on failure.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful rename, cross-schema rename, catalog descriptor changes,
     unchanged columns, stable physical table name, shifted-payload safety,
     reopen persistence, `SHOW TABLES`, failure unwinding, independent
     handles, reserved names, duplicate targets, unknown sources, no selected
     schema, unknown target schema, unsupported multi-table syntax, and induced
     catalog mutation failure.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic table lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor/physical-schema atomicity, file-format safety, VFS
     preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Multi-table `RENAME TABLE` execution.
- `ALTER TABLE` rename forms.
- Temporary tables, view rename execution, triggers, privilege semantics,
  metadata locks, foreign keys, routines, events, and `INFORMATION_SCHEMA`.
- General DML, query execution, expression support, arbitrary SQLite
  pass-through, `TRUNCATE TABLE`, table rebuilds, indexes, constraints,
  defaults, generated columns, table options, and auto-increment semantics.
- SQLite fork patches.

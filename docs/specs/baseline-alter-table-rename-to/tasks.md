# Baseline ALTER TABLE RENAME TO Lifecycle Tasks

## Goal

Add the next narrow public table DDL slice: single-action persistent base-table
`ALTER TABLE ... RENAME` through `mylite_execute()`, backed by authoritative
MyLite catalog descriptors and stable SQLite physical table names.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-alter-table-rename-to/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, cross-schema behavior,
     same-object no-op behavior, descriptor mutation, physical SQLite policy,
     transactions, diagnostics, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally rejected wider MySQL forms.
   - Verify syntax variants, success status, affected rows, warnings,
     `SHOW TABLES` side effects, diagnostics, same-object no-op behavior, and
     schema-resolution behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `ALTER TABLE table_name RENAME table_name`,
     `RENAME TO table_name`, and `RENAME AS table_name`.
   - Add an AST node kind for `ALTER TABLE ... RENAME` statements.
   - Map the existing `ALTER`, `RENAME`, `TO`, and `AS` keywords in the parser
     adapter where needed.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected alter-rename syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path that uses the existing statement context.
   - Resolve source and target names against selected schema and MyLite catalog
     descriptors.
   - Match MySQL's no-selected-schema precedence for an unqualified target when
     no default schema is selected.
   - Support unqualified, schema-qualified, and cross-schema single-action
     renames.
   - Reject reserved `_mylite_*` source and target schema/table names.
   - Reject unknown source tables, duplicate target tables, unknown schemas, no
     selected schema, unsupported object kinds, and unsupported parsed scopes
     with deterministic diagnostics.
   - Return an empty DDL result on success.

5. Catalog and physical policy
   - Reuse the existing catalog mutation helper that updates table `schema_id`,
     `name`, `descriptor_version`, and `updated_catalog_generation` inside the
     caller's active mutation.
   - Preserve stable physical table names such as `_mylite_user_table_<id>`.
   - Do not issue SQLite physical DDL for rename.
   - Advance catalog generation and invalidate descriptor caches only after a
     successful real rename.
   - Preserve catalog generation, descriptor versions, descriptor caches, and
     SQLite schema generation for same-object no-op success.
   - Leave column descriptors unchanged.
   - Roll back catalog changes on failure.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful rename variants, cross-schema rename, catalog descriptor
     changes, unchanged columns, stable physical table name, shifted-payload
     safety, reopen persistence, `SHOW TABLES`, `SHOW CREATE TABLE`, DML
     visibility, failure unwinding, independent handles, reserved names,
     duplicate targets, same-object no-op, unknown sources, no selected schema,
     unknown target schema, unsupported syntax, and induced catalog mutation
     failure.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic/rename/row/update/
     truncate lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor/physical-schema atomicity, file-format safety, VFS
     preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- General `ALTER TABLE` execution.
- Combined alter actions and multiple rename clauses.
- Column, index, key, constraint, table-option, partition, algorithm, lock,
  validation, visibility, and tablespace actions.
- Temporary tables, views, triggers, privilege semantics, metadata locks,
  foreign keys, routines, events, and `INFORMATION_SCHEMA`.
- Table rebuilds, indexes, constraints, defaults, generated columns, table
  options, and auto-increment semantics.
- SQLite fork patches.

# Baseline ALTER TABLE DROP COLUMN Lifecycle Tasks

## Goal

Add the next narrow table lifecycle slice: descriptor-driven
`ALTER TABLE table_name DROP [COLUMN] column_name` through `mylite_execute()`,
backed by authoritative MyLite catalog descriptors, stable SQLite physical
table names, and the existing integer/`NULL` row model.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-alter-table-drop-column/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema/table/column resolution, descriptor ordinal
     compaction, physical SQLite handling, diagnostics, result behavior, and
     unsupported behavior.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only
     for the exact partial subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     drop-column behavior and deliberately rejected wider forms.
   - Verify affected rows, warnings, remaining row values, diagnostics,
     `SHOW COLUMNS`, `SHOW CREATE TABLE`, later DML behavior, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for
     `ALTER TABLE table_name DROP [COLUMN] column_name`.
   - Add an AST node kind for drop-column statements.
   - Reuse existing table-name and column identifier handling.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected drop-column syntax.

4. Catalog support
   - Add an in-mutation catalog helper for deleting one column descriptor and
     compacting remaining ordinals.
   - Keep remaining column ids, names, types, nullability, and creation
     generation unchanged.
   - Increment table descriptor version and catalog generation once.
   - Ensure descriptor-cache invalidation follows the existing catalog mutation
     policy.

5. Analyzer/planner/runtime execution
   - Add a statement execution path using the existing statement context.
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` target schema/table/column names before
     generated SQLite SQL.
   - Resolve the dropped column from descriptors, not SQLite metadata.
   - Reject unknown schemas, unknown tables, unsupported object kinds, unknown
     columns, single-column tables, unsupported grammar, and unsupported
     actions with deterministic diagnostics.
   - Return an empty DDL result with `affected_rows == 0` and no warnings.

6. Physical SQLite mutation
   - Generate SQLite `ALTER TABLE ... DROP COLUMN` only from descriptors and
     stable physical table names.
   - Quote every generated SQLite identifier.
   - Execute catalog mutation and physical SQLite schema mutation as one
     statement boundary.
   - Increment connection-local SQLite schema generation after successful
     physical schema change.
   - Avoid generalized rebuilds, writable-schema editing, arbitrary SQLite
     pass-through, and SQLite fork patches.

7. Atomicity and cleanup
   - Roll back if prepare, step, allocation, catalog mutation, physical SQLite
     execution, or commit fails.
   - Make new planner cleanup functions tolerate zero-initialized state.
   - Preserve existing public API misuse behavior.

8. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful middle/first/last non-only drops, selected-schema,
     schema-qualified, persistence, rename/drop/truncate/DML, diagnostics,
     unsupported syntax, descriptor preservation/update, ordinal compaction,
     physical schema generation, rollback, preamble safety, independent
     handles, and result behavior.
   - Keep tests deterministic and avoid adding a new test framework.

9. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

10. Verification and review
    - Run `cmake --build --preset dev`.
    - Run the new CTest entry and existing parser/basic/rename/alter-rename/
      add-column/row-values/select/update/delete/truncate/show-column/
      show-create lifecycle entries.
    - Run the MySQL expectation script.
    - Run `cmake --workflow --preset check`.
    - Review the final diff for architecture boundaries, public ABI stability,
      independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
      authority, descriptor-driven physical schema mutation, ordinal compaction,
      file-format safety, VFS preservation, zero-init safety, cleanup on
      failure, scope control, compatibility accuracy, and test relevance.

## Out Of Scope

- Full `ALTER TABLE`, multiple drop actions, combined actions, algorithms,
  locks, table rebuilds, indexes, keys, constraints, generated columns,
  invisible columns, auto-increment, temporary tables, views, triggers,
  cascades, foreign keys, privileges, metadata locks, dependency maintenance,
  implicit commit boundaries, arbitrary SQLite pass-through, and SQLite fork
  patches.

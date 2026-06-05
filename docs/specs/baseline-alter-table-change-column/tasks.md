# Baseline ALTER TABLE CHANGE COLUMN Lifecycle Tasks

## Goal

Add the next narrow table lifecycle slice: descriptor-driven
`ALTER TABLE table_name CHANGE [COLUMN] old_column column_definition` through
`mylite_execute()`, backed by authoritative MyLite catalog descriptors, stable
SQLite physical table names, and the existing integer/`NULL` row model.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-alter-table-change-column/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema/table/old-column/replacement-column
     resolution, replacement descriptor semantics, row validation, physical
     rename/rebuild handling, diagnostics, result behavior, and unsupported
     behavior.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only
     for the exact partial subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     change-column behavior and deliberately rejected wider forms.
   - Verify affected rows, warnings, row values, ordinals, diagnostics,
     same-definition no-op, pure rename, case-only spelling changes,
     nullability replacement, range validation, `SHOW COLUMNS`,
     `SHOW CREATE TABLE`, later DML behavior, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for
     `ALTER TABLE table_name CHANGE [COLUMN] old_column column_definition`.
   - Add an AST node kind for change-column statements.
   - Reuse existing table-name, identifier, and column-definition handling.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected change-column syntax.

4. Catalog support
   - Reuse the in-mutation catalog helper for replacing one column descriptor's
     name spelling, logical type, physical type, and nullability.
   - Keep column id, ordinal, and creation generation unchanged.
   - Increment table descriptor version and catalog generation once for
     non-no-op mutations.
   - Ensure descriptor-cache invalidation follows the existing catalog mutation
     policy.

5. Analyzer/planner/runtime execution
   - Add a statement execution path using the existing statement context.
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` target schema/table/old-column/replacement
     names before generated SQLite SQL.
   - Resolve the old column and replacement-name duplicate checks from
     descriptors, not SQLite metadata.
   - Reject unknown schemas, unknown tables, unsupported object kinds, unknown
     old columns, duplicate replacement columns, unsupported grammar, and
     unsupported actions with deterministic diagnostics.
   - Treat byte-for-byte identical descriptor definitions with the same name as
     successful no-ops.
   - Support pure rename and case-only spelling updates.
   - Validate existing rows against target nullability and integer range inside
     the catalog mutation before descriptor replacement and physical rebuild.
   - Admit explicit `AUTO_INCREMENT` on one supported integer-family replacement
     column only when a key starts with that column, and keep `SERIAL` alias
     support out of this slice.
   - Return an empty DDL result with MySQL-compatible affected rows and no
     warnings.

6. Physical SQLite mutation
   - Use native physical column rename only for pure name-spelling changes.
   - Use the descriptor-driven rebuild path for type or nullability changes
     because SQLite has no public `CHANGE COLUMN`.
   - Generate physical SQL only from descriptors and stable physical table
     names.
   - Quote every generated SQLite identifier.
   - Preserve the stable physical table name after successful rebuild.
   - Execute catalog mutation and physical SQLite schema mutation as one
     statement boundary.
   - Increment connection-local SQLite schema generation after successful
     physical schema change.
   - Avoid writable-schema editing, arbitrary SQLite pass-through, and SQLite
     fork patches.

7. Atomicity and cleanup
   - Roll back if validation, prepare, step, allocation, catalog mutation,
     physical SQLite execution, or commit fails.
   - Restore or preserve the old physical table on rebuild failure.
   - Make new planner/rebuild cleanup functions tolerate zero-initialized
     state.
   - Preserve existing public API misuse behavior.

8. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover successful pure rename, type and nullability replacements,
     rename-plus-type changes, same-definition no-op, case-only spelling
     update, selected-schema, schema-qualified, persistence, DML after change,
     diagnostics, unsupported syntax, descriptor preservation/update, physical
     schema generation, rollback, preamble safety, independent handles,
     explicit `AUTO_INCREMENT` replacement, and result behavior.
   - Keep tests deterministic and avoid adding a new test framework.

9. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

10. Verification and review
    - Run `cmake --build --preset dev`.
    - Run the new CTest entry and existing parser/basic/rename/alter-rename/
      add-column/drop-column/rename-column/modify-column/row-values/select/
      update/delete/truncate/show-column/show-create lifecycle entries.
    - Run the MySQL expectation script.
    - Run `cmake --workflow --preset check`.
    - Review the final diff for architecture boundaries, public ABI stability,
      independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
      authority, descriptor-driven rename/rebuild safety, nullability and range
      validation, affected-row semantics, file-format safety, VFS preservation,
      zero-init safety, cleanup on failure, scope control, compatibility
      accuracy, and test relevance.

## Out Of Scope

- Full `ALTER TABLE`, multiple change actions and combined actions outside the
  dedicated limited multi-action ALTER slice, algorithms, locks, non-admitted
  defaults, non-admitted types, indexes, keys, constraints, generated columns,
  invisible columns, `SERIAL` alias expansion, temporary tables, views, triggers,
  cascades, foreign keys, privileges, metadata locks, dependency maintenance,
  implicit commit boundaries, arbitrary SQLite pass-through, and SQLite fork
  patches.

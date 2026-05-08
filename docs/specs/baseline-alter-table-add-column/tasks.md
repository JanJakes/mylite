# Baseline ALTER TABLE ADD COLUMN Lifecycle Tasks

## Goal

Add the next narrow table lifecycle slice: descriptor-driven
`ALTER TABLE table_name ADD [COLUMN] column_definition` through
`mylite_execute()`, backed by authoritative MyLite catalog descriptors, stable
SQLite physical table names, and the existing integer/`NULL` row model.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-alter-table-add-column/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema/table resolution, column descriptors,
     existing-row backfill, physical SQLite handling, diagnostics, result
     behavior, and unsupported behavior.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only
     for the exact partial subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported add-column
     behavior and deliberately rejected wider forms.
   - Verify affected rows, warnings, backfilled row values, diagnostics,
     `SHOW CREATE TABLE`, later insert behavior, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for
     `ALTER TABLE table_name ADD [COLUMN] column_definition`.
   - Add an AST node kind for add-column statements.
   - Reuse the existing create-table `column_definition` grammar and node.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected add-column syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path using the existing statement context.
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` target schema/table/column names before
     generated SQLite SQL.
   - Resolve duplicate columns from descriptors, not SQLite metadata.
   - Reject unknown schemas, unknown tables, unsupported object kinds,
     duplicate columns, unsupported grammar, unsupported types, and unsupported
     column attributes with deterministic diagnostics.
   - Return an empty DDL result with `affected_rows == 0` and no warnings.

5. Catalog and physical SQLite mutation
   - Insert one appended column descriptor inside a MyLite catalog mutation.
   - Increment the table descriptor version and catalog generation once.
   - Generate SQLite `ALTER TABLE ... ADD COLUMN` only from descriptors and
     stable physical table names.
   - Quote every generated SQLite identifier.
   - Use a physical `DEFAULT 0` only as the internal backfill mechanism for
     `NOT NULL` integer columns; do not expose a logical default.
   - Increment connection-local SQLite schema generation after successful
     physical schema change.

6. Atomicity and cleanup
   - Execute supported add-column statements inside one MyLite-owned
     transaction boundary.
   - Roll back if prepare, step, allocation, catalog mutation, physical SQLite
     execution, or commit fails.
   - Make new planner cleanup functions tolerate zero-initialized state.
   - Preserve existing public API misuse behavior.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful nullable/non-null, empty/nonempty, integer-family,
     selected-schema, schema-qualified, persistence, rename/drop/truncate/DML,
     diagnostics, unsupported syntax, descriptor preservation/update, physical
     schema generation, rollback, preamble safety, independent handles, and
     result behavior.
   - Keep tests deterministic and avoid adding a new test framework.

8. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic/rename/alter-rename/
     row-values/select/update/delete/truncate lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor-driven physical schema mutation, integer backfill
     correctness, file-format safety, VFS preservation, zero-init safety,
     cleanup on failure, scope control, compatibility accuracy, and test
     relevance.

## Out Of Scope

- Full `ALTER TABLE`, defaults, column positioning, multiple actions,
  parenthesized add lists, algorithms, locks, table rebuilds, non-integer
  types, keys, constraints, generated columns, invisible columns,
  auto-increment, temporary tables, views, triggers, cascades, foreign keys,
  privileges, metadata locks, arbitrary SQLite pass-through, and SQLite fork
  patches.


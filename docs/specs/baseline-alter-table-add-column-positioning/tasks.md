# Baseline ALTER TABLE ADD COLUMN Positioning Tasks

## Goal

Extend the existing descriptor-driven add-column lifecycle with MySQL-compatible
logical `FIRST` and `AFTER column_name` placement for one added column on a
persistent base table.

## Tasks

1. Design and MySQL evidence
   - Create `docs/specs/baseline-alter-table-add-column-positioning/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, descriptor resolution, logical ordering, physical
     SQLite handling, result semantics, diagnostics, and unsupported behavior.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for `FIRST`, `AFTER`,
     visible order, row values, diagnostics, affected rows, warnings, and
     schema-qualified targets.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for compatibility
     expectations.

3. Parser and AST
   - Reuse the existing `column_position_opt` grammar for
     `ALTER TABLE ... ADD [COLUMN]`.
   - Store the optional position node as an add-column statement child.
   - Add parser tests for accepted and rejected positioned add-column syntax.

4. Analyzer/planner/runtime
   - Resolve target schema/table through existing writable-table policy.
   - Resolve the added descriptor through existing `plan_column`.
   - Resolve `FIRST` and `AFTER` from MyLite descriptors, not SQLite metadata.
   - Preserve duplicate-name precedence before `AFTER` resolution.
   - Return an empty DDL result with affected rows `0` and no warnings.

5. Catalog and SQLite execution
   - Insert the new descriptor and reorder descriptor ordinals in one catalog
     mutation when requested position is not append.
   - Keep SQLite physical `ALTER TABLE ... ADD COLUMN` append-only, using quoted
     descriptor-built identifiers and physical type/default text.
   - Preserve descriptor authority and avoid SQLite schema-text introspection.
   - Roll back logical and physical changes together on failure.

6. Tests
   - Extend fast C parser/runtime tests under `packages/libmylite/tests/`.
   - Cover descriptor order, `SELECT *`, implicit row-value insert, DML,
     metadata surfaces, diagnostics, persistence, independent handles, physical
     failure rollback, and preamble preservation.
   - Keep tests deterministic and avoid a new framework.

7. Documentation and compatibility
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/sql-table-ddl.md`.
   - Keep unsupported surfaces explicit.

8. Verification and review
   - Run focused build/tests, the MySQL expectation script, and
     `cmake --workflow --preset check`.
   - Review the diff for MySQL equivalence, descriptor authority, physical
     storage safety, rollback behavior, ABI stability, and scope control.

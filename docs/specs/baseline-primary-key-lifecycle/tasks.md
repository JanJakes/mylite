# Baseline Primary Key Lifecycle Tasks

## Goal

Add the first descriptor-owned primary-key slice for persistent base tables:
single-column integer-family primary keys declared in `CREATE TABLE`, catalog
descriptors, generated SQLite unique-index enforcement, MySQL-shaped metadata,
duplicate-key diagnostics, and persistence.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-primary-key-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, descriptor ownership, catalog schema, nullability,
     physical SQLite handling, metadata rendering, diagnostics, DML behavior,
     and unsupported behavior.
   - Update compatibility docs only for the exact implemented subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported primary
     keys and deliberately deferred wider forms.
   - Verify `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE TABLE`, duplicate-key
     errors, `INSERT IGNORE` warnings, `CREATE TABLE LIKE`, and `CREATE TABLE
     SELECT` behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend `CREATE TABLE` grammar from column-only lists to table item lists.
   - Add AST nodes for primary-key definitions and key-part lists.
   - Preserve parser independence from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported inline/table-level primary keys and
     rejected unsupported key syntax.

4. Catalog descriptors
   - Advance the catalog schema version and migration path.
   - Add descriptor tables for indexes and index columns.
   - Add catalog APIs for inserting, reading, cloning, deleting, and iterating
     primary-key descriptors.
   - Ensure table/schema deletion removes key descriptors before table
     descriptors.
   - Keep descriptor caches and generation handling consistent with existing
     catalog mutation rules.

5. Analyzer and DDL execution
   - Resolve primary-key definitions against planned columns.
   - Admit exactly one primary key over exactly one unqualified integer-family
     column.
   - Reject explicit `NULL`, `DEFAULT NULL`, duplicate primary keys, unknown
     key columns, unsupported types, and unsupported key forms.
   - Mark admitted key columns effectively `NOT NULL`.
   - Create generated SQLite unique indexes using stable physical names and
     quoted identifiers.
   - Clone primary-key descriptors and physical indexes for `CREATE TABLE ...
     LIKE`.
   - Preserve `CREATE TABLE ... SELECT` no-key target behavior.

6. DML behavior
   - Detect or map duplicate-key errors for `INSERT ... VALUES`, `INSERT ...
     SET`, and single-assignment `UPDATE`.
   - Demote admitted duplicate-key conflicts for `INSERT IGNORE ... VALUES` and
     `INSERT IGNORE ... SET` to warning `1062` and skip conflicting rows.
   - Reject `REPLACE` into key-bearing tables with a deterministic unsupported
     diagnostic until key-aware `REPLACE` is specified.
   - Preserve current affected-row, warning-count, and non-row result
     conventions.

7. Metadata rendering
   - Render `PRI` in `SHOW COLUMNS` for the primary-key column.
   - Render one descriptor-backed `SHOW INDEX` row for admitted primary keys.
   - Render `PRIMARY KEY (`column`)` in `SHOW CREATE TABLE`.
   - Keep no-key table metadata unchanged.

8. Tests
   - Add fast plain C runtime tests under `packages/libmylite/tests/` and
     register dotted CTest names if a new binary is clearer.
   - Cover successful create, metadata, DML enforcement, `INSERT IGNORE`,
     default handling, unsupported syntax, persistence, rename/drop/truncate,
     `CREATE TABLE LIKE`, `CREATE TABLE SELECT`, independent handles, preamble
     safety, and cleanup.
   - Extend parser tests for the new grammar.
   - Keep tests deterministic and avoid adding a new test framework.

9. Build integration
   - Add any new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

10. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entries plus existing parser/runtime lifecycle entries.
   - Run
     `./packages/libmylite/tests/mysql_baseline_primary_key_lifecycle_expectations.sh`.
   - Run `cmake --workflow --preset check`.
   - Review architecture boundaries, public ABI stability, independently
     authored grammar/spec text, MySQL evidence, catalog authority, SQLite
     index lowering, duplicate-key diagnostics, file-format safety,
     zero-init cleanup, compatibility docs, and test relevance.

## Out Of Scope

- Composite primary keys, `VARCHAR` primary keys, named constraints, column
  `KEY` shorthand, secondary indexes, standalone index DDL, ALTER primary-key
  DDL, `AUTO_INCREMENT`, generated invisible primary keys, information-schema
  index metadata, key-aware `REPLACE`, `ON DUPLICATE KEY UPDATE`, foreign keys,
  triggers, cascades, optimizer/index-use assertions, and SQLite fork patches.

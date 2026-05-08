# Baseline Table IF EXISTS Lifecycle Tasks

## Goal

Add the next narrow table lifecycle slice: optional
`CREATE TABLE IF NOT EXISTS` and `DROP TABLE IF EXISTS` for the already
supported persistent base-table subset, preserving MySQL 8.4.9 diagnostics and
warning behavior.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-table-if-exists-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, AST policy, schema/table resolution, no-op
     behavior, warning diagnostics, result behavior, physical SQLite handling,
     and intentionally unsupported wider forms.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only
     for the exact partial subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     existence-clause behavior and deliberately rejected wider forms.
   - Verify `ROW_COUNT()`, `@@warning_count`, `SHOW WARNINGS`, table
     descriptors, existing-table preservation, missing-schema behavior, and
     selected-schema behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `CREATE TABLE IF NOT EXISTS` and
     `DROP TABLE IF EXISTS`.
   - Add compact AST marker nodes or equivalent typed representation for the
     two existence policies.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected existence-clause syntax.

4. Analyzer/planner/runtime execution
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Preserve missing default-schema errors for unqualified create/drop.
   - Preserve unknown explicit-schema errors for create.
   - For drop with `IF EXISTS`, convert missing explicit schema and missing
     table to successful no-op warnings.
   - Reject reserved `_mylite_*` schema/table names before generated SQLite SQL.
   - For create with `IF NOT EXISTS`, skip descriptor validation and physical
     SQL when the target table already exists.
   - For drop with `IF EXISTS`, skip catalog mutation and physical SQL when the
     target table or explicit schema is missing.
   - Return empty DDL results with MySQL-compatible warning count and row count.

5. Diagnostics
   - Add MySQL-compatible warning records for existing-create no-ops:
     `Note 1050 Table '<table>' already exists`.
   - Add MySQL-compatible warning records for missing-drop no-ops:
     `Note 1051 Unknown table '<schema>.<table>'`.
   - Ensure `SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`, `@@warning_count`,
     `@@error_count`, result warning count, and public diagnostics state follow
     existing statement context rules.

6. Physical SQLite mutation
   - Reuse existing physical create/drop SQL for real mutations.
   - Generate no SQLite SQL for existence no-ops.
   - Preserve stable physical table naming, identifier quoting, catalog
     generation, SQLite schema generation, and shifted `.mylite` preamble
     invariants.
   - Avoid arbitrary SQLite pass-through and SQLite fork patches.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover create missing, create existing, drop existing, drop missing,
     missing default schema, missing explicit schema behavior, reserved names,
     diagnostics/warnings, generation invariants, reopen persistence, preamble
     safety, independent handles, and unsupported syntax.
   - Keep tests deterministic and avoid a new test framework.

8. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic-table/schema/diagnostics
     lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for parser scope, diagnostics correctness,
     generation invariants, file-format safety, no-op behavior, compatibility
     accuracy, and test relevance.

## Out Of Scope

- Temporary tables, multi-table drop, `CREATE TABLE ... LIKE`, `CREATE TABLE
  ... SELECT`, table defaults, keys, constraints, generated columns,
  auto-increment, `CREATE DATABASE IF NOT EXISTS`, `DROP DATABASE IF EXISTS`,
  privileges, implicit commit behavior, arbitrary SQLite pass-through, and
  SQLite fork patches.

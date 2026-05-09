# Baseline Schema IF EXISTS Lifecycle Tasks

## Goal

Add the next narrow schema lifecycle slice: optional
`CREATE DATABASE|SCHEMA IF NOT EXISTS` and `DROP DATABASE|SCHEMA IF EXISTS` for
the already supported persistent schema subset, preserving MySQL 8.4.9 result
and diagnostics behavior.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-schema-if-exists-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, AST policy, schema resolution, no-op behavior,
     warning diagnostics, result behavior, physical SQLite handling, and
     intentionally unsupported wider forms.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-schemas.md` only for
     the exact partial subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     existence-clause behavior and deliberately rejected wider forms.
   - Verify `ROW_COUNT()`, `@@warning_count`, `SHOW WARNINGS`,
     `SHOW COUNT(*) WARNINGS`, selected-schema behavior, affected rows, and
     missing-schema behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for schema create/drop existence clauses.
   - Add compact AST marker nodes or equivalent typed representation for the
     two existence policies.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected existence-clause syntax.

4. Analyzer/planner/runtime execution
   - Resolve schema targets against MyLite catalog descriptors.
   - Reject reserved `_mylite_*` schema names before generated SQLite SQL.
   - For create with `IF NOT EXISTS`, skip catalog mutation when the schema
     already exists.
   - For drop with `IF EXISTS`, skip catalog mutation and physical SQL when the
     schema is missing.
   - Distinguish catalog read failure from true not-found before choosing a
     no-op.
   - Return empty DDL results with MySQL-compatible result warning count,
     affected rows, and row-count state.

5. Diagnostics
   - Add stored MySQL-compatible note records for existing-create no-ops:
     `Note 1007 Can't create database '<schema>'; database exists`.
   - Implement MySQL-compatible missing-drop result warning count without
     storing a diagnostics row for later `SHOW WARNINGS` / `@@warning_count`.
   - Ensure diagnostics snapshots follow existing statement context rules.

6. Physical SQLite mutation
   - Reuse existing physical schema-drop cleanup for real mutations.
   - Generate no SQLite SQL for existence no-ops.
   - Preserve stable physical table naming, identifier quoting, catalog
     generation, SQLite schema generation, and shifted `.mylite` preamble
     invariants.
   - Avoid arbitrary SQLite pass-through and SQLite fork patches.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover create missing, create existing, drop existing, drop missing,
     reserved names, diagnostics/warnings, generation invariants, selected
     schema behavior, reopen persistence, preamble safety, independent handles,
     catalog failure handling, and unsupported syntax.
   - Keep tests deterministic and avoid a new test framework.

8. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/schema/table/diagnostics
     lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for parser scope, diagnostics correctness,
     generation invariants, file-format safety, no-op behavior, compatibility
     accuracy, and test relevance.

## Out Of Scope

- Schema options, privileges, system schemas, filesystem directories, implicit
  commit behavior, temporary tables, views, triggers, foreign keys, arbitrary
  SQLite pass-through, and SQLite fork patches.

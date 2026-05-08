# Baseline SHOW Routine Status Empty Introspection Tasks

## Goal

Add the next narrow public introspection slice: `SHOW PROCEDURE STATUS` and
`SHOW FUNCTION STATUS` as empty stored-routine placeholders, returning MySQL's
result columns with zero rows.

## Tasks

1. Design and documentation
   - Create
     `docs/specs/baseline-show-routine-status-empty-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, global schema behavior, result shape,
     diagnostics, physical SQLite policy, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior, upstream routine-row behavior, and
     intentionally deferred wider forms.
   - Verify result headers, no-default-schema behavior, selected-schema
     independence, `LIKE`, upstream `WHERE`, warnings, row-count behavior, and
     diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for
     `SHOW PROCEDURE STATUS [LIKE 'pattern']` and
     `SHOW FUNCTION STATUS [LIKE 'pattern']`.
   - Add AST node kinds for routine-status statements.
   - Map `PROCEDURE` and `FUNCTION` in the parser adapter while preserving
     existing `STATUS` identifier use.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected routine-status syntax.

4. Analyzer/planner/runtime execution
   - Add statement execution paths through the existing statement context.
   - Do not resolve selected/default schema for routine-status statements.
   - Return result sets with the 12 MySQL 8.4.9 routine-status column labels
     and zero rows.
   - Decode supported `LIKE` patterns using the existing `SHOW LIKE` helper so
     unsupported literals fail consistently.

5. Catalog and physical policy
   - Do not iterate schema, table, trigger, event, or routine descriptors.
   - Do not inspect SQLite schema, routine, or information-schema metadata.
   - Do not mutate catalog rows, descriptor versions, descriptor caches,
     catalog generation, physical SQLite objects, or
     `sqlite_schema_generation`.
   - Do not add SQLite fork patches.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover all admitted forms, exact result columns, zero rows, row-count
     behavior, no-selected-default behavior, selected-schema independence,
     reopen/drop behavior, preamble preservation, generation stability,
     independent handles, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/show/schema lifecycle
     entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, result
     shape accuracy, global schema behavior, no-selected-schema behavior,
     file-format safety, VFS preservation, zero-init safety, cleanup on
     failure, scope control, compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Stored routine DDL, routine descriptors, routine body parsing, invocation,
  definers, SQL security, routine SQL modes, character-set capture,
  deterministic metadata, comments, privileges, and
  `INFORMATION_SCHEMA.ROUTINES`.
- Rows for procedures or functions.
- `SHOW CREATE PROCEDURE`, `SHOW CREATE FUNCTION`, `SHOW PROCEDURE CODE`, and
  `SHOW FUNCTION CODE`.
- `SHOW ... STATUS WHERE`, `FROM`, `IN`, `FULL`, `EXTENDED`, `ORDER BY`,
  `LIMIT`, and combined `LIKE ... WHERE`.
- SQLite routine metadata or SQLite fork patches.

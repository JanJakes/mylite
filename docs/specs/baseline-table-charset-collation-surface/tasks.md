# Baseline Table Charset and Collation Surface Tasks

## Goal

Accept MySQL 8.4 default table charset/collation options on MyLite's existing
limited persistent `CREATE TABLE` path, aligned with the fixed
`SHOW CREATE TABLE` suffix already emitted by MyLite.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-table-charset-collation-surface/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, option ordering, option name decoding, diagnostics,
     ownership boundaries, SQLite handling, and compatibility limits.
   - Keep the slice limited to fixed `utf8mb4` and
     `utf8mb4_0900_ai_ci` table defaults.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for admitted syntax,
     `SHOW CREATE TABLE` output, warnings, row-count state, and diagnostics.
   - Verify charset/collation name forms, option order, optional `=`,
     `DEFAULT`, `CHARSET`, `CHARACTER SET`, `COLLATE`, and known deferred
     forms.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for implementation.

3. Parser and AST
   - Replace the single optional engine option after `CREATE TABLE (...)` with
     a table-option list that can contain engine, charset, and collation
     options.
   - Add AST node kinds and parser helpers for the option list, charset option,
     and collation option.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected table-option syntax.

4. Analyzer/planner/runtime validation
   - Validate table options before catalog mutation or physical SQLite SQL
     generation.
   - Preserve existing `ENGINE [=] InnoDB` behavior inside the new option list.
   - Decode identifier, quoted identifier, and string option names safely,
     rejecting raw NUL bytes and decoded `\0` escapes.
   - Accept only `utf8mb4` and `utf8mb4_0900_ai_ci`, compared
     case-insensitively.
   - Reject unsupported charsets/collations with deterministic diagnostics.
   - Do not add table charset/collation catalog fields in this slice.

5. Tests
   - Add a fast C runtime test under `packages/libmylite/tests/` and register
     it with a dotted CTest name.
   - Cover success forms, combined option ordering, schema resolution, row
     storage persistence, `SHOW CREATE TABLE`, result metadata, row-count
     state, unknown/unsupported option names, unsupported syntax, raw NUL
     handling, preamble preservation, and independent handles.
   - Keep tests deterministic and avoid adding a new test framework.

6. Compatibility documentation
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/sql-table-ddl.md`.
   - Update `docs/compatibility/collations.md` only for the exact limited
     fixed-default table-option acceptance.
   - Do not overclaim full charset/collation catalogs or string semantics.

7. Build integration
   - Add any new test target to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run the MySQL expectation script.
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry plus parser and adjacent lifecycle/introspection
     entries.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for grammar independence, MySQL evidence, option
     diagnostics, catalog authority, file-format safety, scope control,
     compatibility documentation accuracy, and test relevance.

## Out Of Scope

- Non-default table charsets or collations.
- Per-table or per-column charset/collation descriptor storage.
- Database charset/collation defaults.
- String column types and string comparison semantics.
- `SHOW CHARACTER SET`, `SHOW COLLATION`, and charset/collation metadata
  tables.
- `ALTER TABLE`, `ALTER DATABASE`, and `CREATE DATABASE` charset/collation
  forms.
- Other table options, temporary tables, views, partitions, triggers, full SQL
  modes, and SQLite fork patches.

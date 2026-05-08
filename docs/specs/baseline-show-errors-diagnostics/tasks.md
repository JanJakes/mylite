# Baseline SHOW Errors Diagnostics Tasks

## Goal

Add the next narrow diagnostics introspection slice:
`SHOW ERRORS`, `SHOW ERRORS LIMIT ...`, and
`SHOW COUNT(*) ERRORS` over MyLite's previous statement diagnostics.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-errors-diagnostics/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, diagnostics snapshot lifecycle, result shapes,
     error-only filtering, `LIMIT` slicing, row-count and warning-count
     behavior, physical SQLite policy, and unsupported behavior.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md`
     only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally deferred wider forms.
   - Verify result headers, warning-only filtering, count output, diagnostic
     chaining, ordinary-statement clearing, limit forms, syntax errors, and
     parse-error exposure.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW ERRORS [LIMIT ...]` and
     `SHOW COUNT(*) ERRORS`.
   - Add AST node kinds for the diagnostics statements.
   - Map `ERRORS` in the parser adapter while preserving lexer behavior.
   - Enforce no space between `COUNT` and `(` for
     `SHOW COUNT(*) ERRORS`.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected diagnostics syntax.

4. Runtime execution
   - Add statement execution paths through the existing result builder.
   - Return `SHOW ERRORS` columns `Level`, `Code`, and `Message`.
   - Return `SHOW COUNT(*) ERRORS` column `@@session.error_count`.
   - Render only the previous error condition as an `Error` row.
   - Ignore previous warning records for display and count while preserving
     the snapshot for later `SHOW WARNINGS`.
   - Implement unsigned decimal `LIMIT` display slicing through `UINT64_MAX`.
   - Preserve the previous snapshot after successful `SHOW ERRORS` and
     `SHOW COUNT(*) ERRORS`.
   - Avoid SQLite SQL, catalog reads, catalog writes, and storage mutations.

5. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover empty diagnostics, warning-only filtering, error display, count
     display, diagnostic chaining, ordinary-statement clearing, parse-error
     display, limit slicing, warning/result row counts, no catalog/storage
     mutation, file preamble preservation, independent handles, and
     unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

6. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/diagnostics/process-list and
     show-warnings diagnostics entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence,
     diagnostics lifecycle correctness, snapshot ownership, result shape
     accuracy, count and limit semantics, file-format safety, VFS
     preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- `GET DIAGNOSTICS`, diagnostics stack behavior, condition handlers,
  `SIGNAL`, and `RESIGNAL`.
- `@@error_count`, notes, `max_error_count`, `sql_notes`, stored-but-not-counted
  or counted-but-not-stored condition behavior.
- New DML warning or error generation, protocol-level warning APIs, or general
  system-variable support.
- `SHOW ERRORS WHERE`, `LIKE`, `ORDER BY`, aliases, expressions, signed limit
  literals, non-integer limit literals, and arbitrary SELECT-like clauses.
- SQLite metadata reads, catalog mutations, storage mutations, and SQLite fork
  patches.

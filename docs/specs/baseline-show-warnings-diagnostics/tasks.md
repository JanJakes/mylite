# Baseline SHOW Warnings Diagnostics Tasks

## Goal

Add the next narrow diagnostics introspection slice:
`SHOW WARNINGS`, `SHOW WARNINGS LIMIT ...`, and
`SHOW COUNT(*) WARNINGS` over MyLite's previous statement diagnostics.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-warnings-diagnostics/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, diagnostics snapshot lifecycle, result shapes,
     `LIMIT` slicing, row-count and warning-count behavior, physical SQLite
     policy, and unsupported behavior.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md`
     only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally deferred wider forms.
   - Verify result headers, process-list warning rows, count output,
     diagnostic chaining, ordinary-statement clearing, limit forms, syntax
     errors, and parse-error exposure.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW WARNINGS [LIMIT ...]` and
     `SHOW COUNT(*) WARNINGS`.
   - Add AST node kinds for the diagnostics statements.
   - Map `WARNINGS` in the parser adapter while preserving lexer behavior.
   - Enforce no space between `COUNT` and `(` for
     `SHOW COUNT(*) WARNINGS`.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected diagnostics syntax.

4. Diagnostics lifecycle
   - Add connection-owned previous diagnostics snapshot storage.
   - Initialize and deinitialize the snapshot with the connection handle.
   - Replace the snapshot after successful nondiagnostic statements and after
     failed statements.
   - Preserve the snapshot after successful `SHOW WARNINGS` and
     `SHOW COUNT(*) WARNINGS`.
   - Ensure successful diagnostic statements leave live public diagnostics OK.

5. Runtime execution
   - Add statement execution paths through the existing result builder.
   - Return `SHOW WARNINGS` columns `Level`, `Code`, and `Message`.
   - Return `SHOW COUNT(*) WARNINGS` column `@@session.warning_count`.
   - Render previous error conditions as `Error` rows and previous warning
     records as `Warning` rows.
   - Implement unsigned decimal `LIMIT` display slicing through `UINT64_MAX`.
   - Avoid SQLite SQL, catalog reads, catalog writes, and storage mutations.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover empty diagnostics, process-list warning display, count display,
     diagnostic chaining, ordinary-statement clearing, parse-error display,
     limit slicing, warning/result row counts, no catalog/storage mutation,
     file preamble preservation, independent handles, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/diagnostics/process-list
     lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence,
     diagnostics lifecycle correctness, snapshot ownership, result shape
     accuracy, count and limit semantics, file-format safety, VFS
     preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- `SHOW ERRORS`, `SHOW COUNT(*) ERRORS`, `GET DIAGNOSTICS`, diagnostics stack
  behavior, condition handlers, `SIGNAL`, and `RESIGNAL`.
- Notes, `max_error_count`, `sql_notes`, stored-but-not-counted or
  counted-but-not-stored condition behavior.
- New DML warning generation, protocol-level warning APIs, or general
  `@@warning_count` system-variable support.
- `SHOW WARNINGS WHERE`, `LIKE`, `ORDER BY`, aliases, expressions, signed
  limit literals, non-integer limit literals, and arbitrary SELECT-like
  clauses.
- SQLite metadata reads, catalog mutations, storage mutations, and SQLite fork
  patches.

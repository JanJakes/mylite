# Baseline Diagnostics Count Variables Tasks

## Goal

Add a narrow system-variable scalar-select slice for `@@warning_count` and
`@@error_count` over MyLite's previous statement diagnostics snapshot.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-diagnostics-count-variables/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, variable resolution, diagnostics snapshot
     lifecycle, result labels, error behavior, physical SQLite policy, and
     unsupported behavior.
   - Update `COMPATIBILITY.md`,
     `docs/compatibility/runtime-system-variables.md`,
     `docs/compatibility/error-warning-result-semantics.md`, and
     `docs/compatibility/sql-show-statements.md` only for the exact partial
     subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify unqualified, session-qualified, local-qualified,
     case-insensitive, parenthesized, and quoted-component count variables.
   - Verify warning-only diagnostics, parse-error diagnostics, diagnostic
     statement preservation, scalar-select clearing, result headers, global
     scope errors, and unknown variable errors.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Map lexer `SYSTEM_VARIABLE` tokens into the Lemon parser.
   - Add a system-variable AST expression node.
   - Extend expression grammar with the independently specified
     `SYSTEM_VARIABLE` atom.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported scalar variable syntax and rejected wider
     forms.

4. Runtime execution
   - Extend the existing session scalar `SELECT` path to admit recognized
     diagnostics count variables.
   - Normalize variable tokens case-insensitively, supporting unquoted and
     quoted final variable-name components.
   - Return warning and error counts from the previous diagnostics snapshot.
   - Preserve existing scalar result label behavior.
   - Report MySQL-compatible diagnostics for global-scope and unknown
     variable reads.
   - Rely on normal nondiagnostic statement completion to clear the previous
     diagnostics snapshot after a successful scalar variable select.
   - Avoid SQLite SQL, catalog reads, catalog writes, and storage mutations.

5. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover empty diagnostics, warning-only diagnostics, parse-error
     diagnostics, count values, result labels, `ROW_COUNT()`, scalar clearing,
     diagnostic `SHOW` preservation, independent handles, file preamble
     preservation, generation invariants, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

6. Build integration
   - Add any new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser, diagnostics, show-warnings,
     and show-errors entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for public ABI stability, independently authored
     grammar/spec text, MySQL 8.4.9 evidence, diagnostics lifecycle
     correctness, count semantics, result label accuracy, file-format safety,
     VFS preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- General system-variable support, `SHOW VARIABLES`, `SET`, persisted
  variables, privilege checks, and global values.
- Variables other than `warning_count` and `error_count`.
- General expression evaluation involving variables.
- Notes, `max_error_count`, `sql_notes`, counted-but-not-stored diagnostics,
  diagnostics stacks, `GET DIAGNOSTICS`, `SIGNAL`, and `RESIGNAL`.
- SQLite metadata reads, catalog mutations, storage mutations, and SQLite fork
  patches.

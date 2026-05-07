# Baseline Current Database Function Tasks

## Goal

Add the next narrow runtime identity slice: `DATABASE()` and `SCHEMA()` in
one-row scalar `SELECT` statements, backed by MyLite connection-local selected
schema state.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-current-database-function/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, AST nodes, result behavior, selected-schema semantics,
     unsupported forms, and diagnostics.
   - Update `COMPATIBILITY.md` and `docs/compatibility/functions-system.md`
     only after implementation, and only for the exact supported subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `DATABASE()` / `SCHEMA()` behavior and intentionally rejected forms.
   - Verify result values, result column names, warning count, selected-schema
     side effects, and syntax errors.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for compatibility
     expectations.

3. Parser and AST
   - Add explicit AST node kinds for zero-argument `DATABASE()` and `SCHEMA()`.
   - Extend the Lemon expression grammar from the feature spec.
   - Add parser builders that preserve source spans through the closing
     parenthesis so default result column names can use source text.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected syntax.

4. Runtime execution
   - Recognize scalar current-database selects with no table source or
     `FROM DUAL`.
   - Evaluate only `DATABASE()`, `SCHEMA()`, and parenthesized wrappers around
     those functions.
   - Return one row with the selected schema text or SQL `NULL`.
   - Use source-expression text as the result column name.
   - Preserve existing result conventions for affected rows and warning count.
   - Keep catalog rows, descriptor caches, catalog generation, physical SQLite
     schema, and file storage unchanged.

5. Tests
   - Add a fast C runtime test under `packages/libmylite/tests/` and register
     it with a dotted CTest name.
   - Cover no selected schema, selected schema, `SCHEMA()` synonym behavior,
     `FROM DUAL`, column names, drop-clears-current-schema behavior,
     close/reopen, same-file independent handles, unsupported syntax, and
     unsupported wider scalar-select shapes.
   - Keep tests deterministic and avoid a new test framework.

6. Build integration
   - Add any new runtime tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry plus existing parser/schema lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for public ABI stability, independently authored
     grammar/spec text, MySQL 8.4.9 evidence, runtime-state ownership,
     result/null behavior, storage safety, scope control, compatibility docs,
     and test relevance.

## Out Of Scope

- General function calls, aliases, expression metadata, table-backed function
  evaluation, `LIMIT`, general scalar expressions, other system functions,
  stored routine semantics, SQLite function registration, arbitrary SQLite
  pass-through, and SQLite fork patches.

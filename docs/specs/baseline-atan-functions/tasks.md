# Baseline ATAN Functions Tasks

## Design And Evidence

- [x] Read the relevant architecture, compatibility, parser, scalar runtime,
      and SQLite integration context.
- [x] Verify the supported `ATAN()` and `ATAN2()` behavior against MySQL 8.4.9.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation coverage for the user-visible behavior
      introduced by this phase.

## Parser And AST

- [x] Add `ATAN` and `ATAN2` lexer and parser keyword handling without making
      bare names stop behaving as identifier lookups.
- [x] Add function and argument-count-error AST nodes for both functions.
- [x] Add Lemon grammar for one-argument calls, two-argument calls, and
      wrong-arity native diagnostics.
- [x] Add parser tests for valid, wrong-arity, parenthesized, aliased, and bare
      identifier forms.

## Runtime

- [x] Admit only top-level no-source, `FROM DUAL`, and `DO` scalar expressions.
- [x] Evaluate direct decimal integer, boolean, `NULL`, existing signed-64
      scalar arithmetic, and existing unsigned-64 scalar bitwise operands.
- [x] Preserve `NULL` propagation, signed value conversion, and two-argument
      first-argument `NULL` short-circuit behavior.
- [x] Preserve existing child arithmetic overflow and division-by-zero warning
      behavior.
- [x] Format non-`NULL` results through the existing scalar double text path
      matching the MySQL 8.4.9 expectations for the admitted subset.
- [x] Preserve existing non-row result behavior for successful `DO`.
- [x] Reject table-backed, nested, and broader operand forms deterministically.

## Tests

- [x] Add `runtime_atan_functions` C tests under `packages/libmylite/tests/`.
- [x] Register the test binary and dotted CTest entry in
      `packages/libmylite/CMakeLists.txt`.
- [x] Cover one-argument and two-argument result values, labels, aliases,
      diagnostics, warnings, row counts, file-backed preamble safety,
      catalog/schema-generation immutability, and independent handles.
- [x] Run
      `./packages/libmylite/tests/mysql_baseline_atan_functions_expectations.sh`.
- [x] Run focused parser/runtime CTest entries.
- [x] Run `cmake --workflow --preset check`.

## Documentation And Review

- [x] Update `COMPATIBILITY.md` and compatibility detail docs for the exact
      supported subset.
- [x] Review public ABI stability, expression-scope control, MySQL evidence,
      diagnostics, warning staging, formatting, file-format safety, and
      compatibility wording.
- [x] Commit the implementation atomically and push `origin main`.

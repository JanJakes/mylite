# Baseline SQRT Function Tasks

## Design And Evidence

- [x] Read the relevant architecture, compatibility, parser, scalar runtime, and
      SQLite integration context.
- [x] Verify the supported `SQRT()` behavior against MySQL 8.4.9.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation coverage for the user-visible behavior
      introduced by this phase.

## Parser And AST

- [x] Add `SQRT` lexer and parser keyword handling without making bare `SQRT`
      stop behaving as an identifier lookup.
- [x] Add `MYLITE_SQL_AST_SQRT_FUNCTION` and
      `MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR` nodes.
- [x] Add Lemon grammar for one-argument `SQRT()` and wrong-arity native
      diagnostics.
- [x] Add parser tests for valid, wrong-arity, parenthesized, aliased, and bare
      identifier forms.

## Runtime

- [x] Admit only top-level no-source, `FROM DUAL`, and `DO` `SQRT()` scalar
      expressions.
- [x] Evaluate direct decimal integer, boolean, `NULL`, existing signed-64
      scalar arithmetic, and existing unsigned-64 scalar bitwise operands.
- [x] Return `NULL` for `NULL` and negative operands without adding warnings.
- [x] Preserve existing child arithmetic overflow and division-by-zero warning
      behavior.
- [x] Format non-`NULL` results as the shortest round-tripping double text
      matching the MySQL 8.4.9 expectations for the admitted subset.
- [x] Preserve existing non-row result behavior for successful `DO SQRT(...)`.
- [x] Reject table-backed, nested, and broader operand forms deterministically.

## Tests

- [x] Add `runtime_sqrt_function` C tests under `packages/libmylite/tests/`.
- [x] Register the test binary and dotted CTest entry in
      `packages/libmylite/CMakeLists.txt`.
- [x] Cover result values, labels, aliases, diagnostics, warnings, row counts,
      file-backed preamble safety, catalog/schema-generation immutability, and
      independent handles.
- [x] Run `./packages/libmylite/tests/mysql_baseline_sqrt_function_expectations.sh`.
- [x] Run focused parser/runtime CTest entries.
- [x] Run `cmake --workflow --preset check`.

## Documentation And Review

- [x] Update `COMPATIBILITY.md` and compatibility detail docs for the exact
      supported subset.
- [x] Review public ABI stability, expression-scope control, MySQL evidence,
      diagnostics, warning staging, formatting, file-format safety, and
      compatibility wording.
- [x] Commit the implementation atomically and push `origin main`.

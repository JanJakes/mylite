# Baseline SQRT Function Tasks

## Design And Evidence

- [x] Read the relevant architecture, compatibility, parser, scalar runtime, and
      SQLite integration context.
- [x] Verify the supported `SQRT()` behavior against MySQL 8.4.9.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation coverage for the user-visible behavior
      introduced by this phase.

## Parser And AST

- [ ] Add `SQRT` lexer and parser keyword handling without making bare `SQRT`
      stop behaving as an identifier lookup.
- [ ] Add `MYLITE_SQL_AST_SQRT_FUNCTION` and
      `MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR` nodes.
- [ ] Add Lemon grammar for one-argument `SQRT()` and wrong-arity native
      diagnostics.
- [ ] Add parser tests for valid, wrong-arity, parenthesized, aliased, and bare
      identifier forms.

## Runtime

- [ ] Admit only top-level no-source, `FROM DUAL`, and `DO` `SQRT()` scalar
      expressions.
- [ ] Evaluate direct decimal integer, boolean, `NULL`, existing signed-64
      scalar arithmetic, and existing unsigned-64 scalar bitwise operands.
- [ ] Return `NULL` for `NULL` and negative operands without adding warnings.
- [ ] Preserve existing child arithmetic overflow and division-by-zero warning
      behavior.
- [ ] Format non-`NULL` results as the shortest round-tripping double text
      matching the MySQL 8.4.9 expectations for the admitted subset.
- [ ] Preserve existing non-row result behavior for successful `DO SQRT(...)`.
- [ ] Reject table-backed, nested, and broader operand forms deterministically.

## Tests

- [ ] Add `runtime_sqrt_function` C tests under `packages/libmylite/tests/`.
- [ ] Register the test binary and dotted CTest entry in
      `packages/libmylite/CMakeLists.txt`.
- [ ] Cover result values, labels, aliases, diagnostics, warnings, row counts,
      file-backed preamble safety, catalog/schema-generation immutability, and
      independent handles.
- [ ] Run `./packages/libmylite/tests/mysql_baseline_sqrt_function_expectations.sh`.
- [ ] Run focused parser/runtime CTest entries.
- [ ] Run `cmake --workflow --preset check`.

## Documentation And Review

- [ ] Update `COMPATIBILITY.md` and compatibility detail docs for the exact
      supported subset.
- [ ] Review public ABI stability, expression-scope control, MySQL evidence,
      diagnostics, warning staging, formatting, file-format safety, and
      compatibility wording.
- [ ] Commit the implementation atomically and push `origin main`.

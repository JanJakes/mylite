# Baseline Base64 Functions Tasks

- [x] Verify `TO_BASE64()` and `FROM_BASE64()` behavior against MySQL 8.4.9 for
  core values, invalid input, whitespace handling, line wrapping, arity errors,
  row-backed columns, and diagnostics.
- [x] Specify supported syntax, runtime semantics, diagnostics, ownership
  boundaries, and tests in `specs.md`.
- [x] Add lexer/parser/AST support for `TO_BASE64()` and `FROM_BASE64()` with
  native-function arity diagnostics.
- [x] Add MyLite-owned Base64 encode/decode helpers and registered SQLite scalar
  callbacks for row-scalar execution.
- [x] Extend scalar and row-scalar planners/executors for the admitted argument
  subset without materializing source rows.
- [x] Add C runtime/parser tests and a MySQL 8.4.9 expectation script.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/functions-string.md`
  with limited support wording.
- [x] Run focused tests, MySQL expectation comparison, and
  `cmake --workflow --preset check`.
- [x] Review, commit, and push the completed feature.

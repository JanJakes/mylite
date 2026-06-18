# Baseline String Bitmask Functions Tasks

- [x] Research official MySQL 8.4 string-function documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for `EXPORT_SET()` and `MAKE_SET()`.
- [x] Specify the supported grammar, runtime semantics, diagnostics, ownership
  boundaries, and unsupported forms.
- [x] Extend lexer/parser/AST support for the admitted function forms.
- [x] Add MyLite-owned scalar and row-backed runtime implementation.
- [x] Register private SQLite helper functions for row-backed projection.
- [x] Extend direct row-backed predicate and non-grouped order-key contexts.
- [x] Add parser, runtime, diagnostics, row-backed, persistence, and MySQL
  expectation tests.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/functions-string.md`.
- [x] Run targeted MySQL expectation, parser/runtime tests, and full check
  workflow.
- [x] Review final diff, amend issues, commit, and push `main`.

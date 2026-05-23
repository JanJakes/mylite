# Baseline STR_TO_DATE Function Tasks

- [x] Verify `STR_TO_DATE()` behavior against MySQL 8.4.9 for core date,
  datetime, time, trailing-input warnings, invalid-input warnings, zero-date
  SQL modes, arity errors, row-backed columns, and diagnostics.
- [x] Specify supported syntax, runtime semantics, diagnostics, ownership
  boundaries, and tests in `specs.md`.
- [x] Add lexer/parser/AST support for `STR_TO_DATE()` with native-function
  arity diagnostics.
- [x] Add MyLite-owned `STR_TO_DATE()` parsing and registered SQLite scalar
  callback for row-scalar execution.
- [x] Extend scalar and row-scalar planners/executors for the admitted argument
  subset without materializing source rows.
- [x] Add C runtime/parser tests and a MySQL 8.4.9 expectation script.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/functions-temporal.md`
  with limited support wording.
- [x] Run focused tests, MySQL expectation comparison, and
  `cmake --workflow --preset check`.
- [x] Review, commit, and push the completed feature.

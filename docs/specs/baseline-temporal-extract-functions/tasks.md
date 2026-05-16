# Baseline Temporal Extract Functions Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing temporal
  scalar/row-scalar expression specs.
- [x] Verify official MySQL 8.4 documentation for `DATE()`, `YEAR()`,
  `MONTH()`, `DAY()` / `DAYOFMONTH()`, `HOUR()`, `MINUTE()`, and `SECOND()`.
- [x] Probe MySQL 8.4.9 runtime behavior for canonical date, datetime, time,
  zero-date, partial-zero date, `NULL`, invalid values, labels, whitespace,
  warnings, row count, and syntax or arity errors.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Extend lexer/parser/AST support for the temporal extractor batch.
- [x] Implement no-source, `DUAL`, and `DO` scalar execution.
- [x] Implement descriptor-backed row-scalar projection execution using a
  MyLite-registered SQLite scalar function.
- [x] Add runtime and parser tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [x] Register any new runtime source and test binary in
  `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.

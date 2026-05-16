# Baseline Trim String Functions Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing scalar/row-scalar
  expression specs.
- [x] Verify official MySQL 8.4 documentation for `LTRIM()`, `RTRIM()`, and
  `TRIM()`.
- [x] Probe MySQL 8.4.9 runtime behavior for default-space trimming, explicit
  remove strings, `NULL`, numeric/boolean conversion, multibyte values, binary
  strings, labels, warnings, row count, and syntax or arity errors.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Extend lexer/parser/AST support for the trim function family.
- [x] Implement no-source, `DUAL`, and `DO` scalar execution.
- [x] Implement descriptor-backed row-scalar projection execution using
  MyLite-registered SQLite scalar functions.
- [x] Add runtime and parser tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [x] Register any new runtime source and test binary in
  `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.

# Baseline String Case Functions Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing scalar/row-scalar
  expression specs.
- [x] Verify official MySQL 8.4 documentation for `LOWER()`, `LCASE()`,
  `UPPER()`, and `UCASE()`.
- [x] Probe MySQL 8.4.9 runtime behavior for ASCII conversion, `NULL`,
  numeric/boolean conversion, non-ASCII conversion, binary strings, labels,
  warnings, row count, and wrong arity.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Write independently authored feature specification with MyLite grammar
  snippets, ownership boundaries, diagnostics, generated SQL shape, and known
  exclusions.
- [x] Extend lexer/parser/AST support for the string case function family.
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

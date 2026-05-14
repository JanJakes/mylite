# Baseline String Length Functions Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing scalar/row-scalar
  expression specs.
- [x] Verify official MySQL 8.4 documentation for `LENGTH()`,
  `OCTET_LENGTH()`, `BIT_LENGTH()`, `CHAR_LENGTH()`, and
  `CHARACTER_LENGTH()`.
- [x] Probe MySQL 8.4.9 runtime behavior for byte length, character length,
  bit length, `NULL`, numeric/boolean conversion, binary strings, `BIT`
  columns, labels, warnings, row count, and wrong arity.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Write independently authored feature specification with MyLite grammar
  snippets, ownership boundaries, diagnostics, generated SQL shape, and known
  exclusions.
- [ ] Extend lexer/parser/AST support for the string length function family.
- [ ] Implement no-source, `DUAL`, and `DO` scalar execution.
- [ ] Implement descriptor-backed row-scalar projection execution using SQLite
  expression pushdown.
- [ ] Add runtime and parser tests under `packages/libmylite/tests/`.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused parser/runtime/MySQL expectation verification.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend findings, commit, and push to remote
  `main`.

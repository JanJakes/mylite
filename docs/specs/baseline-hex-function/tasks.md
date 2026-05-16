# Baseline HEX Function Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing scalar/row-scalar
  expression specs.
- [x] Verify official MySQL 8.4 documentation for `HEX()`.
- [x] Probe MySQL 8.4.9 runtime behavior for one-argument parsing, wrong arity,
  `NULL`, string bytes, UTF-8 bytes, embedded NULs, binary hex literals, signed
  integer and boolean numeric behavior, labels, warnings, row count, and
  deferred decimal/bit/predicate forms.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Write independently authored feature specification with MyLite grammar
  snippets, ownership boundaries, diagnostics, generated SQL shape, and known
  exclusions.
- [ ] Extend lexer/parser/AST support for exact one-argument `HEX()`.
- [ ] Implement no-source, `DUAL`, and `DO` scalar execution.
- [ ] Implement descriptor-backed row-scalar projection execution using
  descriptor-built SQLite expressions.
- [ ] Add runtime and parser tests under `packages/libmylite/tests/`.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [ ] Register any new runtime test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused parser/runtime/MySQL expectation verification.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend findings, commit, and push to remote
  `main`.

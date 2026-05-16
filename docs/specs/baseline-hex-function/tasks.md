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
- [x] Extend lexer/parser/AST support for exact one-argument `HEX()`.
- [x] Implement no-source, `DUAL`, and `DO` scalar execution.
- [x] Implement descriptor-backed row-scalar projection execution using
  descriptor-built SQLite expressions.
- [x] Add runtime and parser tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [x] Register any new runtime test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.

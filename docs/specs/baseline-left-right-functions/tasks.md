# Baseline LEFT/RIGHT Functions Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing scalar/row-scalar
  expression specs.
- [x] Verify official MySQL 8.4 documentation for `LEFT()` and `RIGHT()`.
- [x] Probe MySQL 8.4.9 runtime behavior for two-argument parsing, wrong
  arity, `NULL`, zero and negative lengths, signed lengths, utf8mb4 character
  slicing, numeric/boolean string conversion, labels, warnings, row count, and
  deferred binary/predicate forms.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Write independently authored feature specification with MyLite grammar
  snippets, ownership boundaries, diagnostics, generated SQL shape, and known
  exclusions.
- [x] Extend lexer/parser/AST support for exact two-argument `LEFT()` and
  `RIGHT()`.
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

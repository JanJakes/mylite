# Baseline CHARSET and COLLATION Functions Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, charset/collation, result
  metadata, and row-scalar function specs.
- [x] Verify official MySQL 8.4 documentation for character set and collation
  function result behavior.
- [x] Probe MySQL 8.4.9 runtime behavior for literals, binary casts/converts,
  `NULL`, numeric values, session defaults, `DATABASE()`, descriptor-backed
  columns, labels, warnings, row count, and wrong arity.
- [x] Write independently authored feature specification with MyLite grammar
  snippets, ownership boundaries, diagnostics, generated SQL shape, and known
  exclusions.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Extend parser/AST support for exact one-argument `CHARSET()` and
  `COLLATION()`.
- [x] Implement no-source, `DUAL`, and `DO` scalar execution.
- [x] Implement descriptor-backed row-scalar projection execution using
  descriptor metadata and bound constants.
- [x] Add runtime and parser tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [x] Register any new runtime test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.

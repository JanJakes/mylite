# Baseline EXTRACT Function Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing temporal extractor
  specs.
- [x] Verify official MySQL 8.4 documentation for `EXTRACT()`.
- [x] Probe MySQL 8.4.9 runtime behavior for supported units, negative time
  values, composite units, labels, warnings, row count, and syntax errors.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Extend lexer/parser/AST support for `EXTRACT(unit FROM expr)`.
- [x] Implement no-source, `DUAL`, and `DO` scalar execution.
- [x] Implement descriptor-backed row-scalar projection execution.
- [x] Add runtime and parser tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [x] Reuse the existing temporal extract runtime test binary; no new CTest
  registration needed.
- [x] Run focused parser/runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote `main`.

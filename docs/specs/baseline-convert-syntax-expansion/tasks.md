# Baseline CONVERT Syntax Expansion Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, and existing `CAST()` /
  `CONVERT(... USING BINARY)` specs.
- [x] Verify official MySQL 8.4 documentation for `CONVERT(expr, type)` and
  `CONVERT(expr USING transcoding_name)`.
- [x] Probe MySQL 8.4.9 runtime behavior for supported binary-type and utf8mb4
  character-set forms, labels, `NULL`, numeric/boolean operands, `DO`, warning
  count, and deferred broader targets.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Write independently authored feature specification with MyLite grammar
  snippets, ownership boundaries, diagnostics, and known exclusions.
- [x] Extend parser/AST support for `CONVERT(value, BINARY)` and
  `CONVERT(value USING utf8mb4)`.
- [x] Implement no-source, `DUAL`, and `DO` scalar execution.
- [x] Add or extend parser/runtime tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [x] Run focused parser/runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.

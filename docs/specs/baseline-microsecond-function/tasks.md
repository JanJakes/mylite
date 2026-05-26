# Baseline MICROSECOND Function Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, SQLite integration, and
  existing temporal extractor specs.
- [x] Verify official MySQL 8.4 documentation for `MICROSECOND()` and
  fractional seconds.
- [x] Probe MySQL 8.4.9 runtime behavior for fractional strings, rounding,
  date-only strings, invalid strings, `NULL`, row-backed descriptors, warnings,
  and unsupported broader coercions.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Extend parser/AST support for `MICROSECOND(expr)`.
- [x] Implement no-source, `DUAL`, and `DO` scalar execution.
- [x] Implement descriptor-backed row-scalar projection execution through the
  existing MyLite temporal extraction SQLite scalar function.
- [x] Add runtime and parser tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and temporal-function compatibility docs with
  limited support wording.
- [x] Run focused parser/runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.

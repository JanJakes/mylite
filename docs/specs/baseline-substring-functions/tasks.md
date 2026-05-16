# Baseline SUBSTRING/SUBSTR/MID Functions Tasks

- [x] Read MyLite architecture, engineering standards, compatibility docs, and
  existing `LEFT()` / `RIGHT()` string-slice implementation.
- [x] Research official MySQL 8.4 string function docs and verify MySQL 8.4.9
  runtime behavior for comma forms, `FROM` / `FOR` forms, synonyms, `NULL`,
  signed positions, zero position, length behavior, labels, and diagnostics.
- [x] Specify the narrow parser/runtime/SQLite ownership boundary and
  unsupported expression surface.
- [x] Add MySQL-runtime expectation script for this feature.
- [ ] Extend lexer/parser/AST support for `SUBSTRING`, `SUBSTR`, and `MID`.
- [ ] Extend scalar and row-scalar runtime support using descriptor-driven
  planning and guarded SQLite `substr()` generation.
- [ ] Add parser and runtime C tests.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run focused MySQL expectation, parser/runtime tests, and full check.
- [ ] Review, amend gaps, commit, and push.

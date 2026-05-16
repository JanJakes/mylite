# Baseline Temporary Table LIKE Tasks

- [x] Create an independently authored feature spec with MySQL 8.4.9 runtime
  observations and official MySQL documentation references.
- [x] Add a MySQL 8.4.9 expectation script for the supported and deferred
  temporary `LIKE` behavior.
- [x] Extend parser/AST support for `CREATE TEMPORARY TABLE ... LIKE` and
  parenthesized `(LIKE source)` forms.
- [x] Reuse visible-table source resolution so persistent and temporary
  `CREATE TABLE ... LIKE` sources follow temporary-shadowing rules.
- [x] Route temporary targets through the existing temporary descriptor and
  physical table creation path.
- [x] Preserve source-before-target error precedence and `IF NOT EXISTS`
  semantics.
- [x] Add focused runtime and parser tests under `packages/libmylite/tests/`.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run MySQL expectations, focused CTests, `cmake --build --preset dev`,
  and `cmake --workflow --preset check`.
- [x] Review, commit, push, and continue with the next baseline slice.

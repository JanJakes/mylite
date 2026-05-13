# Baseline BIT Type Tasks

- [x] Verify MySQL 8.4.9 `BIT` type behavior and keep expectation script
      runnable.
- [x] Add parser/AST support for admitted `BIT` type syntax and bit defaults.
- [x] Add descriptor mapping, row-size accounting, and introspection rendering.
- [x] Add fixed-width big-endian BLOB storage conversion and binding.
- [x] Add `INSERT`, `REPLACE`, `UPDATE`, `INSERT IGNORE`, `ALTER ADD`,
      `CREATE LIKE`, descriptor-copy, and scalar-subquery assignment behavior.
- [x] Add `BIT` predicate and one-column order-key support where the current
      descriptor-backed statement slices already admit those shapes.
- [x] Add runtime/parser tests and CMake integration.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [x] Review, commit, push, and address review findings.

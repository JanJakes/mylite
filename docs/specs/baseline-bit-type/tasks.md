# Baseline BIT Type Tasks

- [x] Verify MySQL 8.4.9 `BIT` type behavior and keep expectation script
      runnable.
- [ ] Add parser/AST support for admitted `BIT` type syntax and bit defaults.
- [ ] Add descriptor mapping, row-size accounting, and introspection rendering.
- [ ] Add fixed-width big-endian BLOB storage conversion and binding.
- [ ] Add `INSERT`, `REPLACE`, `UPDATE`, `INSERT IGNORE`, `ALTER ADD`,
      `CREATE LIKE`, descriptor-copy, and scalar-subquery assignment behavior.
- [ ] Add `BIT` predicate and one-column order-key support where the current
      descriptor-backed statement slices already admit those shapes.
- [ ] Add runtime/parser tests and CMake integration.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run focused tests, MySQL expectation script, and full check workflow.
- [ ] Review, commit, push, and address review findings.

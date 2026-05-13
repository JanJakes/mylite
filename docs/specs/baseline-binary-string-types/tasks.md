# Baseline Binary String Types Tasks

- [ ] Verify MySQL 8.4.9 binary string behavior and keep expectation script
      runnable.
- [ ] Add byte-safe public result accessors and internal result cell lengths.
- [ ] Add parser/AST support for admitted binary string type syntax.
- [ ] Add descriptor mapping, row-size accounting, and introspection rendering.
- [ ] Add binary literal conversion, padding, length checks, and BLOB binding.
- [ ] Add binary SELECT readback through byte-safe result cells.
- [ ] Add INSERT, REPLACE, UPDATE, INSERT IGNORE, ALTER ADD, CREATE LIKE, and
      descriptor-copy behavior.
- [ ] Add runtime/parser tests and CMake integration.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run focused tests, MySQL expectation script, and full check workflow.
- [ ] Review, commit, push, and address review findings.

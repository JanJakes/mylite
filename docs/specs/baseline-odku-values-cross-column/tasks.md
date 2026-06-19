# Baseline ODKU VALUES cross-column references tasks

- [x] Verify MySQL 8.4.9 cross-column `VALUES(column)` duplicate-key behavior.
- [x] Relax duplicate-key `VALUES()` planning from same-target-only to
  copy-compatible target/source descriptors.
- [x] Add MyLite runtime coverage for `INSERT ... VALUES` and
  `INSERT ... SELECT` cross-column references.
- [x] Add MySQL expectation coverage for the new cases.
- [x] Update compatibility documentation.

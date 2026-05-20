# Baseline Insert Select Non-Strict Coercion Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for non-strict and `IGNORE` `INSERT ... SELECT` adjustment.
- [x] Specify the independently authored MyLite scope, ownership boundaries,
  diagnostics, generated SQLite handling, and test expectations.
- [x] Add MySQL 8.4.9 expectation probes for the supported adjusted
  `INSERT ... SELECT` subset and strict guardrails.
- [x] Implement descriptor-driven omitted-column, selected-`NULL`, and selected
  integer clipping adjustment for supported `INSERT ... SELECT` paths.
- [x] Add fast C runtime coverage for warning counts, warning rows, stored
  adjusted values, strict guardrails, persistence, and file preamble
  preservation.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests and MySQL expectation probes.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, scope control,
  diagnostics, warning order/counts, affected rows, zero-init cleanup, and
  compatibility docs.
- [ ] Commit and push the completed feature to `origin/main`.

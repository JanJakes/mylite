# Baseline Non-Strict String Truncation Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for strict, non-strict, and `INSERT IGNORE` `CHAR` / `VARCHAR`
  truncation.
- [x] Specify the independently authored MyLite scope, ownership boundaries,
  conversion semantics, diagnostics, generated SQLite handling, and test
  expectations.
- [x] Add MySQL 8.4.9 expectation probes for supported string truncation,
  trailing-space notes, strict guardrails, and affected-row behavior.
- [x] Implement descriptor-driven `CHAR` / `VARCHAR` truncation conversion for
  supported `INSERT`, `INSERT IGNORE`, `REPLACE`, and matched `UPDATE` paths.
- [x] Add fast C runtime coverage for strict errors, warning/note demotion,
  affected rows, warning rows, UTF-8 truncation, no-match/`LIMIT 0`, persistence,
  and file preamble preservation.
- [x] Update compatibility documentation for the exact supported subset and
  deferred string conversion gaps.
- [x] Run focused tests and MySQL expectation probes.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL equivalence,
  descriptor authority, diagnostics, warning counts, affected rows, zero-init
  cleanup, and compatibility docs.
- [x] Commit and push the completed feature to `origin/main`.

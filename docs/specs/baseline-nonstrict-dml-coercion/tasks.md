# Baseline Non-Strict DML Coercion Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for non-strict omitted/default DML and matched update
  `NULL`/`DEFAULT` coercion.
- [x] Specify the independently authored MyLite scope, ownership boundaries,
  diagnostics, generated SQLite handling, and test expectations.
- [x] Add MySQL 8.4.9 expectation probes for the supported non-strict DML
  subset and strict-mode guardrails.
- [x] Implement descriptor-driven non-strict implicit-default materialization
  for supported `INSERT`, `REPLACE`, and matched `UPDATE` paths.
- [x] Add fast C runtime coverage for strict errors, non-strict warnings,
  stored implicit values, no-match updates, `LIMIT 0`, persistence, and file
  preamble preservation.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests and MySQL expectation probes.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, scope control,
  diagnostics, warning counts, affected rows, zero-init cleanup, and
  compatibility docs.
- [x] Commit and push the completed feature to `origin/main`.

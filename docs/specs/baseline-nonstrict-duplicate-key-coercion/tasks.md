# Baseline Non-Strict Duplicate-Key Coercion Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for non-strict duplicate-key assignment conversion.
- [x] Specify the independently authored MyLite scope, ownership boundaries,
  diagnostics, generated SQLite handling, and test expectations.
- [x] Add MySQL 8.4.9 expectation probes for strict and non-strict duplicate
  `NULL`, no-default `DEFAULT`, and string truncation behavior.
- [x] Implement descriptor-driven duplicate-branch conversion adjustments.
- [x] Add fast C runtime coverage for adjusted values, strict guardrails,
  affected rows, warning counts, warning records, and persistence.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests and MySQL expectation probes.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, scope control,
  diagnostics, warning counts, affected rows, zero-init cleanup, and
  compatibility docs.
- [x] Commit and push the completed feature to `origin/main`.

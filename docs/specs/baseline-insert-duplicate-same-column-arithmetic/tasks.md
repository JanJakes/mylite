# Baseline INSERT Duplicate Same-Column Arithmetic Tasks

- [x] Confirm official MySQL 8.4 `INSERT ... ON DUPLICATE KEY UPDATE`
      documentation and MySQL 8.4.9 runtime behavior for duplicate
      same-column arithmetic assignments, no-op affected rows, `NULL`
      propagation, multi-row accumulation, and range failures.
- [x] Specify the independently authored MyLite feature scope, diagnostics,
      ownership boundaries, generated SQL shape, performance approach, and
      deferred cases.
- [x] Add MySQL expectation coverage for the admitted arithmetic subset and
      intentionally deferred expression shapes.
- [x] Extend ODKU parser support for unqualified same-column `+` / `-`
      unsigned integer literals.
- [x] Extend duplicate-update planning to recognize same-column arithmetic,
      resolve the source descriptor, and reject non-integer, key, and
      `AUTO_INCREMENT` targets deterministically.
- [x] Convert arithmetic duplicate assignments from the conflicting stored row,
      preserving `NULL`, range checks, and affected-row semantics.
- [x] Preserve existing duplicate-key conflict validation, value conversion,
      warnings, transaction, persistence, and file-format behavior.
- [x] Add runtime C coverage for success, no-op, `NULL`, multi-row,
      persistence, independent handles, range failures, and unsupported shapes.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused parser/runtime tests, the MySQL expectation script, and
      `cmake --workflow --preset check`.
- [x] Review with a subagent, amend if needed, commit, and push `main`.

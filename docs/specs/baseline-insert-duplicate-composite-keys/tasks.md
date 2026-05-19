# Baseline INSERT Duplicate Composite Keys Tasks

- [x] Confirm official MySQL 8.4 `INSERT ... ON DUPLICATE KEY UPDATE`
      documentation and MySQL 8.4.9 runtime behavior for composite primary-key
      and composite unique-key duplicate updates.
- [x] Add a MySQL expectation script for supported composite-key behavior and
      intentionally deferred wider behavior.
- [ ] Extend duplicate-update planning to allow exactly one enforced composite
      primary or unique key descriptor.
- [ ] Generate duplicate-row `UPDATE` predicates over every key part, reusing
      descriptor-built key expressions and bound tuple parameters.
- [ ] Preserve existing duplicate assignment conversion, affected-row,
      warning, auto-increment, and transaction behavior.
- [ ] Add runtime C coverage for composite primary keys, composite unique keys,
      `NULL` unique parts, warnings, diagnostics, atomicity, persistence,
      file-format safety, independent handles, and unsupported shapes.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run focused parser/runtime tests, the MySQL expectation script, and
      `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend if needed, commit, and push `main`.


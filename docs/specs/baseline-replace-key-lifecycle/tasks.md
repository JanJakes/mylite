# Baseline Replace Key Lifecycle Tasks

- [x] Read current replace, key, auto-increment, foreign-key, architecture, and
  SQLite integration context.
- [x] Verify MySQL 8.4.9 key-bearing `REPLACE` behavior for primary keys,
  unique keys, exact no-op replacements, multiple unique conflicts,
  auto-increment, nullable unique keys, string/prefix keys, and foreign keys.
- [x] Write the independent feature spec with ownership boundaries, physical
  SQLite handling, diagnostics, and MyLite grammar notes.
- [x] Add a MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Extend descriptor-driven `REPLACE ... VALUES` and `REPLACE ... SET`
  execution for key-bearing targets.
- [x] Keep key-bearing `REPLACE ... SELECT` deferred and tested as rejected.
- [x] Add focused C runtime tests and register any new test binary.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority,
  foreign-key correctness, affected rows, auto-increment behavior, persistence,
  file-format safety, cleanup, and scope control.
- [x] Commit, review with a subagent, amend if needed, and push `main`.

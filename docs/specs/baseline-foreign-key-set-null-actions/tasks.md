# Baseline Foreign Key SET NULL Actions Tasks

- [x] Research MySQL 8.4.9 `ON DELETE SET NULL` and `ON UPDATE SET NULL`
  syntax, metadata, parent-write effects, nullable tuple behavior, and
  `NOT NULL` diagnostics.
- [x] Write the independently authored feature spec.
- [x] Prepare a MySQL 8.4.9 expectation script for the new user-visible
  behavior.
- [x] Update compatibility documentation for the exact admitted subset.
- [x] Extend parser and AST support for `SET NULL` foreign-key actions.
- [x] Persist `SET NULL` descriptor rule text from create-time and alter-added
  FK planning.
- [x] Reject `SET NULL` when any child FK column is descriptor `NOT NULL`.
- [x] Implement descriptor-built direct `ON DELETE SET NULL` child updates.
- [x] Implement descriptor-built direct `ON UPDATE SET NULL` child updates for
  the current supported parent update action subset.
- [x] Add fast C parser/runtime coverage, including persistence and diagnostics.
- [x] Run MySQL expectation script, focused CTest entries, and full workflow.
- [x] Review the diff and amend any issues.
- [x] Commit atomically and push `main`.

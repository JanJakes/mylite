# Baseline Foreign Key SET NULL Actions Tasks

- [x] Research MySQL 8.4.9 `ON DELETE SET NULL` and `ON UPDATE SET NULL`
  syntax, metadata, parent-write effects, nullable tuple behavior, and
  `NOT NULL` diagnostics.
- [x] Write the independently authored feature spec.
- [x] Prepare a MySQL 8.4.9 expectation script for the new user-visible
  behavior.
- [x] Update compatibility documentation for the exact admitted subset.
- [ ] Extend parser and AST support for `SET NULL` foreign-key actions.
- [ ] Persist `SET NULL` descriptor rule text from create-time and alter-added
  FK planning.
- [ ] Reject `SET NULL` when any child FK column is descriptor `NOT NULL`.
- [ ] Implement descriptor-built direct `ON DELETE SET NULL` child updates.
- [ ] Implement descriptor-built direct `ON UPDATE SET NULL` child updates for
  the current supported parent update action subset.
- [ ] Add fast C parser/runtime coverage, including persistence and diagnostics.
- [ ] Run MySQL expectation script, focused CTest entries, and full workflow.
- [ ] Review the diff, amend any issues, commit atomically, and push `main`.

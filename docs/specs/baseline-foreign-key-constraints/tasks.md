# Baseline Foreign Key Constraints Tasks

- [x] Review existing primary-key, unique-index, nonunique-index,
  information-schema constraint, DML, table lifecycle, and catalog behavior.
- [x] Verify MySQL 8.4.9 behavior for the admitted FK DDL, metadata, and
  enforcement surface.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for supported behavior and
  deliberately deferred wider forms.
- [x] Extend parser/AST support for the narrow foreign-key grammar.
- [x] Add durable FK catalog descriptors and migration/init support.
- [x] Implement create-time and `ALTER TABLE ... ADD CONSTRAINT` planning.
- [x] Implement descriptor-driven `SHOW CREATE TABLE` and information-schema
  FK metadata.
- [x] Implement child-side and parent-side set-based enforcement for admitted
  DML paths, rejecting unsafe unsupported paths deterministically.
- [x] Add runtime/parser tests for FK DDL, metadata, enforcement, persistence,
  diagnostics, and file-format invariants.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff with a subagent, amend any findings, commit, and
  push to remote `main`.

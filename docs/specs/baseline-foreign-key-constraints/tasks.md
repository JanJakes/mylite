# Baseline Foreign Key Constraints Tasks

- [x] Review existing primary-key, unique-index, nonunique-index,
  information-schema constraint, DML, table lifecycle, and catalog behavior.
- [x] Verify MySQL 8.4.9 behavior for the admitted FK DDL, metadata, and
  enforcement surface.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for supported behavior and
  deliberately deferred wider forms.
- [ ] Extend parser/AST support for the narrow foreign-key grammar.
- [ ] Add durable FK catalog descriptors and migration/init support.
- [ ] Implement create-time and `ALTER TABLE ... ADD CONSTRAINT` planning.
- [ ] Implement descriptor-driven `SHOW CREATE TABLE` and information-schema
  FK metadata.
- [ ] Implement child-side and parent-side set-based enforcement for admitted
  DML paths, rejecting unsafe unsupported paths deterministically.
- [ ] Add runtime/parser tests for FK DDL, metadata, enforcement, persistence,
  diagnostics, and file-format invariants.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff with a subagent, amend any findings, commit, and
  push to remote `main`.

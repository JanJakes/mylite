# Baseline Foreign Key Symbol Actions Tasks

- [x] Review existing FK, composite FK, drop FK, index, update, delete, catalog,
  information-schema, and show-create behavior.
- [x] Verify MySQL 8.4.9 behavior for FK index symbols, action metadata,
  direct cascades, row counts, warning counts, and rendering.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the supported behavior and
  deliberately deferred wider forms.
- [ ] Extend parser/AST support for optional FK index symbols and action
  clauses.
- [ ] Store planned FK index symbols and `update_rule` / `delete_rule` values
  in create-time and alter-time FK plans.
- [ ] Preserve existing descriptor authority and generated child-index naming
  rules while adding optional FK index-symbol naming.
- [ ] Implement descriptor-driven `SHOW CREATE TABLE` rendering for `CASCADE`
  and `RESTRICT` actions.
- [ ] Implement direct set-based `ON DELETE CASCADE` for supported parent
  `DELETE` plans without changing public affected-row counts.
- [ ] Implement direct set-based `ON UPDATE CASCADE` for supported parent key
  update plans without changing public affected-row counts.
- [ ] Keep existing `RESTRICT` / `NO ACTION` rejection behavior for parent
  writes with matching child rows.
- [ ] Reject unsupported action values, duplicate action timings, recursive or
  self-referential cascade shapes, and unsafe FK-related DML deterministically.
- [ ] Add runtime/parser tests for DDL, metadata, cascades, diagnostics,
  persistence, independent handles, and file-format invariants.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff with a subagent, amend any findings, commit, and
  push to remote `main`.

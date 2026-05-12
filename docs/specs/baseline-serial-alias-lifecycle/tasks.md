# Baseline SERIAL Alias Lifecycle Tasks

- [x] Research official MySQL 8.4 documentation for `SERIAL`, `CREATE TABLE`,
  auto-increment columns, and descriptor metadata.
- [x] Verify MySQL 8.4.9 runtime behavior for `SERIAL`, secondary-key
  `AUTO_INCREMENT`, metadata, DML, and deferred forms.
- [x] Specify the narrow supported grammar, descriptor expansion, ownership
  boundaries, diagnostics, metadata, and non-goals.
- [x] Add MySQL-runtime expectation script for the supported and deferred
  behavior.
- [ ] Extend parser/AST support for `SERIAL` while preserving it as a
  nonreserved identifier.
- [ ] Expand `SERIAL` in create-table planning into `BIGINT UNSIGNED`,
  auto-increment metadata, and one generated unique key.
- [ ] Admit create-time secondary-key-backed `AUTO_INCREMENT` columns for the
  supported one-column key subset.
- [ ] Keep `ALTER TABLE ADD` / `MODIFY` / `CHANGE` `SERIAL` execution rejected
  with existing auto-increment unsupported diagnostics.
- [ ] Add fast C runtime tests for metadata, inserts, persistence, diagnostics,
  and file-format safety.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [ ] Run focused parser/runtime/MySQL expectation tests.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority, physical
  SQL safety, metadata accuracy, and scope control.

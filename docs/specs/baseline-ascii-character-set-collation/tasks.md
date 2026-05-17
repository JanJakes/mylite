# Baseline ascii Character Set and Collation Tasks

- [x] Verify MySQL 8.4.9 behavior for `ascii`, `ascii_general_ci`,
  `ascii_bin`, explicit DDL metadata, diagnostics, and deferred bare `ASCII`
  shorthand.
- [x] Write the independently authored feature spec and MyLite grammar notes.
- [x] Add the MySQL 8.4.9 expectation script.
- [ ] Add static `ascii` charset/collation rows to `SHOW` and
  `INFORMATION_SCHEMA`.
- [ ] Generalize schema/table/column charset and collation validation from the
  existing `utf8mb4`/`binary` branches to admitted supported descriptors.
- [ ] Preserve descriptor metadata through create, alter, clone, reopen, and
  independent handles.
- [ ] Add runtime coverage for metadata, diagnostics, persistence, and file
  safety.
- [ ] Update compatibility documentation with limited wording.
- [ ] Run the MySQL expectation script, focused CTests, and full check workflow.
- [ ] Review, commit, and push to `origin/main`.


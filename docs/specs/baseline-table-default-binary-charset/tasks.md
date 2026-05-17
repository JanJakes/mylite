# Baseline Table Default Binary Charset Tasks

- [x] Verify MySQL 8.4.9 behavior for table-default `binary` charset/collation
  inheritance, explicit column overrides, metadata, row storage, and mismatch
  diagnostics.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add MySQL expectation script for the admitted and deferred user-visible
  subset.
- [x] Admit targeted `CREATE TABLE` `binary` table-option grammar without
  broadening general option names.
- [x] Apply descriptor-owned inherited binary normalization for supported
  `CHAR`, `VARCHAR`, and `TEXT` family columns.
- [x] Preserve explicit column charset/collation overrides and existing binary
  column normalization.
- [x] Update descriptor-driven `SHOW CREATE TABLE` table-option rendering for
  `DEFAULT CHARSET=binary`.
- [x] Add parser/runtime coverage for metadata, DML persistence, diagnostics,
  temporary tables, LIKE cloning, reopen, independent handles, and file safety.
- [x] Update compatibility matrix and detail docs.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [x] Review, commit, and push to `origin/main`.

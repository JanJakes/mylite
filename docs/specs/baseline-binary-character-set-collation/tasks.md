# Baseline Binary Character Set and Collation Tasks

- [x] Verify MySQL 8.4.9 behavior for binary charset/collation metadata,
  explicit column conversion, mismatch diagnostics, and deferred table/default
  surfaces.
- [x] Write independently authored feature spec and MyLite grammar notes.
- [x] Add MySQL expectation script for the admitted and deferred user-visible
  subset.
- [x] Add `binary` rows to static charset/collation metadata and
  `INFORMATION_SCHEMA` row builders.
- [x] Normalize supported explicit column `CHARACTER SET binary` /
  `COLLATE binary` declarations to existing binary string descriptors.
- [x] Add runtime/parser coverage for metadata, DDL introspection, DML
  persistence, diagnostics, and file safety.
- [x] Update compatibility matrix and detail docs.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [x] Review, commit, and push to `origin/main`.

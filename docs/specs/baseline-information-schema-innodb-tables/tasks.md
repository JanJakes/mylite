# Baseline INFORMATION_SCHEMA InnoDB Tables Tasks

- [x] Read project guidance, compatibility docs, and relevant architecture
  standards.
- [x] Verify MySQL 8.4.9 `INFORMATION_SCHEMA.INNODB_TABLES` system metadata and
  representative row behavior.
- [x] Document the independently authored feature spec and compatibility
  boundaries.
- [x] Add a MySQL 8.4.9 expectation script for the supported surface.
- [x] Register `INNODB_TABLES` in the information-schema table directory and
  column metadata.
- [x] Emit descriptor-backed rows for persistent base tables.
- [x] Add focused C runtime coverage.
- [x] Update compatibility matrices.
- [x] Run focused tests, MySQL expectations, `git diff --check`, full
  `cmake --workflow --preset check`, review, commit, and push.

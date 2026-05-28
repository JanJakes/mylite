# Baseline INFORMATION_SCHEMA InnoDB Foreign-Key Tables Tasks

- [x] Verify MySQL 8.4.9 `INNODB_FOREIGN` and `INNODB_FOREIGN_COLS` table
  shape, column metadata, descriptor rows, action-bit mapping, warning count,
  and `ROW_COUNT()` behavior.
- [x] Record official MySQL 8.4 documentation sources and runtime evidence in
  `specs.md`.
- [x] Add MySQL expectation coverage for InnoDB foreign-key dictionary views.
- [x] Register both tables in the static information-schema runtime metadata
  with MySQL-shaped table and column rows.
- [x] Emit descriptor-backed `INNODB_FOREIGN` rows for supported persistent
  base-table foreign keys.
- [x] Emit descriptor-backed `INNODB_FOREIGN_COLS` rows for supported
  foreign-key columns and ordering.
- [x] Add focused C runtime coverage for row sets, counts, predicates, aliases,
  case-insensitive names, unqualified selected-schema reads, system table rows,
  column metadata rows, warning count, row count, descriptor drops, and
  file-backed reopen behavior.
- [x] Update `COMPATIBILITY.md` and
  `docs/compatibility/metadata-information-schema.md`.
- [x] Run MySQL expectation, focused CTest, `git diff --check`, and
  `cmake --workflow --preset check`.

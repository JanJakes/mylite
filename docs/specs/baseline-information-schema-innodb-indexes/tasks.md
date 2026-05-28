# Baseline INFORMATION_SCHEMA InnoDB Index Tables Tasks

- [x] Verify MySQL 8.4.9 `INNODB_INDEXES` and `INNODB_FIELDS` table shape,
  column metadata, index type codes, key-field positions, warning count, and
  `ROW_COUNT()` behavior.
- [x] Record official MySQL 8.4 documentation sources and runtime evidence in
  `specs.md`.
- [x] Add MySQL expectation coverage for InnoDB index dictionary views.
- [x] Register both tables in the static information-schema runtime metadata
  with MySQL-shaped table and column rows.
- [x] Emit descriptor-backed `INNODB_INDEXES` rows for supported persistent
  base-table indexes.
- [x] Emit clustered fallback metadata for first all-`NOT NULL` unique keys and
  synthetic `GEN_CLUST_INDEX` rows.
- [x] Emit descriptor-backed `INNODB_FIELDS` rows for supported persistent
  base-table index columns and ordering.
- [x] Add focused C runtime coverage for row sets, counts, predicates, aliases,
  case-insensitive names, unqualified selected-schema reads, system table rows,
  column metadata rows, warning count, row count, descriptor rename/drop, and
  file-backed reopen behavior.
- [x] Update `COMPATIBILITY.md` and
  `docs/compatibility/metadata-information-schema.md`.
- [x] Run MySQL expectation, focused CTest, `git diff --check`, and
  `cmake --workflow --preset check`.

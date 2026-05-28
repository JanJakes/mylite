# Baseline INFORMATION_SCHEMA InnoDB FT Index Tables Tasks

- [x] Verify MySQL 8.4.9 `INNODB_FT_INDEX_CACHE` and
  `INNODB_FT_INDEX_TABLE` table shape, metadata, default row counts, warning
  count, and `ROW_COUNT()` behavior.
- [x] Record official MySQL 8.4 documentation sources and runtime evidence in
  `specs.md`.
- [x] Add MySQL expectation coverage for both InnoDB full-text index
  information-schema tables.
- [x] Register both tables in the static information-schema runtime metadata
  with MySQL-shaped table and column rows.
- [x] Keep both runtime row sets empty until MyLite implements
  `innodb_ft_aux_table`-driven InnoDB full-text auxiliary rows.
- [x] Add focused C runtime coverage for wildcard reads, aliases, predicates,
  case-insensitive names, unqualified selected-schema reads, system table rows,
  column metadata rows, warning count, row count, and file-backed handles.
- [x] Update `COMPATIBILITY.md` and
  `docs/compatibility/metadata-information-schema.md`.
- [x] Run MySQL expectation, focused CTest, `git diff --check`, and
  `cmake --workflow --preset check`.

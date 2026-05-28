# Baseline INFORMATION_SCHEMA InnoDB CMPMEM Tables Tasks

- [x] Verify MySQL 8.4.9 `INNODB_CMPMEM` and `INNODB_CMPMEM_RESET` table
  shape, metadata, baseline page-size rows, warning count, and `ROW_COUNT()`
  behavior.
- [x] Record official MySQL 8.4 documentation sources and runtime evidence in
  `specs.md`.
- [x] Add MySQL expectation coverage for both InnoDB compressed-page
  buffer-pool information-schema tables.
- [x] Register both tables in the static information-schema runtime metadata
  with MySQL-shaped table and column rows.
- [x] Emit fixed five-row zero-counter data sets for both tables.
- [x] Keep `_RESET` reads stable and side-effect-free until MyLite implements
  compressed-page buffer-pool counters.
- [x] Add focused C runtime coverage for wildcard reads, row counts,
  predicates, aliases, case-insensitive names, unqualified selected-schema
  reads, system table rows, column metadata rows, warning count, row count,
  repeated `_RESET` reads, and file-backed handles.
- [x] Update `COMPATIBILITY.md` and
  `docs/compatibility/metadata-information-schema.md`.
- [x] Run MySQL expectation, focused CTest, `git diff --check`, and
  `cmake --workflow --preset check`.

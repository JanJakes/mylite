# Baseline Unique Prefix Index Lifecycle Tasks

- [x] Review existing unique-index, prefix-index, string-key, and duplicate
      enforcement architecture.
- [x] Verify MySQL 8.4.9 behavior for unique prefix DDL, metadata, duplicate
      DML, and diagnostics.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [x] Admit one-part unique prefix key definitions in create-time,
      alter-time, and standalone index planning.
- [x] Reuse descriptor prefix validation for unique `CHAR`, `VARCHAR`, and
      `TEXT` family key parts.
- [x] Compare and format prefix values correctly in insert, insert-ignore,
      update, and ODKU duplicate handling.
- [x] Preserve prefix metadata through descriptor-owned `SHOW`, limited
      `INFORMATION_SCHEMA`, `CREATE TABLE ... LIKE`, drop-index, and
      persistence paths.
- [x] Keep `COLUMN_KEY` classification MySQL-compatible for prefix unique
      indexes.
- [x] Add focused C runtime tests for success paths, diagnostics, DML,
      metadata, persistence, and file-format safety.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run the feature MySQL expectation script.
- [x] Run targeted CTest entries covering parser, unique indexes, prefix
      indexes, add/create/drop index DDL, DML duplicate handling, and metadata.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, push `main`, and run a
      subagent release-gate review.

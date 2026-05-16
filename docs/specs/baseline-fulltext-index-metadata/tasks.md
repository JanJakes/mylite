# Baseline FULLTEXT Index Metadata Tasks

- [x] Research official MySQL 8.4 docs for `FULLTEXT` index syntax, allowed
  columns, prefix handling, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`.
- [x] Probe MySQL 8.4.9 runtime behavior for accepted create-time forms,
  ignored prefixes, generated names, metadata, clone, drop/rename, and
  diagnostics.
- [x] Specify the narrow metadata-only feature boundary and physical SQLite
  ownership model.
- [x] Add MySQL-runtime expectation artifact for this feature.
- [x] Extend parser/AST support for create-time table-level `FULLTEXT` index
  definitions.
- [x] Add catalog support for `MYLITE_CATALOG_INDEX_KIND_FULLTEXT`, including a
  schema migration and fresh-catalog DDL update.
- [x] Plan and validate full-text descriptors from MyLite planned column
  descriptors.
- [x] Store full-text index descriptors and index-column rows without creating
  physical SQLite indexes.
- [x] Render full-text metadata through `SHOW CREATE TABLE`, `SHOW INDEX`,
  `SHOW COLUMNS`, and limited information schema.
- [x] Support descriptor-only drop/rename of existing full-text indexes.
- [x] Add runtime C tests for metadata, diagnostics, clone/reopen/drop/rename,
  and physical SQLite separation.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused MySQL expectation, parser/runtime/index tests, and full
  `cmake --workflow --preset check`.
- [x] Review, amend gaps, commit, and push.

# Baseline Add FULLTEXT Indexes Tasks

- [x] Research official MySQL 8.4 docs for `ALTER TABLE ... ADD FULLTEXT`,
  `CREATE FULLTEXT INDEX`, prefix handling, `SHOW INDEX`, warnings, and
  information-schema metadata.
- [x] Probe MySQL 8.4.9 runtime behavior for accepted forms, generated names,
  ignored prefixes, first-add warning 124, later zero-warning adds, metadata,
  and diagnostics.
- [x] Specify the narrow metadata-only feature boundary and physical SQLite
  ownership model.
- [x] Add MySQL-runtime expectation artifact for this feature.
- [x] Extend parser/AST support for `ALTER TABLE ... ADD FULLTEXT` and
  `CREATE FULLTEXT INDEX`.
- [x] Plan and validate added fulltext descriptors from MyLite catalog column
  descriptors.
- [x] Store fulltext descriptors and index-column rows without creating
  physical SQLite indexes or bumping SQLite schema generation.
- [x] Append the MySQL-compatible first-fulltext-add warning only after
  successful descriptor mutation.
- [x] Add parser and runtime C tests for success metadata, warnings,
  diagnostics, persistence, DML after add, clone/drop/rename interactions,
  independent handles, and physical SQLite separation.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused MySQL expectation, parser/runtime/index tests, and full
  `cmake --workflow --preset check`.
- [x] Review, amend gaps, commit, and push.

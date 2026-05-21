# Baseline Primary-Key Options Metadata Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  primary-key `USING`, `COMMENT`, `VISIBLE`, `INVISIBLE`, metadata rendering,
  warnings, and diagnostics.
- [x] Specify the limited grammar, descriptor ownership, catalog behavior,
  runtime semantics, metadata rendering, and unsupported surfaces.
- [x] Add MySQL-runtime expectation script covering successful metadata, hash
  fallback warnings, repeated options, and diagnostics.
- [x] Extend parser/AST support for primary-key index-option grammar.
- [x] Extend create-table and alter-add-primary-key planning/execution to
  resolve and persist primary-key options.
- [x] Render descriptor primary-key comments/options through
  `SHOW CREATE TABLE`, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`.
- [x] Add or extend fast C runtime coverage for success, diagnostics,
  persistence, cloning, independent handles, and file-format safety.
- [x] Update `COMPATIBILITY.md` and index/show/metadata compatibility docs with
  exact limited wording.
- [x] Run focused parser/runtime tests, the MySQL expectation script,
  `cmake --build --preset dev`, and `cmake --workflow --preset check`.
- [x] Review the final diff for catalog authority, MySQL behavior, warning
  counts, physical SQLite boundaries, cleanup on failure, scope control, docs,
  and test relevance.

# Baseline Index Options Metadata Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  index `USING`, `COMMENT`, `VISIBLE`, `INVISIBLE`, metadata rendering,
  warnings, and diagnostics.
- [x] Specify the limited grammar, descriptor ownership, catalog migration,
  runtime semantics, metadata rendering, and unsupported surfaces.
- [x] Add MySQL-runtime expectation script covering successful metadata,
  hash fallback warnings, visibility, fulltext comments, and diagnostics.
- [x] Extend parser/AST support for the admitted index-option grammar.
- [x] Extend catalog descriptors and migrations for index comments and explicit
  BTREE render state.
- [x] Extend create-table, alter-add-index, and create-index planning/execution
  to resolve and persist options.
- [x] Render descriptor comments/options through `SHOW CREATE TABLE`,
  `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`.
- [x] Add fast C runtime coverage for success, diagnostics, persistence,
  cloning, independent handles, and file-format safety.
- [x] Update `COMPATIBILITY.md` and index/show/metadata compatibility docs with
  exact limited wording.
- [x] Run focused parser/runtime tests, the MySQL expectation script,
  `cmake --build --preset dev`, and `cmake --workflow --preset check`.
- [x] Review the final diff for catalog authority, MySQL behavior, warning
  counts, physical SQLite boundaries, cleanup on failure, scope control, docs,
  and test relevance.

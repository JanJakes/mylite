# Baseline Spatial Index Metadata Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
      spatial types, spatial index DDL, metadata rendering, and diagnostics.
- [x] Specify the limited grammar, descriptor ownership, catalog/runtime
      semantics, physical SQLite boundary, metadata rendering, diagnostics, and
      unsupported surfaces.
- [x] Add MySQL-runtime expectation script for spatial type display, spatial
      index metadata, comments/visibility, DML `NULL` handling, and
      diagnostics.
- [x] Extend lexer/parser/AST support for spatial column types and admitted
      `SPATIAL` index forms.
- [x] Extend catalog descriptors and migrations for spatial column and index
      kinds.
- [x] Extend create-table, alter-add-index, standalone create-index, index
      rename/drop/visibility, and create-like planning/runtime paths.
- [x] Render spatial descriptors through `SHOW CREATE TABLE`, `SHOW COLUMNS`,
      `SHOW INDEX`, and limited `INFORMATION_SCHEMA.COLUMNS` / `STATISTICS`.
- [x] Add fast C runtime coverage for success paths, diagnostics, persistence,
      independent handles, and file-format safety.
- [x] Update `COMPATIBILITY.md` and compatibility detail docs with the exact
      supported subset.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev` and `cmake --workflow --preset check`.
- [x] Review the final diff for descriptor authority, MySQL behavior,
      metadata accuracy, physical SQLite boundaries, cleanup on failure,
      performance, scope control, docs, and test relevance.

# Baseline Table Storage And Statistics Options Tasks

- [x] Read architecture, compatibility, parser, catalog, runtime, metadata,
      temporary catalog, storage/VFS, and test context.
- [x] Verify MySQL 8.4.9 behavior for admitted options, duplicate/default
      handling, metadata rendering, diagnostics, `LIKE` cloning, and invalid
      values.
- [x] Write the feature spec with independently authored grammar snippets,
      descriptor semantics, diagnostics, metadata rendering, and non-goals.
- [x] Add MySQL-runtime expectation artifact for all user-visible behavior in
      this slice.
- [x] Extend parser/AST for storage/statistics table option nodes and
      comma-separated option lists.
- [x] Extend runtime planning to validate, normalize, and apply supported
      storage/statistics options without passing option text to SQLite.
- [x] Extend durable catalog and temporary table descriptors with zero-safe
      option fields and migration coverage.
- [x] Render options from descriptors in `SHOW CREATE TABLE`, `SHOW TABLE
      STATUS`, and `INFORMATION_SCHEMA.TABLES`.
- [x] Add focused C runtime tests for metadata, persistence, diagnostics,
      `LIKE` cloning, independent files, and preamble preservation.
- [x] Update compatibility docs for the exact supported subset and remaining
      deferred table options.
- [x] Register any new tests in CMake.
- [x] Run `cmake --build --preset dev`.
- [x] Run the new CTest entry plus relevant parser/catalog/DDL/metadata tests.
- [x] Run the MySQL expectation artifact.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for descriptor authority, MySQL evidence,
      metadata rendering, catalog migration safety, no SQLite pass-through,
      file-format safety, public ABI stability, scope control, and test
      relevance.

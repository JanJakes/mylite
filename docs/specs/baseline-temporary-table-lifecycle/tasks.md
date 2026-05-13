# Baseline Temporary Table Lifecycle Tasks

## Design and Evidence

- [x] Read the MyLite architecture, compatibility, storage, parser, catalog,
  DML, and introspection context.
- [x] Verify official MySQL 8.4 documentation for temporary table lifetime,
  shadowing, metadata visibility, drop semantics, and implicit-commit rules.
- [x] Probe MySQL 8.4.9 runtime behavior and record executable expectations.
- [x] Specify the initial supported grammar, descriptor ownership, physical
  SQLite strategy, diagnostics, and intentionally deferred behavior.

## Parser and AST

- [ ] Add a `TEMPORARY` parser token mapping without making the keyword
  globally reserved.
- [ ] Add AST node kinds and constructors for `CREATE TEMPORARY TABLE` and
  `DROP TEMPORARY TABLE`.
- [ ] Extend parser tests for supported temporary create/drop grammar and
  deferred unsupported forms.

## Session Temporary Catalog

- [ ] Add a session-owned temporary descriptor catalog with zero-initialized
  init/deinit behavior.
- [ ] Allocate negative table, column, index, and index-column IDs.
- [ ] Store temporary table, column, index, and index-column descriptors with
  generated physical SQLite temp object names.
- [ ] Add lookup, append, remove, and descriptor-copy helpers.
- [ ] Clean up descriptors on `mylite_close()` without touching durable catalog
  state.

## Runtime Integration

- [ ] Plan and execute `CREATE TEMPORARY TABLE` for the existing explicit table
  definition subset.
- [ ] Reject temporary auto-increment definitions in this slice.
- [ ] Plan and execute `DROP TEMPORARY TABLE`.
- [ ] Make `DROP TABLE` drop a shadowing temporary table before the persistent
  fallback.
- [ ] Route readable/writable single-table resolution through temporary
  descriptors before durable descriptors where MySQL shadowing requires it.
- [ ] Route column, primary-key, secondary-index, and index-column descriptor
  loading for negative temporary table/index IDs.
- [ ] Render temporary `SHOW CREATE TABLE` with `CREATE TEMPORARY TABLE`.
- [ ] Keep `SHOW TABLES`, `SHOW TABLE STATUS`, and `INFORMATION_SCHEMA` output
  durable-only.
- [ ] Reject temporary DDL while a user transaction is active.

## Tests and Documentation

- [ ] Add `runtime_temporary_table_lifecycle` C tests and CTest registration.
- [ ] Run the MySQL 8.4.9 expectation script for this feature.
- [ ] Cover creation, shadowing, DML, drop, metadata, close cleanup, file
  preamble preservation, independent handles, diagnostics, and transaction
  limitations.
- [ ] Update `COMPATIBILITY.md` and compatibility detail docs for the exact
  supported temporary-table subset.

## Verification

- [ ] `cmake --build --preset dev`
- [ ] New CTest entry plus existing parser/runtime lifecycle entries
- [ ] MySQL expectation script
- [ ] `cmake --workflow --preset check`
- [ ] Review with a subagent, fix/amend findings, commit, and push `main`.

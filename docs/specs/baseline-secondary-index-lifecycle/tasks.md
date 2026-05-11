# Baseline Secondary Index Lifecycle Tasks

## Design

- [x] Read project architecture, compatibility, parser, catalog, runtime,
  primary-key, auto-increment, show-index, information-schema, storage, and
  SQLite integration guidance.
- [x] Verify MySQL 8.4.9 behavior for table-level secondary indexes, unnamed
  index naming, `SHOW CREATE TABLE`, `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
  and key diagnostics.
- [x] Specify the independently authored MyLite grammar, semantics, descriptor
  model, physical SQLite strategy, diagnostics, compatibility gaps, and test
  plan.

## Implementation

- [ ] Extend parser and AST support for admitted table-level nonunique
  secondary index definitions.
- [ ] Extend catalog index descriptors from primary-only to primary plus
  secondary kinds, including migration and initialization.
- [ ] Plan admitted secondary indexes from `CREATE TABLE`, resolve names and
  columns through descriptors, and reject unsupported forms deterministically.
- [ ] Generate physical SQLite indexes from stable descriptor names and quote
  every identifier.
- [ ] Clone supported secondary index descriptors for `CREATE TABLE ... LIKE`
  and keep `CREATE TABLE ... SELECT` index-free.
- [ ] Render secondary index metadata in `SHOW CREATE TABLE`, `SHOW INDEX`, and
  limited `INFORMATION_SCHEMA.STATISTICS`.
- [ ] Preserve file-format preamble, descriptor authority, DML behavior,
  reopen persistence, independent handles, and cleanup-on-failure behavior.

## Tests and Docs

- [ ] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C parser/runtime tests for supported and rejected behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog migration,
  descriptor-driven SQLite SQL, metadata claims, performance, and file-format
  safety.

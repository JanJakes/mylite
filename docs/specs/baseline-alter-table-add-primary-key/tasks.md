# Baseline ALTER TABLE ADD PRIMARY KEY Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
  storage, SQLite, and existing primary/secondary/unique-index guidance.
- [x] Verify MySQL 8.4.9 behavior for successful integer
  `ALTER TABLE ... ADD PRIMARY KEY`, existing duplicates, existing `NULL`
  values, defaults, metadata, existing secondary indexes, result counts, and
  deliberately deferred wider forms.
- [x] Specify the independently authored MyLite grammar, semantics, descriptor
  updates, physical SQLite handling, diagnostics, compatibility gaps,
  performance boundary, and test plan.

## Implementation

- [ ] Extend parser and AST support for one
  `ALTER TABLE table_name ADD PRIMARY KEY (column_name)` action.
- [ ] Add analyzer/planner support for schema resolution, table resolution,
  primary-key existence checks, one unqualified integer key-column resolution,
  unsupported-form diagnostics, and cleanup-safe zero initialization.
- [ ] Add SQLite-side existing-row validation for `NULL` and duplicate target
  values without materializing the full table in C memory.
- [ ] Add catalog mutation support to make the descriptor column `NOT NULL`,
  normalize dropped/`NULL` default state to MySQL's post-primary-key shape,
  create the primary index descriptor and index-column descriptor, and preserve
  existing unique/nonunique index descriptors.
- [ ] Create the generated SQLite physical unique index from stable descriptor
  names inside the statement transaction.
- [ ] Ensure future DML, `TRUNCATE`, `CREATE TABLE ... LIKE`,
  `CREATE TABLE ... SELECT`, rename/drop, reopen, `SHOW`, and
  `INFORMATION_SCHEMA` behavior observe the added descriptor through existing
  paths.
- [ ] Reject unsupported target types, composite keys, named constraints,
  qualified key parts, multi-action `ALTER TABLE`, key options, algorithms,
  locks, temporary tables, views, standalone index statements, and
  `DROP PRIMARY KEY` deterministically.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog authority,
  descriptor-driven SQLite SQL, existing-row validation, metadata accuracy,
  performance, file-format safety, and scope control.

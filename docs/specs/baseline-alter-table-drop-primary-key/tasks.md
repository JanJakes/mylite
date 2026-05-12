# Baseline ALTER TABLE DROP PRIMARY KEY Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
  storage, SQLite, primary-key, composite-primary-key, alter-add-primary-key,
  auto-increment, and information-schema guidance.
- [x] Verify MySQL 8.4.9 behavior for successful drops, affected rows,
  metadata after drop, preserved nullability/defaults, secondary-index
  preservation, auto-increment restrictions, no-primary-key errors, and
  deliberately deferred wider forms.
- [x] Specify the independently authored MyLite grammar, semantics, descriptor
  deletion, physical SQLite handling, diagnostics, compatibility gaps,
  performance boundary, and test plan.

## Implementation

- [ ] Extend parser and AST support for one
  `ALTER TABLE table_name DROP PRIMARY KEY` action.
- [ ] Add analyzer/planner support for schema resolution, table resolution,
  primary-key descriptor lookup, no-primary-key diagnostics, auto-increment
  index checks, affected-row counting, and cleanup-safe zero initialization.
- [ ] Delete primary index-column descriptors and the primary index descriptor
  atomically while preserving table/column descriptors, row values, secondary
  indexes, catalog authority, and descriptor caches.
- [ ] Drop the generated SQLite physical primary-key index from the descriptor
  physical name, quoting every identifier.
- [ ] Ensure future DML, `TRUNCATE`, `CREATE TABLE ... LIKE`,
  `CREATE TABLE ... SELECT`, rename/drop, reopen, `SHOW`, and
  `INFORMATION_SCHEMA` behavior observe the removed descriptor through existing
  paths.
- [ ] Reject unsupported no-primary-key, target object, auto-increment,
  multi-action `ALTER TABLE`, `DROP INDEX`, `DROP KEY`, `DROP CONSTRAINT`,
  algorithms, locks, temporary tables, and views deterministically.

## Tests and Docs

- [ ] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog authority,
  affected-row correctness, physical SQLite index lowering, metadata accuracy,
  auto-increment interaction, performance, cleanup, and file-format safety.

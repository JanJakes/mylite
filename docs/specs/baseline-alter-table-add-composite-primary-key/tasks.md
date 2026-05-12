# Baseline ALTER TABLE ADD Composite Primary Key Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
  storage, SQLite, existing `ALTER TABLE ... ADD PRIMARY KEY`, and create-time
  composite primary-key guidance.
- [x] Verify MySQL 8.4.9 behavior for successful composite
  `ALTER TABLE ... ADD PRIMARY KEY`, existing duplicates, existing `NULL`
  values, defaults, metadata, existing secondary indexes, result counts, and
  deliberately deferred wider forms.
- [x] Specify the independently authored MyLite grammar, semantics, descriptor
  updates, physical SQLite handling, diagnostics, compatibility gaps,
  performance boundary, and test plan.

## Implementation

- [ ] Extend the current ALTER ADD PRIMARY KEY planner from one key column to
  an ordered key-part list while preserving the one-column path.
- [ ] Resolve every key part from descriptors, rejecting qualified parts,
  duplicate parts, unknown parts, reserved names, existing primary keys,
  unsupported object kinds, and unsupported column types deterministically.
- [ ] Validate existing rows for `NULL` values and duplicate tuples with
  SQLite-side descriptor-built query shapes that materialize only the first
  conflicting tuple for diagnostics.
- [ ] Mutate catalog descriptors atomically: mark all key parts `NOT NULL`,
  normalize `NULL`/no-explicit defaults, insert the primary index descriptor,
  and insert ordered index-column descriptors.
- [ ] Create a generated composite SQLite unique index from stable descriptor
  physical names, quoting every identifier.
- [ ] Preserve existing descriptor-driven behavior for DML duplicate
  diagnostics, `CREATE TABLE ... LIKE`, rename/drop, truncate, reopen,
  `SHOW`, and limited `INFORMATION_SCHEMA`.
- [ ] Keep unsupported named constraints, string keys, key options,
  multi-action `ALTER`, `DROP PRIMARY KEY`, and auto-increment conversion out
  of this slice.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests and parser regressions for supported and
  rejected behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog authority,
  ordered key-part correctness, duplicate diagnostics, physical SQLite index
  lowering, compatibility claims, performance, cleanup, and file-format safety.


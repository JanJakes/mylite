# Baseline Composite Primary Key Lifecycle Tasks

## Design

- [x] Read existing primary-key, alter-primary-key, secondary-index,
  unique-index, information-schema, parser, catalog, DML, storage, and SQLite
  integration guidance.
- [x] Verify MySQL 8.4.9 behavior for create-time composite primary-key
  metadata, DML duplicate/null diagnostics, defaults, cloning/copying, and
  deferred string/auto-increment/key-option forms.
- [x] Specify the independently authored MyLite grammar, descriptor model,
  physical SQLite handling, DML behavior, metadata rendering, diagnostics,
  compatibility gaps, performance boundary, and test plan.

## Implementation

- [ ] Replace runtime single-column loaded-index assumptions with ordered
  descriptor key-part arrays while keeping catalog schema unchanged.
- [ ] Admit table-level `PRIMARY KEY (a,b,...)` for two or more unqualified
  integer-family planned columns in `CREATE TABLE`.
- [ ] Reject duplicate key parts, unknown key parts, qualified parts,
  unsupported key-part types, named constraints, prefixes, directions, key
  options, and composite auto-increment forms deterministically.
- [ ] Mark every admitted key part `NOT NULL`, preserve non-`NULL` defaults,
  and store ordered index-column descriptors.
- [ ] Generate quoted physical SQLite composite unique-index SQL from
  descriptors and stable physical names.
- [ ] Update duplicate-key handling for `INSERT`, `INSERT IGNORE`, and
  single-assignment `UPDATE` so composite tuples produce MySQL-shaped
  diagnostics and warning demotion.
- [ ] Preserve `CREATE TABLE ... LIKE` composite key cloning and
  `CREATE TABLE ... SELECT` key omission.
- [ ] Render descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`,
  `SHOW INDEX`, and limited information-schema rows for ordered key parts.

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

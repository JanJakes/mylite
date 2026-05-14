# Baseline Composite Unique Indexes Tasks

## Design

- [x] Read project architecture, compatibility, parser, catalog, DDL, DML,
  metadata, storage, and SQLite integration guidance.
- [x] Verify MySQL 8.4.9 behavior for composite unique DDL, duplicate
  diagnostics, nullable tuple semantics, `INSERT IGNORE`, `UPDATE`, and
  metadata surfaces.
- [x] Specify the independently authored MyLite grammar, descriptor semantics,
  physical SQLite strategy, diagnostics, compatibility gaps, and test plan.

## Implementation

- [ ] Admit composite full-column unique keys in create-table planning,
  standalone `CREATE UNIQUE INDEX`, and single-action
  `ALTER TABLE ... ADD UNIQUE`.
- [ ] Keep composite unique prefix keys, descending parts, functional parts,
  table-qualified parts, unsupported descriptor families, and composite-key
  ODKU outside the supported subset with deterministic diagnostics.
- [ ] Validate existing rows for add/create composite unique indexes using
  descriptor-built tuple scans that ignore rows with any `NULL` key part.
- [ ] Generate physical SQLite unique indexes from stable descriptor names,
  quoted identifiers, and the existing string-key collation annotations.
- [ ] Preserve descriptor authority, catalog generation, physical rollback on
  failure, `.mylite` preamble safety, reopen persistence, and independent
  handle behavior.
- [ ] Ensure `INSERT`, `INSERT IGNORE`, and single-table `UPDATE` enforce
  composite unique tuples and report MySQL-shaped duplicate diagnostics.
- [ ] Render composite unique metadata in `SHOW CREATE TABLE`, `SHOW COLUMNS`,
  `SHOW INDEX`, and limited `INFORMATION_SCHEMA.STATISTICS`,
  `TABLE_CONSTRAINTS`, and `KEY_COLUMN_USAGE`.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C parser/runtime tests for supported and rejected behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run the new MySQL expectation script and focused CTest entries.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, MySQL behavior,
  descriptor authority, duplicate tuple correctness, metadata accuracy,
  file-format safety, performance, and scope control.

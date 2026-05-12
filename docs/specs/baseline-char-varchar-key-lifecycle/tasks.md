# Baseline CHAR/VARCHAR Key Lifecycle Tasks

## Design

- [x] Read project architecture, compatibility, parser, catalog, runtime,
  string type, key lifecycle, metadata, storage, and SQLite integration
  guidance.
- [x] Verify MySQL 8.4.9 behavior for `CHAR` / `VARCHAR` primary and unique
  keys, duplicate equality, trailing spaces, nullable unique keys, metadata,
  `ALTER TABLE ADD PRIMARY KEY`, diagnostics, and deferred forms.
- [x] Specify the independently authored MyLite grammar, descriptor semantics,
  ASCII string-key collation boundary, physical SQLite strategy, diagnostics,
  compatibility gaps, and test plan.
- [x] Add the MySQL 8.4.9 expectation script for supported and deliberately
  deferred string-key behavior.

## Implementation

- [ ] Register the fixed MyLite SQLite collation callback through public
  SQLite APIs during connection bootstrap.
- [ ] Admit single-column `CHAR` / `VARCHAR` primary-key descriptors in
  `CREATE TABLE` and `ALTER TABLE ... ADD PRIMARY KEY`, while preserving
  existing integer primary-key behavior and rejecting string composite keys.
- [ ] Admit create-time single-column `CHAR` / `VARCHAR` unique-index
  descriptors.
- [ ] Reject `CHAR(0)` / `VARCHAR(0)` key parts and non-ASCII string key
  values deterministically.
- [ ] Generate physical SQLite primary/unique indexes and duplicate-probe SQL
  from descriptors with string-key collation annotations.
- [ ] Validate existing rows for `ALTER TABLE ... ADD PRIMARY KEY` using the
  same descriptor-built string-key collation expressions.
- [ ] Preserve duplicate-key diagnostics and `INSERT IGNORE` warning demotion
  for admitted string primary and unique keys.
- [ ] Ensure `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`,
  `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, DML, rename/drop/truncate,
  reopen, and independent handles observe descriptor-owned string keys.
- [ ] Preserve file-format preamble, descriptor authority, public ABI stability,
  cleanup-on-failure behavior, and no SQLite fork changes.

## Tests and Docs

- [ ] Add focused C parser/runtime tests for supported string keys and rejected
  behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, collation correctness,
  descriptor authority, duplicate-key semantics, metadata claims, performance,
  scope control, and file-format safety.

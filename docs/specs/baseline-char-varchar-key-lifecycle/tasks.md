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

- [x] Register the fixed MyLite SQLite collation callback through public
  SQLite APIs during connection bootstrap.
- [x] Admit single-column `CHAR` / `VARCHAR` primary-key descriptors in
  `CREATE TABLE` and `ALTER TABLE ... ADD PRIMARY KEY`, while preserving
  existing integer primary-key behavior and rejecting string composite keys.
- [x] Admit create-time single-column `CHAR` / `VARCHAR` unique-index
  descriptors.
- [x] Reject `CHAR(0)` / `VARCHAR(0)` key parts and non-ASCII string key
  values deterministically.
- [x] Generate physical SQLite primary/unique indexes and duplicate-probe SQL
  from descriptors with string-key collation annotations.
- [x] Validate existing rows for `ALTER TABLE ... ADD PRIMARY KEY` using the
  same descriptor-built string-key collation expressions.
- [x] Preserve duplicate-key diagnostics and `INSERT IGNORE` warning demotion
  for admitted string primary and unique keys.
- [x] Ensure `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`,
  `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, DML, rename/drop/truncate,
  reopen, and independent handles observe descriptor-owned string keys.
- [x] Preserve file-format preamble, descriptor authority, public ABI stability,
  cleanup-on-failure behavior, and no SQLite fork changes.

## Tests and Docs

- [x] Add focused C parser/runtime tests for supported string keys and rejected
  behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, collation correctness,
  descriptor authority, duplicate-key semantics, metadata claims, performance,
  scope control, and file-format safety.

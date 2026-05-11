# Baseline TEXT Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, storage, and
  SQLite fork guidance relevant to the large-object string slice.
- [x] Verify MySQL 8.4.9 behavior for bare `TEXT` family types, metadata,
  row values, strict overlength failures, `IGNORE` adjustments, `NULL`,
  omitted defaults, `SHOW CREATE TABLE`, and update affected rows.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, diagnostics, compatibility gaps, and test plan.

## Implementation

- [x] Extend parser and AST support for bare `TINYTEXT`, `TEXT`, `MEDIUMTEXT`,
  and `LONGTEXT` column types.
- [x] Map admitted `TEXT` family descriptors to logical type names and physical
  SQLite `TEXT`.
- [x] Add MyLite-owned `TEXT` family string-literal validation with UTF-8,
  `NUL`, and byte-length checks.
- [x] Update DDL generation and descriptor cloning/copying paths to accept the
  `TEXT` family without relaxing unsupported string defaults or indexes.
- [x] Add string support for `INSERT`, `REPLACE`, `UPDATE`, compatible
  `INSERT ... SELECT`, compatible `REPLACE ... SELECT`, compatible
  `CREATE TABLE ... SELECT`, plain `SELECT` readback, and null-test predicates.
- [x] Update `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS` rendering.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [x] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, catalog authority,
  text ownership, descriptor-driven SQLite SQL, compatibility claims,
  performance, and file-format safety.

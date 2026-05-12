# Baseline Index Prefix Key Parts Tasks

- [ ] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  nonunique prefix key parts, composite key-part metadata, diagnostics,
  warnings, and deferred unique/primary prefix behavior.
- [ ] Add MySQL-runtime expectation script covering successful prefix/composite
  metadata, `Sub_part` / `SUB_PART`, schema resolution, diagnostics, and
  deferred wider syntax.
- [ ] Add parser/AST support for key-part nodes with optional positive integer
  prefix lengths while preserving existing unsupported grammar diagnostics.
- [ ] Extend catalog descriptors and migration so index-column rows store a
  nullable prefix length, with old descriptors loading as full-column parts.
- [ ] Extend create-time, alter-add-index, and standalone create-index planners
  from one nonunique key column to ordered key-part lists with descriptor-owned
  validation.
- [ ] Generate physical SQLite indexes from descriptor key-part lists, using
  ordinary column terms for full parts and `substr()` expression terms for
  prefix string parts.
- [ ] Ensure `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `SHOW COLUMNS`, `CREATE TABLE ... LIKE`,
  index drops, DML, reopen, independent handles, and file-format invariants
  observe prefix descriptors correctly.
- [ ] Add fast C runtime/parser coverage for success cases, metadata,
  persistence, file-format safety, diagnostics, unsupported syntax, cleanup,
  and zero initialization.
- [ ] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`, and
  `docs/compatibility/sql-table-ddl.md` with exact limited wording.
- [ ] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, catalog authority, physical SQL
  quoting, prefix conversion, metadata, cleanup on failure, performance, scope
  control, compatibility docs, and test relevance.

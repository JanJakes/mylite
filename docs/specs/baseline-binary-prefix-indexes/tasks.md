# Baseline Binary Prefix Indexes Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  nonunique binary string prefix indexes, metadata, diagnostics, warnings, and
  deferred unique binary prefix behavior.
- [x] Add a MySQL-runtime expectation script covering successful binary prefix
  metadata, `Sub_part` / `SUB_PART`, schema resolution, diagnostics, and
  unsupported unique binary prefixes in MyLite.
- [x] Extend create-time, alter-add-index, and standalone create-index
  validation so nonunique binary string prefix key parts are accepted with byte
  semantics and correct byte caps.
- [x] Keep full-column binary secondary indexes and unique binary prefix
  indexes outside this slice with deterministic diagnostics.
- [x] Generate physical SQLite prefix expressions without text-key collation
  for binary string descriptors while preserving text collation for nonbinary
  string prefixes.
- [x] Ensure descriptor-driven `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `SHOW COLUMNS`, `CREATE TABLE ... LIKE`,
  drop/rename/visibility paths, reopen, independent handles, and file-format
  invariants observe binary prefix descriptors correctly.
- [x] Add fast C runtime coverage for success cases, metadata, persistence,
  file-format safety, diagnostics, unsupported syntax, cleanup, and zero
  initialization.
- [x] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-indexes-constraints.md`, and any affected detail docs
  with exact limited wording.
- [x] Run the MySQL expectation script, focused build/tests, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, catalog authority, physical SQL
  quoting, byte-prefix conversion, metadata, cleanup on failure, performance,
  scope control, compatibility docs, and test relevance.

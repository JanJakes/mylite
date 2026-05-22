# Baseline DROP CONSTRAINT Lifecycle Tasks

## Design

- [x] Read current architecture, parser, catalog, index, primary-key,
      foreign-key, CHECK, information-schema, storage, and SQLite integration
      context.
- [x] Verify MySQL 8.4.9 behavior for primary, unique, foreign-key, CHECK,
      unknown-name, ambiguous-name, nonunique-index, `IF EXISTS`, option-tail,
      auto-increment, and referenced-parent-key cases.
- [x] Write the independently authored feature specification with ownership
      boundaries, grammar snippets, descriptor resolution, physical SQLite
      handling, diagnostics, compatibility gaps, performance boundary, and test
      plan.

## Implementation

- [x] Add the MySQL-runtime expectation script for the feature surface.
- [x] Extend parser and AST support for the admitted single-action
      `ALTER TABLE ... DROP CONSTRAINT identifier` subset.
- [x] Add analyzer/runtime resolution across primary, unique, foreign-key, and
      CHECK descriptors, with MySQL-compatible unknown and ambiguous
      diagnostics.
- [x] Dispatch resolved constraints through the existing primary-key,
      drop-index, foreign-key, and CHECK drop paths while preserving
      descriptor authority, physical SQL quoting, cleanup on failure, zero
      affected rows for unique, foreign-key, and CHECK drops, and table-row
      affected rows for primary-key drops.
- [x] Ensure `SHOW CREATE TABLE`, `SHOW INDEX`, `SHOW COLUMNS`,
      `INFORMATION_SCHEMA`, DML, reopen, rename/drop, and independent handles
      observe the post-drop descriptor state through existing paths.

## Tests and Docs

- [x] Add focused C parser/runtime tests and register any new test binary.
- [x] Update `COMPATIBILITY.md`,
      `docs/compatibility/sql-indexes-constraints.md`, and
      `docs/compatibility/sql-table-ddl.md` for the exact supported subset.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority,
      diagnostics, metadata, affected rows, performance, cleanup, file-format
      safety, and scope control.
- [x] Commit, review with a subagent, amend any findings, and push `main`.

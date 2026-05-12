# Baseline ALTER TABLE AUTO_INCREMENT Option Tasks

## Design

- [x] Create the feature spec for the limited
  `ALTER TABLE table_name AUTO_INCREMENT [=] N` table-option slice.
- [x] Record official MySQL 8.4 documentation sources and observed MySQL 8.4.9
  runtime behavior.
- [x] Specify grammar, schema resolution, descriptor ownership, effective
  counter semantics, physical SQLite handling, diagnostics, metadata, and
  unsupported behavior.

## MySQL Expectations

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Verify upward resets, lowered resets after deletes, `AUTO_INCREMENT=0`,
  empty tables, non-auto-increment no-op behavior, schema-qualified targets,
  diagnostics, and deferred wider forms.

## Implementation

- [x] Extend parser and AST support for one
  `ALTER TABLE table_name AUTO_INCREMENT [=] integer_literal` action.
- [x] Add analyzer/planner support for schema resolution, table resolution,
  reserved target names, persistent base-table checks, literal conversion, and
  cleanup-safe zero initialization.
- [x] Resolve the current auto-increment descriptor column, if present, from
  MyLite descriptors rather than SQLite metadata.
- [x] Compute the effective next value using the requested literal and a
  descriptor-built physical `MAX()` query over the auto-increment column.
- [x] Update `auto_increment_next` through the catalog API without mutating
  table descriptors, column descriptors, descriptor versions, row data,
  `LAST_INSERT_ID()`, or `sqlite_schema_generation`.
- [x] Preserve existing `INSERT`, `UPDATE`, `TRUNCATE`, `CREATE TABLE ... LIKE`,
  reopen, `SHOW`, and limited `INFORMATION_SCHEMA` behavior through the updated
  descriptor counter.
- [x] Reject unsupported literal forms, multi-action `ALTER TABLE`, attribute
  changes, temporary tables, views, algorithms, locks, and unrelated table
  options deterministically.

## Tests and Docs

- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  designed subset.
- [x] Add focused parser and C runtime tests for supported and rejected
  behavior.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, catalog authority,
  descriptor-driven SQLite SQL, effective counter correctness, metadata
  accuracy, performance, file-format safety, and scope control.

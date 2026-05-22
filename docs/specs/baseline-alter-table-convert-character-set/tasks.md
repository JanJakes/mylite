# Baseline ALTER TABLE Convert Character Set Tasks

## Design

- [x] Read the relevant architecture, charset/collation, ALTER TABLE, catalog,
  parser, storage, and compatibility docs.
- [x] Verify MySQL 8.4.9 syntax, diagnostics, metadata rendering, affected-row
  behavior, and deferred conversion surfaces.
- [x] Write the independently authored feature spec.
- [x] Record MySQL-runtime expectations in a local script.
- [x] Update compatibility docs for the exact limited surface.

## Parser And AST

- [x] Add an AST node for
  `ALTER TABLE ... CONVERT TO CHARACTER SET`.
- [x] Add Lemon grammar for the admitted convert syntax.
- [x] Preserve target table, charset, and optional collation nodes.
- [x] Reject `=`, `COLLATE DEFAULT`, option tails, and mixed actions.
- [x] Extend parser tests.

## Runtime

- [x] Resolve unqualified and schema-qualified targets through the existing
  writable-table policy.
- [x] Reject reserved target names and unsupported object kinds.
- [x] Decode and validate target charset/collation names.
- [x] Implement `DEFAULT` target resolution from current database defaults.
- [x] Validate source table descriptor eligibility.
- [x] Mutate table default charset/collation and explicit participating column
  metadata in one catalog transaction.
- [x] Preserve rows, physical SQLite schema, SQLite schema generation, and the
  `.mylite` preamble.
- [x] Return non-row statement results with affected rows `0` and warning count
  `0`.

## Tests

- [x] Add `runtime_alter_table_convert_character_set_test.c`.
- [x] Register the test in `packages/libmylite/CMakeLists.txt`.
- [x] Cover success, metadata, persistence, diagnostics, independent handles,
  and cleanup behavior.
- [x] Run the MySQL expectation script.
- [x] Run targeted parser/runtime tests.
- [x] Run `cmake --workflow --preset check`.

## Review

- [x] Review architecture boundaries, descriptor authority, unsupported
  conversion diagnostics, MySQL evidence, file-format safety, test relevance,
  and compatibility docs.
- [x] Commit and push the completed feature.

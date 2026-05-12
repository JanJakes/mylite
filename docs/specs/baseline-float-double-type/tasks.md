# Baseline FLOAT and DOUBLE Type Tasks

- [x] Verify MySQL 8.4.9 behavior for supported approximate type declarations,
  aliases, `FLOAT(p)` mapping, `UNSIGNED` warnings, defaults, row DML,
  readback, metadata, nullability, and strict diagnostics.
- [x] Record MySQL expectations in
  `packages/libmylite/tests/mysql_baseline_float_double_type_expectations.sh`.
- [x] Add parser/AST support for the limited approximate type grammar and
  approximate literals in admitted DML/default positions.
- [x] Map approximate type AST nodes to descriptor-owned logical types and
  SQLite `REAL` physical storage.
- [x] Extend descriptor helpers, row-size accounting, catalog default
  validation, and unsupported-operation gates for approximate descriptors.
- [x] Add MyLite-owned approximate conversion for supported literals,
  defaults, insert/replace/update values, `INSERT IGNORE` null/default
  adjustments, and readback formatting.
- [x] Extend `SHOW COLUMNS`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS` metadata.
- [x] Add runtime tests for supported DDL, DML, defaults, metadata,
  persistence, independent handles, file-format safety, and diagnostics.
- [x] Update compatibility docs for the exact limited surface.
- [x] Run the MySQL expectation script, focused parser/runtime CTests, and the
  full `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL equivalence, descriptor authority,
  physical `REAL` storage, conversion correctness, formatting stability,
  catalog/file-format safety, scope control, and test relevance.

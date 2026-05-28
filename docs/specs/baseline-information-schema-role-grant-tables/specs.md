# Baseline INFORMATION_SCHEMA Role Grant Tables

## Summary

MyLite exposes limited queryable synthetic role-grant metadata views for:

- `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS`;
- `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS`;
- `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS`.

MySQL 8.4 documents these views as privilege metadata for roles available to
or granted by currently enabled roles. MyLite does not yet implement role graph
storage, role DDL, grant descriptors, routine descriptors, or privilege
enforcement. This slice therefore provides MySQL-shaped empty catalogs so
client tooling can probe the views without treating them as missing.

This slice covers:

- MySQL 8.4.9-shaped columns for the three role grant views;
- zero rows for the embedded `root@%` identity until role and grant
  descriptors are designed;
- matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
  metadata for the system views;
- existing information-schema query features such as projection, `COUNT(*)`,
  supported predicates, `ORDER BY`, `LIMIT`, aliases, and selected
  `information_schema` resolution.

This slice does not implement `CREATE ROLE`, `DROP ROLE`, `GRANT`/`REVOKE` to
roles, role activation, routine descriptors, role privilege rows, privilege
filtering, or physical data-dictionary tables.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-role-column-grants-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-role-routine-grants-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-role-table-grants-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_information_schema_role_grant_tables_expectations.sh`.

The official pages describe role column, routine, and table privileges that are
available to or granted by currently enabled roles. Runtime verification
against MySQL 8.4.9 adds the baseline details used by this feature:

- the target root session has zero rows in all three views;
- successful filtered reads produce no warnings and leave `ROW_COUNT()` at
  `-1`;
- `INFORMATION_SCHEMA.TABLES` reports each view as a `SYSTEM VIEW` with
  `ENGINE = NULL`, `VERSION = 10`, `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, and
  `DATA_LENGTH = 0`;
- `INFORMATION_SCHEMA.COLUMNS` exposes the column metadata recorded by the
  expectation script.

## Syntax

No parser expansion is required. Existing MyLite metadata-query grammar already
admits the supported query shape:

```lemon
select_stmt ::= SELECT select_list FROM table_factor select_tail_opt.
table_factor ::= qualified_name.
table_factor ::= qualified_name AS identifier.
```

The existing limited information-schema query subset continues to apply:
projection, `COUNT(*)`, supported `WHERE` predicates, one-column `ORDER BY`,
and `LIMIT`.

## Semantics

`ROLE_COLUMN_GRANTS` exposes role column privilege metadata columns:

- `GRANTOR`;
- `GRANTOR_HOST`;
- `GRANTEE`;
- `GRANTEE_HOST`;
- `TABLE_CATALOG`;
- `TABLE_SCHEMA`;
- `TABLE_NAME`;
- `COLUMN_NAME`;
- `PRIVILEGE_TYPE`;
- `IS_GRANTABLE`.

`ROLE_ROUTINE_GRANTS` exposes role routine privilege metadata columns:

- `GRANTOR`;
- `GRANTOR_HOST`;
- `GRANTEE`;
- `GRANTEE_HOST`;
- `SPECIFIC_CATALOG`;
- `SPECIFIC_SCHEMA`;
- `SPECIFIC_NAME`;
- `ROUTINE_CATALOG`;
- `ROUTINE_SCHEMA`;
- `ROUTINE_NAME`;
- `PRIVILEGE_TYPE`;
- `IS_GRANTABLE`.

`ROLE_TABLE_GRANTS` exposes role table privilege metadata columns:

- `GRANTOR`;
- `GRANTOR_HOST`;
- `GRANTEE`;
- `GRANTEE_HOST`;
- `TABLE_CATALOG`;
- `TABLE_SCHEMA`;
- `TABLE_NAME`;
- `PRIVILEGE_TYPE`;
- `IS_GRANTABLE`.

Until MyLite has explicit role and grant descriptors, all three views emit no
rows. Empty results must still expose the correct column names and allow valid
predicates over their columns.

No natural row order is claimed. Future role-grant rows must use explicit
ordering in tests when order matters.

## Diagnostics

This feature reuses existing information-schema diagnostics:

- unknown metadata table: `1109 / 42S02`;
- unknown projection, predicate, or ordering column: `1054 / 42S22`;
- unsupported information-schema query shape: existing unsupported-query
  diagnostics;
- allocation failure: existing `MYLITE_NOMEM` handling.

Successful reads introduce no warnings.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime/analyzer: adds static table definitions beside the existing
  information-schema metadata views.
- Catalog metadata: no role, grant, account, routine, table, or column
  descriptors are read for row production in this slice.
- Result builder: unchanged.
- Storage/VFS/SQLite: unchanged. This is MyLite wrapper/translation metadata
  behavior and uses no SQLite fork hook.

## Performance

The row builders are empty, so queries only materialize definition metadata and
run existing predicate/projection code. No user table data, grant tables,
SQLite system tables, or storage-engine statistics are scanned.

## Tests

MySQL 8.4.9 expectation coverage:

- zero baseline rows in all three role grant views;
- system-view metadata in `INFORMATION_SCHEMA.TABLES`;
- system-column metadata in `INFORMATION_SCHEMA.COLUMNS`;
- statement status after a successful filtered read.

MyLite C coverage:

- empty result column names for wildcard projection over each view;
- `COUNT(*)` returns `0` for each view;
- supported predicates over representative columns return zero rows without
  treating the columns as unknown;
- selected `information_schema` resolution;
- system-view rows in `INFORMATION_SCHEMA.TABLES`;
- system-column rows in `INFORMATION_SCHEMA.COLUMNS`;
- unknown-column diagnostics.

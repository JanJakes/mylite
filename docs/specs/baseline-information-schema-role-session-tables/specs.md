# Baseline INFORMATION_SCHEMA Role Session Tables

## Summary

MyLite exposes limited queryable synthetic role-session metadata views for:

- `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS`;
- `INFORMATION_SCHEMA.APPLICABLE_ROLES`;
- `INFORMATION_SCHEMA.ENABLED_ROLES`.

MySQL 8.4 documents these views as metadata for roles applicable to the
current user or role and roles enabled in the current session. MyLite currently
has an embedded `root@%` identity, `CURRENT_ROLE()` returns `NONE`, and there
is no role catalog, role graph, role activation state, account storage, grant
descriptors, or privilege enforcement. This slice therefore provides
MySQL-shaped empty catalogs so client tooling can probe the views without
treating them as missing.

This slice covers:

- MySQL 8.4.9-shaped columns for the three role-session views;
- zero rows for MyLite's no-active-role embedded session until role and grant
  descriptors are designed;
- matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
  metadata for the system views;
- existing information-schema query features such as projection, `COUNT(*)`,
  supported predicates, `ORDER BY`, `LIMIT`, aliases, and selected
  `information_schema` resolution.

This slice does not implement `CREATE ROLE`, `DROP ROLE`, `GRANT`/`REVOKE` of
roles, `SET ROLE`, `SET DEFAULT ROLE`, active-role storage, default-role
storage, mandatory-role storage, account storage, privilege filtering, or
physical data-dictionary tables.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-administrable-role-authorizations-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.APPLICABLE_ROLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-applicable-roles-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ENABLED_ROLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-enabled-roles-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_information_schema_role_session_tables_expectations.sh`.

The official pages describe applicable, administrable, and enabled role
metadata for the current user or session. Runtime verification against MySQL
8.4.9 adds the baseline details used by this feature:

- the target root session has zero rows in all three views;
- successful filtered reads produce no warnings and leave `ROW_COUNT()` at
  `-1`;
- unknown predicate columns fail with `1054 / 42S22`;
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

`ADMINISTRABLE_ROLE_AUTHORIZATIONS` exposes grantable role metadata columns:

- `USER`;
- `HOST`;
- `GRANTEE`;
- `GRANTEE_HOST`;
- `ROLE_NAME`;
- `ROLE_HOST`;
- `IS_GRANTABLE`;
- `IS_DEFAULT`;
- `IS_MANDATORY`.

`APPLICABLE_ROLES` exposes applicable role metadata columns:

- `USER`;
- `HOST`;
- `GRANTEE`;
- `GRANTEE_HOST`;
- `ROLE_NAME`;
- `ROLE_HOST`;
- `IS_GRANTABLE`;
- `IS_DEFAULT`;
- `IS_MANDATORY`.

`ENABLED_ROLES` exposes enabled-session role metadata columns:

- `ROLE_NAME`;
- `ROLE_HOST`;
- `IS_DEFAULT`;
- `IS_MANDATORY`.

Until MyLite has explicit role and grant descriptors plus active-role state,
all three views emit no rows. Empty results must still expose the correct
column names and allow valid predicates over their columns.

No natural row order is claimed. Future role rows must use explicit ordering in
tests when order matters.

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

- zero baseline rows in all three role-session views;
- system-view metadata in `INFORMATION_SCHEMA.TABLES`;
- system-column metadata in `INFORMATION_SCHEMA.COLUMNS`;
- statement status after a successful filtered read;
- unknown-column diagnostic.

MyLite C coverage:

- empty result column names for wildcard projection over each view;
- `COUNT(*)` returns `0` for each view;
- supported predicates over representative columns return zero rows without
  treating the columns as unknown;
- selected `information_schema` resolution;
- system-view rows in `INFORMATION_SCHEMA.TABLES`;
- system-column rows in `INFORMATION_SCHEMA.COLUMNS`;
- unknown-column diagnostics.

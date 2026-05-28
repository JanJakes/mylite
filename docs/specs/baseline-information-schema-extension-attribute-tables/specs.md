# Baseline INFORMATION_SCHEMA Extension Attribute Tables

## Summary

MyLite exposes limited queryable synthetic rows for these MySQL 8.4.9
information-schema system views:

- `INFORMATION_SCHEMA.COLUMNS_EXTENSIONS`;
- `INFORMATION_SCHEMA.TABLES_EXTENSIONS`;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`.

These views report storage-engine extension attributes. MySQL 8.4 documents
the attribute columns as reserved for future use, and MySQL 8.4.9 emits `NULL`
for both primary and secondary engine-attribute columns in the verified
baseline. MyLite mirrors that current visible behavior for supported
descriptor-owned objects and supported synthetic metadata definitions.

This slice covers:

- MySQL 8.4.9-shaped columns for the three extension-attribute views;
- `TABLES_EXTENSIONS` rows for built-in table-directory entries, MyLite base
  tables, and MyLite views;
- `COLUMNS_EXTENSIONS` rows for supported `information_schema` synthetic table
  definitions, MyLite base-table columns, and MyLite view columns;
- `TABLE_CONSTRAINTS_EXTENSIONS` rows for descriptor-owned primary-key, unique,
  and foreign-key constraints on MyLite persistent base tables;
- matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
  metadata for the three new system views.

This slice does not implement storage-engine attributes, secondary engines,
complete built-in-schema column catalogs, built-in-schema constraint catalogs,
check-constraint extension rows, privilege filtering, or physical
`information_schema` tables.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS_EXTENSIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-extensions-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES_EXTENSIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-extensions-table.html>
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-extensions-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_information_schema_extension_attribute_tables_expectations.sh`.

The official pages define these tables as storage-engine attribute metadata and
reserve the engine-attribute columns for future use. Runtime verification
against MySQL 8.4.9 adds the row-emission details used by this feature:

- user base tables and views have `TABLES_EXTENSIONS` rows;
- user base-table and view columns have `COLUMNS_EXTENSIONS` rows;
- primary-key, unique, and foreign-key constraints have
  `TABLE_CONSTRAINTS_EXTENSIONS` rows;
- check constraints do not have `TABLE_CONSTRAINTS_EXTENSIONS` rows;
- `ENGINE_ATTRIBUTE` and `SECONDARY_ENGINE_ATTRIBUTE` are `NULL` in all
  verified rows.

Observed MySQL 8.4.9 system-view metadata:

| Table | `INFORMATION_SCHEMA.TABLES.TABLE_TYPE` | `ENGINE` | `VERSION` | `ROW_FORMAT` | `TABLE_ROWS` | `DATA_LENGTH` |
| --- | --- | --- | --- | --- | --- | --- |
| `COLUMNS_EXTENSIONS` | `SYSTEM VIEW` | `NULL` | `10` | `NULL` | `0` | `0` |
| `TABLES_EXTENSIONS` | `SYSTEM VIEW` | `NULL` | `10` | `NULL` | `0` | `0` |
| `TABLE_CONSTRAINTS_EXTENSIONS` | `SYSTEM VIEW` | `NULL` | `10` | `NULL` | `0` | `0` |

Observed MySQL 8.4.9 column metadata:

| Table | Column | Ordinal | Null | Data type | Character length | Octet length | Character set | Collation | Column type |
| --- | --- | ---: | --- | --- | ---: | ---: | --- | --- | --- |
| `COLUMNS_EXTENSIONS` | `TABLE_CATALOG` | 1 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `COLUMNS_EXTENSIONS` | `TABLE_SCHEMA` | 2 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `COLUMNS_EXTENSIONS` | `TABLE_NAME` | 3 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `COLUMNS_EXTENSIONS` | `COLUMN_NAME` | 4 | `YES` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_tolower_ci` | `varchar(64)` |
| `COLUMNS_EXTENSIONS` | `ENGINE_ATTRIBUTE` | 5 | `YES` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |
| `COLUMNS_EXTENSIONS` | `SECONDARY_ENGINE_ATTRIBUTE` | 6 | `YES` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |
| `TABLES_EXTENSIONS` | `TABLE_CATALOG` | 1 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `TABLES_EXTENSIONS` | `TABLE_SCHEMA` | 2 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `TABLES_EXTENSIONS` | `TABLE_NAME` | 3 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `TABLES_EXTENSIONS` | `ENGINE_ATTRIBUTE` | 4 | `YES` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |
| `TABLES_EXTENSIONS` | `SECONDARY_ENGINE_ATTRIBUTE` | 5 | `YES` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |
| `TABLE_CONSTRAINTS_EXTENSIONS` | `CONSTRAINT_CATALOG` | 1 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `TABLE_CONSTRAINTS_EXTENSIONS` | `CONSTRAINT_SCHEMA` | 2 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `TABLE_CONSTRAINTS_EXTENSIONS` | `CONSTRAINT_NAME` | 3 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_tolower_ci` | `varchar(64)` |
| `TABLE_CONSTRAINTS_EXTENSIONS` | `TABLE_NAME` | 4 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `TABLE_CONSTRAINTS_EXTENSIONS` | `ENGINE_ATTRIBUTE` | 5 | `YES` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |
| `TABLE_CONSTRAINTS_EXTENSIONS` | `SECONDARY_ENGINE_ATTRIBUTE` | 6 | `YES` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |

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

`TABLES_EXTENSIONS` emits rows with:

- `TABLE_CATALOG = 'def'`;
- `TABLE_SCHEMA` equal to the built-in schema name or descriptor schema name;
- `TABLE_NAME` equal to the built-in directory entry, base-table name, or view
  name;
- both engine-attribute columns as SQL `NULL`.

Built-in `TABLES_EXTENSIONS` rows use the same built-in schema table directory
as `INFORMATION_SCHEMA.TABLES`. This keeps MyLite's metadata-only system-table
directory consistent, but it still does not make unsupported built-in tables
queryable.

`COLUMNS_EXTENSIONS` emits rows with:

- `TABLE_CATALOG = 'def'`;
- `TABLE_SCHEMA = 'information_schema'` for supported synthetic
  information-schema definitions, or the descriptor schema for MyLite user
  objects;
- `TABLE_NAME` and `COLUMN_NAME` from the synthetic definition or table/view
  column descriptor;
- both engine-attribute columns as SQL `NULL`.

MyLite emits system-column rows only for supported synthetic
`information_schema` table definitions. It does not invent column rows for
metadata-only built-in directory entries whose column catalogs are not yet
implemented.

`TABLE_CONSTRAINTS_EXTENSIONS` emits rows with:

- `CONSTRAINT_CATALOG = 'def'`;
- `CONSTRAINT_SCHEMA` equal to the descriptor schema;
- `CONSTRAINT_NAME` from the primary-key, unique-index, or foreign-key
  descriptor;
- `TABLE_NAME` equal to the constrained base table;
- both engine-attribute columns as SQL `NULL`.

Check constraints remain visible through `TABLE_CONSTRAINTS` and
`CHECK_CONSTRAINTS`, but not through `TABLE_CONSTRAINTS_EXTENSIONS`, matching
the observed MySQL 8.4.9 behavior.

Rows are materialized through the existing information-schema row-set pipeline.
No natural row order is claimed; tests and users should apply explicit
`ORDER BY` clauses where order matters.

## Diagnostics

This feature reuses existing information-schema diagnostics:

- unknown metadata table: `1109 / 42S02`;
- unknown projection, predicate, or ordering column: `1054 / 42S22`;
- unsupported information-schema query shape: existing unsupported-query
  diagnostics;
- allocation failure: existing `MYLITE_NOMEM` handling;
- descriptor read failures: existing deterministic runtime diagnostics.

Successful reads introduce no warnings.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime/analyzer: adds static table definitions for the three views and row
  builders beside the existing information-schema table, column, and
  constraint builders.
- Catalog metadata: reads schema, table, view, column, index, and foreign-key
  descriptors through the existing catalog APIs.
- Result builder: unchanged.
- Storage/VFS/SQLite: unchanged. This is MyLite wrapper/translation metadata
  behavior and uses no SQLite fork hook.

## Performance

System rows are bounded by the built-in table directory and supported
information-schema definitions. User rows are proportional to descriptor-owned
tables, views, columns, unique indexes, primary keys, and foreign keys. No user
table data or SQLite system tables are scanned.

## Tests

MySQL 8.4.9 expectation coverage:

- column labels for `SELECT *` from all three extension-attribute views;
- base-table and view rows for `TABLES_EXTENSIONS`;
- base-table and view-column rows for `COLUMNS_EXTENSIONS`;
- primary-key, unique, and foreign-key rows for
  `TABLE_CONSTRAINTS_EXTENSIONS`;
- absence of check-constraint rows from `TABLE_CONSTRAINTS_EXTENSIONS`;
- system-view metadata in `INFORMATION_SCHEMA.TABLES`;
- system-column metadata in `INFORMATION_SCHEMA.COLUMNS`;
- statement status after successful filtered reads.

MyLite C coverage:

- descriptor-backed base-table, view, column, primary-key, unique, and
  foreign-key extension rows;
- built-in table-directory rows for `TABLES_EXTENSIONS`;
- supported information-schema system-column extension rows;
- case-insensitive table resolution, aliases, predicates, `ORDER BY`, `LIMIT`,
  and `COUNT(*)`;
- system-view rows in `INFORMATION_SCHEMA.TABLES`;
- system-column rows in `INFORMATION_SCHEMA.COLUMNS`;
- unknown-column diagnostics.

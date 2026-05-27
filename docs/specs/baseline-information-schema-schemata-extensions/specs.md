# Baseline INFORMATION_SCHEMA SCHEMATA_EXTENSIONS

## Summary

MyLite exposes a queryable synthetic
`INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` system view. The view augments the
existing descriptor-backed `INFORMATION_SCHEMA.SCHEMATA` surface with one row
per built-in or MyLite catalog schema and a MySQL-shaped schema-options column.

This slice covers:

- MySQL 8.4.9-shaped `SCHEMATA_EXTENSIONS` result columns;
- rows for `information_schema`, `mysql`, `performance_schema`, `sys`, and
  descriptor-owned user schemas;
- matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
  metadata for the new system view;
- existing `INFORMATION_SCHEMA` projection, predicate, ordering, limit, alias,
  and count support.

This slice does not implement schema read-only state, `ALTER SCHEMA ... READ
ONLY`, storage-engine schema options, privileges, or physical
`information_schema` tables. Because MyLite has no mutable read-only schema
option yet, `OPTIONS` is the empty string for every emitted row.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-schemata-extensions-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.SCHEMATA`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-schemata-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_information_schema_schemata_extensions_expectations.sh`.

Observed MySQL 8.4.9 default rows for this target runtime:

| Schema | CATALOG_NAME | OPTIONS |
| --- | --- | --- |
| `information_schema` | `def` | empty string |
| `mysql` | `def` | empty string |
| `performance_schema` | `def` | empty string |
| `sys` | `def` | empty string |
| user schema | `def` | empty string |

Observed MySQL 8.4.9 column metadata:

| Column | Ordinal | Null | Data type | Character length | Octet length | Character set | Collation | Column type |
| --- | ---: | --- | --- | ---: | ---: | --- | --- | --- |
| `CATALOG_NAME` | 1 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `SCHEMA_NAME` | 2 | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `OPTIONS` | 3 | `YES` | `varchar` | 256 | 768 | `utf8mb3` | `utf8mb3_general_ci` | `varchar(256)` |

`INFORMATION_SCHEMA.TABLES` reports
`information_schema.SCHEMATA_EXTENSIONS` as a `SYSTEM VIEW` with `ENGINE =
NULL`, `VERSION = 10`, `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`,
`DATA_LENGTH = 0`, and `AUTO_INCREMENT = NULL`.

## Syntax

No parser expansion is required. Existing MyLite metadata-query grammar already
admits:

```lemon
select_stmt ::= SELECT select_list FROM table_factor select_tail_opt.
table_factor ::= qualified_name.
table_factor ::= qualified_name AS identifier.
```

The current limited `INFORMATION_SCHEMA` query subset continues to apply:
projection, `COUNT(*)`, supported `WHERE` predicates, one-column `ORDER BY`,
and `LIMIT`.

## Semantics

`INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` emits rows from the same schema sources
as `INFORMATION_SCHEMA.SCHEMATA`:

- four synthetic built-in schema descriptors;
- durable MyLite catalog schema descriptors.

Each row contains:

- `CATALOG_NAME = 'def'`;
- `SCHEMA_NAME` equal to the canonical built-in schema name or durable catalog
  schema name;
- `OPTIONS = ''`.

Rows are generated in the standard metadata-query pipeline. Built-in rows are
appended before catalog rows; callers that need stable presentation order
should use `ORDER BY SCHEMA_NAME`, matching other MyLite metadata tables.

The `OPTIONS` column is nullable in metadata because MySQL reports it that way,
but this slice always emits a non-`NULL` empty string. Future schema read-only
support must update this table to emit `READ ONLY=1` for read-only schemas.

## Diagnostics

This feature reuses existing `INFORMATION_SCHEMA` diagnostics:

- unknown metadata table: `1109 / 42S02`;
- unknown projection, predicate, or ordering column: `1054 / 42S22`;
- unsupported `INFORMATION_SCHEMA` query shape: existing unsupported-query
  diagnostics;
- allocation failure: existing `MYLITE_NOMEM` handling.

No new public API diagnostics are introduced.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime/analyzer: adds a static `SCHEMATA_EXTENSIONS` table definition and
  row builders beside the existing `SCHEMATA` implementation.
- Catalog metadata: reads schema descriptors through
  `mylite_catalog_for_each_schema()`; no catalog format change.
- Result builder: unchanged.
- Storage/VFS/SQLite: unchanged. This is a MyLite wrapper/translation metadata
  feature and uses no SQLite fork hook.

## Performance

The row count is the number of built-in schemas plus durable catalog schemas.
Rows are small and materialized through the existing metadata-query row-set
path. No user table data or SQLite system tables are scanned.

## Tests

MySQL 8.4.9 expectation coverage:

- column labels for `SELECT * FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS`;
- default rows for built-in and user schemas;
- system-view metadata in `INFORMATION_SCHEMA.TABLES`;
- system-column metadata in `INFORMATION_SCHEMA.COLUMNS`;
- statement status after a successful filtered read.

MyLite C coverage:

- wildcard and explicit projections over built-in and user schema rows;
- case-insensitive table name resolution;
- existing alias, predicate, `ORDER BY`, `LIMIT`, and `COUNT(*)` behavior;
- system-view row in `INFORMATION_SCHEMA.TABLES`;
- system-column rows in `INFORMATION_SCHEMA.COLUMNS`;
- unknown-column diagnostics.

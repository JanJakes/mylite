# Baseline INFORMATION_SCHEMA TABLESPACES_EXTENSIONS

## Summary

MyLite exposes a limited queryable synthetic
`INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS` system view. MySQL 8.4 documents
this view as reserved storage-engine extension metadata for tablespaces, and
MySQL 8.4.9 emits `NULL` for the verified engine-attribute values.

This slice covers:

- MySQL 8.4.9-shaped `TABLESPACES_EXTENSIONS` result columns;
- fixed MySQL 8.4.9 default tablespace rows that are useful to applications
  probing the baseline InnoDB data dictionary surface;
- descriptor-owned MyLite persistent base-table rows named
  `<schema>/<table>`;
- matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
  metadata for the system view.

This slice does not implement physical tablespaces, file-per-table storage,
`INFORMATION_SCHEMA.FILES`, `INFORMATION_SCHEMA.INNODB_TABLESPACES`,
temporary-table rows, view rows, storage-engine attributes, privileges, or
mutable tablespace DDL.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tablespaces-extensions-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_information_schema_tablespaces_extensions_expectations.sh`.

The official page describes the view as primary-storage-engine tablespace
attribute metadata and says the engine-attribute column is reserved for future
use. Runtime verification against MySQL 8.4.9 adds the baseline row details
used by this feature:

- the default runtime returns rows for `mysql`, `innodb_system`,
  `innodb_temporary`, `innodb_undo_001`, `innodb_undo_002`, and
  `sys/sys_config`;
- a persistent user base table adds one row named `<schema>/<table>`;
- a view over that table does not add a row;
- a temporary table does not add a row;
- `ENGINE_ATTRIBUTE` is `NULL` in all verified rows.

Observed MySQL 8.4.9 system-view metadata:

| `INFORMATION_SCHEMA.TABLES.TABLE_TYPE` | `ENGINE` | `VERSION` | `ROW_FORMAT` | `TABLE_ROWS` | `DATA_LENGTH` |
| --- | --- | --- | --- | --- | --- |
| `SYSTEM VIEW` | `NULL` | `10` | `NULL` | `0` | `0` |

Observed MySQL 8.4.9 column metadata:

| Column | Ordinal | Null | Data type | Character length | Octet length | Character set | Collation | Column type |
| --- | ---: | --- | --- | ---: | ---: | --- | --- | --- |
| `TABLESPACE_NAME` | 1 | `NO` | `varchar` | 268 | 804 | `utf8mb3` | `utf8mb3_bin` | `varchar(268)` |
| `ENGINE_ATTRIBUTE` | 2 | `YES` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |

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

`TABLESPACES_EXTENSIONS` emits rows with:

- `TABLESPACE_NAME` equal to the fixed baseline system tablespace name or the
  descriptor-owned persistent base table name formatted as `<schema>/<table>`;
- `ENGINE_ATTRIBUTE` as SQL `NULL`.

The fixed baseline system rows are:

- `innodb_system`;
- `innodb_temporary`;
- `innodb_undo_001`;
- `innodb_undo_002`;
- `mysql`;
- `sys/sys_config`.

Persistent MyLite base tables emit one descriptor-backed row. MyLite views do
not emit rows. Temporary tables do not emit rows in this slice.

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
- Runtime/analyzer: adds a static table definition and row builders beside the
  existing information-schema metadata views.
- Catalog metadata: reads schema and table descriptors through existing
  catalog APIs.
- Result builder: unchanged.
- Storage/VFS/SQLite: unchanged. This is MyLite wrapper/translation metadata
  behavior and uses no SQLite fork hook.

## Performance

System rows are a fixed small array. User rows are proportional to
descriptor-owned persistent base tables. No user table data or SQLite system
tables are scanned.

## Tests

MySQL 8.4.9 expectation coverage:

- column labels for `SELECT *`;
- fixed default rows and `NULL` engine attributes;
- one user base-table row formatted as `<schema>/<table>`;
- absence of view and temporary-table rows;
- system-view metadata in `INFORMATION_SCHEMA.TABLES`;
- system-column metadata in `INFORMATION_SCHEMA.COLUMNS`;
- statement status after a successful filtered read.

MyLite C coverage:

- fixed baseline system rows;
- descriptor-owned persistent base-table rows and absence of view rows;
- projection, predicates, `ORDER BY`, `LIMIT`, and `COUNT(*)`;
- selected `information_schema` resolution;
- system-view rows in `INFORMATION_SCHEMA.TABLES`;
- system-column rows in `INFORMATION_SCHEMA.COLUMNS`;
- unknown-column diagnostics.

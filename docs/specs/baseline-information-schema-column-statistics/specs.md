# Baseline INFORMATION_SCHEMA COLUMN_STATISTICS

## Summary

MyLite exposes a limited queryable synthetic
`INFORMATION_SCHEMA.COLUMN_STATISTICS` system view. MySQL 8.4 documents this
view as access to optimizer histogram metadata for table columns. MyLite does
not yet implement histogram descriptors or `ANALYZE TABLE ... UPDATE
HISTOGRAM`, so this slice provides the MySQL-shaped empty catalog surface that
client tools can probe without treating the table as missing.

This slice covers:

- MySQL 8.4.9-shaped `COLUMN_STATISTICS` result columns;
- zero rows for baseline schemas, persistent base tables, views, and temporary
  tables until histogram descriptors are designed;
- matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
  metadata for the system view;
- existing information-schema query features such as projection, `COUNT(*)`,
  supported predicates, `ORDER BY`, `LIMIT`, aliases, and selected
  `information_schema` resolution.

This slice does not implement histograms, histogram DDL, optimizer statistics,
privilege filtering, physical data-dictionary tables, or storage-engine
statistics.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMN_STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-column-statistics-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_information_schema_column_statistics_expectations.sh`.

The official page describes the table as histogram statistics metadata with
columns `SCHEMA_NAME`, `TABLE_NAME`, `COLUMN_NAME`, and `HISTOGRAM`. Runtime
verification against MySQL 8.4.9 adds the baseline details used by this
feature:

- a fresh MySQL 8.4.9 runtime returns zero `COLUMN_STATISTICS` rows;
- creating a database, base table, view, and temporary table does not add rows
  without histogram creation;
- successful filtered reads produce no warnings and leave `ROW_COUNT()` at
  `-1`;
- after `ANALYZE TABLE ... UPDATE HISTOGRAM`, MySQL emits a histogram row for
  the analyzed column, with `HISTOGRAM` as a JSON object. MyLite records this
  as future work rather than part of this slice.

Observed MySQL 8.4.9 system-view metadata:

| `INFORMATION_SCHEMA.TABLES.TABLE_TYPE` | `ENGINE` | `VERSION` | `ROW_FORMAT` | `TABLE_ROWS` | `DATA_LENGTH` |
| --- | --- | --- | --- | --- | --- |
| `SYSTEM VIEW` | `NULL` | `10` | `NULL` | `0` | `0` |

Observed MySQL 8.4.9 column metadata:

| Column | Ordinal | Default | Null | Data type | Character length | Octet length | Character set | Collation | Column type |
| --- | ---: | --- | --- | --- | ---: | ---: | --- | --- | --- |
| `SCHEMA_NAME` | 1 | `NULL` | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `TABLE_NAME` | 2 | `NULL` | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_bin` | `varchar(64)` |
| `COLUMN_NAME` | 3 | `NULL` | `NO` | `varchar` | 64 | 192 | `utf8mb3` | `utf8mb3_tolower_ci` | `varchar(64)` |
| `HISTOGRAM` | 4 | `NULL` | `NO` | `json` | `NULL` | `NULL` | `NULL` | `NULL` | `json` |

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

`COLUMN_STATISTICS` exposes four metadata columns:

- `SCHEMA_NAME`;
- `TABLE_NAME`;
- `COLUMN_NAME`;
- `HISTOGRAM`.

Until MyLite has histogram descriptors, the view emits no rows. This includes
the built-in schemas, MyLite persistent base tables, MyLite views, and session
temporary tables. Empty results must still expose the correct column names and
allow valid predicates over the four columns.

No natural row order is claimed. Future histogram rows must use explicit
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
- Runtime/analyzer: adds a static table definition beside the existing
  information-schema metadata views.
- Catalog metadata: no schema, table, column, or histogram descriptors are read
  for row production in this slice.
- Result builder: unchanged.
- Storage/VFS/SQLite: unchanged. This is MyLite wrapper/translation metadata
  behavior and uses no SQLite fork hook.

## Performance

The row builder is empty, so queries only materialize definition metadata and
run existing predicate/projection code. No user table data, SQLite system
tables, or storage-engine statistics are scanned.

## Tests

MySQL 8.4.9 expectation coverage:

- zero baseline rows;
- zero rows after creating a user base table, view, and temporary table without
  histogram creation;
- system-view metadata in `INFORMATION_SCHEMA.TABLES`;
- system-column metadata in `INFORMATION_SCHEMA.COLUMNS`;
- statement status after a successful filtered read;
- a MySQL-only future-work probe showing that explicit histogram creation
  produces a JSON histogram row.

MyLite C coverage:

- empty result column names for wildcard and explicit projections;
- `COUNT(*)` returns `0` for baseline and user schemas;
- supported predicates over every column return zero rows without treating the
  columns as unknown;
- selected `information_schema` resolution;
- system-view rows in `INFORMATION_SCHEMA.TABLES`;
- system-column rows in `INFORMATION_SCHEMA.COLUMNS`;
- unknown-column diagnostics.

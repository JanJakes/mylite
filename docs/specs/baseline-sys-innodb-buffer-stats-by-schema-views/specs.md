# Baseline sys.innodb_buffer_stats_by_schema Views

This slice adds MySQL-shaped metadata and deterministic read-only empty rows for
the `sys.innodb_buffer_stats_by_schema` and
`sys.x$innodb_buffer_stats_by_schema` views. MySQL uses these sys views to
summarize `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` rows grouped by object
schema. MyLite does not expose a live InnoDB buffer-pool page inventory, so
both views are metadata-complete empty placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.innodb_buffer_stats_by_schema` and
  `sys.x$innodb_buffer_stats_by_schema`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-innodb-buffer-stats-by-schema.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-buffer-page-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_innodb_buffer_stats_by_schema_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, live-row presence, view definition metadata, table
dependency metadata, routine dependency absence, index/constraint absence,
selected-schema behavior, `SHOW CREATE` rendering, `SHOW TABLE STATUS`, and
read status behavior. Direct MySQL rows are environment-dependent because they
reflect live InnoDB buffer-pool contents.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.innodb_buffer_stats_by_schema;
SELECT * FROM sys.`x$innodb_buffer_stats_by_schema`;

USE sys;
SELECT * FROM innodb_buffer_stats_by_schema;
SELECT * FROM `x$innodb_buffer_stats_by_schema`;
```

Both views return zero rows. This differs from a live MySQL server with InnoDB
buffer-pool page rows, but preserves the queryable sys view surface without
inventing schema names, page counts, byte totals, or cached-row estimates.

## Column Metadata

`sys.innodb_buffer_stats_by_schema` has seven columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `object_schema` | `text` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `allocated` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `data` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `pages` | `bigint` | `NO` | `0` | SQL `NULL` |
| `pages_hashed` | `bigint` | `NO` | `0` | SQL `NULL` |
| `pages_old` | `bigint` | `NO` | `0` | SQL `NULL` |
| `rows_cached` | `decimal(45,0)` | `YES` | `NULL` | SQL `NULL` |

`sys.x$innodb_buffer_stats_by_schema` has the same columns except that byte
totals are raw decimal values and `rows_cached` is non-null with a default of
`0`:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `object_schema` | `text` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `allocated` | `decimal(44,0)` | `YES` | `NULL` | SQL `NULL` |
| `data` | `decimal(44,0)` | `YES` | `NULL` | SQL `NULL` |
| `pages` | `bigint` | `NO` | `0` | SQL `NULL` |
| `pages_hashed` | `bigint` | `NO` | `0` | SQL `NULL` |
| `pages_old` | `bigint` | `NO` | `0` | SQL `NULL` |
| `rows_cached` | `decimal(45,0)` | `NO` | `0` | SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape, including text byte lengths,
formatted `varchar(11)` byte lengths, signed `BIGINT` numeric precision, and
decimal precision. The views have no index or constraint metadata, so
`SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
`KEY_COLUMN_USAGE`, and `TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes both built-in rows with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=TEMPTABLE` and the `mysql.sys@localhost` definer. The formatted view
uses `format_bytes()` for allocated and data bytes; the raw `x$` view exposes
the corresponding decimal sums. Both definitions filter out buffer pages with a
`NULL` table name, group by `object_schema`, and order by descending allocated
bytes.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports both views depend on
`information_schema.INNODB_BUFFER_PAGE`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for these two views.
This matches the observed MySQL 8.4.9 runtime, which does not expose the
unqualified `format_bytes()` call as a routine dependency for this view pair.

## Unsupported Behavior

This slice intentionally does not implement:

- live `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` storage-engine rows;
- InnoDB buffer-pool page accounting by schema;
- formatted byte calculation or cached-row aggregation;
- execution of `format_bytes()`, `locate()`, `substring_index()`, or `replace()`
  through the built-in view definition;
- privilege filtering, definer validation, SQL SECURITY enforcement, or view
  execution;
- physical SQLite views or persisted catalog descriptors for built-in sys
  views;
- broader sys view execution.

Writes to the views remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, quoted identifiers for `x$` view names, `SHOW COLUMNS`,
`SHOW INDEX`, `SHOW CREATE VIEW`, `SHOW CREATE TABLE`, and
`INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic system-table descriptor table with
  two `sys` view entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and returns no rows after validating the expected column count.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`, and
  `VIEW_TABLE_USAGE` rows plus empty routine, index, and constraint metadata
  through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Both views return empty rows without scanning SQLite data tables or collecting
InnoDB buffer-pool page statistics.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata for formatted and raw views;
- selected-schema access and live-row presence without depending on variable
  buffer-pool contents;
- view and table-dependency metadata and absence of routine dependencies;
- empty index and constraint metadata for both view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after reads.

MyLite runtime coverage mirrors the supported empty placeholder rows,
selected-schema access, and metadata surfaces for both views.

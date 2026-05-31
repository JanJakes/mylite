# Baseline sys.schema_unused_indexes View

This slice adds MySQL-shaped metadata and deterministic read-only rows for
`sys.schema_unused_indexes`. MySQL reports non-unique, non-primary indexes whose
Performance Schema index-usage counter is still zero. MyLite has persistent
index descriptors but does not collect Performance Schema index-usage events, so
it treats supported user-table non-unique indexes as zero-use rows and documents
that rows are not removed after an index is read.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_unused_indexes`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-unused-indexes.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_unused_indexes_view_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, view definition metadata, table dependency metadata,
empty routine dependency metadata, selected-schema access, SHOW metadata, and
row behavior before and after a secondary index is read.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.schema_unused_indexes;

USE sys;
SELECT object_schema, object_name, index_name
  FROM schema_unused_indexes
 WHERE object_schema = 'app'
 ORDER BY index_name;
```

MyLite returns one row for each supported persistent user base-table index that
is non-unique, non-primary, and has at least one key part. Supported index kinds
are ordinary secondary indexes, metadata-only `FULLTEXT` indexes, and
metadata-only `SPATIAL` indexes. Unique indexes, primary keys, views, temporary
tables, and built-in schemas are excluded.

MySQL removes a row after Performance Schema records a read against that index.
MyLite does not maintain those counters, so the row set remains descriptor
backed and stable until the index descriptor changes.

## Column Metadata

The view has three columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `object_schema` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `object_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `index_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The view has no indexes or
constraints, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows for the view object.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes the built-in row with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'YES'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=MERGE` and the `mysql.sys@localhost` definer. The stored definition
models the MySQL join from
`performance_schema.table_io_waits_summary_by_index_usage` to
`information_schema.STATISTICS`, with filters for non-null index name,
`COUNT_STAR = 0`, non-`mysql` schemas, non-primary indexes, non-unique indexes,
and first key parts.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependencies on
`information_schema.STATISTICS` and
`performance_schema.table_io_waits_summary_by_index_usage`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema index-usage event collection;
- removal of rows after MyLite reads through an index;
- runtime validation of MySQL optimizer index choices;
- temporary-table rows;
- privilege filtering, definer validation, or SQL SECURITY enforcement;
- physical SQLite views or persisted catalog descriptors for built-in sys
  views;
- broader sys view execution.

Writes to the view remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE VIEW`,
`SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic system-table descriptor table with
  the `sys.schema_unused_indexes` view entry.
- Query execution: reuses the existing synthetic system-table SELECT planner and
  adds a descriptor-backed row builder for user base-table non-unique indexes.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`, and
  `VIEW_TABLE_USAGE` rows plus empty index, constraint, and routine-dependency
  metadata through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Reads of the view scan catalog table and index descriptors only. They do not
scan user data or SQLite storage rows. No new dependencies or background
collectors are introduced.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata;
- user-schema row behavior before and after a secondary index read;
- exclusion of primary and unique indexes, and inclusion of non-unique secondary
  and `FULLTEXT` indexes;
- view and table-dependency metadata;
- empty routine-dependency, index, and constraint metadata for the view object;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after a sys view read.

MyLite runtime coverage mirrors the supported metadata and descriptor-backed
row behavior, including the intentionally stable row set after an index read.

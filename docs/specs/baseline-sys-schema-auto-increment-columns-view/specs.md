# Baseline sys.schema_auto_increment_columns View

This slice adds a MySQL-shaped `sys.schema_auto_increment_columns` baseline.
MyLite exposes the sys view as a read-only synthetic catalog over persistent
user-schema base tables that have an `AUTO_INCREMENT` column. The view is useful
for schema health probes and follows the existing synthetic system-table query
surface.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_auto_increment_columns`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-auto-increment-columns.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.VIEWS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-views-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_auto_increment_columns_view_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, empty-table `AUTO_INCREMENT` handling, view
definition metadata, dependency metadata, and selected-schema access.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.schema_auto_increment_columns;

USE sys;
SELECT table_schema, table_name, auto_increment
  FROM schema_auto_increment_columns;
```

The view returns one row for each supported persistent user base table whose
column metadata has `EXTRA = 'auto_increment'`. Rows from `mysql`, `sys`,
`INFORMATION_SCHEMA`, and `performance_schema` are excluded. MyLite currently
has no physical system-schema base tables, so the exclusion primarily documents
the MySQL-compatible boundary for future catalog growth.

The row values are derived from MyLite descriptors:

- `table_schema`, `table_name`, and `column_name` are the descriptor names.
- `data_type` and `column_type` match the existing
  `INFORMATION_SCHEMA.COLUMNS` and `SHOW COLUMNS` rendering.
- `is_signed` is `1` unless the column type contains `unsigned`.
- `is_unsigned` is `1` when the column type contains `unsigned`.
- `max_value` is the MySQL integer-family maximum for the signedness and data
  type.
- `auto_increment` is the descriptor's next value when MyLite has evidence that
  MySQL would expose a value for the table. Empty default tables report `NULL`.
- `auto_increment_ratio` is `auto_increment / max_value`, formatted with four
  decimal places, or `NULL` when `auto_increment` is `NULL`.

The MySQL view definition includes a default ordering by descending usage ratio
and then maximum value. MyLite emits rows in the same order before applying an
explicit query `ORDER BY`.

## Column Metadata

`sys.schema_auto_increment_columns` has ten columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `table_schema` | `varchar(64)` | `NO` | `NULL` | `utf8mb3_bin` |
| `table_name` | `varchar(64)` | `NO` | `NULL` | `utf8mb3_bin` |
| `column_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb3_tolower_ci` |
| `data_type` | `longtext` | `YES` | `NULL` | `utf8mb3_bin` |
| `column_type` | `mediumtext` | `NO` | `NULL` | `utf8mb3_bin` |
| `is_signed` | `int` | `NO` | `0` | SQL `NULL` |
| `is_unsigned` | `int` | `NO` | `0` | SQL `NULL` |
| `max_value` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `auto_increment` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `auto_increment_ratio` | `decimal(25,4) unsigned` | `YES` | `NULL` | SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The view has no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes a built-in row for the view with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `TABLE_NAME = 'schema_auto_increment_columns'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=MERGE` and `DEFINER=\`mysql.sys\`@\`localhost\``. Qualified targets
render the view name as
`` `sys`.`schema_auto_increment_columns` ``. Unqualified targets resolved after
`USE sys` render `` `schema_auto_increment_columns` ``.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports two dependency rows for
`INFORMATION_SCHEMA.COLUMNS` and `INFORMATION_SCHEMA.TABLES`.
`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- the broader sys schema view catalog;
- exact InnoDB statistics-cache behavior for all empty-table
  `AUTO_INCREMENT` edge cases;
- temporary-table rows;
- privilege filtering, definer validation, or SQL SECURITY enforcement;
- physical SQLite views or persisted catalog descriptors for built-in sys
  views;
- sys helper functions outside the static `SHOW CREATE` definition text.

Writes to the view remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No grammar changes are required. The feature uses existing qualified and
selected-schema table references, `SHOW COLUMNS`, `SHOW INDEX`,
`SHOW CREATE VIEW`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` query
support.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic system-table descriptor table with a
  `schema_name = 'sys'`, `name = 'schema_auto_increment_columns'` view entry.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds descriptor-backed rows from catalog schemas, base tables, and
  columns.
- Information schema: adds `COLUMNS`, `VIEWS`, and `VIEW_TABLE_USAGE` rows
  through the existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

The view scans catalog table descriptors and column descriptors, then sorts the
small synthetic row set. It performs no SQLite table scan except the existing
row-count check used to decide whether an empty default table should expose an
`AUTO_INCREMENT` value.

## Tests

MySQL 8.4.9 expectation coverage:

- column, table, view, dependency, and empty index/constraint metadata;
- direct qualified and selected-schema reads;
- signed, unsigned, empty default, and explicit `AUTO_INCREMENT` row values;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after a synthetic view read.

MyLite runtime coverage mirrors the expectation cases and adds focused checks
for default ordering, empty default tables, and selected-schema reads.

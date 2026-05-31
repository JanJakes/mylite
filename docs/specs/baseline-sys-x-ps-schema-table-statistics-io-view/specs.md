# Baseline sys.x$ps_schema_table_statistics_io View

This slice adds MySQL-shaped metadata and deterministic read-only rows for the
`sys.x$ps_schema_table_statistics_io` helper view. MySQL uses this helper view
under the table-statistics sys views to aggregate file-I/O counters from
`performance_schema.file_summary_by_instance`. MyLite does not collect
Performance Schema file-I/O summaries, so the view exposes descriptor-backed
table inventory with raw zero counters.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_table_statistics` and
  `sys.x$schema_table_statistics`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-table-statistics.html>
- MySQL 8.4 Reference Manual, Performance Schema file I/O summary tables:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-file-summary-tables.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_x_ps_schema_table_statistics_io_view_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, view definition metadata, table and routine
dependency metadata, index/constraint absence, `SHOW TABLE STATUS`, selected
schema behavior, and read status behavior. Direct MySQL row values are
environment-dependent because they reflect live file-summary counters.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.`x$ps_schema_table_statistics_io`;

USE sys;
SELECT * FROM `x$ps_schema_table_statistics_io`;
```

MyLite emits one row per known descriptor base table for:

- supported `mysql` system-table descriptors;
- `sys.sys_config`;
- persistent user base-table descriptors.

Temporary tables, views, unsupported Performance Schema tables, and unsupported
system-table descriptors are omitted. Every emitted row reports raw numeric
`0` text for file read, write, and miscellaneous I/O counters because MyLite
does not currently maintain Performance Schema file summary counters.

## Column Metadata

The view has ten columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `table_schema` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `table_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `count_read` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `sum_number_of_bytes_read` | `decimal(41,0)` | `YES` | `NULL` | SQL `NULL` |
| `sum_timer_read` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `count_write` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `sum_number_of_bytes_write` | `decimal(41,0)` | `YES` | `NULL` | SQL `NULL` |
| `sum_timer_write` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `count_misc` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `sum_timer_misc` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The view has no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows for the view object.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes a built-in row with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=TEMPTABLE` and the `mysql.sys@localhost` definer. The definition
selects schema and table names using the `sys.extract_schema_from_file_name`
and `sys.extract_table_from_file_name` helper functions, sums read/write/misc
file counters from `performance_schema.file_summary_by_instance`, and groups
by the extracted schema and table names.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports a dependency on
`performance_schema.file_summary_by_instance`.
`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports dependencies on
`sys.extract_schema_from_file_name` and `sys.extract_table_from_file_name`.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema file summary collection;
- live row production from `performance_schema.file_summary_by_instance`;
- execution of `sys.extract_schema_from_file_name()` or
  `sys.extract_table_from_file_name()`;
- live byte, timer, read, write, or miscellaneous I/O accumulation;
- temporary-table rows;
- privilege filtering, definer validation, or SQL SECURITY enforcement;
- physical SQLite views or persisted catalog descriptors for built-in sys
  views;
- broader sys view execution.

Writes to the view remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, quoted identifiers for `x$` view names, `SHOW COLUMNS`,
`SHOW INDEX`, `SHOW CREATE VIEW`, `SHOW CREATE TABLE`, and
`INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic system-table descriptor table with
  a `sys.x$ps_schema_table_statistics_io` view entry.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds descriptor-backed base-table rows.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`,
  `VIEW_TABLE_USAGE`, and `VIEW_ROUTINE_USAGE` rows plus empty index and
  constraint metadata through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

The view scans supported mysql/sys system-table descriptors and persistent
catalog table descriptors. It performs no SQLite data-table scan and collects
no live file-I/O counters.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata;
- selected-schema access and live-row presence without depending on variable
  counter values;
- view, table-dependency, and routine-dependency metadata;
- empty index and constraint metadata for the view object;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after a view read.

MyLite runtime coverage mirrors the supported metadata, selected-schema, and
descriptor-backed row cases, including zero-counter user and system table rows.

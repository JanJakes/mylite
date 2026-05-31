# Baseline sys.ps_check_lost_instrumentation View

This slice adds MySQL-shaped metadata and a deterministic empty read-only row
set for `sys.ps_check_lost_instrumentation`. MySQL exposes Performance Schema
status variables whose names match lost-instrumentation counters and whose
values are greater than zero. MyLite does not collect Performance Schema
instrumentation-loss counters, so it exposes the view shape and dependency
metadata while returning no rows.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.ps_check_lost_instrumentation`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-ps-check-lost-instrumentation.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_ps_check_lost_instrumentation_view_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, empty result behavior, view definition metadata,
table dependency metadata, empty routine dependency metadata,
selected-schema access, SHOW metadata, and status values after a direct read.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.ps_check_lost_instrumentation;

USE sys;
SELECT * FROM ps_check_lost_instrumentation;
```

MyLite returns zero rows. This matches the local MySQL 8.4.9 target when no
Performance Schema lost-instrumentation status variable has a positive value.
MyLite does not synthesize positive loss counters and does not expose dynamic
Performance Schema status rows.

## Column Metadata

The view has two columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `variable_name` | `varchar(64)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `variable_value` | `varchar(1024)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |

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
models a filter over `performance_schema.global_status` for variable names
matching `perf%lost` and positive values.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports one dependency on
`performance_schema.global_status`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema lost-instrumentation counters;
- positive rows from `performance_schema.global_status`;
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
  the `sys.ps_check_lost_instrumentation` view entry.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and returns an empty row set after validating the view shape.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`, and
  `VIEW_TABLE_USAGE` rows plus empty index, constraint, and routine-dependency
  metadata through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Reads of the view allocate only the result metadata and do not scan catalog,
user data, or SQLite storage rows. No new dependencies or background collectors
are introduced.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata;
- empty direct-read row behavior;
- view and table-dependency metadata;
- empty routine-dependency, index, and constraint metadata for the view object;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after a sys view read.

MyLite runtime coverage mirrors the supported metadata and empty placeholder
row behavior.

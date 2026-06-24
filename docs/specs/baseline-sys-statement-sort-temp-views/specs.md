# Baseline sys Statement Sorting And Temp Table Views

This slice adds MySQL-shaped metadata and empty read-only placeholder rowsets
for these sys statement digest diagnostic views:

- `sys.statements_with_sorting`
- `sys.x$statements_with_sorting`
- `sys.statements_with_temp_tables`
- `sys.x$statements_with_temp_tables`

MySQL defines these views over
`performance_schema.events_statements_summary_by_digest`. The formatted views
use `sys.format_statement()` and formatted latency text, while the `x$` views
expose the raw digest text and timer counters. MyLite does not yet collect
Performance Schema statement digest events, so this slice exposes the MySQL
metadata surface and returns zero rows rather than inventing statement history.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `statements_with_sorting` and
  `x$statements_with_sorting`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-statements-with-sorting.html>
- MySQL 8.4 Reference Manual, `statements_with_temp_tables` and
  `x$statements_with_temp_tables`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-statements-with-temp-tables.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_statement_sort_temp_views_expectations.sh`.

Runtime probes verified column metadata, view definitions, dependency metadata,
table/status metadata, empty index/constraint metadata, selected-schema access,
read row counts, warnings, and `ROW_COUNT()` behavior.

## Supported Behavior

Supported reads:

```sql
SELECT * FROM sys.statements_with_sorting;
SELECT * FROM sys.`x$statements_with_sorting`;
SELECT * FROM sys.statements_with_temp_tables;
SELECT * FROM sys.`x$statements_with_temp_tables`;
```

Each query returns zero rows. This mirrors MyLite's current absence of
Performance Schema statement digest collection without exposing misleading sort
or temporary-table diagnostics.

## Metadata

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose MySQL 8.4.9-shaped columns.
`statements_with_sorting` and `x$statements_with_sorting` have 13 columns.
`statements_with_temp_tables` and `x$statements_with_temp_tables` have 11
columns. Formatted views expose `total_latency` as `varchar(11)`; raw `x$`
views expose it as `bigint unsigned`.

The views have no index or constraint metadata, so `SHOW INDEX`,
`INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

`INFORMATION_SCHEMA.VIEWS` exposes built-in rows with `CHECK_OPTION = 'NONE'`,
MySQL-observed `IS_UPDATABLE = 'YES'`, `DEFINER = 'mysql.sys@localhost'`,
and `SECURITY_TYPE = 'INVOKER'`. MyLite still blocks writes through the
built-in schema write guard.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports each view depends on
`performance_schema.events_statements_summary_by_digest`. Formatted non-`x$`
views also depend on `sys.sys_config`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports `sys.format_statement` for the
formatted non-`x$` views and no routine rows for the raw `x$` variants.

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped built-in view
definitions with `ALGORITHM=MERGE`.

## Unsupported Behavior

This slice intentionally does not implement:

- `performance_schema.events_statements_summary_by_digest` collection;
- statement digest normalization, aggregation, timing, sort counters, or
  temporary-table counters;
- filtering based on live digest rows;
- sys helper-function execution for row production;
- privilege filtering, definer validation, SQL SECURITY enforcement, or true
  updatable-view writes;
- broader sys view execution.

Writes remain blocked by the built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE VIEW`,
`SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the existing sys summary statement descriptor
  provider.
- Query execution: validates the descriptor shape and appends no rows.
- SHOW metadata: reuses synthetic system-table column/index paths and built-in
  sys view `SHOW CREATE` paths.
- Storage/SQLite: unchanged.

## Performance

Row reads are O(1), allocate no statement history, and do not scan SQLite user
data.

## Tests

MySQL 8.4.9 expectation coverage verifies metadata, dependencies, selected
schema access, `SHOW CREATE`, empty rowsets, warnings, and `ROW_COUNT()`.
MyLite runtime coverage verifies the same surfaces against the synthetic
descriptors.

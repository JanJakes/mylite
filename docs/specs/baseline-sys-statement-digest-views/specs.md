# Baseline sys Statement Digest Views

This slice adds MySQL-shaped metadata and empty read-only placeholder rowsets
for these sys statement-digest diagnostic views:

- `sys.statement_analysis`
- `sys.x$statement_analysis`
- `sys.statements_with_errors_or_warnings`
- `sys.x$statements_with_errors_or_warnings`
- `sys.statements_with_full_table_scans`
- `sys.x$statements_with_full_table_scans`
- `sys.statements_with_runtimes_in_95th_percentile`
- `sys.x$statements_with_runtimes_in_95th_percentile`

MySQL defines these views over
`performance_schema.events_statements_summary_by_digest`, with formatted views
using sys helper functions such as `format_statement()`,
`format_pico_time()`, and `format_bytes()`. MyLite does not yet collect
statement digest events or Performance Schema latency counters, so this slice
does not synthesize fake statement history. It exposes the metadata surface and
returns zero rows until real digest aggregation exists.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `statement_analysis` and
  `x$statement_analysis`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-statement-analysis.html>
- MySQL 8.4 Reference Manual, `statements_with_errors_or_warnings` and
  `x$statements_with_errors_or_warnings`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-statements-with-errors-or-warnings.html>
- MySQL 8.4 Reference Manual, `statements_with_full_table_scans` and
  `x$statements_with_full_table_scans`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-statements-with-full-table-scans.html>
- MySQL 8.4 Reference Manual,
  `statements_with_runtimes_in_95th_percentile` and
  `x$statements_with_runtimes_in_95th_percentile`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-statements-with-runtimes-in-95th-percentile.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_statement_digest_views_expectations.sh`.

Runtime probes verified column metadata, view definitions, dependency metadata,
table/status metadata, empty index/constraint metadata, selected-schema access,
read row counts, warnings, and `ROW_COUNT()` behavior.

## Supported Behavior

Supported reads:

```sql
SELECT * FROM sys.statement_analysis;
SELECT * FROM sys.`x$statement_analysis`;
SELECT * FROM sys.statements_with_errors_or_warnings;
SELECT * FROM sys.`x$statements_with_errors_or_warnings`;
SELECT * FROM sys.statements_with_full_table_scans;
SELECT * FROM sys.`x$statements_with_full_table_scans`;
SELECT * FROM sys.statements_with_runtimes_in_95th_percentile;
SELECT * FROM sys.`x$statements_with_runtimes_in_95th_percentile`;
```

Each query returns zero rows. This mirrors MyLite's current absence of
Performance Schema statement digest collection without inventing diagnostics
that could mislead applications.

## Metadata

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose MySQL 8.4.9-shaped columns. Formatted views
use formatted latency and byte text columns; `x$` views expose raw unsigned
integer timer and byte counters where MySQL does.

The views have no index or constraint metadata, so `SHOW INDEX`,
`INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

`INFORMATION_SCHEMA.VIEWS` exposes built-in rows with `CHECK_OPTION = 'NONE'`,
MySQL-observed `IS_UPDATABLE = 'YES'`, `DEFINER = 'mysql.sys@localhost'`,
and `SECURITY_TYPE = 'INVOKER'`. MyLite still blocks writes through the
built-in schema write guard.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports:

- each view depends on
  `performance_schema.events_statements_summary_by_digest`;
- formatted non-`x$` views also depend on `sys.sys_config`;
- percentile views also depend on
  `sys.x$ps_digest_95th_percentile_by_avg_us`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports `sys.format_statement` for the
four formatted non-`x$` views and no routine rows for the raw `x$` variants.

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped built-in view
definitions with `ALGORITHM=MERGE`.

## Unsupported Behavior

This slice intentionally does not implement:

- `performance_schema.events_statements_summary_by_digest` collection;
- statement digest normalization, aggregation, timing, memory, row, sort, or
  warning/error counters;
- percentile helper execution beyond the existing empty helper metadata;
- filtering based on live digest rows;
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
- Runtime metadata: adds a dedicated sys summary statement descriptor provider.
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

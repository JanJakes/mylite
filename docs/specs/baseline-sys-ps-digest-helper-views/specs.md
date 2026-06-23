# Baseline sys Digest Helper Views

This slice adds MySQL-shaped metadata and deterministic read-only empty rows for
the `sys.x$ps_digest_avg_latency_distribution` and
`sys.x$ps_digest_95th_percentile_by_avg_us` helper views. MySQL uses these
helpers under the statement-runtime percentile sys views. MyLite does not
collect Performance Schema statement digest summaries, so both views are
metadata-complete empty placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `statements_with_runtimes_in_95th_percentile`
  and helper-view note:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-statements-with-runtimes-in-95th-percentile.html>
- MySQL 8.4 Reference Manual, sys schema views:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-views.html>
- MySQL 8.4 Reference Manual, Performance Schema statement summary tables:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-statement-summary-tables.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_ps_digest_helper_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, live-row presence, view definition metadata, table
dependency metadata, routine dependency absence, index/constraint absence,
selected-schema behavior, `SHOW CREATE` rendering, `SHOW TABLE STATUS`, and
read status behavior. Direct MySQL row values are environment-dependent because
they reflect live Performance Schema statement digest instrumentation.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.`x$ps_digest_avg_latency_distribution`;
SELECT * FROM sys.`x$ps_digest_95th_percentile_by_avg_us`;

USE sys;
SELECT * FROM `x$ps_digest_avg_latency_distribution`;
SELECT * FROM `x$ps_digest_95th_percentile_by_avg_us`;
```

Both views return zero rows. This differs from a live MySQL server when
statement digest instrumentation has rows, but preserves the queryable sys
helper surface without inventing statement-latency histograms.

## Column Metadata

`sys.x$ps_digest_avg_latency_distribution` has two columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `cnt` | `bigint` | `NO` | `0` | SQL `NULL` |
| `avg_us` | `decimal(21,0)` | `YES` | `NULL` | SQL `NULL` |

`sys.x$ps_digest_95th_percentile_by_avg_us` has two columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `avg_us` | `decimal(21,0)` | `YES` | `NULL` | SQL `NULL` |
| `percentile` | `decimal(46,4)` | `NO` | `0.0000` | SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The views have no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

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
`ALGORITHM=TEMPTABLE` and the `mysql.sys@localhost` definer. The
average-latency distribution helper groups statement digests by rounded
microsecond average latency. The percentile helper joins the distribution
helper to find the first cumulative percentile above 0.95.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports:

- `x$ps_digest_avg_latency_distribution` depends on
  `performance_schema.events_statements_summary_by_digest`;
- `x$ps_digest_95th_percentile_by_avg_us` depends on
  `performance_schema.events_statements_summary_by_digest` and
  `sys.x$ps_digest_avg_latency_distribution`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for these helper views.

## Unsupported Behavior

This slice intentionally does not implement:

- live `performance_schema.events_statements_summary_by_digest` rows;
- statement digest collection, latency histograms, or percentile calculations;
- execution of the aggregate expressions through the built-in view
  definitions;
- dependent `statements_with_runtimes_in_95th_percentile` view execution;
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
- Runtime metadata: extends the synthetic sys schema descriptor table with two
  helper view entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and returns no rows after validating the expected column counts.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`, and
  `VIEW_TABLE_USAGE` rows plus empty routine, index, and constraint metadata
  through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Both helper views return empty rows without scanning SQLite data tables or
collecting Performance Schema statement digest statistics.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata for both helper views;
- selected-schema access and live-row presence without depending on variable
  statement-digest row values;
- view and table-dependency metadata and absence of routine dependencies;
- empty index and constraint metadata for both view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after reads.

MyLite runtime coverage mirrors the supported empty placeholder rows,
selected-schema access, and metadata surfaces for both views.

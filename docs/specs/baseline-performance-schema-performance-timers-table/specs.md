# Baseline Performance Schema Performance Timers Table

## Scope

MyLite exposes `performance_schema.performance_timers` as a read-only built-in
metadata table. The table is queryable and returns the five MySQL 8.4.9 timer
names with deterministic non-NULL timer values.

This slice implements:

- `SELECT` reads from `performance_schema.performance_timers`
- selected-schema reads after `USE performance_schema`
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and `SHOW INDEX`
- `INFORMATION_SCHEMA.COLUMNS`, `STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, `TABLE_CONSTRAINTS_EXTENSIONS`, and `TABLES`
- `SHOW TABLE STATUS`
- built-in-schema write protection

It does not implement live Performance Schema instrumentation, runtime timer
calibration, platform-specific cycle frequency, event timing collection, or
mutable Performance Schema state.

## Compatibility Sources

- MySQL 8.4 Reference Manual, The performance_timers Table:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-performance-timers-table.html
- MySQL 8.4.9 runtime probes against local container `mylite-mysql-849`.

Observed MySQL 8.4.9 behavior:

- The table has four columns: `TIMER_NAME`, `TIMER_FREQUENCY`,
  `TIMER_RESOLUTION`, and `TIMER_OVERHEAD`.
- `TIMER_NAME` is
  `enum('CYCLE','NANOSECOND','MICROSECOND','MILLISECOND','THREAD_CPU')`
  using `utf8mb4_0900_ai_ci`, is `NOT NULL`, and is not indexed.
- The numeric columns are nullable `bigint` columns with precision `19` and
  scale `0`.
- `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` return no rows for the table.
- `INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` report
  `PERFORMANCE_SCHEMA`, version `10`, row format `Fixed`, `Rows`/`TABLE_ROWS`
  as `5`, `AUTO_INCREMENT` as `NULL`, and `utf8mb4_0900_ai_ci` collation.
- The table returns the timer names `CYCLE`, `NANOSECOND`, `MICROSECOND`,
  `MILLISECOND`, and `THREAD_CPU`. MySQL's numeric timer values are non-NULL
  but platform-dependent.

Representative runtime probes:

```sql
SHOW FULL COLUMNS FROM performance_schema.performance_timers;
SHOW INDEX FROM performance_schema.performance_timers;
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_TYPE
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME = 'performance_timers'
 ORDER BY ORDINAL_POSITION;
SELECT TIMER_NAME, TIMER_FREQUENCY IS NULL, TIMER_RESOLUTION IS NULL,
       TIMER_OVERHEAD IS NULL
  FROM performance_schema.performance_timers
 ORDER BY TIMER_NAME;
SHOW TABLE STATUS FROM performance_schema LIKE 'performance_timers';
```

## MyLite Semantics

The table definition is a catalog-owned built-in system-table descriptor.
Metadata surfaces use the descriptor rather than a SQLite storage table.

Rows are synthesized at query time with deterministic placeholder timer values:

| TIMER_NAME | TIMER_FREQUENCY | TIMER_RESOLUTION | TIMER_OVERHEAD |
| --- | ---: | ---: | ---: |
| `CYCLE` | `1000000000` | `1` | `1` |
| `NANOSECOND` | `1000000000` | `1` | `1` |
| `MICROSECOND` | `1000000` | `1` | `1` |
| `MILLISECOND` | `1000` | `1` | `1` |
| `THREAD_CPU` | `1000000000` | `1` | `1` |

The placeholder values are intentionally stable because MyLite does not yet
measure or expose live event timings. Compatibility tests compare MySQL for the
schema, row names, non-NULL timer value contract, and metadata, while MyLite
tests assert the deterministic embedded values.

## Parser And Storage

No new SQL grammar is required. Existing metadata-query parsing covers:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
qualified_table_name ::= ident DOT ident.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
```

No SQLite table is created. The implementation is a MyLite metadata provider
and query-row dispatcher backed by static descriptors. No SQLite fork hook is
required.

## Diagnostics And Write Access

The table inherits built-in-schema write protection. `INSERT`, `UPDATE`,
`DELETE`, `REPLACE`, `CREATE`, `DROP`, `ALTER`, `TRUNCATE`, index DDL, and
rename attempts targeting `performance_schema` continue to return MySQL-shaped
access-denied diagnostics for built-in schemas.

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_performance_timers_expectations.sh`
  verifies MySQL 8.4.9 column/index/constraint/table metadata and row-name
  behavior without depending on platform-specific timer values.
- `packages/libmylite/tests/runtime_performance_schema_performance_timers_test.c`
  verifies MyLite query rows, deterministic placeholder values, selected-schema
  resolution, metadata surfaces, write protection, and row-count state.

## Known Gaps

- Timer values are deterministic placeholders rather than calibrated runtime or
  platform values.
- MyLite does not collect event timings or populate other live Performance
  Schema timing/instrumentation tables.

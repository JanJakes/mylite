# Baseline Performance Schema instrumentation placeholders

## Scope

This baseline covers four MySQL 8.4.9 Performance Schema tables whose rows are
derived from server instrumentation that MyLite does not yet collect:

- `performance_schema.binary_log_transaction_compression_stats`
- `performance_schema.data_locks`
- `performance_schema.data_lock_waits`
- `performance_schema.prepared_statements_instances`

The implemented surface is metadata-compatible and read-only. MyLite exposes
table descriptors, column metadata, primary-key and secondary-index metadata
where MySQL exposes them, table-status metadata, empty selected rows,
selected-schema resolution, and built-in-schema write protection.

## Compatibility authority

The specification is based on the MySQL 8.4 Reference Manual pages for the
covered Performance Schema tables:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-binary-log-transaction-compression-stats-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-data-locks-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-data-lock-waits-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-prepared-statements-instances-table.html>

Runtime metadata was verified against a MySQL 8.4.9 container named
`mylite-mysql-849` with:

```sql
SELECT VERSION();
SELECT 'binary_log_transaction_compression_stats', COUNT(*)
  FROM performance_schema.binary_log_transaction_compression_stats;
SELECT 'data_locks', COUNT(*) FROM performance_schema.data_locks;
SELECT 'data_lock_waits', COUNT(*) FROM performance_schema.data_lock_waits;
SELECT 'prepared_statements_instances', COUNT(*)
  FROM performance_schema.prepared_statements_instances;
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
       COLUMN_KEY, COLLATION_NAME, CHARACTER_SET_NAME,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, COLUMN_DEFAULT,
       EXTRA, PRIVILEGES
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('binary_log_transaction_compression_stats',
                      'data_locks',
                      'data_lock_waits',
                      'prepared_statements_instances')
 ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                'data_locks', 'data_lock_waits',
                'prepared_statements_instances'),
          ORDINAL_POSITION;
SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
       COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('binary_log_transaction_compression_stats',
                      'data_locks',
                      'data_lock_waits',
                      'prepared_statements_instances')
 ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                'data_locks', 'data_lock_waits',
                'prepared_statements_instances'),
          INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('binary_log_transaction_compression_stats',
                      'data_locks',
                      'data_lock_waits',
                      'prepared_statements_instances')
 ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                'data_locks', 'data_lock_waits',
                'prepared_statements_instances'),
          CONSTRAINT_NAME;
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, AUTO_INCREMENT, TABLE_COLLATION
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('binary_log_transaction_compression_stats',
                      'data_locks',
                      'data_lock_waits',
                      'prepared_statements_instances')
 ORDER BY FIELD(TABLE_NAME, 'binary_log_transaction_compression_stats',
                'data_locks', 'data_lock_waits',
                'prepared_statements_instances');
```

Observed MySQL 8.4.9 metadata:

- `binary_log_transaction_compression_stats` has 14 columns covering binary or
  relay log type, compression type, transaction counters, byte counters,
  compression percentage, first/last transaction ids, byte counts, and
  `timestamp(6)` first/last timestamps. It has no indexes or table
  constraints.
- `data_locks` has 15 columns. Its primary key is
  `ENGINE_LOCK_ID, ENGINE`; secondary HASH indexes cover
  `ENGINE_TRANSACTION_ID, ENGINE`, `THREAD_ID, EVENT_ID`, and
  `OBJECT_SCHEMA, OBJECT_NAME, PARTITION_NAME, SUBPARTITION_NAME`.
- `data_lock_waits` has 11 columns. Its primary key is
  `REQUESTING_ENGINE_LOCK_ID, BLOCKING_ENGINE_LOCK_ID, ENGINE`; secondary HASH
  indexes cover both requesting and blocking lock ids, transaction ids, and
  thread/event pairs.
- `prepared_statements_instances` has 40 columns. Its primary key is
  `OBJECT_INSTANCE_BEGIN`; secondary HASH indexes cover `STATEMENT_ID`,
  `STATEMENT_NAME`, `OWNER_OBJECT_TYPE, OWNER_OBJECT_SCHEMA,
  OWNER_OBJECT_NAME`, and the unique `OWNER_THREAD_ID, OWNER_EVENT_ID` pair.
- All four tables are `BASE TABLE` objects with
  `ENGINE='PERFORMANCE_SCHEMA'`, `ROW_FORMAT='Dynamic'`,
  `AUTO_INCREMENT=NULL`, and `TABLE_COLLATION='utf8mb4_0900_ai_ci'`.
- The target runtime returned zero rows for all four tables in the baseline
  container.

## MyLite behavior

MyLite exposes all four tables as read-only synthetic Performance Schema
tables. Each table returns an empty result set. This is intentional: MyLite
does not currently maintain binary-log or relay-log compression statistics,
InnoDB-style data lock wait graphs, or Performance Schema prepared-statement
instrumentation.

The placeholders let application and diagnostic SQL discover the expected table
shape, join against the tables, and distinguish "no instrumentation rows" from
"table does not exist".

## Explicit gaps

- No binary log, relay log, transaction-compression counters, or reset/truncate
  behavior is implemented.
- No live data-lock or data-lock-wait rows are collected from SQLite or MyLite
  transaction internals.
- No prepared-statement instrumentation rows, timers, statement counters, memory
  maxima, or stored-program ownership rows are exposed.
- `TRUNCATE TABLE` and other write-like operations remain blocked by MyLite's
  built-in schema write-protection policy rather than MySQL's per-table
  Performance Schema mutation rules.

## Runtime and storage design

This is descriptor-only metadata plus empty query dispatch. It uses MyLite's
existing built-in table descriptor framework. No SQLite table, file format
change, public SQLite extension API, targeted SQLite fork hook, or new
dependency is required.

No new SQL grammar is required. Existing metadata query parsing covers:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
qualified_table_name ::= ident DOT ident.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
show_table_status_statement ::= SHOW TABLE STATUS from_schema_opt show_filter_opt.
```

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_instrumentation_placeholders_expectations.sh`
  verifies MySQL 8.4.9 row counts, column, index, constraint, and table
  metadata.
- `packages/libmylite/tests/runtime_performance_schema_instrumentation_placeholders_test.c`
  verifies MyLite empty query results, metadata surfaces, selected-schema
  resolution, and write protection.

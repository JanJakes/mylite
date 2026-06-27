# Baseline Performance Schema Connection Tables

## Scope

MyLite exposes limited current-handle metadata for these MySQL 8.4.9
Performance Schema tables:

- `performance_schema.accounts`
- `performance_schema.hosts`
- `performance_schema.users`
- `performance_schema.processlist`
- `performance_schema.threads`

The tables are read-only built-in metadata tables. They mirror MySQL's column,
index, constraint, `SHOW COLUMNS`, `SHOW INDEX`, `INFORMATION_SCHEMA`, and
`SHOW TABLE STATUS` shapes. Row reads synthesize deterministic snapshots from
MyLite's embedded processlist registry rather than from full Performance Schema
instrumentation.

## Compatibility Sources

- MySQL 8.4 Reference Manual, Performance Schema Connection Tables:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-connection-tables.html
- MySQL 8.4 Reference Manual, Performance Schema Processlist Table:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-processlist-table.html
- MySQL 8.4 Reference Manual, Performance Schema Threads Table:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-threads-table.html
- MySQL 8.4.9 runtime probes against local container `mylite-mysql-849`.

Observed MySQL 8.4.9 behavior:

- `accounts` has nullable `USER char(32)` / `HOST char(255)`, non-null
  `CURRENT_CONNECTIONS bigint`, `TOTAL_CONNECTIONS bigint`,
  `MAX_SESSION_CONTROLLED_MEMORY bigint unsigned`, and
  `MAX_SESSION_TOTAL_MEMORY bigint unsigned`. It has a `HASH` unique key named
  `ACCOUNT` over `(USER, HOST)`.
- `hosts` has nullable `HOST char(255)` plus the same four non-null counter
  columns. It has a `HASH` unique key named `HOST` over `HOST`.
- `users` has nullable `USER char(32)` plus the same four non-null counter
  columns. It has a `HASH` unique key named `USER` over `USER`.
- `processlist` has a `HASH` primary key on `ID bigint unsigned` and exposes
  MySQL processlist columns plus nullable `EXECUTION_ENGINE enum('PRIMARY',
  'SECONDARY')`.
- `threads` has a `HASH` primary key on `THREAD_ID bigint unsigned` and
  secondary `HASH` indexes on `NAME`, `PROCESSLIST_ID`, `PROCESSLIST_HOST`,
  `(PROCESSLIST_USER, PROCESSLIST_HOST)`, `THREAD_OS_ID`, and
  `RESOURCE_GROUP`.
- `accounts`, `hosts`, and `users` report `ROW_FORMAT = Fixed`; `processlist`
  and `threads` report `ROW_FORMAT = Dynamic`.

Representative runtime probes:

```sql
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
       COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL')
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('accounts', 'hosts', 'users', 'processlist', 'threads')
 ORDER BY TABLE_NAME, ORDINAL_POSITION;

SHOW INDEX FROM performance_schema.threads;

SELECT ID = CONNECTION_ID(), USER, DB, COMMAND, EXECUTION_ENGINE
  FROM performance_schema.processlist
 WHERE ID = CONNECTION_ID();

SELECT THREAD_ID > 0, NAME, TYPE, PROCESSLIST_ID = CONNECTION_ID(),
       PROCESSLIST_USER, PROCESSLIST_DB, PROCESSLIST_COMMAND, INSTRUMENTED,
       HISTORY, EXECUTION_ENGINE, TELEMETRY_ACTIVE
  FROM performance_schema.threads
 WHERE PROCESSLIST_ID = CONNECTION_ID();
```

## MyLite Semantics

The table definitions are catalog-owned built-in system-table descriptors.
Metadata surfaces use the same descriptor path as existing `performance_schema`
baseline tables.

Rows are synthesized at query time from
`mylite_connection_collect_processlist_sessions()`:

- `processlist` emits one row per open MyLite handle. The current handle has
  `COMMAND = 'Query'`, `STATE = 'executing'`, current SQL text in `INFO`, and
  `EXECUTION_ENGINE = 'PRIMARY'`. Other open handles have `COMMAND = 'Sleep'`
  and no current statement fields.
- `threads` emits one foreground thread row per open MyLite handle using the
  MyLite connection id as `THREAD_ID` and `PROCESSLIST_ID`. It exposes
  `thread/sql/one_connection`, user, selected schema, command, current SQL text,
  deterministic instrumentation flags, zero memory counters, and no operating
  system thread id.
- `accounts` emits one limited `root@%` aggregate row counting currently open
  MyLite handles.
- `hosts` emits one limited `%` aggregate row counting currently open MyLite
  handles.
- `users` emits one limited `root` aggregate row counting currently open MyLite
  handles.

Connection summary rows use current open-handle counts for both
`CURRENT_CONNECTIONS` and `TOTAL_CONNECTIONS`. MyLite does not persist
disconnected connection history in this slice.

## Parser And Storage

No new SQL grammar is required. Existing metadata-query parsing covers:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
qualified_table_name ::= ident DOT ident.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
```

No SQLite storage table is created. The implementation is a MyLite
wrapper/translation-layer metadata provider backed by connection snapshots. No
SQLite fork hook is required.

## Diagnostics And Write Access

The tables inherit built-in-schema write protection. `INSERT`, `UPDATE`,
`DELETE`, `REPLACE`, `CREATE`, `DROP`, `ALTER`, `TRUNCATE`, index DDL, and
rename attempts targeting `performance_schema` continue to return MySQL-shaped
access-denied diagnostics for built-in schemas.

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_connection_tables_expectations.sh`
  verifies MySQL 8.4.9 metadata and representative row behavior.
- `packages/libmylite/tests/runtime_performance_schema_connection_tables_test.c`
  verifies MyLite row synthesis, metadata surfaces, selected-schema resolution,
  write protection, and row-count state.

## Known Gaps

- MyLite does not implement full Performance Schema instrumentation, statement
  history, wait/stage/transaction timing, memory accounting, operating-system
  thread ids, thread resource groups, privilege-sensitive visibility, or
  instrumentation filters.
- MyLite does not keep disconnected account/host/user history, so
  `TOTAL_CONNECTIONS` is a current embedded handle count in this slice.
- MyLite uses the embedded identity `root@%`, host `%`, and user `root` for
  summary rows.

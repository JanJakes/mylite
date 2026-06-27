# Baseline Performance Schema instance tables

## Scope

This baseline covers MySQL 8.4.9 Performance Schema instance tables that expose
server synchronization and I/O object instances:

- `performance_schema.cond_instances`
- `performance_schema.file_instances`
- `performance_schema.mutex_instances`
- `performance_schema.rwlock_instances`
- `performance_schema.socket_instances`

The implemented surface includes MySQL-shaped table descriptors, column
metadata, primary and secondary HASH index metadata, table-status metadata,
empty read-only result sets, selected-schema resolution, and built-in-schema
write protection.

## Compatibility authority

The specification is based on the MySQL 8.4 Reference Manual pages for
Performance Schema instance tables:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-cond-instances-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-file-instances-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-mutex-instances-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-rwlock-instances-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-socket-instances-table.html>

Runtime metadata was verified against a MySQL 8.4.9 container named
`mylite-mysql-849` with:

```sql
SELECT VERSION();
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
       COLUMN_KEY, COLLATION_NAME
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('cond_instances', 'mutex_instances',
                      'rwlock_instances', 'file_instances',
                      'socket_instances')
 ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances',
                'rwlock_instances', 'file_instances', 'socket_instances'),
          ORDINAL_POSITION;
SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
       COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('cond_instances', 'mutex_instances',
                      'rwlock_instances', 'file_instances',
                      'socket_instances')
 ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances',
                'rwlock_instances', 'file_instances', 'socket_instances'),
          INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('cond_instances', 'mutex_instances',
                      'rwlock_instances', 'file_instances',
                      'socket_instances')
 ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances',
                'rwlock_instances', 'file_instances', 'socket_instances'),
          CONSTRAINT_NAME;
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, AUTO_INCREMENT, TABLE_COLLATION
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('cond_instances', 'mutex_instances',
                      'rwlock_instances', 'file_instances',
                      'socket_instances')
 ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances',
                'rwlock_instances', 'file_instances', 'socket_instances');
```

Observed MySQL 8.4.9 metadata:

- `cond_instances` has `NAME varchar(128)` and
  `OBJECT_INSTANCE_BEGIN bigint unsigned`, with a primary key on
  `OBJECT_INSTANCE_BEGIN` and a HASH index on `NAME`.
- `mutex_instances` has the same object columns plus nullable
  `LOCKED_BY_THREAD_ID bigint unsigned`, with HASH indexes on `NAME` and
  `LOCKED_BY_THREAD_ID`.
- `rwlock_instances` has the same object columns plus nullable
  `WRITE_LOCKED_BY_THREAD_ID bigint unsigned` and
  `READ_LOCKED_BY_COUNT int unsigned`, with HASH indexes on `NAME` and
  `WRITE_LOCKED_BY_THREAD_ID`.
- `file_instances` has `FILE_NAME varchar(512)`, `EVENT_NAME varchar(128)`,
  and `OPEN_COUNT int unsigned`, with a primary key on `FILE_NAME` and a HASH
  index on `EVENT_NAME`.
- `socket_instances` has seven columns: `EVENT_NAME`,
  `OBJECT_INSTANCE_BEGIN`, `THREAD_ID`, `SOCKET_ID`, `IP`, `PORT`, and
  `STATE enum('IDLE','ACTIVE')`, with a primary key on
  `OBJECT_INSTANCE_BEGIN`, HASH indexes on `THREAD_ID` and `SOCKET_ID`, and a
  composite HASH index on `IP, PORT`.
- All five tables are `BASE TABLE` objects with
  `ENGINE='PERFORMANCE_SCHEMA'`, `ROW_FORMAT='Dynamic'`,
  `AUTO_INCREMENT=NULL`, and `TABLE_COLLATION='utf8mb4_0900_ai_ci'`.

## MyLite behavior

MyLite exposes all five tables as read-only synthetic Performance Schema
tables. Each table returns an empty result set. MyLite does not instrument
internal mutexes, condition variables, rwlocks, sockets, or server file I/O
objects, so it does not fabricate rows from unrelated embedded state.

The tables are still useful as compatibility placeholders: applications and
diagnostic queries can discover the expected table shapes, join against them,
or test for absence of live instrumentation rows without receiving a missing
table error.

## Explicit gaps

- No live Performance Schema instance instrumentation is implemented.
- No object addresses, lock ownership, open-file counters, socket states, or
  thread relationships are exposed.
- `TRUNCATE TABLE` and other write-like operations remain blocked by MyLite's
  built-in schema write-protection policy rather than MySQL's more specific
  per-table Performance Schema mutation rules.

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
```

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_instance_tables_expectations.sh`
  verifies MySQL 8.4.9 column, index, constraint, and table metadata.
- `packages/libmylite/tests/runtime_performance_schema_instance_tables_test.c`
  verifies MyLite empty query results, metadata surfaces, selected-schema
  resolution, and write protection.

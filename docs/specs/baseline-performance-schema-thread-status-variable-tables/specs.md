# Baseline Performance Schema Thread And Status Summary Tables

## Scope

MyLite exposes a limited current-handle implementation for these MySQL 8.4.9
Performance Schema tables:

- `performance_schema.variables_by_thread`
- `performance_schema.status_by_thread`
- `performance_schema.status_by_account`
- `performance_schema.status_by_host`
- `performance_schema.status_by_user`

The tables are read-only built-in metadata tables. They mirror MySQL's column,
index, constraint, `SHOW COLUMNS`, `SHOW INDEX`, `INFORMATION_SCHEMA`, and
`SHOW TABLE STATUS` shapes. Row reads synthesize current MyLite session
snapshots from the existing system-variable and status registries.

## Compatibility Sources

- MySQL 8.4 Reference Manual, Performance Schema System Variable Tables:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-system-variable-tables.html
- MySQL 8.4 Reference Manual, Performance Schema Status Variable Tables:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-status-variable-tables.html
- MySQL 8.4 Reference Manual, Performance Schema Summary Tables:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema-summary-tables.html
- MySQL 8.4.9 runtime probes against local container `mylite-mysql-849`.

Observed MySQL 8.4.9 behavior:

- `variables_by_thread` and `status_by_thread` have `THREAD_ID bigint unsigned
  NOT NULL`, `VARIABLE_NAME varchar(64) NOT NULL`, and nullable
  `VARIABLE_VALUE varchar(1024)`.
- Both thread tables have a `HASH` primary key on
  `(THREAD_ID, VARIABLE_NAME)`.
- `status_by_account` has nullable `USER char(32)` / `HOST char(255)`,
  `VARIABLE_NAME varchar(64) NOT NULL`, and nullable
  `VARIABLE_VALUE varchar(1024)`, with a `HASH` unique key named `ACCOUNT` on
  `(USER, HOST, VARIABLE_NAME)`.
- `status_by_host` has nullable `HOST char(255)`, `VARIABLE_NAME varchar(64)
  NOT NULL`, and nullable `VARIABLE_VALUE varchar(1024)`, with a `HASH` unique
  key named `HOST` on `(HOST, VARIABLE_NAME)`.
- `status_by_user` has nullable `USER char(32)`, `VARIABLE_NAME varchar(64)
  NOT NULL`, and nullable `VARIABLE_VALUE varchar(1024)`, with a `HASH` unique
  key named `USER` on `(USER, VARIABLE_NAME)`.
- `variables_by_thread` and `status_by_thread` contain foreground active-session
  rows. The status summary tables aggregate account, host, and user status
  information and can include `NULL` grouping rows.

Representative runtime probes:

```sql
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
       COLUMN_KEY
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('variables_by_thread', 'status_by_thread',
                      'status_by_account', 'status_by_host', 'status_by_user')
 ORDER BY TABLE_NAME, ORDINAL_POSITION;

SHOW INDEX FROM performance_schema.status_by_account;

SELECT VARIABLE_NAME, VARIABLE_VALUE
  FROM performance_schema.variables_by_thread
 WHERE THREAD_ID = (
       SELECT THREAD_ID
         FROM performance_schema.threads
        WHERE PROCESSLIST_ID = CONNECTION_ID())
   AND VARIABLE_NAME IN ('autocommit', 'time_zone')
 ORDER BY VARIABLE_NAME;
```

## MyLite Semantics

The table definitions are catalog-owned built-in system-table descriptors.
Metadata surfaces use the same descriptor path as existing `performance_schema`
baseline tables.

Rows are synthesized at query time:

- `variables_by_thread` emits current-session variable rows using MyLite's
  current system-variable registry and the current handle's connection id as
  `THREAD_ID`.
- `status_by_thread` emits current-session status rows using MyLite's current
  status registry and the current handle's connection id as `THREAD_ID`.
- `status_by_account` emits the same limited status rows for the embedded
  current account `root@%`.
- `status_by_host` emits the same limited status rows for the embedded host
  `%`.
- `status_by_user` emits the same limited status rows for the embedded user
  `root`.

Performance Schema status rows continue to omit `Com_%` command counters except
`Com_stmt_reprepare`, matching the existing Performance Schema variable/status
table slice.

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
wrapper/translation-layer metadata provider backed by in-memory descriptors.
No SQLite fork hook is required.

## Diagnostics And Write Access

The tables inherit built-in-schema write protection. `INSERT`, `UPDATE`,
`DELETE`, `REPLACE`, `CREATE`, `DROP`, `ALTER`, `TRUNCATE`, index DDL, and
rename attempts targeting `performance_schema` continue to return MySQL-shaped
access-denied diagnostics for built-in schemas.

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_thread_status_variable_tables_expectations.sh`
  verifies MySQL 8.4.9 metadata and representative row behavior.
- `packages/libmylite/tests/runtime_performance_schema_thread_status_variable_tables_test.c`
  verifies MyLite row synthesis, metadata surfaces, selected-schema resolution,
  write protection, and row-count state.

## Known Gaps

- MyLite does not expose cross-handle foreground threads in these rowsets.
- MyLite does not implement disconnected account/host/user aggregation,
  Performance Schema instrumentation filters, privilege-sensitive visibility,
  sensitive-variable masking, or status reset semantics.
- The rowsets are limited to MyLite's current variable/status registries rather
  than the full optional MySQL server/plugin universe.

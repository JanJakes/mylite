# Baseline Performance Schema Socket Summary Placeholders

## Scope

This slice adds MySQL 8.4.9-shaped Performance Schema metadata placeholders for:

- `performance_schema.socket_summary_by_event_name`
- `performance_schema.socket_summary_by_instance`

The tables are exposed as read-only Performance Schema base tables. MyLite
returns zero rows from each table until live Performance Schema socket
instrumentation exists. The metadata surface follows the MySQL 8.4.9 target
runtime for table names, columns, key/index metadata, information-schema rows,
`SHOW COLUMNS`, `DESCRIBE`, `SHOW INDEX`, and `SHOW TABLE STATUS`.

## Compatibility Sources

Official MySQL 8.4 reference pages used for the feature surface:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-socket-summary-tables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-characteristics.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-descriptions.html>

Expected metadata was verified against a local MySQL 8.4.9 runtime container:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names
```

The verification script for this slice is:

```sh
packages/libmylite/tests/mysql_baseline_performance_schema_socket_summary_placeholders_expectations.sh
```

## Semantics

MyLite implements these tables as metadata placeholders:

- `SELECT` from each table succeeds and returns an empty result set with the
  MySQL-shaped column list.
- `USE performance_schema` followed by unqualified reads resolves these tables.
- `COUNT(*)` returns `0`.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and `SHOW INDEX` expose the
  MySQL 8.4.9 metadata observed from the target runtime.
- `INFORMATION_SCHEMA.COLUMNS`, `STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, `TABLE_CONSTRAINTS_EXTENSIONS`, and `TABLES` include the
  observed metadata rows.
- `SHOW TABLE STATUS` exposes the same engine, row format, row estimate,
  auto-increment, and collation metadata as MySQL 8.4.9 for the covered fields.
- Schema, table, index, rename, truncate, and single-table DML writes targeting
  these Performance Schema tables return MyLite's existing Performance Schema
  access-denied diagnostic.

The `socket_summary_by_event_name` table has a primary HASH key on
`EVENT_NAME`. The `socket_summary_by_instance` table has a primary HASH key on
`OBJECT_INSTANCE_BEGIN` and a nonunique HASH index on `EVENT_NAME`.

## Intentional Gaps

This slice does not implement live Performance Schema socket I/O aggregation. A
MySQL 8.4.9 runtime exposes nonzero rows when socket instruments exist and
socket activity has been observed. MyLite intentionally returns empty rows until
it has a real socket instrumentation pipeline and an explicit summary lifecycle.
It also does not implement the MySQL `TRUNCATE TABLE` reset semantics for these
summary tables.

The placeholder approach uses MyLite-owned catalog descriptors and runtime
dispatch. It does not require a SQLite fork hook because the feature is metadata
surface plus empty fixed rows, not SQLite planner, storage, or type behavior.

## Test Plan

- Verify the expectation script syntax with `sh -n`.
- Run the expectation script against MySQL 8.4.9 to lock observed metadata.
- Build and run `mylite_runtime_performance_schema_socket_summary_placeholders_test`.
- Run the focused CTest filter
  `^libmylite\.runtime\.performance_schema_socket_summary_placeholders$`.
- Run the broader Performance Schema runtime CTest filter.
- Run `git diff --check`, `git diff --cached --check`, formatting checks, and
  the full `cmake --workflow --preset check` before committing.

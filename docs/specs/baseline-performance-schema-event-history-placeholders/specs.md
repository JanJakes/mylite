# Baseline Performance Schema Event History Placeholders

## Scope

This slice adds MySQL 8.4.9-shaped Performance Schema metadata placeholders for:

- `performance_schema.events_stages_current`
- `performance_schema.events_stages_history`
- `performance_schema.events_stages_history_long`
- `performance_schema.events_statements_history_long`
- `performance_schema.events_statements_summary_by_program`
- `performance_schema.events_waits_current`
- `performance_schema.events_waits_history`
- `performance_schema.events_waits_history_long`

The tables are exposed as read-only Performance Schema base tables. Reads return
zero rows in MyLite. The table names, columns, primary-key/index metadata,
information_schema rows, `SHOW COLUMNS`, `DESCRIBE`, `SHOW INDEX`, and
`SHOW TABLE STATUS` are shaped after the MySQL 8.4.9 target runtime.

## Compatibility Sources

Official MySQL 8.4 reference pages used for the feature surface:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-characteristics.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-descriptions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-wait-tables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-stage-tables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-statement-tables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-statement-summary-tables.html>

Expected metadata was verified against a local MySQL 8.4.9 runtime container:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names
```

The verification script for this slice is:

```sh
packages/libmylite/tests/mysql_baseline_performance_schema_event_history_placeholders_expectations.sh
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

The current/history stage and wait tables expose a two-column HASH primary key
on `THREAD_ID, EVENT_ID`. `events_statements_summary_by_program` exposes a
three-column HASH primary key on `OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME`.
The selected history-long tables do not expose indexes or constraints in the
target runtime.

## Intentional Gaps

This slice does not implement live Performance Schema instrumentation. MyLite
does not collect stage, wait, statement-history, stored-program, timer, nesting,
error, memory, or execution-engine event state for these tables. It also does
not implement Performance Schema truncation/reset semantics for event history or
summary tables. Those behaviors remain future compatibility work.

The placeholder approach uses MyLite-owned catalog descriptors and runtime
dispatch. It does not require a SQLite fork hook because the feature is metadata
surface plus empty fixed rows, not SQLite planner, storage, or type behavior.

## Test Plan

- Verify the expectation script syntax with `sh -n`.
- Run the expectation script against MySQL 8.4.9 to lock observed metadata.
- Build and run `mylite_runtime_performance_schema_event_history_placeholders_test`.
- Run the focused CTest filter
  `^libmylite\.runtime\.performance_schema_event_history_placeholders$`.
- Run the broader Performance Schema runtime CTest filter.
- Run `git diff --check`, `git diff --cached --check`, formatting checks, and
  the full `cmake --workflow --preset check` before committing.

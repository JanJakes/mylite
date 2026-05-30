# Baseline mysql replication metadata tables

This slice adds MyLite's baseline metadata surface for the MySQL replication
metadata repository tables:

- `mysql.slave_master_info`
- `mysql.slave_relay_log_info`
- `mysql.slave_worker_info`

The target MySQL 8.4.9 runtime exposes all three tables in the `mysql` schema
with zero rows in the standalone container used by MyLite's compatibility
suite.

## Compatibility Authority

- MySQL 8.4 Reference Manual, The mysql System Schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, Relay Log and Replication Metadata
  Repositories:
  <https://dev.mysql.com/doc/refman/8.4/en/replica-logs.html>
- MySQL 8.4 Reference Manual, Replication Metadata Repositories:
  <https://dev.mysql.com/doc/refman/8.4/en/replica-logs-status.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_replication_metadata_tables_expectations.sh`.

The MySQL manual describes the `slave_master_info` and
`slave_relay_log_info` tables as the connection and applier metadata
repositories for replica channels. It also describes `slave_worker_info` as the
applier worker metadata repository when applier metadata is table-backed.
Those repository tables use InnoDB and should not be modified manually.

## Supported Behavior

MyLite exposes the three tables as read-only empty system-table placeholders
with the MySQL-observed target-runtime shapes:

```sql
SELECT COUNT(*) FROM mysql.slave_master_info;
SELECT COUNT(*) FROM mysql.slave_relay_log_info;
SELECT COUNT(*) FROM mysql.slave_worker_info;
USE mysql;
SELECT COUNT(*) FROM slave_master_info;
```

These statements return zero rows or a count of `0`.

The table definitions expose the MySQL 8.4.9-observed columns, nullability,
defaults, collations, column comments, privileges, and primary keys through:

- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, and `DESCRIBE`
- `SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS`
- `INFORMATION_SCHEMA.COLUMNS`
- `INFORMATION_SCHEMA.TABLES`
- `INFORMATION_SCHEMA.STATISTICS`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`
- `SHOW TABLE STATUS`

The primary keys are:

```sql
PRIMARY KEY (Channel_name)              -- slave_master_info
PRIMARY KEY (Channel_name)              -- slave_relay_log_info
PRIMARY KEY (Channel_name, Id)          -- slave_worker_info
```

## Syntax

No parser change is required. Existing MyLite grammar already admits the
targeted statement shapes:

```lemon
select_stmt ::= SELECT select_options select_list from_clause select_tail.
from_clause ::= FROM table_factor.
table_factor ::= qualified_name alias_opt index_hint_list_opt.
cmd ::= SHOW show_columns_kind FROM qualified_name show_columns_tail.
cmd ::= SHOW show_index_kind FROM qualified_name show_index_tail.
cmd ::= SHOW TABLE STATUS show_table_status_tail.
cmd ::= DESCRIBE qualified_name describe_tail.
cmd ::= USE identifier.
```

## Semantics

The three tables are added to MyLite's supported `mysql` system-table
definition registry. Each table is read-only and returns no data rows. The
descriptors are synthetic; no physical SQLite table, replication channel,
binary log, relay log, applier worker state, credential store, or metadata
repository persistence is created.

Metadata values match the observed MySQL 8.4.9 target runtime where MyLite has
the corresponding metadata surface:

- `TABLE_TYPE = 'BASE TABLE'`
- `ENGINE = 'InnoDB'`
- `ROW_FORMAT = 'Dynamic'`
- `TABLE_ROWS = 0`
- `AVG_ROW_LENGTH = 0`
- `DATA_LENGTH = 16384`
- `INDEX_LENGTH = 0`
- `DATA_FREE = 4194304`
- `TABLE_COLLATION = 'utf8mb3_general_ci'`
- `CREATE_OPTIONS = 'row_format=DYNAMIC stats_persistent=0'`
- `TABLE_COMMENT` values:
  - `Master Information`
  - `Relay Log Information`
  - `Worker Information`

`CREATE_TIME` remains MyLite's existing statement-time placeholder for built-in
system-table status rows. Mutable replication repository row content is
intentionally unsupported.

## Diagnostics And Limits

- Writes to the tables remain blocked by existing built-in schema write
  protection.
- Index hints on direct reads remain unsupported for MyLite's current
  synthetic `mysql` system-table query path.
- MyLite does not implement replication channels, `CHANGE REPLICATION SOURCE
  TO`, `START REPLICA`, `RESET REPLICA`, `SHOW REPLICA STATUS`, binary log
  position persistence, GTID-only synchronization, relay logs, worker recovery,
  `PRIVILEGE_CHECKS_USER`, connection failover metadata, credential storage, or
  table-backed repository updates.
- Replication repository writes are stricter than MySQL's privileged internal
  repository maintenance path because MyLite has no replication subsystem.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: add supported `mysql` system-table definitions and their
  empty-row query behavior through the existing synthetic system-table path.
- Metadata: extend existing `INFORMATION_SCHEMA` and `SHOW` metadata
  generators for the new definitions.
- Catalog/storage/SQLite: unchanged; no physical table, SQLite extension
  point, or SQLite fork hook is required.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe verifies:

```sql
SELECT COUNT(*) FROM mysql.slave_master_info;
SELECT COUNT(*) FROM mysql.slave_relay_log_info;
SELECT COUNT(*) FROM mysql.slave_worker_info;
SHOW FULL COLUMNS FROM mysql.slave_master_info;
SHOW FULL COLUMNS FROM mysql.slave_relay_log_info;
SHOW FULL COLUMNS FROM mysql.slave_worker_info;
SHOW INDEX FROM mysql.slave_worker_info;
SELECT ...
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('slave_master_info',
                      'slave_relay_log_info',
                      'slave_worker_info');
SHOW TABLE STATUS FROM mysql
 WHERE Name IN ('slave_master_info',
                'slave_relay_log_info',
                'slave_worker_info');
```

The target runtime returned zero rows for all three tables, 33 columns for
`slave_master_info`, 15 columns for `slave_relay_log_info`, 13 columns for
`slave_worker_info`, single-column primary keys on `Channel_name` for the first
two tables, a composite primary key on `(Channel_name, Id)` for
`slave_worker_info`, and the metadata/status fields captured by the expectation
script.

## Test Plan

- Add a MySQL expectation script that verifies row counts, column metadata,
  primary-key metadata, constraints metadata, table-status metadata, and
  unqualified reads after `USE mysql`.
- Add focused C runtime coverage for direct reads, `SHOW COLUMNS`,
  `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`, information-schema metadata,
  `SHOW TABLE STATUS`, and selected-schema reads.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_replication_metadata_tables_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_replication_metadata_tables_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_replication_metadata_tables_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_replication_metadata_tables$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`

# Baseline mysql.ndb_binlog_index

This slice adds MyLite's baseline metadata surface for
`mysql.ndb_binlog_index`, the MySQL NDB Cluster replication binary-log index
table. The target MySQL 8.4.9 runtime exposes the table in the `mysql` schema
with zero rows in the standard non-NDB container used by MyLite's compatibility
suite.

## Compatibility Authority

- MySQL 8.4 Reference Manual, NDB Cluster Replication Schema and Tables:
  <https://dev.mysql.com/doc/refman/8.4/en/mysql-cluster-replication-schema.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_ndb_binlog_index_expectations.sh`.

The MySQL manual describes `mysql.ndb_binlog_index` as an NDB Cluster
replication table maintained by the NDB binary-log injector thread. It is local
to each MySQL server and uses InnoDB storage. The target MySQL 8.4.9 runtime
keeps the table present but empty when NDB Cluster replication is not active.

## Supported Behavior

MyLite exposes `mysql.ndb_binlog_index` as a read-only empty system-table
placeholder with the MySQL-observed target-runtime shape:

```sql
SELECT COUNT(*) FROM mysql.ndb_binlog_index;
SELECT * FROM mysql.ndb_binlog_index;
USE mysql;
SELECT COUNT(*) FROM ndb_binlog_index;
```

These statements return zero rows or a count of `0`.

The table exposes these columns, in order:

- `Position bigint unsigned NOT NULL`
- `File varchar(255) NOT NULL`
- `epoch bigint unsigned NOT NULL`
- `inserts int unsigned NOT NULL`
- `updates int unsigned NOT NULL`
- `deletes int unsigned NOT NULL`
- `schemaops int unsigned NOT NULL`
- `orig_server_id int unsigned NOT NULL`
- `orig_epoch bigint unsigned NOT NULL`
- `gci int unsigned NOT NULL`
- `next_position bigint unsigned NOT NULL`
- `next_file varchar(255) NOT NULL`

The primary key is ordered as:

```sql
PRIMARY KEY (epoch, orig_server_id, orig_epoch)
```

MyLite provides matching metadata through:

- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, and `DESCRIBE`
- `SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS`
- `INFORMATION_SCHEMA.COLUMNS`
- `INFORMATION_SCHEMA.TABLES`
- `INFORMATION_SCHEMA.STATISTICS`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`
- `SHOW TABLE STATUS`

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

`mysql.ndb_binlog_index` is added to MyLite's supported `mysql` system-table
definition registry. The table is read-only and returns no data rows. Its
descriptor is synthetic; no physical SQLite table, NDB Cluster integration,
binary-log injector, replication state, or table-maintenance behavior is
created.

Metadata values match the observed MySQL 8.4.9 target runtime where MyLite has
the corresponding metadata surface:

- `TABLE_TYPE = 'BASE TABLE'`
- `ENGINE = 'InnoDB'`
- `ROW_FORMAT = 'Dynamic'`
- `TABLE_ROWS = 0`
- `DATA_LENGTH = 16384`
- `INDEX_LENGTH = 0`
- `DATA_FREE = 4194304`
- `TABLE_COLLATION = 'latin1_swedish_ci'`
- `CREATE_OPTIONS = 'row_format=DYNAMIC stats_persistent=0'`
- `TABLE_COMMENT = ''`

`CREATE_TIME` remains MyLite's existing statement-time placeholder for built-in
system-table status rows. Mutable NDB binlog-index row content is intentionally
unsupported.

## Diagnostics And Limits

- Writes to `mysql.ndb_binlog_index` remain blocked by existing built-in schema
  write protection.
- Index hints on direct reads remain unsupported for MyLite's current synthetic
  `mysql` system-table query path.
- No NDB Cluster, NDB storage engine, binary logging, epoch mapping,
  `ndb_apply_status`, `ndb_replication`, failover metadata, or injector-thread
  behavior is implemented.
- MyLite does not persist or synthesize NDB event rows.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: add one supported `mysql` system-table definition and its
  empty-row query behavior through the existing synthetic system-table path.
- Metadata: extend existing `INFORMATION_SCHEMA` and `SHOW` metadata generators
  for the new definition.
- Catalog/storage/SQLite: unchanged; no physical table, SQLite extension point,
  or SQLite fork hook is required.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe verifies:

```sql
SELECT COUNT(*) FROM mysql.ndb_binlog_index;
SHOW COLUMNS FROM mysql.ndb_binlog_index;
SHOW INDEX FROM mysql.ndb_binlog_index;
SELECT ...
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME = 'ndb_binlog_index';
SHOW TABLE STATUS FROM mysql LIKE 'ndb_binlog_index';
```

The target runtime returned zero rows, the 12-column shape above, a three-part
primary key, and the metadata/status fields captured by the expectation script.

## Test Plan

- Add a MySQL expectation script that verifies row count, column metadata,
  primary-key metadata, constraints metadata, table-status metadata, and
  unqualified reads after `USE mysql`.
- Add focused C runtime coverage for direct reads, `SHOW COLUMNS`,
  `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`, information-schema metadata,
  `SHOW TABLE STATUS`, and selected-schema reads.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_ndb_binlog_index_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_ndb_binlog_index_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_ndb_binlog_index_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_ndb_binlog_index$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`

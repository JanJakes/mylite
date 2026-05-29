# Baseline mysql System Stats Table Status

This slice refines the built-in `mysql` schema table directory for the two
synthetic system tables that MyLite already supports as read-only data sources:
`mysql.innodb_index_stats` and `mysql.innodb_table_stats`. The tables continue
to be synthetic metadata rows, but their `INFORMATION_SCHEMA.TABLES` and
`SHOW TABLE STATUS` status fields now match the stable MySQL 8.4.9 values
observed for a fresh target runtime.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- Existing built-in schema table directory:
  `docs/specs/baseline-built-in-schema-table-directory/specs.md`
- Existing `mysql.innodb_table_stats` and `mysql.innodb_index_stats` specs:
  `docs/specs/baseline-mysql-innodb-table-stats/specs.md` and
  `docs/specs/baseline-mysql-innodb-index-stats/specs.md`
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_system_stats_table_status_expectations.sh`.

The MySQL manual documents `INFORMATION_SCHEMA.TABLES` and
`SHOW TABLE STATUS` as exposing table status and storage statistics. Runtime
checks against MySQL 8.4.9 show stable table-status values for the two InnoDB
persistent-statistics tables in a fresh target runtime, while creation and
update timestamps are non-`NULL` but host-runtime-specific.

## Supported Behavior

The following metadata surfaces are supported for the two stats tables:

```sql
SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats');

SHOW TABLE STATUS FROM mysql
 WHERE Name IN ('innodb_table_stats', 'innodb_index_stats');
```

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior for `INFORMATION_SCHEMA.TABLES` are inherited from the existing
information-schema query engine. `SHOW TABLE STATUS` reuses the existing
`LIKE` and limited `WHERE` filter support. No parser changes are required.

## Row Metadata

`INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose these stable values
for the supported rows:

| Table | Rows / TABLE_ROWS | Avg_row_length / AVG_ROW_LENGTH | Data_length / DATA_LENGTH | Max_data_length / MAX_DATA_LENGTH | Index_length / INDEX_LENGTH | Data_free / DATA_FREE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `innodb_index_stats` | `6` | `2730` | `16384` | `0` | `0` | `4194304` |
| `innodb_table_stats` | `2` | `8192` | `16384` | `0` | `0` | `4194304` |

For both rows:

- `TABLE_TYPE` is `BASE TABLE`;
- `ENGINE` is `InnoDB`;
- `VERSION` is `10`;
- `ROW_FORMAT` is `Dynamic`;
- `AUTO_INCREMENT`, `CHECK_TIME`, and `CHECKSUM` are SQL `NULL`;
- `CREATE_TIME` and `UPDATE_TIME` are non-`NULL` datetime strings;
- `TABLE_COLLATION` / `Collation` is `utf8mb3_bin`;
- `CREATE_OPTIONS` / `Create_options` is
  `row_format=DYNAMIC stats_persistent=0`;
- `TABLE_COMMENT` / `Comment` is the empty string.

MySQL's exact `CREATE_TIME` and `UPDATE_TIME` values are installation and
runtime dependent. MyLite renders both timestamp fields from the current
statement timestamp for these synthetic rows, preserving the non-`NULL`
datetime shape without inventing durable server startup or InnoDB update-time
state.

## Diagnostics And Limits

- This slice only refines the two supported synthetic stats tables. Other
  built-in `mysql` table-directory rows keep their existing placeholder status
  values unless a separate feature specifies them.
- Built-in directory visibility still does not make unsupported system tables
  queryable.
- The fixed row and length fields are MySQL 8.4.9 baseline metadata values,
  not live InnoDB statistics. Direct reads from `mysql.innodb_table_stats` and
  `mysql.innodb_index_stats` may expose descriptor-backed synthetic data, but
  table-status metadata remains a bounded MySQL-shaped directory snapshot for
  this slice.
- MyLite does not implement writable optimizer statistics, `ANALYZE TABLE`
  side effects, physical InnoDB page accounting, privilege filtering,
  checksum computation, or complete data-dictionary table status.
- Existing built-in schema write protection remains unchanged.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `INFORMATION_SCHEMA.TABLES` and
  `SHOW TABLE STATUS` syntax is reused.
- Analyzer/runtime: owns synthetic built-in table-status row construction and
  filter evaluation.
- Catalog metadata: unchanged. The rows are in-process built-in directory
  metadata, not persisted table descriptors.
- Storage/VFS/SQLite: unchanged. No physical `mysql` table, SQLite reflection,
  SQLite virtual table, or SQLite fork patch is introduced.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
       AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
       AUTO_INCREMENT IS NULL, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
       CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
       TABLE_COMMENT
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')
 ORDER BY TABLE_NAME;
SHOW TABLE STATUS FROM mysql LIKE 'innodb\_index\_stats';
SHOW TABLE STATUS FROM mysql LIKE 'innodb\_table\_stats';
```

Observed output:

```text
8.4.9
innodb_index_stats	BASE TABLE	InnoDB	10	Dynamic	6	2730	16384	0	0	4194304	1	1	0	1	utf8mb3_bin	1	row_format=DYNAMIC stats_persistent=0	<empty TABLE_COMMENT>
innodb_table_stats	BASE TABLE	InnoDB	10	Dynamic	2	8192	16384	0	0	4194304	1	1	0	1	utf8mb3_bin	1	row_format=DYNAMIC stats_persistent=0	<empty TABLE_COMMENT>
innodb_index_stats	InnoDB	10	Dynamic	6	2730	16384	0	0	4194304	NULL	2026-05-27 21:13:20	2026-05-29 13:57:24	NULL	utf8mb3_bin	NULL	row_format=DYNAMIC stats_persistent=0	<empty Comment>
innodb_table_stats	InnoDB	10	Dynamic	2	8192	16384	0	0	4194304	NULL	2026-05-27 21:13:20	2026-05-29 12:07:33	NULL	utf8mb3_bin	NULL	row_format=DYNAMIC stats_persistent=0	<empty Comment>
```

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_system_stats_table_status_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_system_stats_table_status|builtin_schema_table_directory|mysql_innodb_table_stats|mysql_innodb_index_stats)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_system_stats_table_status_expectations.sh
git diff --check
cmake --workflow --preset check
```

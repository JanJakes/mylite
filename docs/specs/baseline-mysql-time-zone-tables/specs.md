# Baseline mysql Time Zone Tables

This slice adds MySQL-shaped metadata for the five time-zone system tables in
the `mysql` schema:

- `mysql.time_zone`
- `mysql.time_zone_leap_second`
- `mysql.time_zone_name`
- `mysql.time_zone_transition`
- `mysql.time_zone_transition_type`

The tables already appear in MyLite's built-in `mysql` table directory. This
feature makes their column catalogs, primary-key metadata, table-status rows,
and direct read placeholders coherent with MySQL 8.4.9 system-table shape.
It does not load zoneinfo rows or implement named time-zone conversion rules.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, MySQL Server time zone support:
  <https://dev.mysql.com/doc/refman/8.4/en/time-zone-support.html>
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_time_zone_tables_expectations.sh`.

The MySQL manual identifies the five tables as the system tables that store
time-zone IDs, names, transitions, transition types, and leap-second data. It
also documents that MySQL creates these tables during installation but time
zone information must be loaded separately, normally from a system zoneinfo
database. The target MySQL 8.4.9 runtime used by MyLite has loaded time-zone
rows, so row contents are explicitly outside this small slice.

## Supported Behavior

The supported direct-read forms are metadata placeholders:

```sql
SELECT COUNT(*) FROM mysql.time_zone;
SELECT COUNT(*) FROM mysql.time_zone_leap_second;
SELECT COUNT(*) FROM mysql.time_zone_name;
SELECT COUNT(*) FROM mysql.time_zone_transition;
SELECT COUNT(*) FROM mysql.time_zone_transition_type;

USE mysql;
SELECT Name FROM time_zone_name ORDER BY Name;
```

MyLite returns zero rows from all five tables. This is a deliberate placeholder
until a later feature implements zoneinfo import, named time-zone storage,
cache invalidation, and named-zone timestamp conversion. The placeholder keeps
common discovery queries from failing while preserving an explicit documented
incompatibility with a populated MySQL server.

The supported metadata surfaces are:

```sql
SHOW COLUMNS FROM mysql.time_zone;
SHOW FULL COLUMNS FROM mysql.time_zone_transition_type;
DESCRIBE mysql.time_zone_name;

SHOW INDEX FROM mysql.time_zone;
SHOW INDEX FROM mysql.time_zone_transition;

SELECT ... FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME LIKE 'time_zone%';

SELECT ... FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME LIKE 'time_zone%';

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME LIKE 'time_zone%';

SELECT ... FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME LIKE 'time_zone%';

SELECT ... FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
 WHERE CONSTRAINT_SCHEMA = 'mysql'
   AND TABLE_NAME LIKE 'time_zone%';

SELECT ... FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME LIKE 'time_zone%';

SHOW TABLE STATUS FROM mysql LIKE 'time\_zone%';
```

Projection, aliases, `COUNT(*)`, limited `WHERE`, `ORDER BY`, and `LIMIT`
behavior are inherited from the existing MySQL-system-table and
information-schema query engines. Existing `SHOW COLUMNS`, `SHOW INDEX`, and
`SHOW TABLE STATUS` filters apply to the generated rows.

Existing `SHOW TABLES` / `SHOW FULL TABLES` directory behavior already lists
all five names as `BASE TABLE`.

## Column Metadata

`mysql.time_zone` has two columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `Time_zone_id` | `int unsigned` | `NO` | `PRI` | `NULL` | `auto_increment` | `NULL` |
| `Use_leap_seconds` | `enum('Y','N')` | `NO` | `` | `N` | `` | `utf8mb3_general_ci` |

`mysql.time_zone_leap_second` has two columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `Transition_time` | `bigint` | `NO` | `PRI` | `NULL` | `` | `NULL` |
| `Correction` | `int` | `NO` | `` | `NULL` | `` | `NULL` |

`mysql.time_zone_name` has two columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `Name` | `char(64)` | `NO` | `PRI` | `NULL` | `` | `utf8mb3_general_ci` |
| `Time_zone_id` | `int unsigned` | `NO` | `` | `NULL` | `` | `NULL` |

`mysql.time_zone_transition` has three columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `Time_zone_id` | `int unsigned` | `NO` | `PRI` | `NULL` | `` | `NULL` |
| `Transition_time` | `bigint` | `NO` | `PRI` | `NULL` | `` | `NULL` |
| `Transition_type_id` | `int unsigned` | `NO` | `` | `NULL` | `` | `NULL` |

`mysql.time_zone_transition_type` has five columns:

| Column | Type | Null | Key | Default | Extra | Collation |
| --- | --- | --- | --- | --- | --- | --- |
| `Time_zone_id` | `int unsigned` | `NO` | `PRI` | `NULL` | `` | `NULL` |
| `Transition_type_id` | `int unsigned` | `NO` | `PRI` | `NULL` | `` | `NULL` |
| `Offset` | `int` | `NO` | `` | `0` | `` | `NULL` |
| `Is_DST` | `tinyint unsigned` | `NO` | `` | `0` | `` | `NULL` |
| `Abbreviation` | `char(8)` | `NO` | `` | `` | `` | `utf8mb3_general_ci` |

`SHOW FULL COLUMNS` reports fixed privileges
`select,insert,update,references` and empty column comments for every column.
`INFORMATION_SCHEMA.COLUMNS` reports MySQL 8.4.9 ordinal positions, defaults,
nullability, data types, character lengths, numeric precision and scale,
character set and collation names, column types, key markers, extras,
privileges, empty comments, and empty generation expressions.

## Key And Constraint Metadata

The primary keys are:

- `mysql.time_zone`: `PRIMARY(Time_zone_id)`;
- `mysql.time_zone_leap_second`: `PRIMARY(Transition_time)`;
- `mysql.time_zone_name`: `PRIMARY(Name)`;
- `mysql.time_zone_transition`: `PRIMARY(Time_zone_id, Transition_time)`;
- `mysql.time_zone_transition_type`:
  `PRIMARY(Time_zone_id, Transition_type_id)`.

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` expose visible BTREE primary
key rows with `NON_UNIQUE = 0`, `COLLATION = 'A'`, empty `NULLABLE`, empty
storage-engine and index comments, `SUB_PART`, `PACKED`, and `EXPRESSION` as
SQL `NULL`, and `IS_VISIBLE = 'YES'`.

The target MySQL 8.4.9 runtime reported these cardinality placeholders:

| Table | Key part | Cardinality |
| --- | --- | ---: |
| `time_zone` | `Time_zone_id` | `1457` |
| `time_zone_leap_second` | `Transition_time` | `0` |
| `time_zone_name` | `Name` | `1712` |
| `time_zone_transition` | `Time_zone_id` | `1252` |
| `time_zone_transition` | `Transition_time` | `119074` |
| `time_zone_transition_type` | `Time_zone_id` | `1954` |
| `time_zone_transition_type` | `Transition_type_id` | `10529` |

These are deterministic MyLite metadata constants for this slice, not live
storage-engine estimates.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes one enforced primary-key row per
table. `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes ordered primary-key
column rows with no referenced table or column values.
`TABLE_CONSTRAINTS_EXTENSIONS` includes one `NULL`-attribute primary-key row
per table.

## Table Status

`INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose the MySQL-observed
target-runtime status shape:

| Table | Rows | Avg row length | Data length | Data free | Auto increment | Comment |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `time_zone` | `1815` | `45` | `81920` | `4194304` | `1796` | `Time zones` |
| `time_zone_leap_second` | `0` | `0` | `16384` | `4194304` | `NULL` | `Leap seconds information for time zones` |
| `time_zone_name` | `2311` | `113` | `262144` | `4194304` | `NULL` | `Time zone names` |
| `time_zone_transition` | `119074` | `39` | `4734976` | `4194304` | `NULL` | `Time zone transitions` |
| `time_zone_transition_type` | `9871` | `48` | `475136` | `4194304` | `NULL` | `Time zone transition types` |

All five rows report `ENGINE = 'InnoDB'`, `VERSION = 10`,
`ROW_FORMAT = 'Dynamic'`, `MAX_DATA_LENGTH = 0`, `INDEX_LENGTH = 0`,
`UPDATE_TIME = NULL`, `CHECK_TIME = NULL`, `CHECKSUM = NULL`,
`TABLE_COLLATION = 'utf8mb3_general_ci'`, and
`CREATE_OPTIONS = 'row_format=DYNAMIC stats_persistent=0'`. `CREATE_TIME` is
non-`NULL` in MySQL. MyLite renders the current statement timestamp for the
synthetic built-in row.

The table-status row estimates intentionally remain MySQL-observed metadata
constants even though direct reads return zero rows in this slice. A future
zoneinfo-loading feature can replace both with real loaded data.

## Diagnostics And Limits

- MyLite does not load zoneinfo rows into the five tables.
- Named time zones other than the existing supported fixed cases remain
  outside this slice. `SET time_zone = 'Europe/Helsinki'` continues to use the
  current unknown-time-zone diagnostic until MyLite implements named-zone data.
- Leap-second support, time-zone cache invalidation, `mysql_tzinfo_to_sql`
  import, daylight-saving transition conversion, and TIMESTAMP row
  storage/retrieval conversion are out of scope.
- Writes to all five tables remain blocked by the built-in schema write guard
  before catalog mutation.
- `SHOW CREATE TABLE mysql.time_zone*` remains out of scope.
- Privilege filtering is not implemented.
- No physical `mysql` time-zone tables, SQLite virtual tables, zoneinfo parser,
  or SQLite fork patch are introduced.

## Ownership Boundary

- Public API: unchanged. The feature returns ordinary `mylite_result` objects.
- Parser/AST: unchanged. Existing `SELECT`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA`, and `SHOW TABLE STATUS` forms are reused.
- Analyzer/runtime: resolves the five tables through the existing
  MySQL-system-table definition path and synthesizes empty placeholder rows
  plus metadata rows.
- Catalog metadata: unchanged. Definitions and rows are static MyLite-owned
  system metadata and are not stored in user catalogs.
- Storage/VFS/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe is:

```sql
SELECT VERSION();
SELECT 'time_zone', COUNT(*) FROM mysql.time_zone
UNION ALL SELECT 'time_zone_leap_second', COUNT(*) FROM mysql.time_zone_leap_second
UNION ALL SELECT 'time_zone_name', COUNT(*) FROM mysql.time_zone_name
UNION ALL SELECT 'time_zone_transition', COUNT(*) FROM mysql.time_zone_transition
UNION ALL SELECT 'time_zone_transition_type', COUNT(*) FROM mysql.time_zone_transition_type;
SHOW FULL COLUMNS FROM mysql.time_zone;
SHOW FULL COLUMNS FROM mysql.time_zone_leap_second;
SHOW FULL COLUMNS FROM mysql.time_zone_name;
SHOW FULL COLUMNS FROM mysql.time_zone_transition;
SHOW FULL COLUMNS FROM mysql.time_zone_transition_type;
SHOW INDEX FROM mysql.time_zone;
SHOW INDEX FROM mysql.time_zone_transition_type;
SELECT TABLE_NAME, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH, DATA_FREE,
       AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
       TABLE_COLLATION, CREATE_OPTIONS, TABLE_COMMENT
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME LIKE 'time_zone%'
 ORDER BY TABLE_NAME;
```

Observed output summary:

```text
8.4.9
row counts:
  time_zone 1795
  time_zone_leap_second 0
  time_zone_name 1795
  time_zone_transition 118801
  time_zone_transition_type 10200
columns:
  time_zone(Time_zone_id int unsigned auto_increment primary key,
            Use_leap_seconds enum('Y','N') not null default N)
  time_zone_leap_second(Transition_time bigint primary key, Correction int)
  time_zone_name(Name char(64) primary key, Time_zone_id int unsigned)
  time_zone_transition(Time_zone_id int unsigned primary key,
                       Transition_time bigint primary key,
                       Transition_type_id int unsigned)
  time_zone_transition_type(Time_zone_id int unsigned primary key,
                            Transition_type_id int unsigned primary key,
                            Offset int default 0,
                            Is_DST tinyint unsigned default 0,
                            Abbreviation char(8) default '')
status:
  time_zone Rows 1815, Avg_row_length 45, Data_length 81920,
    Data_free 4194304, Auto_increment 1796, Create_time non-NULL,
    Update_time NULL, Comment Time zones
  time_zone_leap_second Rows 0, Avg_row_length 0, Data_length 16384,
    Data_free 4194304, Auto_increment NULL, Create_time non-NULL,
    Update_time NULL, Comment Leap seconds information for time zones
  time_zone_name Rows 2311, Avg_row_length 113, Data_length 262144,
    Data_free 4194304, Auto_increment NULL, Create_time non-NULL,
    Update_time NULL, Comment Time zone names
  time_zone_transition Rows 119074, Avg_row_length 39, Data_length 4734976,
    Data_free 4194304, Auto_increment NULL, Create_time non-NULL,
    Update_time NULL, Comment Time zone transitions
  time_zone_transition_type Rows 9871, Avg_row_length 48,
    Data_length 475136, Data_free 4194304, Auto_increment NULL,
    Create_time non-NULL, Update_time NULL,
    Comment Time zone transition types
```

## Verification

```sh
cmake --build --preset dev --target mylite_runtime_mysql_time_zone_tables_test
ctest --preset dev -R '^libmylite\.runtime\.mysql_time_zone_tables$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_time_zone_tables_expectations.sh
git diff --check
cmake --workflow --preset check
```

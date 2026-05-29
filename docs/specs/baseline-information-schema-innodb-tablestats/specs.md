# Baseline INFORMATION_SCHEMA InnoDB Tablestats

## Status

This phase adds a limited descriptor-backed
`INFORMATION_SCHEMA.INNODB_TABLESTATS` system view. The view exposes
MySQL 8.4.9-shaped table and column metadata and emits synthetic low-level
status rows for MyLite persistent base-table descriptors.

The slice does not add an InnoDB statistics cache, physical InnoDB pages,
optimizer statistics, built-in `mysql` system-table rows, temporary-table
rows, `ANALYZE TABLE` effects, or privilege filtering.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing descriptor-backed `INFORMATION_SCHEMA.INNODB_TABLES`,
  `INFORMATION_SCHEMA.INNODB_INDEXES`, and `SHOW TABLE STATUS` code paths in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_TABLESTATS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-tablestats-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_TABLESTATS` exists as a `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for the system view row.
- The view has nine columns: `TABLE_ID`, `NAME`, `STATS_INITIALIZED`,
  `NUM_ROWS`, `CLUST_INDEX_SIZE`, `OTHER_INDEX_SIZE`, `MODIFIED_COUNTER`,
  `AUTOINC`, and `REF_COUNT`.
- `TABLE_ID`, `NUM_ROWS`, `CLUST_INDEX_SIZE`, `OTHER_INDEX_SIZE`,
  `MODIFIED_COUNTER`, and `AUTOINC` are non-null `bigint unsigned`.
  `REF_COUNT` is a non-null `int`.
- `NAME` and `STATS_INITIALIZED` are non-null `varchar(193)` columns using
  `utf8mb3_general_ci`. MySQL reports `CHARACTER_MAXIMUM_LENGTH = 64` and
  `CHARACTER_OCTET_LENGTH = 193` for both.
- All nine system-view columns report empty-string `COLUMN_DEFAULT` values.
- A simple persistent table with one primary key and one secondary index
  produced one row with `NAME = schema/table`, `STATS_INITIALIZED =
  'Initialized'`, exact current `NUM_ROWS`, `CLUST_INDEX_SIZE = 1`,
  `OTHER_INDEX_SIZE = 1`, `AUTOINC = 0`, and a positive physical `TABLE_ID`.
- A table with two secondary indexes produced `OTHER_INDEX_SIZE = 2`.
- A table with `AUTO_INCREMENT` rows reported the next auto-increment value in
  `AUTOINC`.
- Empty-table and `ANALYZE TABLE` behavior is intentionally volatile: MySQL can
  show `Initialized` or `Uninitialized` depending on table access and stats
  recalculation state.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
DROP DATABASE IF EXISTS mylite_tablestats_probe;
CREATE DATABASE mylite_tablestats_probe;
USE mylite_tablestats_probe;
CREATE TABLE t_primary (
  id INT PRIMARY KEY,
  v INT,
  KEY ix_v (v),
  KEY ix_v_id (v, id)
) ENGINE=InnoDB;
CREATE TABLE t_auto (
  id INT AUTO_INCREMENT PRIMARY KEY,
  v INT
) ENGINE=InnoDB;
INSERT INTO t_primary VALUES (1,10),(2,20),(3,20);
INSERT INTO t_auto(v) VALUES (7),(8);
SELECT SUBSTRING_INDEX(NAME,'/',-1), STATS_INITIALIZED, NUM_ROWS,
       CLUST_INDEX_SIZE, OTHER_INDEX_SIZE, MODIFIED_COUNTER, AUTOINC,
       REF_COUNT
  FROM INFORMATION_SCHEMA.INNODB_TABLESTATS
 WHERE NAME LIKE 'mylite_tablestats_probe/%'
 ORDER BY NAME;
DROP DATABASE mylite_tablestats_probe;
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_TABLESTATS` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_TABLESTATS` reads while `information_schema` is the
  selected schema;
- one row per persistent MyLite base-table descriptor;
- `TABLE_ID` derived from the MyLite table descriptor id;
- `NAME` formatted as `schema/table`;
- `STATS_INITIALIZED = 'Initialized'`, because MyLite computes current
  descriptor-backed values on demand rather than maintaining an uninitialized
  InnoDB stats cache;
- `NUM_ROWS` from the exact physical row count used by `SHOW TABLE STATUS`;
- `CLUST_INDEX_SIZE = 1` as a small synthetic clustered-index page placeholder;
- `OTHER_INDEX_SIZE` as the count of non-primary MyLite index descriptors on
  the table;
- `MODIFIED_COUNTER = 0`, because MyLite does not maintain InnoDB's in-memory
  stats modified-row counter;
- `AUTOINC` from the table descriptor next auto-increment value when the table
  has an auto-increment column, otherwise `0`;
- `REF_COUNT = 1` as a deterministic metadata-cache placeholder;
- descriptor changes from supported DDL and row-count changes from supported
  DML are reflected in subsequent reads;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- rows for built-in `mysql` system tables, temporary tables, views, or physical
  InnoDB dictionary objects;
- exact MySQL physical `TABLE_ID`, clustered-index page counts, secondary-index
  page counts, modified-counter updates, table-cache reference counts, or
  persistent optimizer statistics;
- `ANALYZE TABLE` stats invalidation and reinitialization behavior;
- privilege checks or account-specific filtering;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_TABLESTATS` as a supported
  information-schema system view and emits rows from loaded catalog
  descriptors and current physical row counts.
- Catalog metadata: unchanged. Existing schema, table, column, and index
  descriptors are authoritative.
- Storage/SQLite: unchanged. Row counts are read through public SQLite SQL
  against the existing MyLite physical table name.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLESTATS;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESTATS WHERE NAME LIKE 'app/%';
SELECT s.NAME, s.NUM_ROWS, s.OTHER_INDEX_SIZE
  FROM INFORMATION_SCHEMA.INNODB_TABLESTATS AS s
 WHERE s.STATS_INITIALIZED = 'Initialized'
 ORDER BY s.NAME;
```

## Runtime Semantics

`INNODB_TABLESTATS` is registered in the static information-schema table
registry. Row production is descriptor-backed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads iterate persistent catalog schemas and base tables;
- `TABLE_ID` is the loaded MyLite table descriptor id;
- `NAME` is `schema/table`;
- `STATS_INITIALIZED` is always `Initialized`;
- `NUM_ROWS` is computed with `SELECT COUNT(*)` from the physical table;
- `CLUST_INDEX_SIZE` is `1`;
- `OTHER_INDEX_SIZE` counts non-primary index descriptors;
- `MODIFIED_COUNTER` is `0`;
- `AUTOINC` is the descriptor's next auto-increment value for tables with an
  auto-increment column and `0` otherwise;
- `REF_COUNT` is `1`;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, and limits retain the
  current information-schema query subset behavior;
- stale or invalid catalog descriptors fail with existing runtime diagnostics;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- `SHOW FULL TABLES` and `INFORMATION_SCHEMA.TABLES` metadata for the system
  view;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all nine columns;
- descriptor rows for primary-key, no-primary-key, multi-index, and
  auto-increment base tables;
- exact `NUM_ROWS`, non-primary index counts, auto-increment metadata, and
  deterministic synthetic fields;
- row-count changes after `INSERT`;
- `COUNT(*)`, case-insensitive table lookup, aliases, predicates, and
  unqualified reads after `USE information_schema`;
- persistence across reopen;
- `@@warning_count` and `ROW_COUNT()` status after a successful read.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_tablestats_test
ctest --preset dev -R '^libmylite\.runtime\.information_schema_innodb_tablestats$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_tablestats_expectations.sh
git diff --check
cmake --workflow --preset check
```

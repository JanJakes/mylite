# Baseline mysql.innodb_index_stats

## Status

This slice makes `mysql.innodb_index_stats` queryable as a read-only,
MyLite-owned synthetic system table. It extends the limited direct `mysql`
schema read surface introduced for `mysql.innodb_table_stats` to the companion
optimizer-statistics table that applications may inspect when checking InnoDB
persistent statistics.

The implementation is not a writable InnoDB persistent-statistics subsystem.
Rows are synthesized from stable built-in placeholders and MyLite persistent
base-table index descriptors.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing `mysql.innodb_table_stats` baseline:
  `docs/specs/baseline-mysql-innodb-table-stats/specs.md`
- Existing descriptor-backed `INFORMATION_SCHEMA.INNODB_INDEXES` and
  `INFORMATION_SCHEMA.INNODB_FIELDS`:
  `docs/specs/baseline-information-schema-innodb-indexes/specs.md`
- MySQL 8.4 Reference Manual, InnoDB persistent statistics:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-persistent-stats.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_mysql_innodb_index_stats_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`:

- `mysql.innodb_index_stats` exists as a `BASE TABLE` in the `mysql` schema.
- The table has eight columns: `database_name`, `table_name`, `index_name`,
  `last_update`, `stat_name`, `stat_value`, `sample_size`, and
  `stat_description`.
- `database_name`, `table_name`, `index_name`, and `stat_name` are non-null
  primary-key `varchar` columns using `utf8mb3_bin` metadata collation.
- `last_update` is `timestamp NOT NULL` with `CURRENT_TIMESTAMP` default and
  `DEFAULT_GENERATED on update CURRENT_TIMESTAMP` extra metadata.
- `stat_value` is non-null `bigint unsigned`; `sample_size` is nullable
  `bigint unsigned`; `stat_description` is non-null `varchar(1024)` using
  `utf8mb3_bin`.
- The fresh target runtime has stable built-in primary-index rows for
  `mysql.component` and `sys.sys_config`.
- For user InnoDB tables, `ANALYZE TABLE` makes visible persistent-statistics
  rows stable enough for expectation probes. Small observed tables report
  `n_diff_pfxNN` rows for indexed key prefixes, then `n_leaf_pages` and `size`
  rows with `stat_value = 1`.
- Nonunique secondary indexes append the clustered key to prefix statistics
  when needed. Tables without a primary key or all-`NOT NULL` unique key use a
  generated clustered row described as `DB_ROW_ID`.
- `SET timestamp` does not control `last_update`; MySQL uses current runtime
  time when updating persistent statistics rows.
- Successful supported reads emit no warnings, and the following `ROW_COUNT()`
  returns `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
DROP DATABASE IF EXISTS mylite_mysql_index_stats_probe;
CREATE DATABASE mylite_mysql_index_stats_probe;
USE mylite_mysql_index_stats_probe;
CREATE TABLE t_primary(
  id INT PRIMARY KEY,
  v INT,
  w INT,
  KEY ix_v(v),
  KEY ix_v_id(v,id),
  UNIQUE KEY uq_w(w)
) ENGINE=InnoDB;
CREATE TABLE t_generated(a INT, b INT, KEY ix_b(b)) ENGINE=InnoDB;
INSERT INTO t_primary VALUES (1,10,100),(2,20,200),(3,20,300);
INSERT INTO t_generated VALUES (1,10),(2,10),(3,20);
ANALYZE TABLE t_primary, t_generated;
SELECT table_name, index_name, stat_name, stat_value, sample_size,
       stat_description
  FROM mysql.innodb_index_stats
 WHERE database_name = 'mylite_mysql_index_stats_probe'
 ORDER BY table_name, index_name, stat_name;
DROP DATABASE mylite_mysql_index_stats_probe;
SQL
```

## Scope

Supported:

- direct `SELECT` from `mysql.innodb_index_stats`;
- unqualified `innodb_index_stats` reads while `mysql` is the selected schema;
- wildcard projection, explicit column projection, aliases, `COUNT(*)`,
  supported metadata-style predicates, one-column `ORDER BY`, and
  `LIMIT row_count` through the existing metadata query mechanics;
- stable built-in rows for `mysql.component.PRIMARY` and
  `sys.sys_config.PRIMARY`;
- descriptor-backed rows for persistent base-table primary, unique secondary,
  nonunique secondary, prefix-key-part, and generated clustered indexes;
- `database_name`, `table_name`, and `index_name` from schema, table, and index
  descriptors;
- `last_update` as a non-`NULL` wall-clock timestamp string in the session time
  zone, unaffected by `SET timestamp`;
- `n_diff_pfxNN` rows for supported indexed prefix statistics, with exact
  MyLite distinct indexed-prefix counts over current stored rows;
- `sample_size = 1` for `n_diff_pfxNN` rows;
- `n_leaf_pages` and `size` rows with deterministic placeholder
  `stat_value = 1` and SQL `NULL` sample size;
- `stat_description` as comma-joined indexed prefix names, generated
  `DB_ROW_ID`, or the MySQL-shaped page-count descriptions;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for the eight
  `mysql.innodb_index_stats` columns;
- existing `INFORMATION_SCHEMA.TABLES`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` directory rows, with table-status field parity refined
  by `baseline-mysql-system-stats-table-status`;
- existing `mysql` schema write protection for all currently supported
  mutating statements.

Out of scope:

- writable persistent optimizer statistics, manual statistics edits, `FLUSH
  TABLE` statistics reload, `ANALYZE TABLE` side effects, asynchronous
  statistics recalculation, histograms, optimizer behavior, and
  `innodb_stats_*` runtime behavior;
- full-text and spatial persistent-statistics auxiliary rows;
- exact InnoDB sampling behavior, physical page counts, physical tablespace
  identifiers, table partitions, subpartitions, temporary tables, views,
  privilege filtering, or complete data-dictionary tables;
- broader `mysql` system-table shape support beyond the separately specified
  `SHOW COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW KEYS`,
  `INFORMATION_SCHEMA.STATISTICS`, and information-schema constraint metadata
  for this table.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Parser/AST: unchanged. Existing `SELECT ... FROM qualified_name` parsing is
  reused; no Lemon grammar change is required.
- Analyzer/runtime: detects the supported `mysql.innodb_index_stats` source
  before ordinary descriptor-backed `SELECT` planning and reuses the limited
  metadata projection, predicate, ordering, and limit logic.
- Catalog metadata: schema, table, column, and index descriptors are
  authoritative for user rows.
- Result builder: emits MySQL-shaped text/`NULL` values through
  `mylite_result`.
- Storage/VFS/SQLite: unchanged. Exact row and distinct-prefix counts are read
  from MyLite-owned physical SQLite tables through internal runtime SQL; no
  SQLite schema reflection, SQLite virtual table, storage-format change, or
  SQLite fork patch is required.

## Syntax

No grammar is added. The supported forms are:

```sql
SELECT select_list
FROM mysql.innodb_index_stats [AS alias]
[WHERE supported_metadata_predicate]
[ORDER BY one_column [ASC | DESC]]
[LIMIT row_count]

USE mysql;
SELECT select_list
FROM innodb_index_stats [AS alias]
[WHERE supported_metadata_predicate]
[ORDER BY one_column [ASC | DESC]]
[LIMIT row_count]
```

The supported predicate and projection subset matches the current
information-schema metadata query envelope: metadata column references,
`COUNT(*)`, comparisons, `IS [NOT] NULL`, `LIKE`, literal-list `IN`,
`BETWEEN`, `NOT`, `AND`, `OR`, `XOR`, parentheses, one-column ordering, and
simple row-count limit. Joins, subqueries, expressions, grouping, aggregate
functions other than `COUNT(*)`, explicit `ESCAPE`, non-ASCII `LIKE`
patterns, and index hints remain unsupported.

## Row Semantics

`mysql.innodb_index_stats` has eight columns:

| Column | MyLite value |
| --- | --- |
| `database_name` | built-in schema name or descriptor schema name |
| `table_name` | built-in table name or descriptor table name |
| `index_name` | built-in or descriptor index name, or `GEN_CLUST_INDEX` |
| `last_update` | current wall-clock timestamp rendered as `YYYY-MM-DD HH:MM:SS` |
| `stat_name` | `n_diff_pfxNN`, `n_leaf_pages`, or `size` |
| `stat_value` | exact prefix cardinality or deterministic page-count placeholder |
| `sample_size` | `1` for prefix rows, SQL `NULL` for page-count rows |
| `stat_description` | prefix column names, `DB_ROW_ID`, or page-count description |

Built-in rows:

| database_name | table_name | index_name | stat_name | stat_value | sample_size | stat_description |
| --- | --- | --- | --- | ---: | ---: | --- |
| `mysql` | `component` | `PRIMARY` | `n_diff_pfx01` | `0` | `1` | `component_id` |
| `mysql` | `component` | `PRIMARY` | `n_leaf_pages` | `1` | `NULL` | `Number of leaf pages in the index` |
| `mysql` | `component` | `PRIMARY` | `size` | `1` | `NULL` | `Number of pages in the index` |
| `sys` | `sys_config` | `PRIMARY` | `n_diff_pfx01` | `6` | `1` | `variable` |
| `sys` | `sys_config` | `PRIMARY` | `n_leaf_pages` | `1` | `NULL` | `Number of leaf pages in the index` |
| `sys` | `sys_config` | `PRIMARY` | `size` | `1` | `NULL` | `Number of pages in the index` |

Persistent MyLite base tables receive rows for supported index descriptors.
Primary and unique secondary descriptors emit one prefix row per explicit key
part. Nonunique secondary descriptors emit prefix rows for explicit key parts
and any clustered-key parts that are not already explicit key parts. Tables
without an explicit primary key or all-`NOT NULL` unique key receive a
`GEN_CLUST_INDEX` row group and nonunique secondary prefixes may append
`DB_ROW_ID`.

Views and temporary tables are omitted. Descriptor changes from supported DDL
and row-count or distinct-prefix changes from supported DML are reflected in
later reads.

## Diagnostics

Successful supported reads emit no warnings and set the prior result status as
a row result so `ROW_COUNT()` returns `-1`.

The feature reuses existing metadata-query diagnostics for unsupported query
shapes and unknown columns. Unknown or unsupported `mysql` system tables remain
outside this slice and continue through the existing ordinary table-resolution
diagnostics. Mutating statements targeting `mysql` keep the existing
`3552 / HY000` system-schema access error.

## Tests

Add a MySQL expectation script and focused C runtime test covering:

- MySQL 8.4.9 table shape and `INFORMATION_SCHEMA.COLUMNS` metadata;
- direct `mysql.innodb_index_stats` projection, `COUNT(*)`, aliases,
  predicates, ordering, and limit;
- unqualified reads after `USE mysql`;
- built-in `mysql.component.PRIMARY` and `sys.sys_config.PRIMARY` rows;
- descriptor-backed rows for primary, unique secondary, nonunique secondary,
  prefix-key-part, and generated clustered indexes;
- exact distinct-prefix counts after DML;
- row removal after `DROP INDEX`;
- `last_update` stays independent from `SET timestamp`;
- successful-read warning count and `ROW_COUNT()`;
- unknown-column diagnostics and preserved `mysql` write protection.

The separate `baseline-mysql-system-stats-table-status` slice refines the
`INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` directory metadata for this
supported synthetic table.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_mysql_innodb_index_stats_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_innodb_index_stats|mysql_innodb_table_stats|information_schema_innodb_indexes|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_innodb_index_stats_expectations.sh
git diff --check
cmake --workflow --preset check
```

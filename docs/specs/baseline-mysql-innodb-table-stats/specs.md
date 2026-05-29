# Baseline mysql.innodb_table_stats

## Status

This slice makes `mysql.innodb_table_stats` queryable as a read-only,
MyLite-owned synthetic system table. MyLite already lists the table in the
`mysql` built-in schema directory through `INFORMATION_SCHEMA.TABLES`,
`SHOW TABLES`, `SHOW FULL TABLES`, and `SHOW TABLE STATUS`; this slice adds a
limited direct `SELECT` surface so common optimizer-statistics probes no
longer fail when they read the table.

The implementation is not a writable InnoDB persistent-statistics subsystem.
Rows are synthesized from built-in metadata placeholders and MyLite persistent
base-table descriptors.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing built-in schema table directory:
  `docs/specs/baseline-built-in-schema-table-directory/specs.md`
- Existing descriptor-backed `INFORMATION_SCHEMA.INNODB_TABLESTATS`:
  `docs/specs/baseline-information-schema-innodb-tablestats/specs.md`
- MySQL 8.4 Reference Manual, InnoDB persistent statistics:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-persistent-stats.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_mysql_innodb_table_stats_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`:

- `mysql.innodb_table_stats` exists as a `BASE TABLE` in the `mysql` schema.
- `SHOW COLUMNS FROM mysql.innodb_table_stats` reports six columns:
  `database_name`, `table_name`, `last_update`, `n_rows`,
  `clustered_index_size`, and `sum_of_other_index_sizes`.
- `database_name` is `varchar(64) NOT NULL`, part of the primary key, and uses
  `utf8mb3_bin` metadata collation.
- `table_name` is `varchar(199) NOT NULL`, part of the primary key, and uses
  `utf8mb3_bin` metadata collation.
- `last_update` is `timestamp NOT NULL` with `CURRENT_TIMESTAMP` default and
  `DEFAULT_GENERATED on update CURRENT_TIMESTAMP` extra metadata.
- `n_rows`, `clustered_index_size`, and `sum_of_other_index_sizes` are
  non-null `bigint unsigned` columns.
- The fresh target runtime has built-in rows for `mysql.component` and
  `sys.sys_config`.
- For user InnoDB tables, `ANALYZE TABLE` makes the visible row count and
  index-page fields stable enough for expectation probes. Automatic
  recalculation is asynchronous and direct DML does not guarantee immediate
  row-count changes.
- `SET timestamp` does not control `last_update`; MySQL uses current runtime
  time when updating persistent statistics rows.
- Successful supported reads emit no warnings, and the following `ROW_COUNT()`
  returns `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
DROP DATABASE IF EXISTS mylite_mysql_stats_probe;
CREATE DATABASE mylite_mysql_stats_probe;
USE mylite_mysql_stats_probe;
CREATE TABLE t_primary(
  id INT PRIMARY KEY,
  v INT,
  KEY ix_v(v),
  KEY ix_v_id(v,id)
) ENGINE=InnoDB;
CREATE TABLE t_plain(a INT, b INT) ENGINE=InnoDB;
INSERT INTO t_primary VALUES (1,10),(2,20),(3,20);
INSERT INTO t_plain VALUES (1,2),(3,4);
ANALYZE TABLE t_primary, t_plain;
SELECT database_name, table_name, n_rows, clustered_index_size,
       sum_of_other_index_sizes
  FROM mysql.innodb_table_stats
 WHERE database_name = 'mylite_mysql_stats_probe'
 ORDER BY table_name;
DROP DATABASE mylite_mysql_stats_probe;
SQL
```

## Scope

Supported:

- direct `SELECT` from `mysql.innodb_table_stats`;
- unqualified `innodb_table_stats` reads while `mysql` is the selected schema;
- wildcard projection, explicit column projection, aliases, `COUNT(*)`,
  supported metadata-style predicates, one-column `ORDER BY`, and
  `LIMIT row_count` through the existing metadata query mechanics;
- the two stable built-in rows observed in the target runtime:
  `mysql.component` and `sys.sys_config`;
- one row for each persistent MyLite base table;
- `database_name` and `table_name` from schema and table descriptors;
- `last_update` as a non-`NULL` wall-clock timestamp string in the session time
  zone, unaffected by `SET timestamp`;
- `n_rows` from MyLite's exact physical row count;
- `clustered_index_size = 1` as a deterministic clustered-index page
  placeholder;
- `sum_of_other_index_sizes` as the count of non-primary MyLite index
  descriptors on the table;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for the six
  `mysql.innodb_table_stats` columns;
- existing `INFORMATION_SCHEMA.TABLES`, `SHOW TABLES`, `SHOW FULL TABLES`,
  and `SHOW TABLE STATUS` directory rows;
- existing `mysql` schema write protection for all currently supported
  mutating statements.

Out of scope:

- direct reads from `mysql.innodb_index_stats` or other `mysql` schema tables;
- writable persistent optimizer statistics, manual statistics edits, `FLUSH
  TABLE` statistics reload, `ANALYZE TABLE` side effects, asynchronous
  statistics recalculation, histograms, optimizer behavior, and
  `innodb_stats_*` runtime behavior;
- exact InnoDB `last_update` volatility, physical table or index page counts,
  table partitions, subpartitions, temporary tables, views, privilege
  filtering, or complete data-dictionary tables;
- broader `mysql` system-table shape support beyond the separately specified
  `SHOW COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW KEYS`,
  `INFORMATION_SCHEMA.STATISTICS`, and information-schema constraint metadata
  for this table.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Parser/AST: unchanged. Existing `SELECT ... FROM qualified_name` parsing is
  reused; no Lemon grammar change is required.
- Analyzer/runtime: detects the supported `mysql.innodb_table_stats` source
  before ordinary descriptor-backed `SELECT` planning and reuses the limited
  metadata projection, predicate, ordering, and limit logic.
- Catalog metadata: schema, table, and index descriptors are authoritative for
  user rows.
- Result builder: emits MySQL-shaped text/`NULL` values through
  `mylite_result`.
- Storage/VFS/SQLite: unchanged. Row counts are read through existing MyLite
  physical table helpers; no SQLite schema reflection, SQLite virtual table,
  storage-format change, or SQLite fork patch is required.

## Syntax

No grammar is added. The supported forms are:

```sql
SELECT select_list
FROM mysql.innodb_table_stats [AS alias]
[WHERE supported_metadata_predicate]
[ORDER BY one_column [ASC | DESC]]
[LIMIT row_count]

USE mysql;
SELECT select_list
FROM innodb_table_stats [AS alias]
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

`mysql.innodb_table_stats` has six columns:

| Column | MyLite value |
| --- | --- |
| `database_name` | built-in schema name or descriptor schema name |
| `table_name` | built-in table name or descriptor table name |
| `last_update` | current wall-clock timestamp rendered as `YYYY-MM-DD HH:MM:SS` |
| `n_rows` | built-in placeholder or exact persistent base-table row count |
| `clustered_index_size` | `1` |
| `sum_of_other_index_sizes` | built-in placeholder or non-primary descriptor index count |

Built-in rows:

| database_name | table_name | n_rows | clustered_index_size | sum_of_other_index_sizes |
| --- | --- | ---: | ---: | ---: |
| `mysql` | `component` | `0` | `1` | `0` |
| `sys` | `sys_config` | `6` | `1` | `0` |

Persistent MyLite base tables receive one row each. Views and temporary tables
are omitted. Descriptor changes from supported DDL and row-count changes from
supported DML are reflected in later reads.

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
- direct `mysql.innodb_table_stats` projection, `COUNT(*)`, aliases,
  predicates, ordering, and limit;
- unqualified reads after `USE mysql`;
- built-in `mysql.component` and `sys.sys_config` placeholder rows;
- descriptor-backed rows for primary-key, non-primary-index, and no-index
  persistent base tables;
- row-count changes after DML and row removal after `DROP TABLE`;
- `last_update` stays independent from `SET timestamp`;
- successful-read warning count and `ROW_COUNT()`;
- unknown-column diagnostics and preserved `mysql` write protection.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_mysql_innodb_table_stats_test
ctest --preset dev -R '^libmylite\.runtime\.(mysql_innodb_table_stats|builtin_schema_table_directory|information_schema_innodb_tablestats)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_mysql_innodb_table_stats_expectations.sh
git diff --check
cmake --workflow --preset check
```

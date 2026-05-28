# Baseline INFORMATION_SCHEMA InnoDB Index Tables

## Status

This phase adds descriptor-backed `INFORMATION_SCHEMA.INNODB_INDEXES` and
`INFORMATION_SCHEMA.INNODB_FIELDS` system views. The views expose MySQL
8.4.9-shaped table and column metadata and return rows for MyLite's current
persistent base-table index descriptors, plus a synthetic generated-clustered
index row for persistent base tables that have no explicit primary key and no
all-`NOT NULL` unique key usable as the clustered key.

The slice does not add physical InnoDB dictionary tables, clustered-index
storage layout, hidden clustered-field rows in `INNODB_FIELDS`, full-text
auxiliary indexes, buffer-pool state, `INFORMATION_SCHEMA.INNODB_TABLES`, or
privilege filtering.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing index descriptor, `INFORMATION_SCHEMA.STATISTICS`, `SHOW INDEX`,
  full-text index, and spatial index implementations in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_INDEXES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-indexes-table.html
- MySQL 8.4 Reference Manual, `INNODB_FIELDS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-fields-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_INDEXES` and
  `INFORMATION_SCHEMA.INNODB_FIELDS` exist as `SYSTEM VIEW` tables.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both views.
- `INNODB_INDEXES` has columns `INDEX_ID`, `NAME`, `TABLE_ID`, `TYPE`,
  `N_FIELDS`, `PAGE_NO`, `SPACE`, and `MERGE_THRESHOLD`. Every column is
  non-null. Numeric columns report SQL `NULL` precision and scale metadata in
  `INFORMATION_SCHEMA.COLUMNS`; `NAME` is `varchar(64)` with octet length
  `193`, collation `utf8mb3_general_ci`, and column type `varchar(193)`.
- `INNODB_FIELDS` has columns `INDEX_ID`, `NAME`, and `POS`. `INDEX_ID` is a
  nullable `varbinary(256)`; `NAME` is non-null `varchar(64)` with
  `utf8mb3_tolower_ci`; `POS` is non-null `bigint unsigned` with default `0`.
- MySQL `INNODB_INDEXES.TYPE` values observed for a base table were:
  nonunique secondary `0`, unique secondary `2`, primary clustered `3`,
  full-text `32`, and spatial `64`.
- When a table has no primary key, MySQL uses the first unique index whose key
  columns are all `NOT NULL` as the clustered index and reports that descriptor
  with `TYPE = 3`. Later unique descriptors remain `TYPE = 2`.
- When a table has no primary key and no all-`NOT NULL` unique index, MySQL
  reports a hidden `GEN_CLUST_INDEX` row with `TYPE = 1`; no
  `INNODB_FIELDS` row was observed for that generated clustered index.
- MySQL `INNODB_FIELDS.POS` is zero-based for explicit key columns.
- MySQL `INNODB_INDEXES.N_FIELDS` includes physical InnoDB fields such as
  implicit primary-key and clustered record fields. `INNODB_FIELDS` exposes the
  explicit key fields for the observed rows.
- Full-text indexes introduce InnoDB auxiliary metadata such as
  `FTS_DOC_ID_INDEX`; this slice intentionally does not synthesize those
  physical auxiliary indexes.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.

Representative probe:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "CREATE DATABASE mylite_innodb_index_probe; \
   USE mylite_innodb_index_probe; \
   CREATE TABLE idx_sample(
     id INT NOT NULL, a INT NOT NULL, b INT NOT NULL,
     body TEXT, p POINT NOT NULL,
     PRIMARY KEY(id),
     UNIQUE KEY uq_ab(a,b),
     KEY ix_b_desc(b DESC),
     FULLTEXT KEY ft_body(body),
     SPATIAL KEY sp_p(p)
   ) ENGINE=InnoDB; \
   SELECT i.NAME,i.TYPE,i.N_FIELDS,i.PAGE_NO,i.MERGE_THRESHOLD
     FROM INFORMATION_SCHEMA.INNODB_INDEXES i
     JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID
    WHERE t.NAME = 'mylite_innodb_index_probe/idx_sample'
      AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p')
    ORDER BY i.NAME; \
   SELECT i.NAME,f.NAME,f.POS
     FROM INFORMATION_SCHEMA.INNODB_INDEXES i
     JOIN INFORMATION_SCHEMA.INNODB_FIELDS f ON f.INDEX_ID = i.INDEX_ID
     JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID = i.TABLE_ID
    WHERE t.NAME = 'mylite_innodb_index_probe/idx_sample'
      AND i.NAME IN ('PRIMARY','uq_ab','ix_b_desc','ft_body','sp_p')
    ORDER BY i.NAME,f.POS; \
   DROP DATABASE mylite_innodb_index_probe;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_INDEXES` and
  `INFORMATION_SCHEMA.INNODB_FIELDS` using the existing information-schema
  query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- rows for persistent base-table primary, secondary, unique, full-text, and
  spatial index descriptors;
- clustered fallback classification for the first all-`NOT NULL` unique key on
  tables without an explicit primary key;
- a synthetic `GEN_CLUST_INDEX` row for tables without an explicit primary key
  or all-`NOT NULL` unique key;
- `INDEX_ID` values derived from MyLite index descriptor ids;
- deterministic synthetic `INDEX_ID` values for `GEN_CLUST_INDEX` rows;
- `TABLE_ID` values derived from MyLite table descriptor ids;
- `TYPE` values matching the observed MySQL code points for MyLite's current
  index classes;
- `N_FIELDS` values approximating MySQL's physical field count from available
  MyLite metadata: clustered descriptors use table column count plus two
  InnoDB system fields, generated clustered rows use table column count plus
  three synthetic system fields, and secondary descriptors add clustered key
  parts that are not already explicit key parts;
- `INNODB_FIELDS.POS` as zero-based explicit key-part position;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- descriptor changes from supported index create/drop/rename DDL are reflected
  in subsequent reads.

Out of scope:

- exact physical InnoDB clustered-record field counts, root pages, space ids,
  or merge-threshold mutation;
- hidden clustered fields in `INNODB_FIELDS`, hidden full-text auxiliary
  indexes, and other storage engine internal indexes;
- `INFORMATION_SCHEMA.INNODB_TABLES`, `INNODB_COLUMNS`, or `INNODB_TABLESTATS`;
- temporary tables, views, built-in-schema rows, privilege checks, and
  account-specific filtering.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes both InnoDB index views as supported
  information-schema system views and emits rows from loaded catalog
  descriptors.
- Catalog metadata: unchanged. Existing `_mylite_catalog_indexes` and
  `_mylite_catalog_index_columns` descriptors are authoritative.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_INDEXES;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FIELDS WHERE INDEX_ID = 1;
SELECT i.NAME, i.TYPE
  FROM INFORMATION_SCHEMA.INNODB_INDEXES AS i
 WHERE i.TYPE IN (0, 2, 3, 32, 64)
 ORDER BY i.NAME;
```

## Runtime Semantics

`INNODB_INDEXES` and `INNODB_FIELDS` are registered in the static
information-schema table registry. Row production is descriptor-backed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads iterate persistent catalog schemas and base tables;
- each loaded index descriptor produces one `INNODB_INDEXES` row;
- each explicit key part produces one `INNODB_FIELDS` row;
- if a table has no primary-key descriptor, the first all-`NOT NULL` unique
  secondary descriptor is reported as the clustered index with `TYPE = 3`;
- if a table has no primary-key descriptor and no all-`NOT NULL` unique
  secondary descriptor, a synthetic `GEN_CLUST_INDEX` row is emitted with
  `TYPE = 1` and no `INNODB_FIELDS` rows;
- `INDEX_ID` is the loaded MyLite index descriptor id;
- `GEN_CLUST_INDEX.INDEX_ID` is a deterministic synthetic id derived from the
  MyLite table id;
- `TABLE_ID` is the loaded MyLite table descriptor id;
- `TYPE` maps primary indexes to `3`, unique secondary indexes to `2`,
  clustered unique fallback indexes to `3`, nonunique secondary indexes to
  `0`, generated clustered indexes to `1`, full-text indexes to `32`, and
  spatial indexes to `64`;
- `N_FIELDS` is a synthetic physical field count from available descriptors:
  full-text indexes keep their explicit key-part count, clustered descriptors
  use table column count plus two InnoDB system fields, generated clustered
  indexes use table column count plus three synthetic system fields, and
  secondary descriptors add clustered key parts that are not already explicit
  key parts;
- `PAGE_NO` is `-1` for full-text indexes and `0` for all other descriptors;
- `SPACE` is `0`;
- `MERGE_THRESHOLD` is the MySQL default value `50`;
- `INNODB_FIELDS.NAME` is the indexed column name;
- `INNODB_FIELDS.POS` is `ordinal_position - 1`;
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

- table kind and `INFORMATION_SCHEMA.TABLES` metadata for both views;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all columns in both views;
- primary, unique, nonunique secondary, full-text, and spatial index rows;
- first all-`NOT NULL` unique-key clustered fallback rows;
- generated clustered index fallback rows for tables without a primary key or
  all-`NOT NULL` unique key;
- `TYPE` mapping, `N_FIELDS`, `PAGE_NO`, `SPACE`, and `MERGE_THRESHOLD`;
- `INNODB_FIELDS` key-part names with zero-based positions;
- case-insensitive table-name lookup, aliases, predicates, ordering, and
  unqualified selected-schema reads;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- descriptor updates after supported `ALTER TABLE ... RENAME INDEX` and
  `DROP INDEX`;
- file-backed reopen behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_indexes_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_indexes|information_schema_static_catalogs|builtin_schema_table_directory|fulltext_index_metadata|spatial_index_metadata|index_options_metadata)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_indexes_expectations.sh
git diff --check
cmake --workflow --preset check
```

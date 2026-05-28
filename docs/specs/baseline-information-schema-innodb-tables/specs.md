# Baseline INFORMATION_SCHEMA InnoDB Tables

## Status

This phase adds descriptor-backed `INFORMATION_SCHEMA.INNODB_TABLES` rows for
MyLite persistent base tables. The view exposes MySQL 8.4.9-shaped system table
and column metadata and reports stable InnoDB table-dictionary fields from
existing MyLite catalog descriptors.

The slice does not add physical InnoDB dictionary storage, `.ibd` tablespaces,
temporary-table rows, `INFORMATION_SCHEMA.INNODB_COLUMNS`,
`INFORMATION_SCHEMA.INNODB_TABLESTATS`, or privilege filtering.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing table descriptor, table-status, built-in schema table-directory, and
  InnoDB index dictionary implementations in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-tables-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_TABLES` exists as a `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for the system view row.
- `INNODB_TABLES` has columns `TABLE_ID`, `NAME`, `FLAG`, `N_COLS`, `SPACE`,
  `ROW_FORMAT`, `ZIP_PAGE_SIZE`, `SPACE_TYPE`, `INSTANT_COLS`, and
  `TOTAL_ROW_VERSIONS`.
- `TABLE_ID` is `bigint unsigned`, `NAME` is `varchar(655)`, `FLAG` and
  `N_COLS` are `int`, `SPACE` is `bigint`, `ROW_FORMAT` is nullable
  `varchar(12)`, `ZIP_PAGE_SIZE` is `int unsigned`, `SPACE_TYPE` is nullable
  `varchar(10)`, and `INSTANT_COLS` / `TOTAL_ROW_VERSIONS` are `int`.
- Numeric columns in this view report SQL `NULL` numeric precision and scale in
  `INFORMATION_SCHEMA.COLUMNS`.
- For simple persistent InnoDB tables, `NAME` is formatted as
  `schema/table`, `N_COLS` was the visible table column count plus three
  InnoDB system fields, `ROW_FORMAT` was `Dynamic`, `ZIP_PAGE_SIZE = 0`,
  `SPACE_TYPE = 'Single'`, `INSTANT_COLS = 0`, and
  `TOTAL_ROW_VERSIONS = 0`.
- Observed row-format-specific values were: `Redundant` with `FLAG = 0` and
  `ZIP_PAGE_SIZE = 0`; `Compact` with `FLAG = 1` and `ZIP_PAGE_SIZE = 0`;
  `Dynamic` with `FLAG = 33` and `ZIP_PAGE_SIZE = 0`; and `Compressed` with
  `FLAG = 41` and `ZIP_PAGE_SIZE = 8192` for the default compressed page size
  or explicit `KEY_BLOCK_SIZE=8`.
- MySQL-owned `TABLE_ID` and `SPACE` values are physical dictionary values and
  varied by runtime. MyLite will use deterministic descriptor/synthetic values
  instead.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.

Representative probe:

```sh
probe_db="mylite_innodb_tables_probe_$$"
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<SQL
DROP DATABASE IF EXISTS $probe_db;
CREATE DATABASE $probe_db;
USE $probe_db;
CREATE TABLE t_primary(id INT NOT NULL, v VARCHAR(10), PRIMARY KEY(id)) ENGINE=InnoDB;
CREATE TABLE t_no_pk(a INT, b INT) ENGINE=InnoDB;
CREATE TABLE t_unique(a INT NOT NULL, b INT NOT NULL, UNIQUE KEY uq_a(a)) ENGINE=InnoDB;
CREATE TABLE t_compact(a INT) ENGINE=InnoDB ROW_FORMAT=COMPACT;
CREATE TABLE t_redundant(a INT) ENGINE=InnoDB ROW_FORMAT=REDUNDANT;
CREATE TABLE t_compressed(a INT) ENGINE=InnoDB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8;
SELECT SUBSTRING_INDEX(NAME,'/',-1), FLAG, N_COLS, ROW_FORMAT, ZIP_PAGE_SIZE,
       SPACE_TYPE, INSTANT_COLS, TOTAL_ROW_VERSIONS, TABLE_ID > 0, SPACE > 0
  FROM INFORMATION_SCHEMA.INNODB_TABLES
 WHERE NAME IN ('$probe_db/t_primary', '$probe_db/t_no_pk',
                '$probe_db/t_unique', '$probe_db/t_compact',
                '$probe_db/t_redundant', '$probe_db/t_compressed')
 ORDER BY NAME;
SELECT @@warning_count, ROW_COUNT();
DROP DATABASE $probe_db;
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_TABLES` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_TABLES` reads while `information_schema` is the selected
  schema;
- one row per persistent MyLite base-table descriptor;
- `TABLE_ID` derived from the MyLite table descriptor id;
- `NAME` formatted as `schema/table`;
- `FLAG` values matching observed MySQL 8.4.9 row-format code points:
  `Redundant = 0`, `Compact = 1`, `Dynamic = 33`, and `Compressed = 41`;
- `N_COLS` as visible descriptor column count plus three InnoDB hidden/system
  fields;
- `SPACE = 0`, because MyLite does not expose physical InnoDB tablespaces;
- `ROW_FORMAT` from the same descriptor logic used by `SHOW TABLE STATUS`;
- `ZIP_PAGE_SIZE = 0` for non-compressed rows, `8192` for compressed rows with
  no stored key-block size, or `KEY_BLOCK_SIZE * 1024` for compressed rows with
  an explicit key-block size;
- `SPACE_TYPE = 'Single'`, `INSTANT_COLS = 0`, and `TOTAL_ROW_VERSIONS = 0`;
- descriptor changes from supported `ALTER TABLE` and `RENAME TABLE` paths are
  reflected in subsequent reads;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- exact MySQL physical `TABLE_ID`, `SPACE`, tablespace path, or file-per-table
  state;
- temporary tables, views, built-in-schema rows, privilege checks, and
  account-specific filtering;
- instant DDL version history, row-version counters, InnoDB compression zip
  page sizes, and complete InnoDB dictionary flag bit semantics.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_TABLES` as a supported
  information-schema system view and emits rows from loaded catalog
  descriptors.
- Catalog metadata: unchanged. Existing table and column descriptors are
  authoritative.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLES;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE 'app/%';
SELECT t.NAME, t.N_COLS
  FROM INFORMATION_SCHEMA.INNODB_TABLES AS t
 WHERE t.ROW_FORMAT = 'Dynamic'
 ORDER BY t.NAME;
```

## Runtime Semantics

`INNODB_TABLES` is registered in the static information-schema table registry.
Row production is descriptor-backed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads iterate persistent catalog schemas and base tables;
- `TABLE_ID` is the loaded MyLite table descriptor id;
- `NAME` is `schema/table`;
- `FLAG` is synthesized from the resolved row format: `Redundant` maps to `0`,
  `Compact` maps to `1`, `Dynamic` maps to `33`, and `Compressed` maps to
  `41`;
- `N_COLS` is `column_count + 3`;
- `SPACE` is `0`;
- `ROW_FORMAT` is the resolved table row format;
- `ZIP_PAGE_SIZE` is `0` for non-compressed rows, `8192` for compressed rows
  with no stored key-block size, or `KEY_BLOCK_SIZE * 1024` for compressed
  rows with an explicit key-block size;
- `SPACE_TYPE` is `Single`;
- `INSTANT_COLS` is `0`;
- `TOTAL_ROW_VERSIONS` is `0`;
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
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 10 columns;
- descriptor rows for primary-key, no-primary-key, unique-key, and explicit
  `ROW_FORMAT=COMPACT`, `ROW_FORMAT=REDUNDANT`, and
  `ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8` base tables;
- `TABLE_ID`, `NAME`, `FLAG`, `N_COLS`, `SPACE`, `ROW_FORMAT`,
  `ZIP_PAGE_SIZE`, `SPACE_TYPE`, `INSTANT_COLS`, and
  `TOTAL_ROW_VERSIONS`;
- `COUNT(*)`, case-insensitive table lookup, aliases, predicates, and
  unqualified reads after `USE information_schema`;
- descriptor updates after supported `ALTER TABLE` and `RENAME TABLE` paths;
- persistence across reopen;
- `@@warning_count` and `ROW_COUNT()` status after a successful read.

## Compatibility Notes

The view is intentionally marked partial. It is useful for applications that
join InnoDB dictionary views or inspect row format and base-table presence, but
it remains synthetic metadata. MyLite does not implement InnoDB's physical
dictionary, tablespaces, or privilege-filtered visibility.

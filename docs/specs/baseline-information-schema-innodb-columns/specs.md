# Baseline INFORMATION_SCHEMA InnoDB Columns

## Status

This phase adds descriptor-backed `INFORMATION_SCHEMA.INNODB_COLUMNS` rows for
MyLite persistent base tables. The view exposes MySQL 8.4.9-shaped system table
and column metadata and reports stable InnoDB column-dictionary fields from
existing MyLite catalog descriptors.

The slice does not add physical InnoDB dictionary storage, hidden InnoDB system
columns, virtual-column `POS` bit encoding, instant DDL default history,
`INFORMATION_SCHEMA.INNODB_VIRTUAL`, temporary-table rows, or privilege
filtering.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing table descriptor, column descriptor, `INFORMATION_SCHEMA.COLUMNS`,
  `INFORMATION_SCHEMA.INNODB_TABLES`, and InnoDB index dictionary
  implementations in `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-columns-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_COLUMNS` exists as a `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for the system view row.
- `INNODB_COLUMNS` has columns `TABLE_ID`, `NAME`, `POS`, `MTYPE`, `PRTYPE`,
  `LEN`, `HAS_DEFAULT`, and `DEFAULT_VALUE`.
- `TABLE_ID` and `POS` are `bigint unsigned`, `NAME` is non-null
  `varchar(193)` with `CHARACTER_MAXIMUM_LENGTH = 64`,
  `CHARACTER_OCTET_LENGTH = 193`, charset `utf8mb3`, and collation
  `utf8mb3_general_ci`; `MTYPE`, `PRTYPE`, `LEN`, and `HAS_DEFAULT` are
  non-null `int`; `DEFAULT_VALUE` is nullable `text`.
- The system-view column metadata reports empty-string `COLUMN_DEFAULT` values
  for all eight columns, including nullable `DEFAULT_VALUE`.
- Numeric columns in this view report SQL `NULL` numeric precision and scale
  metadata in `INFORMATION_SCHEMA.COLUMNS`.
- User column rows use zero-based `POS` values.
- `HAS_DEFAULT = 0` and `DEFAULT_VALUE = NULL` for ordinary table columns,
  even when MySQL would have implicit DDL defaults. MySQL uses those fields for
  columns added with instant DDL.
- For common current MyLite descriptor families, observed `MTYPE` code points
  were `3` for fixed binary, bit, decimal, time, datetime, and timestamp; `4`
  for varbinary; `5` for text, blob, and JSON; `6` for integer, `YEAR`,
  `DATE`, `ENUM`, and `SET`; `9` for `FLOAT`; `10` for `DOUBLE`; `12` for
  `VARCHAR`; `13` for `CHAR`; and `14` for spatial columns.
- Observed `PRTYPE` values combine the MySQL type code, nullability,
  unsignedness, and for string/blob descriptors the MySQL collation id. Current
  mappings are documented in tests for integer, decimal, approximate, bit,
  temporal, utf8mb4/utf8mb3/ascii character strings, binary strings, blob/text
  families, enum, set, JSON, and spatial descriptors.
- Observed `LEN` values use storage-byte lengths: integer widths, packed
  decimal byte counts, maximum byte lengths for character and binary string
  descriptors, `9`/`10`/`11`/`12` for tiny/regular/medium/long text or blob
  families, `12` for JSON, and `12` for spatial descriptors.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.
- `RENAME TABLE` keeps the observed InnoDB `TABLE_ID` stable, and the renamed
  table remains visible through `INNODB_TABLES` / `INNODB_COLUMNS` joins.

Representative probe:

```sh
probe_db="mylite_innodb_columns_probe_$$"
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<SQL
DROP DATABASE IF EXISTS $probe_db;
CREATE DATABASE $probe_db;
USE $probe_db;
CREATE TABLE c_sample(
  id INT NOT NULL,
  nullable_int INT,
  big BIGINT UNSIGNED,
  c CHAR(4),
  v VARCHAR(10) NOT NULL,
  b BINARY(3),
  vb VARBINARY(5),
  d DECIMAL(8,2),
  f FLOAT,
  dbl DOUBLE,
  t TEXT,
  bl BLOB,
  js JSON,
  y YEAR,
  da DATE,
  ti TIME,
  dt DATETIME,
  ts TIMESTAMP NULL,
  PRIMARY KEY(id)
) ENGINE=InnoDB;
SELECT c.NAME,c.POS,c.MTYPE,c.PRTYPE,c.LEN,c.HAS_DEFAULT,c.DEFAULT_VALUE IS NULL
  FROM INFORMATION_SCHEMA.INNODB_COLUMNS c
  JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON t.TABLE_ID=c.TABLE_ID
 WHERE t.NAME='$probe_db/c_sample'
 ORDER BY c.POS;
SELECT @@warning_count, ROW_COUNT();
DROP DATABASE $probe_db;
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_COLUMNS` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_COLUMNS` reads while `information_schema` is the selected
  schema;
- one row per persistent MyLite base-table column descriptor;
- `TABLE_ID` derived from the MyLite table descriptor id;
- `NAME` derived from the MyLite column descriptor name;
- `POS` derived from `ordinal_position - 1`;
- `MTYPE`, `PRTYPE`, and `LEN` for current supported integer, exact decimal,
  approximate, bit, year, date, time, datetime, timestamp, `CHAR`, `VARCHAR`,
  text family, binary string/blob family, `ENUM`, `SET`, JSON, and spatial
  descriptors;
- `HAS_DEFAULT = 0` and `DEFAULT_VALUE = NULL`, because MyLite does not expose
  InnoDB instant-DDL column-version metadata;
- descriptor changes from supported `ALTER TABLE` and `RENAME TABLE` paths are
  reflected in subsequent reads;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- exact MySQL physical `TABLE_ID` values beyond MyLite descriptor ids;
- hidden InnoDB system columns, temporary tables, views, built-in-schema rows,
  privilege checks, and account-specific filtering;
- `INFORMATION_SCHEMA.INNODB_VIRTUAL` rows and MySQL's special virtual-column
  `POS` encoding;
- instant DDL `HAS_DEFAULT` and binary `DEFAULT_VALUE` history;
- complete InnoDB internal `PRTYPE` bit semantics for descriptor families not
  admitted by current MyLite DDL.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_COLUMNS` as a supported
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
SELECT * FROM INFORMATION_SCHEMA.INNODB_COLUMNS;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_COLUMNS WHERE TABLE_ID > 0;
SELECT c.NAME, c.POS, c.MTYPE, c.PRTYPE, c.LEN
  FROM INFORMATION_SCHEMA.INNODB_COLUMNS AS c
 WHERE c.NAME IN ('id', 'title')
 ORDER BY c.TABLE_ID, c.POS;
```

## Runtime Semantics

`INNODB_COLUMNS` is registered in the static information-schema table registry.
Row production is descriptor-backed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads iterate persistent catalog schemas and base tables;
- each loaded column descriptor produces one `INNODB_COLUMNS` row;
- `TABLE_ID` is the loaded MyLite table descriptor id;
- `NAME` is the descriptor column name;
- `POS` is `ordinal_position - 1`;
- `MTYPE` maps each supported descriptor family to the observed MySQL 8.4.9
  InnoDB main type code;
- `PRTYPE` is synthesized from observed MySQL 8.4.9 code points for the
  supported descriptor families, using effective collation ids for
  character/blob descriptors where MySQL includes them;
- `LEN` is synthesized from descriptor storage bytes rather than display text
  width;
- `HAS_DEFAULT` is always `0`;
- `DEFAULT_VALUE` is SQL `NULL`;
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
- `INFORMATION_SCHEMA.COLUMNS` metadata for all eight columns;
- descriptor rows for integer, decimal, approximate, bit, temporal,
  character, binary, text/blob, enum, set, JSON, and spatial descriptors;
- effective utf8mb4, utf8mb3 national, ascii, and binary charset/collation
  `PRTYPE` cases;
- nullable and `NOT NULL` flag contributions;
- zero-based `POS`, `HAS_DEFAULT = 0`, and `DEFAULT_VALUE IS NULL`;
- `COUNT(*)`, case-insensitive table lookup, aliases, predicates, and
  unqualified reads after `USE information_schema`;
- descriptor updates after supported `ALTER TABLE` and `RENAME TABLE` paths;
- persistence across reopen;
- `@@warning_count` and `ROW_COUNT()` status after a successful read.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_columns_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_columns|information_schema_innodb_tables|information_schema_innodb_indexes|builtin_schema_table_directory|show_table_status_introspection)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_columns_expectations.sh
git diff --check
cmake --workflow --preset check
```

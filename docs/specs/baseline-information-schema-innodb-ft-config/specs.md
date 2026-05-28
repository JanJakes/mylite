# Baseline INFORMATION_SCHEMA INNODB_FT_CONFIG

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_FT_CONFIG` as a MySQL-shaped
synthetic information-schema system view. The table is queryable, exposes
MySQL 8.4.9 table and column metadata, and returns an empty row set in MyLite
because MyLite does not model `innodb_ft_aux_table` or physical InnoDB
full-text index internals.

The slice is metadata-only. It does not add full-text tokenization,
`MATCH ... AGAINST`, custom stopword tables, parser plugins, physical InnoDB
full-text auxiliary tables, privilege filtering, or mutable InnoDB monitoring
state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing full-text metadata implementation:
  `docs/specs/baseline-fulltext-index-metadata/specs.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_FT_CONFIG`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-ft-config-table.html
- MySQL 8.4 Reference Manual, InnoDB full-text information-schema tables:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-information-schema-fulltext-index-tables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_FT_CONFIG` exists as an `information_schema`
  `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports two columns in order: `KEY` and
  `VALUE`.
- Both columns are non-null `varchar(193)`, report character maximum length
  `64`, octet length `193`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, empty string defaults, SQL `NULL` numeric and
  datetime metadata, and `PRIVILEGES = 'select'`.
- A default fresh server returns zero rows. Supported reads leave
  `@@warning_count = 0`, and `ROW_COUNT()` reports `-1` after the `SELECT`.
- The `KEY` column must be quoted when selected directly in SQL because
  unquoted `KEY` is parsed as a keyword by MySQL.
- Official MySQL documentation states that the table is initially empty and
  becomes populated after `innodb_ft_aux_table` is set to a table containing an
  InnoDB `FULLTEXT` index. MyLite does not implement that auxiliary-table
  mechanism in this slice.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT VERSION(); SHOW FULL TABLES FROM INFORMATION_SCHEMA LIKE 'INNODB_FT_CONFIG'; \
   SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT \
     FROM INFORMATION_SCHEMA.TABLES \
    WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='INNODB_FT_CONFIG'; \
   SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG; \
   SELECT @@warning_count, ROW_COUNT();"

docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, \
          CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION, \
          NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME, \
          COLUMN_TYPE,PRIVILEGES \
     FROM INFORMATION_SCHEMA.COLUMNS \
    WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='INNODB_FT_CONFIG' \
    ORDER BY ORDINAL_POSITION;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_FT_CONFIG` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- wildcard reads with `KEY` and `VALUE` column labels;
- direct reads of the quoted `` `KEY` `` column through the existing quoted
  identifier path;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- stable empty-row behavior in all MyLite sessions;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- dynamic rows driven by `innodb_ft_aux_table`;
- InnoDB full-text index metadata values such as optimize checkpoints, synced
  document ids, selected stopword tables, or stopword usage flags;
- `MATCH ... AGAINST`, full-text ranking, parser plugins, generated full-text
  auxiliary index tables, index caches, or deleted-doc tables;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, and quoted identifiers.
- Analyzer/runtime: recognizes `INNODB_FT_CONFIG` as a supported
  information-schema system view and returns an empty row set.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- Full-text subsystem: unchanged. MyLite keeps current metadata-only full-text
  index behavior and does not produce InnoDB auxiliary table configuration
  rows.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG;
SELECT `KEY`, VALUE
  FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG
 WHERE VALUE = '';
```

## Runtime Semantics

`INNODB_FT_CONFIG` is registered in the static information-schema table
registry. Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite table or index descriptors because the
  MySQL table reflects the selected InnoDB full-text auxiliary table, not the
  ordinary data dictionary;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- unquoted `KEY` remains subject to parser keyword handling; applications can
  use `SELECT *`, `COUNT(*)`, or quoted `` `KEY` ``;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Performance

The row set is static and empty. Metadata rows are generated from in-memory
descriptors and do not read or write MyLite catalog descriptors, physical row
storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels `KEY` and `VALUE` with an empty row set;
- row count and representative empty predicates;
- quoted `` `KEY` `` projection;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for both columns;
- `USE information_schema` unqualified-table reads;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_ft_config_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_ft_config|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_ft_config_expectations.sh
git diff --check
cmake --workflow --preset check
```

# Baseline INFORMATION_SCHEMA InnoDB FT Index Tables

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE` and
`INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE` as MySQL-shaped synthetic
information-schema system views. Both tables are queryable, expose MySQL
8.4.9 table and column metadata, and return empty row sets in MyLite because
MyLite does not model `innodb_ft_aux_table`, physical InnoDB full-text
auxiliary tables, or token-position indexes.

The slice is metadata-only. It does not add full-text tokenization,
`MATCH ... AGAINST`, parser plugins, physical InnoDB full-text auxiliary
tables, privilege filtering, or mutable InnoDB monitoring state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing full-text metadata implementation:
  `docs/specs/baseline-fulltext-index-metadata/specs.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_FT_INDEX_CACHE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-ft-index-cache-table.html
- MySQL 8.4 Reference Manual, `INNODB_FT_INDEX_TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-ft-index-table-table.html
- MySQL 8.4 Reference Manual, InnoDB full-text information-schema tables:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-information-schema-fulltext_index-tables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE` and
  `INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE` exist as `information_schema`
  `SYSTEM VIEW` tables.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both tables.
- `INFORMATION_SCHEMA.COLUMNS` reports six columns for each table in order:
  `WORD`, `FIRST_DOC_ID`, `LAST_DOC_ID`, `DOC_COUNT`, `DOC_ID`, and
  `POSITION`.
- `WORD` is non-null `varchar(337)`, reports character maximum length `112`,
  octet length `337`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, empty string default, and `PRIVILEGES = 'select'`.
- `FIRST_DOC_ID`, `LAST_DOC_ID`, `DOC_COUNT`, `DOC_ID`, and `POSITION` are
  non-null `bigint unsigned` columns, report empty string defaults, SQL `NULL`
  character, numeric, and datetime metadata, and `PRIVILEGES = 'select'`.
- A default fresh server returns zero rows from both tables. Supported reads
  leave `@@warning_count = 0`, and `ROW_COUNT()` reports `-1` after the
  `SELECT`.
- Official MySQL documentation states that these tables are initially empty
  and become populated only after `innodb_ft_aux_table` is set to a table with
  an InnoDB `FULLTEXT` index. MyLite does not implement that auxiliary-table
  mechanism in this slice.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT VERSION(); \
   SHOW FULL TABLES FROM INFORMATION_SCHEMA \
    WHERE Tables_in_INFORMATION_SCHEMA IN ('INNODB_FT_INDEX_CACHE','INNODB_FT_INDEX_TABLE'); \
   SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT \
     FROM INFORMATION_SCHEMA.TABLES \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_FT_INDEX_CACHE','INNODB_FT_INDEX_TABLE') \
    ORDER BY TABLE_NAME; \
   SELECT 'CACHE', COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE; \
   SELECT 'TABLE', COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE; \
   SELECT @@warning_count, ROW_COUNT();"

docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, \
          CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION, \
          NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME, \
          COLUMN_TYPE,PRIVILEGES \
     FROM INFORMATION_SCHEMA.COLUMNS \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_FT_INDEX_CACHE','INNODB_FT_INDEX_TABLE') \
    ORDER BY TABLE_NAME, ORDINAL_POSITION;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE` and
  `INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- wildcard reads with the six MySQL column labels;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- stable empty-row behavior in all MyLite sessions;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- dynamic rows driven by `innodb_ft_aux_table`;
- InnoDB token storage, full-text index cache state, optimized full-text index
  rows, document-position lists, or physical auxiliary table rows;
- `MATCH ... AGAINST`, full-text ranking, parser plugins, generated full-text
  auxiliary index tables, custom stopword tables, or deleted-doc queues;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, and identifiers.
- Analyzer/runtime: recognizes both InnoDB full-text index views as supported
  information-schema system views and returns empty row sets.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- Full-text subsystem: unchanged. MyLite keeps current metadata-only full-text
  index behavior and does not produce InnoDB token-position rows.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE;
SELECT c.WORD, c.DOC_ID, c.POSITION
  FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE AS c
 WHERE c.WORD = '';
```

## Runtime Semantics

`INNODB_FT_INDEX_CACHE` and `INNODB_FT_INDEX_TABLE` are registered in the
static information-schema table registry. Row production is intentionally
empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite table or index descriptors because the
  MySQL tables reflect selected InnoDB full-text auxiliary state, not ordinary
  data-dictionary metadata;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Performance

Both row sets are static and empty. Metadata rows are generated from in-memory
descriptors and do not read or write MyLite catalog descriptors, physical row
storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels and empty row sets for both views;
- row counts and representative empty predicates;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view rows for both tables;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all columns in both tables;
- `USE information_schema` unqualified-table reads;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_ft_index_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_ft_index|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_ft_index_expectations.sh
git diff --check
cmake --workflow --preset check
```

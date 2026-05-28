# Baseline INFORMATION_SCHEMA INNODB_FT_DEFAULT_STOPWORD

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD` as a
MySQL-shaped synthetic information-schema system view. The table is queryable,
exposes MySQL 8.4.9 table and column metadata, and returns the observed
default InnoDB full-text stopword list.

The slice is metadata-only. It does not add full-text tokenization,
`MATCH ... AGAINST`, custom stopword-table variables, parser plugins,
privilege filtering, or physical InnoDB full-text index tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing full-text metadata implementation:
  `docs/specs/baseline-fulltext-index-metadata/specs.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_FT_DEFAULT_STOPWORD`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-ft-default-stopword-table.html
- MySQL 8.4 Reference Manual, full-text stopwords:
  https://dev.mysql.com/doc/refman/8.4/en/fulltext-stopwords.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD` exists as an
  `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports one lower-case column named `value`.
  It is non-null `varchar(18)`, reports character maximum length `6`, octet
  length `18`, character set `utf8mb3`, collation `utf8mb3_general_ci`, empty
  string default, and `PRIVILEGES = 'select'`.
- A default session returns 36 rows in this order:
  `a`, `about`, `an`, `are`, `as`, `at`, `be`, `by`, `com`, `de`, `en`,
  `for`, `from`, `how`, `i`, `in`, `is`, `it`, `la`, `of`, `on`, `or`,
  `that`, `the`, `this`, `to`, `was`, `what`, `when`, `where`, `who`,
  `will`, `with`, `und`, `the`, `www`.
- The duplicate `the` row is preserved. `COUNT(*) WHERE value = 'the'`
  returns `2`.
- Supported reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports `-1`
  after the `SELECT`.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SHOW FULL TABLES FROM INFORMATION_SCHEMA LIKE 'INNODB_FT_DEFAULT_STOPWORD'; \
   SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT \
     FROM INFORMATION_SCHEMA.TABLES \
    WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='INNODB_FT_DEFAULT_STOPWORD'; \
   SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD; \
   SELECT @@warning_count, ROW_COUNT();"

docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, \
          CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION, \
          NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME, \
          COLUMN_TYPE,PRIVILEGES \
     FROM INFORMATION_SCHEMA.COLUMNS \
    WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='INNODB_FT_DEFAULT_STOPWORD' \
    ORDER BY ORDINAL_POSITION;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD` using the
  existing information-schema query subset;
- case-insensitive information-schema table name lookup;
- lower-case `value` result column naming and column metadata;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- the 36 observed default InnoDB full-text stopword rows, preserving order and
  duplicates;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- using these rows for real full-text tokenization or search;
- `MATCH ... AGAINST`, full-text ranking, parser plugins, generated full-text
  index tables, index caches, or deleted-doc tables;
- `innodb_ft_server_stopword_table`,
  `innodb_ft_user_stopword_table`, `innodb_ft_enable_stopword`, or
  `ft_stopword_file` semantics;
- custom stopword table descriptors;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `INNODB_FT_DEFAULT_STOPWORD` as a supported
  information-schema system view and appends static rows.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- Full-text subsystem: unchanged. MyLite keeps current metadata-only full-text
  index behavior and does not consume stopwords for matching.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD;
SELECT value
  FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD
 WHERE value IN ('a', 'www')
 ORDER BY value;
```

## Runtime Semantics

`INNODB_FT_DEFAULT_STOPWORD` is registered in the static information-schema
table registry. Row production emits the observed MySQL 8.4.9 default rows as
static strings:

- one row per default stopword value;
- insertion order matches the observed default `SELECT *` order;
- duplicate rows are preserved, including the second `the` row;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

The row set is independent of MyLite full-text index descriptors and does not
interact with tokenization, storage, or query planning.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Performance

The view emits 36 static one-column rows. It does not read or write MyLite
catalog descriptors, physical row storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column label `value` and exact ordered default rows;
- row count and duplicate `the` count;
- representative predicates and ordering;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for the lower-case `value` column;
- `USE information_schema` unqualified-table reads;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_ft_default_stopword_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_ft_default_stopword|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_ft_default_stopword_expectations.sh
git diff --check
cmake --workflow --preset check
```

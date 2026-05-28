# Baseline INFORMATION_SCHEMA INNODB_TEMP_TABLE_INFO

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO` as a
MySQL-shaped synthetic information-schema system view. The table is queryable,
exposes MySQL 8.4.9 table and column metadata, and returns no rows in MyLite
because MyLite does not model InnoDB temporary-table descriptors or per-session
InnoDB temporary tablespaces.

The slice is metadata-only. It does not add physical InnoDB temporary
tablespaces, runtime InnoDB table identifiers, temporary-table descriptor
reflection, privilege filtering, or mutable InnoDB monitoring state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing information-schema tests under `packages/libmylite/tests/`
- MySQL 8.4 Reference Manual, `INNODB_TEMP_TABLE_INFO`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-temp-table-info-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO` exists as an
  `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports four columns in order:
  `TABLE_ID`, `NAME`, `N_COLS`, and `SPACE`.
- `TABLE_ID` is non-null `bigint unsigned`; `N_COLS` and `SPACE` are non-null
  `int unsigned`. The three numeric columns report empty string defaults and
  SQL `NULL` for numeric precision, scale, character set, collation, and
  datetime precision metadata.
- `NAME` is nullable `varchar(64)`, reports character maximum length `21`,
  octet length `64`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, and an empty string default.
- A default session with no active user-created InnoDB temporary tables returns
  zero rows. The supported `SELECT` leaves `@@warning_count = 0`, and
  `ROW_COUNT()` reports `-1`.
- Creating a user `InnoDB` temporary table in the session causes MySQL to
  expose a dynamic row with the generated temporary table name, an InnoDB table
  id, `N_COLS` including InnoDB hidden columns, and a temporary tablespace id.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT VERSION(); SHOW FULL TABLES FROM INFORMATION_SCHEMA LIKE 'INNODB_TEMP_TABLE_INFO'; \
   SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT \
     FROM INFORMATION_SCHEMA.TABLES \
    WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='INNODB_TEMP_TABLE_INFO'; \
   SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO; \
   SELECT @@warning_count, ROW_COUNT();"

docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, \
          CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION, \
          NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME, \
          COLUMN_TYPE,PRIVILEGES \
     FROM INFORMATION_SCHEMA.COLUMNS \
    WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='INNODB_TEMP_TABLE_INFO' \
    ORDER BY ORDINAL_POSITION;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- stable empty-row behavior in all MyLite sessions;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- dynamic rows for user-created MyLite temporary tables;
- InnoDB temporary table identifiers, generated InnoDB temporary names,
  hidden-column accounting, or temporary tablespace ids;
- internal optimizer temporary table reporting;
- `PROCESS` privilege checks or account-specific visibility;
- physical InnoDB temporary tablespace files;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `INNODB_TEMP_TABLE_INFO` as a supported
  information-schema system view and returns an empty row set.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO;
SELECT TABLE_ID, NAME, N_COLS, SPACE
  FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO;
SELECT t.NAME
  FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO AS t
 WHERE t.NAME IS NOT NULL;
```

## Runtime Semantics

`INNODB_TEMP_TABLE_INFO` is registered in the static information-schema table
registry. Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite temporary-table descriptors because MyLite
  does not expose InnoDB temporary table ids or InnoDB temporary tablespace
  ids;
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

The row set is static and empty. Metadata rows are generated from in-memory
descriptors and do not read or write MyLite catalog descriptors, physical row
storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels and empty row set;
- row count and representative predicates;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all four columns;
- `USE information_schema` unqualified-table reads;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_temp_table_info_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_temp_table_info|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_temp_table_info_expectations.sh
git diff --check
cmake --workflow --preset check
```

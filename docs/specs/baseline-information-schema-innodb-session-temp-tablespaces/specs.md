# Baseline INFORMATION_SCHEMA INNODB_SESSION_TEMP_TABLESPACES

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES` as a
queryable synthetic information-schema system view. The view exposes
MySQL 8.4.9-shaped table and column metadata and returns a deterministic
baseline pool of session temporary tablespace rows, including one current
session intrinsic tablespace row keyed to MyLite's connection id.

The slice is metadata-only. It does not add physical InnoDB temporary
tablespace files, user-created temporary tablespace allocation, replica
temporary tablespaces, privilege filtering, or mutable InnoDB monitoring state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing information-schema tests under `packages/libmylite/tests/`
- MySQL 8.4 Reference Manual, `INNODB_SESSION_TEMP_TABLESPACES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-session-temp-tablespaces-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES` exists as an
  `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports six columns in order: `ID`, `SPACE`,
  `PATH`, `SIZE`, `STATE`, and `PURPOSE`.
- `ID` and `SPACE` are non-null `int unsigned`; `SIZE` is non-null
  `bigint unsigned`. The numeric columns report empty string defaults and SQL
  `NULL` for character, numeric precision, numeric scale, and datetime
  precision metadata.
- `PATH` is non-null `varchar(4001)`, reports character maximum length `1333`,
  octet length `4001`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, and an empty string default.
- `STATE` and `PURPOSE` are non-null `varchar(192)`, report character maximum
  length `64`, octet length `192`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, and empty string defaults.
- A session reports ten pooled rows ordered by `SPACE`: nine inactive
  available rows with `ID = 0`, `SIZE = 81920`, `STATE = 'INACTIVE'`, and
  `PURPOSE = 'NONE'`; one row uses the current `CONNECTION_ID()`,
  `SIZE = 81920`, `STATE = 'ACTIVE'`, and `PURPOSE = 'INTRINSIC'`.
- The active intrinsic row can move between pooled temp-file paths after prior
  temporary-table activity in the MySQL runtime, so this slice treats the
  ten-row pool and one active intrinsic row as the stable compatibility
  contract.
- Creating a user temporary InnoDB table in the session makes MySQL mark one
  additional pooled row as `ACTIVE` / `USER` for that connection.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probes:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,
       AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_SESSION_TEMP_TABLESPACES';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_SESSION_TEMP_TABLESPACES'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES;
SELECT ID = CONNECTION_ID(), SPACE, PATH, SIZE, STATE, PURPOSE
  FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES
 ORDER BY SPACE;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES` using the
  existing information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_SESSION_TEMP_TABLESPACES` reads while
  `information_schema` is the selected schema;
- ten deterministic baseline pooled session temporary tablespace rows;
- the active intrinsic row uses MyLite's current session connection id in
  `ID`;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- dynamic `ACTIVE` / `USER` rows for user-created MyLite temporary tables;
- replica `SLAVE` rows or InnoDB replica temporary tablespaces;
- physical `#innodb_temp` files, InnoDB tablespace allocation, persisted space
  identifiers, or mutable tablespace sizes;
- exact server-pool variability across MySQL restarts;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_SESSION_TEMP_TABLESPACES` as a
  supported information-schema system view and emits baseline synthetic rows.
- Catalog metadata: unchanged. No schema, table, column, or temporary-table
  descriptors are introduced by this slice.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES;
SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES
 WHERE STATE = 'ACTIVE';
SELECT t.PATH
  FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES AS t
 WHERE t.PURPOSE = 'INTRINSIC';
```

## Runtime Semantics

`INNODB_SESSION_TEMP_TABLESPACES` is registered in the static
information-schema table registry. Row production is synthetic:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads emit nine inactive rows for `temp_1.ibt` through `temp_9.ibt`
  using `ID = 0`, `SIZE = 81920`, `STATE = 'INACTIVE'`, and
  `PURPOSE = 'NONE'`;
- direct reads emit one active intrinsic row for `temp_10.ibt` using the
  current MyLite connection id, `SIZE = 81920`, `STATE = 'ACTIVE'`, and
  `PURPOSE = 'INTRINSIC'`;
- no row is added for user-created MyLite temporary tables because MyLite does
  not expose physical InnoDB temporary tablespace allocation;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, and limits retain the
  current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- `SHOW FULL TABLES` and `INFORMATION_SCHEMA.TABLES` metadata for the system
  view;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all six columns;
- exact MyLite baseline rows, including current MyLite connection id on the
  active intrinsic row;
- `COUNT(*)`, case-insensitive table lookup, aliases, predicates, and
  unqualified reads after `USE information_schema`;
- `@@warning_count` and `ROW_COUNT()` status after a successful read;
- file-backed read behavior and unchanged MyLite file preamble;
- MySQL runtime evidence for the additional `ACTIVE` / `USER` row that appears
  after creating a user temporary InnoDB table.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_session_temp_tablespaces_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_session_temp_tablespaces|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_session_temp_tablespaces_expectations.sh
git diff --check
cmake --workflow --preset check
```

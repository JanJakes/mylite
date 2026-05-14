# Baseline INFORMATION_SCHEMA ROUTINES

## Status

This phase adds a narrow stored-routine metadata compatibility surface:
`INFORMATION_SCHEMA.ROUTINES`. MyLite already exposes empty
`SHOW PROCEDURE STATUS` and `SHOW FUNCTION STATUS` introspection, but it does
not support stored routine DDL, routine descriptors, routine execution,
routine body storage, or routine privileges. The supported behavior is an empty
but queryable synthetic information-schema table with MySQL 8.4.9 column
metadata, plus matching `INFORMATION_SCHEMA.TABLES` and
`INFORMATION_SCHEMA.COLUMNS` system-view rows.

This is a compatibility shim for clients that probe stored routine metadata.
It turns common discovery queries from "unknown table" failures into empty
result sets while keeping routine functionality explicitly unsupported.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- Existing empty routine status introspection:
  `docs/specs/baseline-show-routine-status-empty-introspection/specs.md`
- Existing `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.TRIGGERS`, and
  `INFORMATION_SCHEMA.EVENTS` surfaces:
  `docs/specs/baseline-information-schema-views/specs.md`,
  `docs/specs/baseline-information-schema-triggers/specs.md`, and
  `docs/specs/baseline-information-schema-events/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ROUTINES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-routines-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.PARAMETERS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-parameters-table.html
- MySQL 8.4 Reference Manual, `SHOW PROCEDURE STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-procedure-status.html
- MySQL 8.4 Reference Manual, `SHOW FUNCTION STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-function-status.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_routines_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.ROUTINES` has these result columns in order:
  `SPECIFIC_NAME`, `ROUTINE_CATALOG`, `ROUTINE_SCHEMA`, `ROUTINE_NAME`,
  `ROUTINE_TYPE`, `DATA_TYPE`, `CHARACTER_MAXIMUM_LENGTH`,
  `CHARACTER_OCTET_LENGTH`, `NUMERIC_PRECISION`, `NUMERIC_SCALE`,
  `DATETIME_PRECISION`, `CHARACTER_SET_NAME`, `COLLATION_NAME`,
  `DTD_IDENTIFIER`, `ROUTINE_BODY`, `ROUTINE_DEFINITION`, `EXTERNAL_NAME`,
  `EXTERNAL_LANGUAGE`, `PARAMETER_STYLE`, `IS_DETERMINISTIC`,
  `SQL_DATA_ACCESS`, `SQL_PATH`, `SECURITY_TYPE`, `CREATED`, `LAST_ALTERED`,
  `SQL_MODE`, `ROUTINE_COMMENT`, `DEFINER`, `CHARACTER_SET_CLIENT`,
  `COLLATION_CONNECTION`, and `DATABASE_COLLATION`.
- `SHOW COLUMNS FROM INFORMATION_SCHEMA.ROUTINES` reports:
  - routine name columns as non-null `varchar(64)`;
  - `ROUTINE_TYPE` as a non-null enum of `FUNCTION` and `PROCEDURE`;
  - return-type metadata as nullable string, numeric, datetime, charset, and
    collation columns because procedures have no return value;
  - `ROUTINE_BODY`, `PARAMETER_STYLE`, and `IS_DETERMINISTIC` as non-null
    short `varchar(...)` columns with empty-string defaults in
    `INFORMATION_SCHEMA.COLUMNS`;
  - `ROUTINE_DEFINITION` and routine comments as text-family metadata columns;
  - `EXTERNAL_NAME` and `SQL_PATH` as nullable `varbinary(0)`;
  - `EXTERNAL_LANGUAGE` as non-null `varchar(64)` with default `SQL`;
  - `SQL_DATA_ACCESS`, `SECURITY_TYPE`, and `SQL_MODE` as MySQL-shaped enum or
    set metadata;
  - `CREATED` and `LAST_ALTERED` as non-null `timestamp` with
    `DATETIME_PRECISION = 0`.
- In a database containing tables but no routines,
  `SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = db`
  returns `0`.
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for `ROUTINES` with
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per `ROUTINES`
  column. Text metadata uses `utf8mb3`; routine names, short body/style flags,
  charset, connection-collation, and database-collation columns use
  `utf8mb3_general_ci`; most other textual columns use `utf8mb3_bin`. Temporal,
  numeric, and binary columns have no character set or collation.
- Creating a real MySQL stored procedure and stored function produces two
  `INFORMATION_SCHEMA.ROUTINES` rows. MyLite defers routine descriptors and
  therefore emits no routine rows in this phase.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.
- Unknown predicate columns fail before row scanning with
  `1054 / 42S22` and a message containing
  `Unknown column 'name' in 'where clause'`, even when the result set would be
  empty.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.ROUTINES` table definition to the existing synthetic
  information-schema registry;
- the exact 31-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.ROUTINES`;
- zero user rows until MyLite has stored routine descriptors;
- an `INFORMATION_SCHEMA.TABLES` system-view row for `ROUTINES`;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the 31 `ROUTINES`
  metadata columns;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, source aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- `CREATE PROCEDURE`, `CREATE FUNCTION`, `ALTER PROCEDURE`,
  `ALTER FUNCTION`, `DROP PROCEDURE`, `DROP FUNCTION`, `CALL`,
  `SHOW CREATE PROCEDURE`, `SHOW CREATE FUNCTION`, routine execution, routine
  descriptors, routine persistence, routine privileges, definers, stored
  routine SQL mode capture, parameter metadata, return-value rows in
  `INFORMATION_SCHEMA.PARAMETERS`, or routine character-set metadata;
- rows in `INFORMATION_SCHEMA.ROUTINES` for any routine kind;
- physical `information_schema` SQLite tables, SQLite function reflection,
  arbitrary SQLite SQL pass-through, storage-format changes, or SQLite fork
  patches;
- wider `INFORMATION_SCHEMA` query support beyond the existing planner.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Statement context: no new state. The existing SELECT result-set path owns
  diagnostics reset, warning count, and previous row-count updates.
- Parser/AST: no grammar changes. The existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` syntax is reused.
- Analyzer/planner: the existing information-schema query resolver owns source
  matching, projection, aliases, predicates, ordering, and limits against the
  synthetic table definition.
- Catalog module: no persistent catalog rows are read or written for
  `INFORMATION_SCHEMA.ROUTINES`. Future routine descriptors must be designed
  before this table emits user rows.
- Result builder: emits MySQL-shaped text/`NULL` metadata values through
  `mylite_result`.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: no metadata introspection or fork patch is needed;
  this is a MyLite-owned synthetic metadata view.

## Supported Query Surface

No new Lemon grammar is required. The existing limited information-schema
`SELECT` surface admits:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.ROUTINES [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.ROUTINES` source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through this table.

## `ROUTINES` Columns

`INFORMATION_SCHEMA.ROUTINES` has 31 columns:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `SPECIFIC_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `ROUTINE_CATALOG` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `ROUTINE_SCHEMA` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `ROUTINE_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `ROUTINE_TYPE` | non-null enum `FUNCTION` / `PROCEDURE`, `utf8mb3_bin` | No rows |
| `DATA_TYPE` | nullable `longtext`, `utf8mb3_bin` | No rows |
| `CHARACTER_MAXIMUM_LENGTH` | nullable `bigint` | No rows |
| `CHARACTER_OCTET_LENGTH` | nullable `bigint` | No rows |
| `NUMERIC_PRECISION` | nullable `int unsigned` | No rows |
| `NUMERIC_SCALE` | nullable `int unsigned` | No rows |
| `DATETIME_PRECISION` | nullable `int unsigned` | No rows |
| `CHARACTER_SET_NAME` | nullable `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `COLLATION_NAME` | nullable `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `DTD_IDENTIFIER` | nullable `longtext`, `utf8mb3_bin` | No rows |
| `ROUTINE_BODY` | non-null `varchar(8)`, `utf8mb3_general_ci`, empty default | No rows |
| `ROUTINE_DEFINITION` | nullable `longtext`, `utf8mb3_bin` | No rows |
| `EXTERNAL_NAME` | nullable `varbinary(0)` | No rows |
| `EXTERNAL_LANGUAGE` | non-null `varchar(64)`, `utf8mb3_bin`, default `SQL` | No rows |
| `PARAMETER_STYLE` | non-null `varchar(3)`, `utf8mb3_general_ci`, empty default | No rows |
| `IS_DETERMINISTIC` | non-null `varchar(3)`, `utf8mb3_general_ci`, empty default | No rows |
| `SQL_DATA_ACCESS` | non-null SQL data access enum, `utf8mb3_bin` | No rows |
| `SQL_PATH` | nullable `varbinary(0)` | No rows |
| `SECURITY_TYPE` | non-null security enum, `utf8mb3_bin` | No rows |
| `CREATED` | non-null `timestamp`, precision `0` | No rows |
| `LAST_ALTERED` | non-null `timestamp`, precision `0` | No rows |
| `SQL_MODE` | non-null MySQL 8.4 SQL-mode `set(...)`, `utf8mb3_bin` | No rows |
| `ROUTINE_COMMENT` | non-null `text`, `utf8mb3_bin` | No rows |
| `DEFINER` | non-null `varchar(288)`, `utf8mb3_bin` | No rows |
| `CHARACTER_SET_CLIENT` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `COLLATION_CONNECTION` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `DATABASE_COLLATION` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |

Because there are no rows, this phase's user-visible value behavior is limited
to column labels, predicates over an empty row set, ordering/limit over an
empty row set, and `COUNT(*) = 0` under user-schema predicates.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for `ROUTINES`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `ROUTINES` |
| `TABLE_TYPE` | `SYSTEM VIEW` |
| `ENGINE` | SQL `NULL` |
| `VERSION` | `10` |
| `ROW_FORMAT` | SQL `NULL` |
| `TABLE_ROWS` | `0` |
| `AVG_ROW_LENGTH` | `0` |
| `DATA_LENGTH` | `0` |
| `MAX_DATA_LENGTH` | `0` |
| `INDEX_LENGTH` | `0` |
| `DATA_FREE` | `0` |
| `AUTO_INCREMENT` | SQL `NULL` |
| `CREATE_TIME` / `UPDATE_TIME` / `CHECK_TIME` | SQL `NULL` |
| `TABLE_COLLATION` | `utf8mb3_general_ci` |
| `CHECKSUM` | SQL `NULL` |
| `CREATE_OPTIONS` | empty string |
| `TABLE_COMMENT` | empty string |

`INFORMATION_SCHEMA.COLUMNS` must expose the 31 observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and `TABLE_NAME = 'ROUTINES'`.
`PRIVILEGES` is the fixed system-view value `select`, `COLUMN_KEY` and `EXTRA`
are empty strings, `COLUMN_COMMENT` and `GENERATION_EXPRESSION` are empty
strings, and `SRS_ID` is SQL `NULL`.

## Diagnostics

The implementation must preserve existing information-schema diagnostics:

- unsupported query shapes keep deterministic MyLite unsupported errors;
- unknown metadata columns fail with `1054 / 42S22`;
- unknown information-schema tables fail with `1109 / 42S02`;
- unknown predicate columns are validated before row scanning, even for empty
  synthetic system views;
- allocation failures use existing public API conventions.

Successful supported statements emit no warnings.

## Tests

Tests must cover:

- wildcard and explicit projection from `INFORMATION_SCHEMA.ROUTINES` with
  zero rows;
- `COUNT(*)` with and without a user-schema predicate returning zero for
  MyLite user schemas;
- source aliases, supported predicates, `ORDER BY`, and `LIMIT` over the empty
  row set;
- `INFORMATION_SCHEMA.TABLES` system-view row for `ROUTINES`;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for every `ROUTINES` column;
- unknown predicate-column diagnostics over the empty row set;
- case-insensitive resolution of the `ROUTINES` metadata table name;
- warning count, affected rows, and `ROW_COUNT()` behavior;
- independent handles and file-backed reopen safety;
- existing information-schema core/static/constraints/views/triggers/events
  tests still pass;
- MySQL 8.4.9 expectation script for the introduced user-visible behavior.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/metadata-information-schema.md`,
`docs/compatibility/sql-routines.md`, and
`docs/compatibility/sql-show-statements.md` to mark
`INFORMATION_SCHEMA.ROUTINES` as partial: queryable with MySQL-shaped columns
and system metadata, but empty until MyLite implements real routine
descriptors.

Do not claim support for stored routines, parameters, return metadata,
`SHOW CREATE PROCEDURE`, `SHOW CREATE FUNCTION`, routine execution, privileges,
the `mysql` routine dictionary tables, or full information-schema parity.

# Baseline INFORMATION_SCHEMA PARAMETERS

## Status

This phase adds the stored-routine parameter companion metadata surface:
`INFORMATION_SCHEMA.PARAMETERS`. MyLite now exposes empty
`INFORMATION_SCHEMA.ROUTINES` plus empty `SHOW PROCEDURE STATUS` and
`SHOW FUNCTION STATUS`, but it still has no stored routine DDL, descriptors,
parameters, bodies, execution, or privileges.

The supported behavior is an empty but queryable synthetic information-schema
table with MySQL 8.4.9 column metadata, plus matching
`INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` system-view rows.
This is a discovery compatibility shim; real parameter rows must wait for a
stored-routine descriptor design.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- Existing `INFORMATION_SCHEMA.ROUTINES` surface:
  `docs/specs/baseline-information-schema-routines/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.PARAMETERS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-parameters-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ROUTINES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-routines-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_parameters_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.PARAMETERS` has these result columns in order:
  `SPECIFIC_CATALOG`, `SPECIFIC_SCHEMA`, `SPECIFIC_NAME`,
  `ORDINAL_POSITION`, `PARAMETER_MODE`, `PARAMETER_NAME`, `DATA_TYPE`,
  `CHARACTER_MAXIMUM_LENGTH`, `CHARACTER_OCTET_LENGTH`,
  `NUMERIC_PRECISION`, `NUMERIC_SCALE`, `DATETIME_PRECISION`,
  `CHARACTER_SET_NAME`, `COLLATION_NAME`, `DTD_IDENTIFIER`, and
  `ROUTINE_TYPE`.
- `SHOW COLUMNS FROM INFORMATION_SCHEMA.PARAMETERS` reports:
  - `SPECIFIC_CATALOG` and `SPECIFIC_SCHEMA` as non-null `varchar(64)` with
    `utf8mb3_bin`;
  - `SPECIFIC_NAME` and `PARAMETER_NAME` as `varchar(64)` with
    `utf8mb3_general_ci`, with `PARAMETER_NAME` nullable;
  - `ORDINAL_POSITION` as non-null `bigint unsigned` with default `0`;
  - `PARAMETER_MODE` as nullable `varchar(5)` with `utf8mb3_bin`;
  - return and parameter type metadata as nullable string, numeric, datetime,
    character-set, and collation columns;
  - `DTD_IDENTIFIER` as non-null `mediumtext` with `utf8mb3_bin`;
  - `ROUTINE_TYPE` as a non-null enum of `FUNCTION` and `PROCEDURE`.
- In a database containing tables but no routines,
  `SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARAMETERS WHERE SPECIFIC_SCHEMA = db`
  returns `0`.
- Unfiltered `INFORMATION_SCHEMA.PARAMETERS` queries on a real MySQL server may
  include server-provided routine metadata outside the test database. This
  phase verifies MySQL zero-row behavior under a user-schema predicate and
  separately tests MyLite's intentionally empty synthetic table without
  claiming MySQL's global row count is zero.
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for `PARAMETERS` with
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per `PARAMETERS`
  column. Text metadata uses `utf8mb3`; routine and parameter names use
  `utf8mb3_general_ci`; catalog/schema/mode/type metadata uses `utf8mb3_bin`.
  Numeric and temporal metadata columns have no character set or collation.
- Creating a real MySQL stored procedure with `IN`, `OUT`, and `INOUT`
  parameters plus a stored function with one input parameter produces
  parameter rows. The stored function also has a return-value row with
  `ORDINAL_POSITION = 0`, `PARAMETER_NAME = NULL`, and
  `PARAMETER_MODE = NULL`. MyLite defers routine descriptors and therefore
  emits no parameter rows in this phase.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.
- Unknown predicate columns fail before row scanning with
  `1054 / 42S22` and a message containing
  `Unknown column 'name' in 'where clause'`, even when the result set would be
  empty.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.PARAMETERS` table definition to the existing
  synthetic information-schema registry;
- the exact 16-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.PARAMETERS`;
- zero user rows until MyLite has stored routine descriptors and parameter
  descriptors;
- an `INFORMATION_SCHEMA.TABLES` system-view row for `PARAMETERS`;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the 16
  `PARAMETERS` metadata columns;
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
  descriptors, routine parameter descriptors, routine body storage,
  parameter default values, parameter privileges, definers, or stored-routine
  SQL mode capture;
- rows in `INFORMATION_SCHEMA.PARAMETERS` for any routine or return value;
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
  `INFORMATION_SCHEMA.PARAMETERS`. Future routine and parameter descriptors
  must be designed before this table emits user rows.
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
FROM INFORMATION_SCHEMA.PARAMETERS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.PARAMETERS` source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through this table.

## `PARAMETERS` Columns

`INFORMATION_SCHEMA.PARAMETERS` has 16 columns:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `SPECIFIC_CATALOG` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `SPECIFIC_SCHEMA` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `SPECIFIC_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `ORDINAL_POSITION` | non-null `bigint unsigned`, default `0` | No rows |
| `PARAMETER_MODE` | nullable `varchar(5)`, `utf8mb3_bin` | No rows |
| `PARAMETER_NAME` | nullable `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `DATA_TYPE` | nullable `longtext`, `utf8mb3_bin` | No rows |
| `CHARACTER_MAXIMUM_LENGTH` | nullable `bigint` | No rows |
| `CHARACTER_OCTET_LENGTH` | nullable `bigint` | No rows |
| `NUMERIC_PRECISION` | nullable `int unsigned` | No rows |
| `NUMERIC_SCALE` | nullable `bigint` | No rows |
| `DATETIME_PRECISION` | nullable `int unsigned` | No rows |
| `CHARACTER_SET_NAME` | nullable `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `COLLATION_NAME` | nullable `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `DTD_IDENTIFIER` | non-null `mediumtext`, `utf8mb3_bin` | No rows |
| `ROUTINE_TYPE` | non-null enum `FUNCTION` / `PROCEDURE`, `utf8mb3_bin` | No rows |

Because there are no rows, this phase's user-visible value behavior is limited
to column labels, predicates over an empty row set, ordering/limit over an
empty row set, and `COUNT(*) = 0` under user-schema predicates.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for `PARAMETERS`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `PARAMETERS` |
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

`INFORMATION_SCHEMA.COLUMNS` must expose the 16 observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and `TABLE_NAME = 'PARAMETERS'`.
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

- wildcard and explicit projection from `INFORMATION_SCHEMA.PARAMETERS` with
  zero rows;
- `COUNT(*)` under a user-schema predicate returning zero according to MySQL
  8.4.9, plus MyLite's unfiltered empty synthetic-table count until routine
  descriptors exist;
- source aliases, supported predicates, `ORDER BY`, and `LIMIT` over the empty
  row set;
- `INFORMATION_SCHEMA.TABLES` system-view row for `PARAMETERS`;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for every `PARAMETERS` column;
- unknown predicate-column diagnostics over the empty row set;
- case-insensitive resolution of the `PARAMETERS` metadata table name;
- warning count, affected rows, and `ROW_COUNT()` behavior;
- independent handles and file-backed reopen safety;
- existing information-schema core/static/constraints/views/triggers/events/
  routines tests still pass;
- MySQL 8.4.9 expectation script for the introduced user-visible behavior.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/metadata-information-schema.md`,
and `docs/compatibility/sql-routines.md` to mark
`INFORMATION_SCHEMA.PARAMETERS` as partial: queryable with MySQL-shaped columns
and system metadata, but empty until MyLite implements real stored routine and
parameter descriptors.

Do not claim support for stored routines, routine parameters, function return
metadata, `SHOW CREATE PROCEDURE`, `SHOW CREATE FUNCTION`, routine execution,
privileges, or the `mysql` routine dictionary tables.

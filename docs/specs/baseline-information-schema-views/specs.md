# Baseline INFORMATION_SCHEMA VIEWS

## Status

This phase adds the first view-catalog compatibility surface:
`INFORMATION_SCHEMA.VIEWS`. MyLite does not support `CREATE VIEW`, stored view
definitions, view execution, or view DDL in this phase. The supported behavior
is an empty but queryable synthetic information-schema table with MySQL 8.4.9
column metadata, plus matching `INFORMATION_SCHEMA.TABLES` and
`INFORMATION_SCHEMA.COLUMNS` system-view rows.

This is a compatibility shim for clients that probe view metadata. It should
turn common discovery queries from "unknown table" failures into empty result
sets while keeping view functionality explicitly unsupported.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- Existing static information-schema catalogs:
  `docs/specs/baseline-information-schema-static-catalogs/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.VIEWS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-views-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_views_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.VIEWS` has these result columns in order:
  `TABLE_CATALOG`, `TABLE_SCHEMA`, `TABLE_NAME`, `VIEW_DEFINITION`,
  `CHECK_OPTION`, `IS_UPDATABLE`, `DEFINER`, `SECURITY_TYPE`,
  `CHARACTER_SET_CLIENT`, and `COLLATION_CONNECTION`.
- `SHOW COLUMNS FROM INFORMATION_SCHEMA.VIEWS` reports:
  - the first three columns as non-null `varchar(64)`;
  - `VIEW_DEFINITION` as nullable `longtext`;
  - `CHECK_OPTION` as nullable `enum('NONE','LOCAL','CASCADED')`;
  - `IS_UPDATABLE` as nullable `enum('NO','YES')`;
  - `DEFINER` as nullable `varchar(288)`;
  - `SECURITY_TYPE` as nullable `varchar(7)`;
  - `CHARACTER_SET_CLIENT` and `COLLATION_CONNECTION` as non-null
    `varchar(64)`.
- In a database containing no views,
  `SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = db`
  returns `0`.
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for `VIEWS` with
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per `VIEWS` column.
  Text metadata uses `utf8mb3`; the first eight columns use `utf8mb3_bin`,
  while `CHARACTER_SET_CLIENT` and `COLLATION_CONNECTION` use
  `utf8mb3_general_ci`.
- Creating a real MySQL view produces one `INFORMATION_SCHEMA.VIEWS` row and
  a `TABLES.TABLE_TYPE = 'VIEW'` row. MyLite defers real view descriptors and
  therefore emits no user view rows in this phase.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.VIEWS` table definition to the existing synthetic
  information-schema registry;
- the exact ten-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.VIEWS`;
- zero user rows until MyLite has view descriptors;
- an `INFORMATION_SCHEMA.TABLES` system-view row for `VIEWS`;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the ten `VIEWS`
  metadata columns;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, source aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- `CREATE VIEW`, `ALTER VIEW`, `DROP VIEW`, `SHOW CREATE VIEW`, view execution,
  view descriptors, view dependencies, privilege filtering, or stored
  definitions;
- real `VIEW` rows in `INFORMATION_SCHEMA.TABLES`;
- rows in `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` or
  `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE`;
- `mysql`, `performance_schema`, or `sys` schemas;
- physical `information_schema` SQLite tables, SQLite pragma reflection,
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
  `INFORMATION_SCHEMA.VIEWS`. Future view descriptors must be designed before
  this table emits user rows.
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
FROM INFORMATION_SCHEMA.VIEWS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.VIEWS` source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through this table.

## `VIEWS` Columns

`INFORMATION_SCHEMA.VIEWS` has ten columns:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `TABLE_CATALOG` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `TABLE_SCHEMA` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `TABLE_NAME` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `VIEW_DEFINITION` | nullable `longtext`, `utf8mb3_bin` | No rows |
| `CHECK_OPTION` | nullable `enum('NONE','LOCAL','CASCADED')`, `utf8mb3_bin` | No rows |
| `IS_UPDATABLE` | nullable `enum('NO','YES')`, `utf8mb3_bin` | No rows |
| `DEFINER` | nullable `varchar(288)`, `utf8mb3_bin` | No rows |
| `SECURITY_TYPE` | nullable `varchar(7)`, `utf8mb3_bin` | No rows |
| `CHARACTER_SET_CLIENT` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `COLLATION_CONNECTION` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |

Because there are no rows, this phase's user-visible value behavior is limited
to column labels, predicates over an empty row set, ordering/limit over an
empty row set, and `COUNT(*) = 0` under user-schema predicates.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for `VIEWS`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `VIEWS` |
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

`INFORMATION_SCHEMA.COLUMNS` must expose the ten observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and `TABLE_NAME = 'VIEWS'`. `PRIVILEGES`
is the fixed system-view value `select`, `COLUMN_KEY` and `EXTRA` are empty
strings, `COLUMN_COMMENT` and `GENERATION_EXPRESSION` are empty strings, and
`SRS_ID` is SQL `NULL`.

## Diagnostics

The implementation must preserve existing information-schema diagnostics:

- unsupported query shapes keep deterministic MyLite unsupported errors;
- unknown metadata columns fail with `1054 / 42S22`;
- unknown information-schema tables fail with `1109 / 42S02`;
- allocation failures use existing public API conventions.

Successful supported statements emit no warnings.

## Tests

Tests must cover:

- wildcard and explicit projection from `INFORMATION_SCHEMA.VIEWS` with zero
  rows;
- `COUNT(*)` with and without a user-schema predicate returning zero for
  MyLite user schemas;
- source aliases, supported predicates, `ORDER BY`, and `LIMIT` over the empty
  row set;
- `INFORMATION_SCHEMA.TABLES` system-view row for `VIEWS`;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for every `VIEWS` column;
- case-insensitive resolution of the `VIEWS` metadata table name;
- warning count, affected rows, and `ROW_COUNT()` behavior;
- independent handles and file-backed reopen safety;
- existing information-schema core/static/constraints/statistics tests still
  pass;
- MySQL 8.4.9 expectation script for the introduced user-visible behavior.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/metadata-information-schema.md` to mark
`INFORMATION_SCHEMA.VIEWS` as partial: queryable with MySQL-shaped columns and
system metadata, but empty until MyLite implements real view descriptors.

Do not claim support for views, `SHOW CREATE VIEW`, view dependencies,
privileges, the `sys` schema, or full information-schema parity.

# Baseline INFORMATION_SCHEMA PROCESSLIST

## Status

This phase adds a narrow process-list metadata compatibility surface:
`INFORMATION_SCHEMA.PROCESSLIST`. MyLite already supports limited
`SHOW [FULL] PROCESSLIST`; this feature makes the corresponding
information-schema table queryable through the existing synthetic metadata
planner.

The supported behavior is one current embedded-handle row with MySQL 8.4.9
column labels and metadata, plus matching `INFORMATION_SCHEMA.TABLES` and
`INFORMATION_SCHEMA.COLUMNS` system-view rows. It is an embedded compatibility
shim, not a server thread monitor.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing `SHOW PROCESSLIST` support:
  `docs/specs/baseline-show-processlist-introspection/specs.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.PROCESSLIST`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-processlist-table.html
- MySQL 8.4 Reference Manual, process-list access:
  https://dev.mysql.com/doc/refman/8.4/en/processlist-access.html
- MySQL 8.4 Reference Manual, `SHOW PROCESSLIST`:
  https://dev.mysql.com/doc/refman/8.4/en/show-processlist.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_processlist_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.PROCESSLIST` is deprecated. Successful selects that read
  at least one process-list row append warning `1287 / HY000` with the same
  message currently used by MyLite's `SHOW PROCESSLIST` implementation.
  `LIMIT 0` and predicates that match no rows do not emit the warning.
- `SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST` has these result columns in
  order: `ID`, `USER`, `HOST`, `DB`, `COMMAND`, `TIME`, `STATE`, and `INFO`.
- For the current query row selected by `ID = CONNECTION_ID()`:
  - `ID` is the current connection identifier;
  - `USER` is `root` in the test runtime;
  - `HOST` is the TCP peer with a client port;
  - `DB` is `NULL` before a default schema is selected and the schema name
    after `USE db`;
  - `COMMAND` is `Query`;
  - `TIME` is `0` for immediate self-inspection;
  - `STATE` is `executing`;
  - `INFO` is the full current statement text, with trailing statement
    terminator omitted and without the 100-byte non-`FULL` truncation used by
    `SHOW PROCESSLIST`.
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID =
  CONNECTION_ID()` returns `1`.
- Successful supported `SELECT` statements that read the current row return
  warning count `1` and make the following `ROW_COUNT()` return `-1`. Matching
  no rows, including `LIMIT 0`, returns warning count `0` and still makes the
  following `ROW_COUNT()` return `-1`.
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for `PROCESSLIST`
  with `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per `PROCESSLIST`
  column. All `COLUMN_DEFAULT` values are empty strings. Integer process-list
  columns report `DATA_TYPE` / `COLUMN_TYPE` as `bigint unsigned` or `int` but
  do not expose numeric precision or scale through `INFORMATION_SCHEMA.COLUMNS`.
- Unknown projection, predicate, and order columns fail with `1054 / 42S22`
  and clause-specific messages containing `field list`, `where clause`, or
  `order clause`.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.PROCESSLIST` table definition to the existing
  synthetic information-schema registry;
- the exact 8-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.PROCESSLIST`;
- one current embedded-handle row per query;
- an `INFORMATION_SCHEMA.TABLES` system-view row for `PROCESSLIST`;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the 8
  `PROCESSLIST` metadata columns;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, source aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- deprecation warning `1287 / HY000` for successful selects that read at least
  one `INFORMATION_SCHEMA.PROCESSLIST` row;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `1` when the current row is read, warning
  count `0` when no row is read, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- server-wide thread enumeration, sleeping/background rows, another connection
  list, privilege filtering, anonymous-user behavior, `PROCESS` privilege
  semantics, `KILL`, `performance_schema.processlist`, `performance_schema`
  thread tables, sys-schema process-list views, mutex or thread monitoring
  behavior, status variables such as
  `Deprecated_use_i_s_processlist_count`, or exact TCP host/port formatting;
- physical `information_schema` SQLite tables, SQLite process reflection,
  arbitrary SQLite SQL pass-through, storage-format changes, or SQLite fork
  patches;
- wider `INFORMATION_SCHEMA` query support beyond the existing planner.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Statement context: owns the statement text used for the `INFO` value. The
  process-list table must use the same context trimming policy as
  `SHOW FULL PROCESSLIST` and must not truncate `INFO`.
- Parser/AST: no grammar changes. The existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` syntax is reused.
- Analyzer/planner: the existing information-schema query resolver owns source
  matching, projection, aliases, predicates, ordering, and limits against the
  synthetic table definition.
- Catalog module: no persistent catalog rows are read or written for
  `INFORMATION_SCHEMA.PROCESSLIST`.
- Result builder: emits the current-handle row and MySQL-shaped text/`NULL`
  metadata values through `mylite_result`.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: no metadata introspection or fork patch is needed;
  this is a MyLite-owned synthetic metadata view.

## Supported Query Surface

No new Lemon grammar is required. The existing limited information-schema
`SELECT` surface admits:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.PROCESSLIST [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.PROCESSLIST` source only;
- metadata predicates already supported by the information-schema planner:
  column-left comparisons to string, integer, `DATABASE()`, and `SCHEMA()`
  values; `IS NULL` / `IS NOT NULL`; `NOT`, `AND`, `XOR`, `OR`, and
  parentheses;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expression projection, general function
  predicates such as `ID = CONNECTION_ID()`, `LIKE`, `IN`, `BETWEEN`,
  grouping, mutation, or DDL through this table.

## `PROCESSLIST` Columns

`INFORMATION_SCHEMA.PROCESSLIST` has 8 columns:

| Column | Type metadata | MyLite row value |
| --- | --- | --- |
| `ID` | non-null `bigint unsigned`; empty default; no numeric precision/scale in `INFORMATION_SCHEMA.COLUMNS` | Current MyLite connection id |
| `USER` | non-null `varchar(32)`, character length `10`, octet length `32`, `utf8mb3_general_ci`; empty default | User part of current client identity |
| `HOST` | non-null `varchar(261)`, character length `87`, octet length `261`, `utf8mb3_general_ci`; empty default | Host part of current client identity |
| `DB` | nullable `varchar(64)`, character length `21`, octet length `64`, `utf8mb3_general_ci`; empty default | Selected schema or SQL `NULL` |
| `COMMAND` | non-null `varchar(16)`, character length `5`, octet length `16`, `utf8mb3_general_ci`; empty default | `Query` |
| `TIME` | non-null `int`; empty default; no numeric precision/scale in `INFORMATION_SCHEMA.COLUMNS` | `0` |
| `STATE` | nullable `varchar(64)`, character length `21`, octet length `64`, `utf8mb3_general_ci`; empty default | `executing` |
| `INFO` | nullable `varchar(65535)`, character length `21845`, octet length `65535`, `utf8mb3_general_ci`; empty default | Full current SQL text without trailing terminator |

`SHOW PROCESSLIST` keeps its existing `State = init` behavior and non-`FULL`
100-byte `Info` truncation. `INFORMATION_SCHEMA.PROCESSLIST` follows MySQL's
table behavior: `STATE = executing` for the current query row and untruncated
`INFO`.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for `PROCESSLIST`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `PROCESSLIST` |
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

`INFORMATION_SCHEMA.COLUMNS` must expose the 8 observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and `TABLE_NAME = 'PROCESSLIST'`.
`PRIVILEGES` is the fixed system-view value `select`, `COLUMN_KEY` and `EXTRA`
are empty strings, `COLUMN_COMMENT` and `GENERATION_EXPRESSION` are empty
strings, and `SRS_ID` is SQL `NULL`.

## Diagnostics

- successful selects from `INFORMATION_SCHEMA.PROCESSLIST` that read at least
  one row append warning `1287 / HY000`;
- supported `LIMIT 0` and no-match predicates emit no warnings;
- unknown metadata columns fail with `1054 / 42S22`;
- unknown information-schema tables fail with `1109 / 42S02`;
- unsupported projection, predicate, order, or limit forms reuse existing
  information-schema planner diagnostics;
- allocation failures use existing public API conventions.

No catalog, storage, or SQLite schema generation counters may change.

## Tests

Tests must cover:

- wildcard and explicit projection from `INFORMATION_SCHEMA.PROCESSLIST`;
- current handle row values: decimal `ID`, `USER`, `HOST`, selected-schema
  `DB`, `COMMAND`, `TIME`, `STATE`, and full `INFO`;
- `INFO` remains untruncated for statements longer than 100 bytes;
- `COUNT(*)`, source aliases, supported predicates, `ORDER BY`, and `LIMIT`;
- lowercase table-name resolution;
- deprecation warning, result warning count for matched and no-match reads,
  affected rows, and `ROW_COUNT()`;
- `INFORMATION_SCHEMA.TABLES` system-view row for `PROCESSLIST`;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for every `PROCESSLIST` column;
- unknown projection, predicate, and order-column diagnostics;
- independent handles with independent ids and selected schemas;
- reopen safety and `.mylite` preamble preservation;
- no catalog generation or SQLite schema generation mutation.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/metadata-information-schema.md`,
and `docs/compatibility/sql-show-statements.md` to mark
`INFORMATION_SCHEMA.PROCESSLIST` as partial: queryable with one current
embedded-handle row and MySQL-shaped metadata, but without server-wide thread
monitoring, privileges, Performance Schema, sys-schema views, or `KILL`.

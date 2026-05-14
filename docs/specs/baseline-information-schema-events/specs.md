# Baseline INFORMATION_SCHEMA EVENTS

## Status

This phase adds a narrow event-metadata compatibility surface:
`INFORMATION_SCHEMA.EVENTS`. MyLite already exposes empty `SHOW EVENTS`
introspection, but it does not support `CREATE EVENT`, stored event
descriptors, event scheduling, event execution, or event DDL. The supported
behavior is an empty but queryable synthetic information-schema table with
MySQL 8.4.9 column metadata, plus matching `INFORMATION_SCHEMA.TABLES` and
`INFORMATION_SCHEMA.COLUMNS` system-view rows.

This is a compatibility shim for clients that probe Event Scheduler metadata.
It turns common discovery queries from "unknown table" failures into empty
result sets while keeping event functionality explicitly unsupported.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- Existing empty event introspection:
  `docs/specs/baseline-show-events-empty-introspection/specs.md`
- Existing `INFORMATION_SCHEMA.VIEWS` and `INFORMATION_SCHEMA.TRIGGERS`
  surfaces:
  `docs/specs/baseline-information-schema-views/specs.md` and
  `docs/specs/baseline-information-schema-triggers/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.EVENTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-events-table.html
- MySQL 8.4 Reference Manual, `SHOW EVENTS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-events.html
- MySQL 8.4 Reference Manual, `CREATE EVENT`:
  https://dev.mysql.com/doc/refman/8.4/en/create-event.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_events_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.EVENTS` has these result columns in order:
  `EVENT_CATALOG`, `EVENT_SCHEMA`, `EVENT_NAME`, `DEFINER`, `TIME_ZONE`,
  `EVENT_BODY`, `EVENT_DEFINITION`, `EVENT_TYPE`, `EXECUTE_AT`,
  `INTERVAL_VALUE`, `INTERVAL_FIELD`, `SQL_MODE`, `STARTS`, `ENDS`, `STATUS`,
  `ON_COMPLETION`, `CREATED`, `LAST_ALTERED`, `LAST_EXECUTED`,
  `EVENT_COMMENT`, `ORIGINATOR`, `CHARACTER_SET_CLIENT`,
  `COLLATION_CONNECTION`, and `DATABASE_COLLATION`.
- `SHOW COLUMNS FROM INFORMATION_SCHEMA.EVENTS` reports:
  - catalog, schema, name, definer, time-zone, character-set, connection
    collation, and database-collation columns as non-null `varchar(...)`;
  - `EVENT_BODY`, `EVENT_TYPE`, `STATUS`, and `ON_COMPLETION` as non-null
    `varchar(...)` with an empty-string default in `INFORMATION_SCHEMA.COLUMNS`;
  - `EVENT_DEFINITION` as non-null `longtext`;
  - `EXECUTE_AT`, `STARTS`, `ENDS`, and `LAST_EXECUTED` as nullable
    `datetime` with `DATETIME_PRECISION = 0`;
  - `INTERVAL_VALUE` as nullable `varchar(256)`;
  - `INTERVAL_FIELD` as a nullable enum of Event Scheduler interval units;
  - `SQL_MODE` as a non-null `set(...)` of MySQL 8.4 SQL mode names;
  - `CREATED` and `LAST_ALTERED` as non-null `timestamp` with
    `DATETIME_PRECISION = 0`;
  - `EVENT_COMMENT` as non-null `varchar(2048)`;
  - `ORIGINATOR` as non-null `int unsigned`.
- In a database containing tables but no events,
  `SELECT COUNT(*) FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = db`
  returns `0`.
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for `EVENTS` with
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per `EVENTS` column.
  Text metadata uses `utf8mb3`; event name, event body, event type, interval
  value, on-completion, character-set, connection-collation, and
  database-collation columns use `utf8mb3_general_ci`; most other textual
  columns use `utf8mb3_bin`. Temporal and numeric columns have no character set
  or collation.
- Creating a real MySQL event produces one `INFORMATION_SCHEMA.EVENTS` row.
  MyLite defers event descriptors and therefore emits no event rows in this
  phase.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.EVENTS` table definition to the existing synthetic
  information-schema registry;
- the exact 24-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.EVENTS`;
- zero user rows until MyLite has event descriptors;
- an `INFORMATION_SCHEMA.TABLES` system-view row for `EVENTS`;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the 24 `EVENTS`
  metadata columns;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, source aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- `CREATE EVENT`, `ALTER EVENT`, `DROP EVENT`, `SHOW CREATE EVENT`, event
  execution, event descriptors, event scheduling, event persistence, Event
  Scheduler state, event privileges, definers, stored-program SQL mode capture,
  time-zone conversion, or event character-set metadata;
- rows in `INFORMATION_SCHEMA.EVENTS` for any event kind;
- physical `information_schema` SQLite tables, SQLite trigger/timer
  reflection, arbitrary SQLite SQL pass-through, storage-format changes, or
  SQLite fork patches;
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
  `INFORMATION_SCHEMA.EVENTS`. Future event descriptors must be designed before
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
FROM INFORMATION_SCHEMA.EVENTS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.EVENTS` source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through this table.

## `EVENTS` Columns

`INFORMATION_SCHEMA.EVENTS` has 24 columns:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `EVENT_CATALOG` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `EVENT_SCHEMA` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `EVENT_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `DEFINER` | non-null `varchar(288)`, `utf8mb3_bin` | No rows |
| `TIME_ZONE` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `EVENT_BODY` | non-null `varchar(3)`, `utf8mb3_general_ci`, empty default | No rows |
| `EVENT_DEFINITION` | non-null `longtext`, `utf8mb3_bin` | No rows |
| `EVENT_TYPE` | non-null `varchar(9)`, `utf8mb3_general_ci`, empty default | No rows |
| `EXECUTE_AT` | nullable `datetime`, precision `0` | No rows |
| `INTERVAL_VALUE` | nullable `varchar(256)`, `utf8mb3_general_ci` | No rows |
| `INTERVAL_FIELD` | nullable Event Scheduler interval enum, `utf8mb3_bin` | No rows |
| `SQL_MODE` | non-null MySQL 8.4 SQL-mode `set(...)`, `utf8mb3_bin` | No rows |
| `STARTS` | nullable `datetime`, precision `0` | No rows |
| `ENDS` | nullable `datetime`, precision `0` | No rows |
| `STATUS` | non-null `varchar(21)`, `utf8mb3_bin`, empty default | No rows |
| `ON_COMPLETION` | non-null `varchar(12)`, `utf8mb3_general_ci`, empty default | No rows |
| `CREATED` | non-null `timestamp`, precision `0` | No rows |
| `LAST_ALTERED` | non-null `timestamp`, precision `0` | No rows |
| `LAST_EXECUTED` | nullable `datetime`, precision `0` | No rows |
| `EVENT_COMMENT` | non-null `varchar(2048)`, `utf8mb3_bin` | No rows |
| `ORIGINATOR` | non-null `int unsigned` | No rows |
| `CHARACTER_SET_CLIENT` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `COLLATION_CONNECTION` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `DATABASE_COLLATION` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |

Because there are no rows, this phase's user-visible value behavior is limited
to column labels, predicates over an empty row set, ordering/limit over an
empty row set, and `COUNT(*) = 0` under user-schema predicates.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for `EVENTS`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `EVENTS` |
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

`INFORMATION_SCHEMA.COLUMNS` must expose the 24 observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and `TABLE_NAME = 'EVENTS'`.
`PRIVILEGES` is the fixed system-view value `select`, `COLUMN_KEY` and `EXTRA`
are empty strings, `COLUMN_COMMENT` and `GENERATION_EXPRESSION` are empty
strings, and `SRS_ID` is SQL `NULL`.

## Diagnostics

The implementation must preserve existing information-schema diagnostics:

- unsupported query shapes keep deterministic MyLite unsupported errors;
- unknown metadata columns fail with `1054 / 42S22`;
- unknown information-schema tables fail with `1109 / 42S02`;
- allocation failures use existing public API conventions.

Successful supported statements emit no warnings.

## Tests

Tests must cover:

- wildcard and explicit projection from `INFORMATION_SCHEMA.EVENTS` with zero
  rows;
- `COUNT(*)` with and without a user-schema predicate returning zero for
  MyLite user schemas;
- source aliases, supported predicates, `ORDER BY`, and `LIMIT` over the empty
  row set;
- `INFORMATION_SCHEMA.TABLES` system-view row for `EVENTS`;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for every `EVENTS` column;
- case-insensitive resolution of the `EVENTS` metadata table name;
- warning count, affected rows, and `ROW_COUNT()` behavior;
- independent handles and file-backed reopen safety;
- existing information-schema core/static/constraints/views/triggers tests
  still pass;
- MySQL 8.4.9 expectation script for the introduced user-visible behavior.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/metadata-information-schema.md`,
`docs/compatibility/sql-events.md`, and `docs/compatibility/sql-show-statements.md`
to mark `INFORMATION_SCHEMA.EVENTS` as partial: queryable with MySQL-shaped
columns and system metadata, but empty until MyLite implements real Event
Scheduler descriptors.

Do not claim support for events, event scheduling, `SHOW CREATE EVENT`, event
execution, privileges, the `mysql` event tables, or full information-schema
parity.

# Baseline INFORMATION_SCHEMA TRIGGERS

## Status

This phase adds a narrow trigger-metadata compatibility surface:
`INFORMATION_SCHEMA.TRIGGERS`. MyLite already exposes empty `SHOW TRIGGERS`
introspection, but it does not support `CREATE TRIGGER`, stored trigger
descriptors, trigger execution, or trigger DDL. The supported behavior is an
empty but queryable synthetic information-schema table with MySQL 8.4.9 column
metadata, plus matching `INFORMATION_SCHEMA.TABLES` and
`INFORMATION_SCHEMA.COLUMNS` system-view rows.

This is a compatibility shim for clients that probe trigger metadata. It turns
common discovery queries from "unknown table" failures into empty result sets
while keeping trigger functionality explicitly unsupported.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- Existing empty trigger introspection:
  `docs/specs/baseline-show-triggers-empty-introspection/specs.md`
- Existing `INFORMATION_SCHEMA.VIEWS` surface:
  `docs/specs/baseline-information-schema-views/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TRIGGERS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-triggers-table.html
- MySQL 8.4 Reference Manual, `SHOW TRIGGERS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-triggers.html
- MySQL 8.4 Reference Manual, `CREATE TRIGGER`:
  https://dev.mysql.com/doc/refman/8.4/en/create-trigger.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_triggers_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.TRIGGERS` has these result columns in order:
  `TRIGGER_CATALOG`, `TRIGGER_SCHEMA`, `TRIGGER_NAME`,
  `EVENT_MANIPULATION`, `EVENT_OBJECT_CATALOG`, `EVENT_OBJECT_SCHEMA`,
  `EVENT_OBJECT_TABLE`, `ACTION_ORDER`, `ACTION_CONDITION`,
  `ACTION_STATEMENT`, `ACTION_ORIENTATION`, `ACTION_TIMING`,
  `ACTION_REFERENCE_OLD_TABLE`, `ACTION_REFERENCE_NEW_TABLE`,
  `ACTION_REFERENCE_OLD_ROW`, `ACTION_REFERENCE_NEW_ROW`, `CREATED`,
  `SQL_MODE`, `DEFINER`, `CHARACTER_SET_CLIENT`,
  `COLLATION_CONNECTION`, and `DATABASE_COLLATION`.
- `SHOW COLUMNS FROM INFORMATION_SCHEMA.TRIGGERS` reports:
  - catalog/schema/name and event-object name columns as non-null
    `varchar(64)`;
  - `EVENT_MANIPULATION` as non-null
    `enum('INSERT','UPDATE','DELETE')`;
  - `ACTION_ORDER` as non-null `int unsigned`;
  - `ACTION_CONDITION`, `ACTION_REFERENCE_OLD_TABLE`, and
    `ACTION_REFERENCE_NEW_TABLE` as nullable `varbinary(0)`;
  - `ACTION_STATEMENT` as non-null `longtext`;
  - `ACTION_ORIENTATION`, `ACTION_REFERENCE_OLD_ROW`, and
    `ACTION_REFERENCE_NEW_ROW` as non-null `varchar(3)` with an empty string
    default in `INFORMATION_SCHEMA.COLUMNS`;
  - `ACTION_TIMING` as non-null `enum('BEFORE','AFTER')`;
  - `CREATED` as non-null `timestamp(2)`;
  - `SQL_MODE` as a non-null `set(...)` of MySQL 8.4 SQL mode names;
  - `DEFINER` as non-null `varchar(288)`;
  - character-set, connection-collation, and database-collation columns as
    non-null `varchar(64)`.
- In a database containing tables but no triggers,
  `SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = db`
  returns `0`.
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for `TRIGGERS` with
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per `TRIGGERS`
  column. Text metadata uses `utf8mb3`; binary metadata has no character set
  or collation. `TRIGGER_NAME`, `ACTION_ORIENTATION`,
  `ACTION_REFERENCE_OLD_ROW`, `ACTION_REFERENCE_NEW_ROW`,
  `CHARACTER_SET_CLIENT`, `COLLATION_CONNECTION`, and `DATABASE_COLLATION`
  use `utf8mb3_general_ci`; most other textual metadata columns use
  `utf8mb3_bin`.
- Creating a real MySQL trigger produces one `INFORMATION_SCHEMA.TRIGGERS`
  row. MyLite defers trigger descriptors and therefore emits no trigger rows
  in this phase.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.TRIGGERS` table definition to the existing synthetic
  information-schema registry;
- the exact 22-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.TRIGGERS`;
- zero user rows until MyLite has trigger descriptors;
- an `INFORMATION_SCHEMA.TABLES` system-view row for `TRIGGERS`;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the 22 `TRIGGERS`
  metadata columns;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, source aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- `CREATE TRIGGER`, `DROP TRIGGER`, `SHOW CREATE TRIGGER`, trigger execution,
  trigger descriptors, trigger ordering, trigger persistence, trigger
  privileges, definers, stored-program SQL mode capture, or trigger
  character-set metadata;
- rows in `INFORMATION_SCHEMA.TRIGGERS` for any trigger kind;
- physical `information_schema` SQLite tables, SQLite trigger reflection,
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
  `INFORMATION_SCHEMA.TRIGGERS`. Future trigger descriptors must be designed
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
FROM INFORMATION_SCHEMA.TRIGGERS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.TRIGGERS` source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through this table.

## `TRIGGERS` Columns

`INFORMATION_SCHEMA.TRIGGERS` has 22 columns:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `TRIGGER_CATALOG` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `TRIGGER_SCHEMA` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `TRIGGER_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `EVENT_MANIPULATION` | non-null `enum('INSERT','UPDATE','DELETE')`, `utf8mb3_bin` | No rows |
| `EVENT_OBJECT_CATALOG` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `EVENT_OBJECT_SCHEMA` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `EVENT_OBJECT_TABLE` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `ACTION_ORDER` | non-null `int unsigned` | No rows |
| `ACTION_CONDITION` | nullable `varbinary(0)` | No rows |
| `ACTION_STATEMENT` | non-null `longtext`, `utf8mb3_bin` | No rows |
| `ACTION_ORIENTATION` | non-null `varchar(3)`, `utf8mb3_general_ci`, empty default | No rows |
| `ACTION_TIMING` | non-null `enum('BEFORE','AFTER')`, `utf8mb3_bin` | No rows |
| `ACTION_REFERENCE_OLD_TABLE` | nullable `varbinary(0)` | No rows |
| `ACTION_REFERENCE_NEW_TABLE` | nullable `varbinary(0)` | No rows |
| `ACTION_REFERENCE_OLD_ROW` | non-null `varchar(3)`, `utf8mb3_general_ci`, empty default | No rows |
| `ACTION_REFERENCE_NEW_ROW` | non-null `varchar(3)`, `utf8mb3_general_ci`, empty default | No rows |
| `CREATED` | non-null `timestamp(2)` | No rows |
| `SQL_MODE` | non-null MySQL 8.4 SQL-mode `set(...)`, `utf8mb3_bin` | No rows |
| `DEFINER` | non-null `varchar(288)`, `utf8mb3_bin` | No rows |
| `CHARACTER_SET_CLIENT` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `COLLATION_CONNECTION` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `DATABASE_COLLATION` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |

Because there are no rows, this phase's user-visible value behavior is limited
to column labels, predicates over an empty row set, ordering/limit over an
empty row set, and `COUNT(*) = 0` under user-schema predicates.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for `TRIGGERS`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `TRIGGERS` |
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

`INFORMATION_SCHEMA.COLUMNS` must expose the 22 observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and `TABLE_NAME = 'TRIGGERS'`.
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

- wildcard and explicit projection from `INFORMATION_SCHEMA.TRIGGERS` with
  zero rows;
- `COUNT(*)` with and without a user-schema predicate returning zero for
  MyLite user schemas;
- source aliases, supported predicates, `ORDER BY`, and `LIMIT` over the empty
  row set;
- `INFORMATION_SCHEMA.TABLES` system-view row for `TRIGGERS`;
- `INFORMATION_SCHEMA.COLUMNS` metadata rows for every `TRIGGERS` column;
- case-insensitive resolution of the `TRIGGERS` metadata table name;
- warning count, affected rows, and `ROW_COUNT()` behavior;
- independent handles and file-backed reopen safety;
- existing information-schema core/static/constraints/statistics/views tests
  still pass;
- MySQL 8.4.9 expectation script for the introduced user-visible behavior.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/metadata-information-schema.md` to mark
`INFORMATION_SCHEMA.TRIGGERS` as partial: queryable with MySQL-shaped columns
and system metadata, but empty until MyLite implements real trigger
descriptors.

Do not claim support for triggers, `CREATE TRIGGER`, `DROP TRIGGER`,
`SHOW CREATE TRIGGER`, privileges, or full information-schema parity.

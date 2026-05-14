# Baseline INFORMATION_SCHEMA CHECK_CONSTRAINTS

## Status

This phase adds a narrow check-constraint metadata compatibility surface:
`INFORMATION_SCHEMA.CHECK_CONSTRAINTS`. MyLite does not support `CHECK`
constraint DDL, expression validation, stored check descriptors, or check
enforcement in this phase. The supported behavior is an empty but queryable
synthetic information-schema table with MySQL 8.4.9 column metadata, plus
matching `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS`
system-view rows.

This is a compatibility shim for clients that probe constraint metadata. It
turns common discovery queries from "unknown table" failures into empty result
sets while keeping check-constraint functionality explicitly unsupported.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- Existing descriptor-backed constraint metadata:
  `docs/specs/baseline-information-schema-constraints/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-check-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_check_constraints_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` has these result columns in order:
  `CONSTRAINT_CATALOG`, `CONSTRAINT_SCHEMA`, `CONSTRAINT_NAME`, and
  `CHECK_CLAUSE`.
- `SHOW COLUMNS FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS` reports:
  - `CONSTRAINT_CATALOG` and `CONSTRAINT_SCHEMA` as non-null `varchar(64)`;
  - `CONSTRAINT_NAME` as non-null `varchar(64)`;
  - `CHECK_CLAUSE` as non-null `longtext`.
- In a database containing no check constraints,
  `SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS WHERE
  CONSTRAINT_SCHEMA = db` returns `0`.
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for
  `CHECK_CONSTRAINTS` with `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`,
  `VERSION = 10`, `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`,
  `DATA_LENGTH = 0`, and `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per
  `CHECK_CONSTRAINTS` column. Text metadata uses `utf8mb3`;
  `CONSTRAINT_CATALOG`, `CONSTRAINT_SCHEMA`, and `CHECK_CLAUSE` use
  `utf8mb3_bin`; `CONSTRAINT_NAME` uses `utf8mb3_tolower_ci`.
- Creating a real MySQL table-level check constraint produces one
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` row with `CONSTRAINT_CATALOG = 'def'`,
  the user schema name, the constraint name, and a normalized check clause
  string. MyLite defers check descriptors and therefore emits no user check
  rows in this phase.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` table definition to the existing
  synthetic information-schema registry;
- the exact four-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.CHECK_CONSTRAINTS`;
- zero user rows until MyLite has check descriptors;
- an `INFORMATION_SCHEMA.TABLES` system-view row for `CHECK_CONSTRAINTS`;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the four
  `CHECK_CONSTRAINTS` metadata columns;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, source aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus reproducible MySQL 8.4.9 expectation coverage.

## Non-Goals

This feature must not implement:

- `CHECK` constraint syntax in `CREATE TABLE` or `ALTER TABLE`;
- stored check-constraint descriptors, generated names, duplicate-name checks,
  enforcement, validation, `ENFORCED` / `NOT ENFORCED`, or expression
  normalization;
- check-constraint rows in `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`;
- physical `information_schema` SQLite tables, SQLite constraint reflection,
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
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`. Future check descriptors must be
  designed before this table emits user rows.
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
FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through this table.

## `CHECK_CONSTRAINTS` Columns

`INFORMATION_SCHEMA.CHECK_CONSTRAINTS` has four columns:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `CONSTRAINT_CATALOG` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `CONSTRAINT_SCHEMA` | non-null `varchar(64)`, `utf8mb3_bin` | No rows |
| `CONSTRAINT_NAME` | non-null `varchar(64)`, `utf8mb3_tolower_ci` | No rows |
| `CHECK_CLAUSE` | non-null `longtext`, `utf8mb3_bin` | No rows |

Because there are no rows, this phase's user-visible value behavior is limited
to column labels, predicates over an empty row set, ordering/limit over an
empty row set, and `COUNT(*) = 0` under user-schema predicates.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for `CHECK_CONSTRAINTS`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `CHECK_CONSTRAINTS` |
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

`INFORMATION_SCHEMA.COLUMNS` must expose the four observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and `TABLE_NAME = 'CHECK_CONSTRAINTS'`.
`PRIVILEGES` is the fixed system-view value `select`, `COLUMN_KEY` and
`EXTRA` are empty strings, `COLUMN_COMMENT` and `GENERATION_EXPRESSION` are
empty strings, and `SRS_ID` is SQL `NULL`.

## Diagnostics

The implementation must preserve existing information-schema diagnostics:

- unsupported query shapes keep deterministic MyLite unsupported errors;
- unknown metadata columns fail with `1054 / 42S22`;
- unknown information-schema tables fail with `1109 / 42S02`;
- allocation failures use existing public API conventions.

Successful supported statements emit no warnings.

## Performance And Storage

`CHECK_CONSTRAINTS` adds one static table descriptor and four static column
descriptors. Empty-row queries allocate result metadata and then flow through
the existing information-schema predicate/order/limit code over zero user rows.
System `TABLES` and `COLUMNS` metadata are synthesized from the same registry.
No user table is scanned, no SQLite table is queried for check metadata, and no
file-format or preamble bytes change.

## Tests

Tests must cover:

- wildcard projection and explicit projection over empty user-schema rows;
- `COUNT(*) = 0` for schemas without checks;
- source aliases, supported predicates, one-column order, and `LIMIT` over an
  empty row set;
- `INFORMATION_SCHEMA.TABLES` system-view metadata for `CHECK_CONSTRAINTS`;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all four columns;
- warning count `0`, affected rows `0`, and subsequent `ROW_COUNT() = -1`;
- unknown projection, predicate, and order columns through existing
  information-schema diagnostics;
- close/reopen and independent-handle safety;
- MySQL 8.4.9 expectation script coverage for the table shape, empty result
  behavior, system metadata, and the observed deferred real-check row behavior.

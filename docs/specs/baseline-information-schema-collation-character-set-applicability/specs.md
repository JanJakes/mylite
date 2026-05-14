# Baseline INFORMATION_SCHEMA COLLATION_CHARACTER_SET_APPLICABILITY

## Status

This phase adds a narrow static charset/collation metadata surface:
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`. MyLite already
exposes a limited `utf8mb4` character-set catalog and a limited set of
supported `utf8mb4` collations. This phase makes the corresponding
collation-to-character-set mapping queryable through a MySQL 8.4.9-shaped
synthetic information-schema table.

The feature is metadata-only. It does not widen MyLite's character-set or
collation semantics, and it does not add new string comparison behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing static information-schema catalogs:
  `docs/specs/baseline-information-schema-static-catalogs/specs.md`
- Existing legacy `utf8mb4` collation slice:
  `docs/specs/baseline-utf8mb4-legacy-collations/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collation-character-set-applicability-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_collation_applicability_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` has these result
  columns in order: `COLLATION_NAME` and `CHARACTER_SET_NAME`.
- The columns are non-null `varchar(64)` metadata columns using `utf8mb3` and
  `utf8mb3_general_ci`.
- For the five `utf8mb4` collations currently exposed by MyLite,
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` returns:
  - `utf8mb4_0900_ai_ci` -> `utf8mb4`
  - `utf8mb4_bin` -> `utf8mb4`
  - `utf8mb4_general_ci` -> `utf8mb4`
  - `utf8mb4_unicode_520_ci` -> `utf8mb4`
  - `utf8mb4_unicode_ci` -> `utf8mb4`
- `INFORMATION_SCHEMA.TABLES` contains a system-view row for
  `COLLATION_CHARACTER_SET_APPLICABILITY` with `TABLE_TYPE = 'SYSTEM VIEW'`,
  `ENGINE = NULL`, `VERSION = 10`, `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`,
  `DATA_LENGTH = 0`, and `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` contains one metadata row per table column.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.

## Scope

The implementation must add:

- an `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` table
  definition to the existing synthetic information-schema registry;
- the exact two-column result shape for `SELECT ... FROM
  INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`;
- one row for each currently supported MyLite `utf8mb4` collation:
  `utf8mb4_general_ci`, `utf8mb4_bin`, `utf8mb4_unicode_ci`,
  `utf8mb4_unicode_520_ci`, and `utf8mb4_0900_ai_ci`, each mapped to
  `utf8mb4`;
- an `INFORMATION_SCHEMA.TABLES` system-view row for the table;
- `INFORMATION_SCHEMA.COLUMNS` system-view column rows for the two metadata
  columns;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit metadata-column projection, source aliases, `COUNT(*)`, supported
  metadata predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- character sets other than MyLite's supported `utf8mb4`;
- collations beyond the currently supported MyLite `utf8mb4` collation rows;
- `mysql.collations`, `mysql.character_sets`, or broader MySQL data-dictionary
  tables;
- collation comparison, coercibility, conversion, sorting, grouping, or index
  behavior beyond already supported slices;
- joins, grouping, or wider information-schema query support;
- storage-format changes or SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Statement context: no new state. Existing SELECT result-set handling owns
  diagnostics reset, warning count, and previous row-count updates.
- Parser/AST: no grammar changes. The existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` syntax is reused.
- Analyzer/planner: the existing information-schema query resolver owns source
  matching, projection, aliases, predicates, ordering, and limits against the
  synthetic table definition.
- Catalog module: no persistent catalog rows are read or written. The rows are
  derived from MyLite's static supported collation descriptors.
- Result builder: emits MySQL-shaped text metadata values through
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
FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
  source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through this table.

## Rows

The table has two non-null `varchar(64)` columns:

| Column | Type metadata | MyLite value source |
| --- | --- | --- |
| `COLLATION_NAME` | `utf8mb3`, `utf8mb3_general_ci` | Supported MyLite collation descriptor name |
| `CHARACTER_SET_NAME` | `utf8mb3`, `utf8mb3_general_ci` | Fixed `utf8mb4` |

Rows correspond exactly to the current MyLite `INFORMATION_SCHEMA.COLLATIONS`
slice:

| `COLLATION_NAME` | `CHARACTER_SET_NAME` |
| --- | --- |
| `utf8mb4_general_ci` | `utf8mb4` |
| `utf8mb4_bin` | `utf8mb4` |
| `utf8mb4_unicode_ci` | `utf8mb4` |
| `utf8mb4_unicode_520_ci` | `utf8mb4` |
| `utf8mb4_0900_ai_ci` | `utf8mb4` |

Queries that need deterministic presentation must use `ORDER BY`; the
underlying static row enumeration is an internal MyLite detail.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include a row for
`COLLATION_CHARACTER_SET_APPLICABILITY`:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_NAME` | `COLLATION_CHARACTER_SET_APPLICABILITY` |
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

`INFORMATION_SCHEMA.COLUMNS` must expose the two observed metadata rows for
`TABLE_SCHEMA = 'information_schema'` and
`TABLE_NAME = 'COLLATION_CHARACTER_SET_APPLICABILITY'`. `PRIVILEGES` is the
fixed system-view value `select`, `COLUMN_KEY` and `EXTRA` are empty strings,
`COLUMN_COMMENT` and `GENERATION_EXPRESSION` are empty strings, and `SRS_ID`
is SQL `NULL`.

## Diagnostics

The implementation must preserve existing information-schema diagnostics:

- unsupported query shapes keep deterministic MyLite unsupported errors;
- unknown metadata columns fail with `1054 / 42S22`;
- unknown information-schema tables fail with `1109 / 42S02`;
- allocation failures use existing public API conventions.

Successful supported statements emit no warnings.

## Performance And Storage

The table adds one static table descriptor and two static column descriptors.
Rows are generated by iterating the existing static `utf8mb4` collation
descriptors and appending one small text row per descriptor. No user schema is
scanned, no SQLite table is queried for metadata, and no file-format or
preamble bytes change.

## Tests

Tests must cover:

- wildcard and explicit projection for the five supported collation mappings;
- `COUNT(*) = 5` for the MyLite slice;
- source aliases, supported predicates, one-column order, and `LIMIT`;
- `INFORMATION_SCHEMA.TABLES` system-view metadata for the table;
- `INFORMATION_SCHEMA.COLUMNS` metadata for both columns;
- warning count `0`, affected rows `0`, and subsequent `ROW_COUNT() = -1`;
- unknown projection, predicate, and order columns through existing
  information-schema diagnostics;
- existing static catalog behavior for `CHARACTER_SETS`, `COLLATIONS`, and
  `ENGINES`;
- MySQL 8.4.9 expectation script coverage for row shape, system metadata, and
  diagnostics.

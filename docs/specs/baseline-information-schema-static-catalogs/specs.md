# Baseline INFORMATION_SCHEMA Static Catalogs

## Summary

This phase adds a narrow static metadata slice for common client discovery
queries:

- `INFORMATION_SCHEMA.ENGINES`
- `INFORMATION_SCHEMA.CHARACTER_SETS`
- `INFORMATION_SCHEMA.COLLATIONS`

The supported rows expose only MyLite's existing embedded compatibility
surface: one default `InnoDB` engine row, one `utf8mb4` character-set row, and
one `utf8mb4_0900_ai_ci` collation row. This is not full MySQL engine,
character-set, or collation support.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ENGINES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-engines-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.CHARACTER_SETS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-character-sets-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLLATIONS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html
- MySQL 8.4 Reference Manual, `SHOW ENGINES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-engines.html
- MySQL 8.4 Reference Manual, `SHOW CHARACTER SET`:
  https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html
- MySQL 8.4 Reference Manual, `SHOW COLLATION`:
  https://dev.mysql.com/doc/refman/8.4/en/show-collation.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_static_catalogs_expectations.sh`.

This specification is independently authored from the public documentation and
runtime observations above. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 Observations

Observed against the local MySQL 8.4.9 runtime:

- `SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE='InnoDB'` returns:

```text
InnoDB  DEFAULT  Supports transactions, row-level locking, and foreign keys  YES  YES  YES
```

- `SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS WHERE CHARACTER_SET_NAME='utf8mb4'`
  returns:

```text
utf8mb4  utf8mb4_0900_ai_ci  UTF-8 Unicode  4
```

- `SELECT * FROM INFORMATION_SCHEMA.COLLATIONS WHERE COLLATION_NAME='utf8mb4_0900_ai_ci'`
  returns:

```text
utf8mb4_0900_ai_ci  utf8mb4  255  Yes  Yes  0  NO PAD
```

- Successful `SELECT` statements leave `@@warning_count == 0` and make the
  following `ROW_COUNT()` return `-1`.
- Name predicates are case-insensitive for the observed metadata columns:
  `ENGINE='innodb'`, `CHARACTER_SET_NAME='UTF8MB4'`, and
  `COLLATION_NAME='UTF8MB4_0900_AI_CI'` all match.
- The full MySQL runtime exposes wider catalogs: 11 engines, 41 character
  sets, and 286 collations in the tested container. MyLite intentionally exposes
  only rows that correspond to implemented semantics.
- System-view rows in `INFORMATION_SCHEMA.TABLES` use `TABLE_TYPE =
  'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`, `ROW_FORMAT = NULL`, and
  `TABLE_ROWS = 0`.
- `INFORMATION_SCHEMA.COLUMNS` metadata for these system views is pinned to
  observed MySQL 8.4.9 values by the expectation script. Notable details:
  `ENGINES.ENGINE` has `CHARACTER_MAXIMUM_LENGTH = 21` while its
  `COLUMN_TYPE` is `varchar(64)`, and engine capability columns are nullable.

## Scope

The implementation must add:

- table definitions for `ENGINES`, `CHARACTER_SETS`, and `COLLATIONS` to
  MyLite's limited information-schema registry;
- one static system row for each new table;
- system `INFORMATION_SCHEMA.TABLES` rows for the new views through the
  existing information-schema metadata path;
- system `INFORMATION_SCHEMA.COLUMNS` rows for every column in the new views;
- reuse of the existing information-schema query surface: wildcard projection,
  explicit projection, aliases, `COUNT(*)`, the supported metadata predicate
  subset, one-column `ORDER BY`, and row-count `LIMIT`;
- case-insensitive metadata comparisons for the admitted name predicates,
  following the metadata collation already used by the information-schema query
  engine;
- warning and row-count behavior matching observed MySQL 8.4.9 and existing
  MyLite result conventions;
- fast C tests and a MySQL 8.4.9 expectation artifact.

## Non-Goals

This feature must not implement:

- alternate storage engines or engine plugins;
- full MySQL character-set or collation catalogs;
- any character set other than `utf8mb4`;
- any collation other than `utf8mb4_0900_ai_ci`;
- `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`,
  `mysql.collations`, `mysql.character_sets`, or engine-internal metadata;
- mutable engine, charset, or collation state;
- charset conversion, collation comparison semantics, coercibility, or string
  ordering behavior;
- privilege filtering;
- joins, subqueries, grouping, aggregation beyond existing `COUNT(*)`, or wider
  information-schema query support;
- SQLite fork patches.

## Ownership Boundaries

- Public API: no new ABI. Applications use existing `mylite_execute()` and
  result accessors.
- Statement context: no new session state. Existing diagnostics, warning
  count, and previous-row-count behavior apply.
- Parser/AST: no grammar changes. Existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` parsing is reused.
- Analyzer/planner: the existing limited information-schema query planner
  resolves projections, aliases, predicates, ordering, and limits against the
  synthetic table definitions.
- Catalog module: no catalog rows are read or written for these static rows.
  Existing descriptors remain authoritative for user schemas and tables.
- Result builder: emits MySQL-shaped column labels and text/`NULL` values
  through existing `mylite_result` conventions.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: no SQLite metadata, arbitrary SQL pass-through, fork
  patch, or extension hook is required. This is a MyLite-owned synthetic
  metadata view.

## Supported Query Surface

This phase adds three table names to the existing limited information-schema
query engine:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.ENGINES [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]

SELECT select_list
FROM INFORMATION_SCHEMA.CHARACTER_SETS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]

SELECT select_list
FROM INFORMATION_SCHEMA.COLLATIONS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The existing information-schema query limits still apply:

- wildcard projection, explicit column projection, aliases, and `COUNT(*)`;
- one source table;
- optional source alias;
- existing metadata predicate subset;
- one-column `ORDER BY`;
- existing `LIMIT` subset;
- no joins, subqueries, arbitrary expressions, functions beyond the existing
  information-schema support, grouping, mutation, or DDL.

No new Lemon grammar is required for this phase.

## `ENGINES` Columns and Row

`ENGINES` has six columns in this order:

| Column | MyLite value |
| --- | --- |
| `ENGINE` | `InnoDB` |
| `SUPPORT` | `DEFAULT` |
| `COMMENT` | `Supports transactions, row-level locking, and foreign keys` |
| `TRANSACTIONS` | `YES` |
| `XA` | `YES` |
| `SAVEPOINTS` | `YES` |

System `COLUMNS` metadata follows the observed MySQL 8.4.9 values:

- `ENGINE` is non-null `varchar(64)` with observed maximum length `21`;
- `SUPPORT` is non-null `varchar(8)` with observed maximum length `2`;
- `COMMENT` is non-null `varchar(80)` with observed maximum length `26`;
- `TRANSACTIONS`, `XA`, and `SAVEPOINTS` are nullable `varchar(3)` with
  observed maximum length `1`;
- text columns use `utf8mb3` / `utf8mb3_general_ci`.

## `CHARACTER_SETS` Columns and Row

`CHARACTER_SETS` has four columns in this order:

| Column | MyLite value |
| --- | --- |
| `CHARACTER_SET_NAME` | `utf8mb4` |
| `DEFAULT_COLLATE_NAME` | `utf8mb4_0900_ai_ci` |
| `DESCRIPTION` | `UTF-8 Unicode` |
| `MAXLEN` | `4` |

System `COLUMNS` metadata follows the observed MySQL 8.4.9 values:

- the first three columns are non-null `varchar` columns using `utf8mb3` /
  `utf8mb3_general_ci`;
- `CHARACTER_SET_NAME` and `DEFAULT_COLLATE_NAME` are `varchar(64)`;
- `DESCRIPTION` is `varchar(2048)`;
- `MAXLEN` is non-null `int unsigned`.

## `COLLATIONS` Columns and Row

`COLLATIONS` has seven columns in this order:

| Column | MyLite value |
| --- | --- |
| `COLLATION_NAME` | `utf8mb4_0900_ai_ci` |
| `CHARACTER_SET_NAME` | `utf8mb4` |
| `ID` | `255` |
| `IS_DEFAULT` | `Yes` |
| `IS_COMPILED` | `Yes` |
| `SORTLEN` | `0` |
| `PAD_ATTRIBUTE` | `NO PAD` |

System `COLUMNS` metadata follows the observed MySQL 8.4.9 values:

- `COLLATION_NAME` and `CHARACTER_SET_NAME` are non-null `varchar(64)` using
  `utf8mb3` / `utf8mb3_general_ci`;
- `ID` is non-null `bigint unsigned` with default `0`;
- `IS_DEFAULT` and `IS_COMPILED` are non-null `varchar(3)` with default empty
  string;
- `SORTLEN` is non-null `int unsigned`;
- `PAD_ATTRIBUTE` is non-null `enum('PAD SPACE','NO PAD')` using
  `utf8mb3_bin`.

## Diagnostics

The feature reuses existing information-schema diagnostics:

- unknown information-schema table: MySQL-compatible `1109 / 42S02`;
- unknown projected column: MySQL-compatible `1054 / 42S22`;
- unknown `WHERE` or `ORDER BY` column: existing context-specific `1054 /
  42S22`;
- unsupported query shape: existing deterministic MyLite unsupported syntax or
  runtime diagnostic;
- allocation failure: `MYLITE_NOMEM` with handle diagnostics.

Successful queries introduce no warnings.

## Performance

Rows for these tables are fixed static row arrays. The implementation must not
open catalog scans or inspect SQLite schema metadata for the direct static
catalog rows. The existing `INFORMATION_SCHEMA.TABLES` and
`INFORMATION_SCHEMA.COLUMNS` synthetic metadata paths iterate the registered
table definitions; adding three tiny definitions keeps this bounded and avoids
materializing user data.

## Compatibility Documentation

`COMPATIBILITY.md` and detailed compatibility docs must describe this as a
limited one-row static catalog surface. They must not claim full
`INFORMATION_SCHEMA` parity, alternate engines, alternate charsets, alternate
collations, privilege filtering, mutable state, or collation semantics.

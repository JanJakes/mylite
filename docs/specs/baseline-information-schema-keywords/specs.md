# Baseline INFORMATION_SCHEMA KEYWORDS

## Summary

This phase adds a synthetic `INFORMATION_SCHEMA.KEYWORDS` table for clients that
discover reserved words at runtime before generating identifiers or DDL.

The implementation exposes MySQL 8.4.9-shaped `WORD` and `RESERVED` columns and
a MyLite-owned static keyword list verified against a MySQL 8.4.9 runtime. It
does not add parser support for every listed keyword and does not make the
`KEYWORDS` table a catalog authority for SQL syntax.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEYWORDS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-keywords-table.html
- MySQL 8.4 Reference Manual, keyword and reserved word overview:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_keywords_expectations.sh`.

This specification is independently authored from the public documentation and
runtime observations above. It does not copy MySQL grammar, generated keyword
headers, or implementation sources.

## MySQL 8.4.9 Observations

Observed against the local MySQL 8.4.9 container:

- `INFORMATION_SCHEMA.KEYWORDS` has two columns:
  - `WORD`
  - `RESERVED`
- `RESERVED` is an integer value where `1` means reserved and `0` means
  nonreserved.
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS` returns `734`.
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1` returns
  `259`.
- `SELECT * FROM INFORMATION_SCHEMA.KEYWORDS ORDER BY WORD LIMIT 3` returns:

```text
ACCESSIBLE  1
ACCOUNT     0
ACTION      0
```

- `SELECT WORD, RESERVED FROM INFORMATION_SCHEMA.KEYWORDS WHERE WORD = 'SELECT'`
  returns `SELECT, 1`.
- `WORD` predicates use MySQL's metadata collation behavior for this table:
  `WORD = 'select'` matches the `SELECT` row.
- `INFORMATION_SCHEMA.TABLES` reports `KEYWORDS` as a system view with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_TYPE = 'SYSTEM VIEW'`,
  `ENGINE = NULL`, `VERSION = 10`, `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, and
  `TABLE_COLLATION = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` metadata for the table is:
  - `WORD`: nullable `varchar(128)`, character length `128`, octet length
    `512`, character set `utf8mb4`, collation `utf8mb4_0900_ai_ci`;
  - `RESERVED`: nullable `int`, numeric precision `10`, scale `0`.
- Successful reads leave `@@warning_count = 0`, and a following `ROW_COUNT()`
  returns `-1`.

## Scope

The implementation must add:

- `INFORMATION_SCHEMA.KEYWORDS` to the limited information-schema table
  registry;
- a static MyLite-owned keyword row list containing the MySQL 8.4.9 `WORD` and
  `RESERVED` values verified by runtime probes;
- system `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` rows
  through the existing metadata paths;
- reuse of the existing information-schema query surface:
  - wildcard and explicit column projections;
  - source aliases;
  - `COUNT(*)`;
  - the existing metadata predicate subset;
  - one-column `ORDER BY`;
  - `LIMIT row_count`;
- case-insensitive `WORD` comparisons through the existing metadata collation
  helper;
- numeric comparison and ordering for `RESERVED` through the existing
  information-schema integer metadata path;
- MySQL 8.4.9 expectation coverage and fast C runtime tests.

## Non-Goals

This feature must not implement:

- new SQL keywords, grammar productions, statements, or functions just because a
  word appears in the metadata table;
- parser behavior driven from the `INFORMATION_SCHEMA.KEYWORDS` row list;
- MySQL's generated keyword source header;
- a mutable keyword catalog;
- joins, grouping, aggregate functions beyond existing `COUNT(*)`, subqueries,
  or arbitrary expressions in information-schema queries;
- bare truth predicates such as `WHERE RESERVED` or `WHERE NOT RESERVED`; the
  current supported MyLite form is `WHERE RESERVED = 1` or
  `WHERE RESERVED = 0`;
- privilege filtering;
- SQLite table storage, SQLite virtual tables, or SQLite fork patches.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result accessors.
- Statement context: no new mutable session state. Diagnostics, warning count,
  affected rows, and previous-row-count behavior follow the existing
  information-schema result conventions.
- Parser/AST: no grammar changes. Existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` parsing is reused.
- Analyzer/planner: the existing information-schema planner resolves
  projections, aliases, predicates, ordering, and limits against the new
  synthetic table definition.
- Catalog module: no durable catalog rows are read or written. This table is a
  static MyLite-owned system view, not a descriptor-backed object.
- Result builder: rows are emitted as text values and `NULL`s through existing
  `mylite_result` conventions.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: no SQLite schema, physical table, extension API, or
  fork hook is required.

## Query Surface

No new Lemon grammar is required. The existing information-schema select form
admits:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.KEYWORDS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY WORD|RESERVED [ASC|DESC]]
[LIMIT row_count]
```

The table has two columns in this order:

| Column | Type surface | Values |
| --- | --- | --- |
| `WORD` | nullable `varchar(128)` | uppercase keyword text |
| `RESERVED` | nullable `int` | `1` for reserved, `0` for nonreserved |

The supported predicate subset is inherited from the current
information-schema engine:

- `WORD = string_literal`, `WORD <> string_literal`, and supported string
  ordering predicates;
- `RESERVED` integer comparisons against integer, `TRUE`, or `FALSE` values;
- `IS NULL`, `IS NOT NULL`, `AND`, `OR`, `XOR`, and `NOT` over the admitted
  predicates.

The current engine still rejects unsupported predicate forms deterministically,
including `IN`, `LIKE`, functions other than the already admitted
`DATABASE()`/`SCHEMA()` metadata predicate values, scalar subqueries, arbitrary
expressions, parameters, and bare truth predicates.

## Row Ordering

The static row list is stored in MySQL 8.4.9 `WORD` order. A query without
`ORDER BY` returns that internal order through the existing information-schema
row builder, but tests should use explicit ordering for stable user-visible
claims.

`ORDER BY WORD` uses the `utf8mb4_0900_ai_ci` metadata collation approximation
already used by MyLite information-schema text comparisons. For the uppercase
ASCII keyword list, this matches the observed ordering. `ORDER BY RESERVED`
uses numeric ordering because `RESERVED` is declared as `int` metadata.

## Diagnostics

The existing information-schema diagnostics apply:

- unknown `KEYWORDS` columns use the existing unknown-column diagnostic;
- unsupported predicates use the existing
  `INFORMATION_SCHEMA WHERE supports metadata predicates` or narrower
  value/comparison diagnostics;
- unsupported projections use the existing
  `INFORMATION_SCHEMA SELECT supports only metadata columns and COUNT(*)`
  diagnostic;
- unsupported `ORDER BY` and `LIMIT` forms use the existing deterministic
  information-schema diagnostics;
- allocation failures return `MYLITE_NOMEM` with handle-owned diagnostics;
- public API misuse behavior is unchanged because there is no public surface
  change.

## Performance

This table is a small static metadata view. The existing information-schema
engine materializes synthetic rows in memory, then applies predicates, ordering,
and limits. For 734 two-column rows this is acceptable for the current embedded
metadata slice and avoids adding a SQLite virtual table or fork extension point.
This feature must not route arbitrary SQL into SQLite.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/metadata-information-schema.md`

Only the limited synthetic keyword catalog should be claimed. Do not claim full
SQL keyword grammar support, generated parser integration, or arbitrary
information-schema query support.

## Verification

Required before completion:

1. `packages/libmylite/tests/mysql_baseline_information_schema_keywords_expectations.sh`
2. `cmake --build --preset dev`
3. `ctest --preset dev -R 'libmylite\.runtime\.information_schema_keywords$' --output-on-failure`
4. `ctest --preset dev -R 'libmylite\.runtime\.information_schema_static_catalogs$' --output-on-failure`
5. `cmake --workflow --preset check`


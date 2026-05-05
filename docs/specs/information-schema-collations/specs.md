# INFORMATION_SCHEMA.COLLATIONS

## Scope

This feature implements the first executable slice of MySQL's collation
metadata table:

- `SELECT * FROM INFORMATION_SCHEMA.COLLATIONS`

This slice defines the row source and wildcard row shape for
`INFORMATION_SCHEMA.COLLATIONS`. Broader MySQL-supported `SELECT` forms are
covered by the composable `information_schema` system-view path when this table
is routed through the regular binder:

- explicit projections
- explicit `ALL` or `DISTINCT` modifiers
- `WHERE`
- `ORDER BY`
- `LIMIT`
- joins
- aliases
- qualified wildcards
- expressions
- aggregate queries such as `COUNT(*)`

The same shared collation registry must back this table and `SHOW COLLATION`.
MyLite must not maintain a second duplicate catalog for this table or for
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `COLLATIONS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html
- MySQL 8.4 Reference Manual, `SHOW COLLATION` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-collation.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA`
  `COLLATION_CHARACTER_SET_APPLICABILITY` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collation-character-set-applicability-table.html
- Runtime observations verified against MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.COLLATIONS` exposes collations available for each character
set. MySQL documents it as the queryable counterpart to `SHOW COLLATION`.

The verified MySQL 8.4.9 runtime accepts case-insensitive references such as
`information_schema.collations`, mixed-case references, and quoted forms such
as `` `information_schema`.`COLLATIONS` ``.

The result has exactly these columns in order:

1. `COLLATION_NAME`
2. `CHARACTER_SET_NAME`
3. `ID`
4. `IS_DEFAULT`
5. `IS_COMPILED`
6. `SORTLEN`
7. `PAD_ATTRIBUTE`

Column metadata observed for the `binary` row in MySQL 8.4.9:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `COLLATION_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL NO_DEFAULT_VALUE` |
| `CHARACTER_SET_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL NO_DEFAULT_VALUE` |
| `ID` | `LONGLONG` | `binary` | 20 | `NOT_NULL UNSIGNED NUM` |
| `IS_DEFAULT` | `VAR_STRING` | `latin1_swedish_ci` | 3 | `NOT_NULL` |
| `IS_COMPILED` | `VAR_STRING` | `latin1_swedish_ci` | 3 | `NOT_NULL` |
| `SORTLEN` | `LONG` | `binary` | 10 | `NOT_NULL UNSIGNED NO_DEFAULT_VALUE NUM` |
| `PAD_ATTRIBUTE` | `STRING` | `latin1_swedish_ci` | 9 | `NOT_NULL BINARY ENUM NO_DEFAULT_VALUE` |

The verified supported-subset probe, ordered by collation name, returned:

| COLLATION_NAME | CHARACTER_SET_NAME | ID | IS_DEFAULT | IS_COMPILED | SORTLEN | PAD_ATTRIBUTE |
| --- | --- | ---: | --- | --- | ---: | --- |
| `binary` | `binary` | 63 | `Yes` | `Yes` | 1 | `NO PAD` |
| `latin1_bin` | `latin1` | 47 | `` | `Yes` | 1 | `PAD SPACE` |
| `latin1_swedish_ci` | `latin1` | 8 | `Yes` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb3_bin` | `utf8mb3` | 83 | `` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb3_general_ci` | `utf8mb3` | 33 | `Yes` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb4_0900_ai_ci` | `utf8mb4` | 255 | `Yes` | `Yes` | 0 | `NO PAD` |
| `utf8mb4_bin` | `utf8mb4` | 46 | `` | `Yes` | 1 | `PAD SPACE` |

The full MySQL catalog is larger. In the verified runtime,
`SELECT * FROM INFORMATION_SCHEMA.COLLATIONS LIMIT 2` began with
`armscii8_general_ci`, `armscii8`, `32`, `Yes`, `Yes`, `1`, `PAD SPACE`,
followed by `armscii8_bin`, `armscii8`, `64`, an empty default marker, `Yes`,
`1`, `PAD SPACE`. MyLite intentionally exposes only collations implemented in
its registry for this first slice.

MySQL supports normal query processing over this table. Projection and filtered
forms therefore work in MySQL, but MyLite intentionally defers those
non-wildcard and filtered forms in this first slice.

## Syntax

MyLite owns the grammar below. It describes the accepted executable slice for
this feature and is authored for MyLite's Lemon parser:

```lemon
select_statement ::= SELECT STAR FROM information_schema_collations_name.

information_schema_collations_name ::= identifier DOT identifier.
```

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.COLLATIONS`. Broader projections, filters, aliases,
ordering, limits, and aggregates are handled by the composable
information-schema system-view path where the corresponding `SELECT` feature is
implemented. Schema and table names match case-insensitively after identifier
unquoting.

## Runtime Semantics

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.COLLATIONS
```

Result columns:

- The result set has exactly seven columns in the uppercase order documented
  above.
- `COLLATION_NAME`, `CHARACTER_SET_NAME`, `IS_DEFAULT`, `IS_COMPILED`, and
  `PAD_ATTRIBUTE` are non-null text values.
- `ID` and `SORTLEN` are emitted as numeric integer values, not string
  placeholders.
- `IS_DEFAULT` is `Yes` for the default collation of its character set and an
  empty string otherwise.
- `IS_COMPILED` is `Yes` for every supported collation in this slice.
- `PAD_ATTRIBUTE` is emitted from the shared registry.
- Successful execution is read-only, returns affected rows `-1`, and does not
  mutate diagnostics beyond the existing successful-prepare/step behavior for
  MyLite `SELECT` statements.

MyLite first-slice rows:

| COLLATION_NAME | CHARACTER_SET_NAME | ID | IS_DEFAULT | IS_COMPILED | SORTLEN | PAD_ATTRIBUTE |
| --- | --- | ---: | --- | --- | ---: | --- |
| `binary` | `binary` | 63 | `Yes` | `Yes` | 1 | `NO PAD` |
| `latin1_bin` | `latin1` | 47 | `` | `Yes` | 1 | `PAD SPACE` |
| `latin1_swedish_ci` | `latin1` | 8 | `Yes` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb3_bin` | `utf8mb3` | 83 | `` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb3_general_ci` | `utf8mb3` | 33 | `Yes` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb4_0900_ai_ci` | `utf8mb4` | 255 | `Yes` | `Yes` | 0 | `NO PAD` |
| `utf8mb4_bin` | `utf8mb4` | 46 | `` | `Yes` | 1 | `PAD SPACE` |

Rows must be generated from `mylite_collation_count()` and
`mylite_collation_at()`, the same registry used by `SHOW COLLATION`.
Unsupported MySQL collations are omitted so applications do not discover
collations that MyLite cannot accept in DDL or session character-set
statements.

`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` uses this same row
source and exposes the `COLLATION_NAME` / `CHARACTER_SET_NAME` mapping.

Rows are ordered by collation name using the same deterministic ordering as
`SHOW COLLATION`. The current registry is materialized and ordered by
collation name for this subset.

## INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` must include
`TABLE_SCHEMA='information_schema'`, `TABLE_NAME='COLLATIONS'` as a system-view
row alongside MyLite's existing information-schema system views. The companion
`COLLATION_CHARACTER_SET_APPLICABILITY` system-view row is specified in
[INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY](../information-schema-collation-character-set-applicability/specs.md).

For this row, MyLite uses the same system-view values as the existing narrow
metadata views:

- `TABLE_CATALOG='def'`
- `TABLE_TYPE='SYSTEM VIEW'`
- `ENGINE=NULL`
- `VERSION=10`
- `TABLE_ROWS=0`
- `TABLE_COLLATION=NULL`
- `TABLE_COMMENT=''`

`SHOW TABLES FROM information_schema` should expose `COLLATIONS` through the
same system-view list, including `SHOW FULL TABLES FROM information_schema LIKE
'collations'` returning `COLLATIONS`, `SYSTEM VIEW`.

## Metadata Limitation

Existing `INFORMATION_SCHEMA` `SELECT` execution prepares SQLite-backed
statements directly and does not attach full MySQL field metadata for any
supported information-schema table. This first slice keeps that behavior
consistent: tests verify column names, values, row order, numeric `ID` and
`SORTLEN`, and case-insensitive resolution, while exact field descriptors for
`INFORMATION_SCHEMA.COLLATIONS` remain deferred to a unified
information-schema metadata pass.

## Composable Query Forms

The following MySQL-supported forms are covered by the shared system-view
`SELECT` path after the composable information-schema update:

- `SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATIONS`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.COLLATIONS`
- `SELECT ALL * FROM INFORMATION_SCHEMA.COLLATIONS`
- `SELECT * FROM INFORMATION_SCHEMA.COLLATIONS WHERE COLLATION_NAME = 'utf8mb4_bin'`
- `SELECT * FROM INFORMATION_SCHEMA.COLLATIONS ORDER BY COLLATION_NAME`
- `SELECT * FROM INFORMATION_SCHEMA.COLLATIONS LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATIONS`
- table aliases
- qualified wildcards such as
  `SELECT INFORMATION_SCHEMA.COLLATIONS.* FROM INFORMATION_SCHEMA.COLLATIONS`
- joins and expressions involving `INFORMATION_SCHEMA.COLLATIONS`

Unqualified `SELECT * FROM COLLATIONS` is not part of the feature and follows
the ordinary selected-schema table-resolution path.

## Runtime And Storage Impact

This feature is read-only and requires no file-format change. Runtime execution
materializes the small immutable collation registry into a SQLite read
statement with uppercase column aliases for `INFORMATION_SCHEMA.COLLATIONS`.
No mutable process-global state or new dependency is needed.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.COLLATIONS`
- lower-case and mixed-case schema/table names
- quoted schema/table names
- explicit projection and filter forms remain parseable and are executable
  through the shared system-view path

Runtime coverage:

- exact uppercase result column names
- exact row values in collation-name order
- `ID` and `SORTLEN` are numeric through direct integer accessor assertions
- lower-case, mixed-case, and quoted table references resolve successfully
- explicit projection, `DISTINCT`, `ALL`, `WHERE`, `ORDER BY`, `LIMIT`,
  `COUNT(*)`, table aliases, and qualified wildcards execute through the shared
  system-view path
- `INFORMATION_SCHEMA.TABLES` includes a `COLLATIONS` system-view row with the
  same system-view values as the existing metadata views
- `SHOW TABLES FROM information_schema` exposes `COLLATIONS`

## Known Incompatibilities

- MyLite exposes only the collations implemented in its registry instead of
  MySQL's full collation catalog.
- Full MySQL field metadata for information-schema `SELECT` statements remains
  deferred.

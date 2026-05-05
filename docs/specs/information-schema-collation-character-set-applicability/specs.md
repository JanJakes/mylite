# INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY

## Scope

This feature implements the first executable slice of MySQL's collation to
character-set applicability metadata table:

- `SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`

This slice defines the row source and wildcard row shape for
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`. Broader
MySQL-supported `SELECT` forms are covered by the composable
`information_schema` system-view path when this table is routed through the
regular binder:

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

The same shared collation registry must back this table, `SHOW COLLATION`, and
`INFORMATION_SCHEMA.COLLATIONS`. MyLite must not maintain a second duplicate
catalog for this table.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA`
  `COLLATION_CHARACTER_SET_APPLICABILITY` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collation-character-set-applicability-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `COLLATIONS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html
- MySQL 8.4 Reference Manual, `SHOW COLLATION` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-collation.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- Runtime observations verified against MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` reports the
character set associated with each collation. MySQL documents its columns as
the same mapping represented by the first two `SHOW COLLATION` result columns.

The verified MySQL 8.4.9 runtime accepts case-insensitive references such as
`information_schema.collation_character_set_applicability`, mixed-case
references, and quoted forms such as
`` `information_schema`.`COLLATION_CHARACTER_SET_APPLICABILITY` ``.

The result has exactly these columns in order:

1. `COLLATION_NAME`
2. `CHARACTER_SET_NAME`

Column metadata observed for the `binary` row in MySQL 8.4.9:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `COLLATION_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL UNIQUE_KEY NO_DEFAULT_VALUE PART_KEY` |
| `CHARACTER_SET_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL UNIQUE_KEY NO_DEFAULT_VALUE PART_KEY` |

The verified supported-subset probe, ordered by collation name, returned:

| COLLATION_NAME | CHARACTER_SET_NAME |
| --- | --- |
| `binary` | `binary` |
| `latin1_bin` | `latin1` |
| `latin1_swedish_ci` | `latin1` |
| `utf8mb3_bin` | `utf8mb3` |
| `utf8mb3_general_ci` | `utf8mb3` |
| `utf8mb4_0900_ai_ci` | `utf8mb4` |
| `utf8mb4_bin` | `utf8mb4` |

The full MySQL catalog is larger. In the verified runtime,
`SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY LIMIT 2`
began with `armscii8_general_ci`, `armscii8`, followed by `armscii8_bin`,
`armscii8`. MyLite intentionally exposes only collations implemented in its
registry for this first slice.

MySQL supports normal query processing over this table. For example, filtering
for `CHARACTER_SET_NAME = 'utf8mb4'` returns the utf8mb4 collations. MyLite
intentionally defers those non-wildcard and filtered forms in this first slice.

## Syntax

MyLite owns the grammar below. It describes the accepted executable slice for
this feature and is authored for MyLite's Lemon parser:

```lemon
select_statement ::= SELECT STAR FROM information_schema_collation_charset_name.

information_schema_collation_charset_name ::= identifier DOT identifier.
```

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`. Broader
projections, filters, aliases, ordering, limits, and aggregates are handled by
the composable information-schema system-view path where the corresponding
`SELECT` feature is implemented. Schema and table names match
case-insensitively after identifier unquoting.

## Runtime Semantics

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY
```

Result columns:

- The result set has exactly two columns in the uppercase order documented
  above.
- `COLLATION_NAME` and `CHARACTER_SET_NAME` are non-null text values.
- Successful execution is read-only, returns affected rows `-1`, and does not
  mutate diagnostics beyond the existing successful-prepare/step behavior for
  MyLite `SELECT` statements.

MyLite first-slice rows:

| COLLATION_NAME | CHARACTER_SET_NAME |
| --- | --- |
| `binary` | `binary` |
| `latin1_bin` | `latin1` |
| `latin1_swedish_ci` | `latin1` |
| `utf8mb3_bin` | `utf8mb3` |
| `utf8mb3_general_ci` | `utf8mb3` |
| `utf8mb4_0900_ai_ci` | `utf8mb4` |
| `utf8mb4_bin` | `utf8mb4` |

Rows must be generated from `mylite_collation_count()` and
`mylite_collation_at()`, the same registry used by `SHOW COLLATION` and
`INFORMATION_SCHEMA.COLLATIONS`. Unsupported MySQL collations are omitted so
applications do not discover collations that MyLite cannot accept in DDL or
session character-set statements.

Rows are ordered by collation name using the same deterministic ordering as
`INFORMATION_SCHEMA.COLLATIONS` and `SHOW COLLATION`.

## INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` must include
`TABLE_SCHEMA='information_schema'`,
`TABLE_NAME='COLLATION_CHARACTER_SET_APPLICABILITY'` as a system-view row
alongside MyLite's existing information-schema system views.

For this row, MyLite uses the same system-view values as the existing narrow
metadata views:

- `TABLE_CATALOG='def'`
- `TABLE_TYPE='SYSTEM VIEW'`
- `ENGINE=NULL`
- `VERSION=10`
- `TABLE_ROWS=0`
- `TABLE_COLLATION=NULL`
- `TABLE_COMMENT=''`

`SHOW TABLES FROM information_schema` should expose
`COLLATION_CHARACTER_SET_APPLICABILITY` through the same system-view list,
including `SHOW FULL TABLES FROM information_schema LIKE
'collation_character_set_applicability'` returning
`COLLATION_CHARACTER_SET_APPLICABILITY`, `SYSTEM VIEW`.

## Metadata Limitation

Existing `INFORMATION_SCHEMA` `SELECT` execution prepares SQLite-backed
statements directly and does not attach full MySQL field metadata for any
supported information-schema table. This first slice keeps that behavior
consistent: tests verify column names, values, row order, and case-insensitive
resolution, while exact field descriptors for
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` remain deferred to a
unified information-schema metadata pass.

## Composable Query Forms

The following MySQL-supported forms are covered by the shared system-view
`SELECT` path after the composable information-schema update:

- `SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
- `SELECT ALL * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
- `SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY WHERE CHARACTER_SET_NAME = 'utf8mb4'`
- `SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY ORDER BY COLLATION_NAME`
- `SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
- table aliases
- qualified wildcards such as
  `SELECT INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY.* FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
- joins and expressions involving
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`

Unqualified `SELECT * FROM COLLATION_CHARACTER_SET_APPLICABILITY` is not part of
the feature and follows the ordinary selected-schema table-resolution path.

## Runtime And Storage Impact

This feature is read-only and requires no file-format change. Runtime execution
materializes the small immutable collation registry into a SQLite read statement
with uppercase column aliases for
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`. No mutable
process-global state or new dependency is needed.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
- lower-case and mixed-case schema/table names
- quoted schema/table names
- explicit projection and filter forms remain parseable and are executable
  through the shared system-view path

Runtime coverage:

- exact uppercase result column names
- exact row values in collation-name order
- lower-case, mixed-case, and quoted table references resolve successfully
- explicit projection, `DISTINCT`, `ALL`, `WHERE`, `ORDER BY`, `LIMIT`,
  `COUNT(*)`, table aliases, and qualified wildcards execute through the shared
  system-view path
- `INFORMATION_SCHEMA.TABLES` includes a
  `COLLATION_CHARACTER_SET_APPLICABILITY` system-view row with the same
  system-view values as the existing metadata views
- `SHOW TABLES FROM information_schema` exposes
  `COLLATION_CHARACTER_SET_APPLICABILITY`

## Known Incompatibilities

- MyLite exposes only the collations implemented in its registry instead of
  MySQL's full collation catalog.
- Full MySQL field metadata for information-schema `SELECT` statements remains
  deferred.

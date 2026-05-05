# INFORMATION_SCHEMA.CHARACTER_SETS

## Scope

This feature implements the first executable slice of MySQL's character-set
metadata table:

- `SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS`

This slice defines the row source and wildcard row shape for
`INFORMATION_SCHEMA.CHARACTER_SETS`. Broader MySQL-supported `SELECT` forms are
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

The same shared character-set registry must back this table and
`SHOW CHARACTER SET`; MyLite must not maintain a second duplicate catalog for
this table.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `CHARACTER_SETS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-character-sets-table.html
- MySQL 8.4 Reference Manual, `SHOW CHARACTER SET` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- Runtime observations verified against Docker container `mylite-mysql-849`,
  MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.CHARACTER_SETS` exposes character sets available in the
server. MySQL documents the table as the queryable counterpart to
`SHOW CHARACTER SET`.

The verified MySQL 8.4.9 runtime accepts case-insensitive references such as
`information_schema.character_sets`, mixed-case references, and quoted forms
such as `` `information_schema`.`CHARACTER_SETS` ``.

The result has exactly these columns in order:

1. `CHARACTER_SET_NAME`
2. `DEFAULT_COLLATE_NAME`
3. `DESCRIPTION`
4. `MAXLEN`

Column metadata observed for the `binary` row in MySQL 8.4.9:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `CHARACTER_SET_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL UNIQUE_KEY NO_DEFAULT_VALUE PART_KEY` |
| `DEFAULT_COLLATE_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL UNIQUE_KEY NO_DEFAULT_VALUE PART_KEY` |
| `DESCRIPTION` | `VAR_STRING` | `latin1_swedish_ci` | 2048 | `NOT_NULL NO_DEFAULT_VALUE` |
| `MAXLEN` | `LONG` | `binary` | 10 | `NOT_NULL UNSIGNED NO_DEFAULT_VALUE NUM` |

The verified supported-subset probe, ordered by character-set name, returned:

| CHARACTER_SET_NAME | DEFAULT_COLLATE_NAME | DESCRIPTION | MAXLEN |
| --- | --- | --- | ---: |
| `binary` | `binary` | `Binary pseudo charset` | 1 |
| `latin1` | `latin1_swedish_ci` | `cp1252 West European` | 1 |
| `utf8mb3` | `utf8mb3_general_ci` | `UTF-8 Unicode` | 3 |
| `utf8mb4` | `utf8mb4_0900_ai_ci` | `UTF-8 Unicode` | 4 |

The full MySQL catalog is larger. In the verified runtime,
`SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS LIMIT 2` began with
`big5`, `big5_chinese_ci`, `Big5 Traditional Chinese`, `2`, followed by
`dec8`, `dec8_swedish_ci`, `DEC West European`, `1`. MyLite intentionally
exposes only character sets implemented in its registry for this first slice.

MySQL supports normal query processing over this table. For example,
`SELECT CHARACTER_SET_NAME, DEFAULT_COLLATE_NAME, MAXLEN FROM
INFORMATION_SCHEMA.CHARACTER_SETS WHERE CHARACTER_SET_NAME = 'utf8mb4'`
returns `utf8mb4`, `utf8mb4_0900_ai_ci`, and `4`. MyLite intentionally defers
those non-wildcard and filtered forms in this first slice.

## Syntax

MyLite owns the grammar below. It describes the accepted executable slice for
this feature and is authored for MyLite's Lemon parser:

```lemon
select_statement ::= SELECT STAR FROM information_schema_character_sets_name.

information_schema_character_sets_name ::= identifier DOT identifier.
```

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.CHARACTER_SETS`. Broader projections, filters, aliases,
ordering, limits, and aggregates are handled by the composable
information-schema system-view path where the corresponding `SELECT` feature is
implemented. Schema and table names match case-insensitively after identifier
unquoting.

## Runtime Semantics

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS
```

Result columns:

- The result set has exactly four columns in the uppercase order documented
  above.
- `CHARACTER_SET_NAME`, `DEFAULT_COLLATE_NAME`, and `DESCRIPTION` are non-null
  text values.
- `MAXLEN` is emitted as a numeric integer value, not a string placeholder.
- Successful execution is read-only, returns affected rows `-1`, and does not
  mutate diagnostics beyond the existing successful-prepare/step behavior for
  MyLite `SELECT` statements.

MyLite first-slice rows:

| CHARACTER_SET_NAME | DEFAULT_COLLATE_NAME | DESCRIPTION | MAXLEN |
| --- | --- | --- | ---: |
| `binary` | `binary` | `Binary pseudo charset` | 1 |
| `latin1` | `latin1_swedish_ci` | `cp1252 West European` | 1 |
| `utf8mb3` | `utf8mb3_general_ci` | `UTF-8 Unicode` | 3 |
| `utf8mb4` | `utf8mb4_0900_ai_ci` | `UTF-8 Unicode` | 4 |

Rows must be generated from `mylite_charset_count()` and `mylite_charset_at()`,
the same registry used by `SHOW CHARACTER SET`. Unsupported MySQL character
sets are omitted so applications do not discover character sets that MyLite
cannot accept in DDL or session character-set statements.

Rows are ordered by character-set name using the existing registry order. The
current registry order is already the expected ascending order for this subset.

## INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` must include
`TABLE_SCHEMA='information_schema'`, `TABLE_NAME='CHARACTER_SETS'` as a
system-view row alongside MyLite's existing information-schema system views.

For this row, MyLite uses the same system-view values as the existing narrow
metadata views:

- `TABLE_CATALOG='def'`
- `TABLE_TYPE='SYSTEM VIEW'`
- `ENGINE=NULL`
- `VERSION=10`
- `TABLE_ROWS=0`
- `TABLE_COLLATION=NULL`
- `TABLE_COMMENT=''`

`SHOW TABLES FROM information_schema` should expose `CHARACTER_SETS` through the
same system-view list, including `SHOW FULL TABLES FROM information_schema LIKE
'character_sets'` returning `CHARACTER_SETS`, `SYSTEM VIEW`.

## Metadata Limitation

Existing `INFORMATION_SCHEMA` `SELECT` execution prepares SQLite-backed
statements directly and does not attach full MySQL field metadata for any
supported information-schema table. This first slice keeps that behavior
consistent: tests verify column names, values, row order, numeric `MAXLEN`, and
case-insensitive resolution, while exact field descriptors for
`INFORMATION_SCHEMA.CHARACTER_SETS` remain deferred to a unified
information-schema metadata pass.

## Composable Query Forms

The following MySQL-supported forms are covered by the shared system-view
`SELECT` path after the composable information-schema update:

- `SELECT CHARACTER_SET_NAME FROM INFORMATION_SCHEMA.CHARACTER_SETS`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.CHARACTER_SETS`
- `SELECT ALL * FROM INFORMATION_SCHEMA.CHARACTER_SETS`
- `SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS WHERE CHARACTER_SET_NAME = 'utf8mb4'`
- `SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS ORDER BY CHARACTER_SET_NAME`
- `SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHARACTER_SETS`
- table aliases
- qualified wildcards such as
  `SELECT INFORMATION_SCHEMA.CHARACTER_SETS.* FROM INFORMATION_SCHEMA.CHARACTER_SETS`
- joins and expressions involving `INFORMATION_SCHEMA.CHARACTER_SETS`

Unqualified `SELECT * FROM CHARACTER_SETS` is not part of the feature and
follows the ordinary selected-schema table-resolution path.

## Runtime And Storage Impact

This feature is read-only and requires no file-format change. Runtime execution
materializes the small immutable character-set registry into a SQLite read
statement with uppercase column aliases for `INFORMATION_SCHEMA.CHARACTER_SETS`.
No mutable process-global state or new dependency is needed.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS`
- lower-case and mixed-case schema/table names
- quoted schema/table names
- explicit projection and filter forms remain parseable and are executable
  through the shared system-view path

Runtime coverage:

- exact uppercase result column names
- exact row values in the shared registry order
- `MAXLEN` is numeric through a direct integer accessor assertion
- lower-case, mixed-case, and quoted table references resolve successfully
- explicit projection, `WHERE`, `ORDER BY`, `LIMIT`, `COUNT(*)`, table aliases,
  and qualified wildcards execute through the shared system-view path
- `INFORMATION_SCHEMA.TABLES` includes a `CHARACTER_SETS` system-view row with
  the same system-view values as the existing metadata views
- `SHOW TABLES FROM information_schema` exposes `CHARACTER_SETS`

## Known Incompatibilities

- MyLite exposes only the character sets implemented in its registry instead of
  MySQL's full character-set catalog.
- Full MySQL field metadata for information-schema `SELECT` statements remains
  deferred.

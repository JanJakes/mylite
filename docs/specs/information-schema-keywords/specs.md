# INFORMATION_SCHEMA.KEYWORDS

## Scope

This feature implements the first executable slice of MySQL's keyword metadata
table:

- `SELECT * FROM INFORMATION_SCHEMA.KEYWORDS`

This slice defines the row source and wildcard row shape for
`INFORMATION_SCHEMA.KEYWORDS`. Broader MySQL-supported `SELECT` forms are
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

The same shared lexer keyword catalog must back lexical keyword recognition and
`INFORMATION_SCHEMA.KEYWORDS`. MyLite must not maintain a second duplicate
keyword list for this table.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `KEYWORDS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-keywords-table.html
- MySQL 8.4 Reference Manual, Keywords and Reserved Words:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- Runtime observations verified against MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.KEYWORDS` exposes MySQL keyword names and whether each word
is reserved. Applications can use it to decide when generated identifiers need
quoting.

The verified MySQL 8.4.9 runtime accepts case-insensitive references such as
`information_schema.keywords`, mixed-case references, and quoted forms such as
`` `information_schema`.`KEYWORDS` ``.

The result has exactly these columns in order:

1. `WORD`
2. `RESERVED`

Column metadata observed for a `SELECT` row in MySQL 8.4.9:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `WORD` | `VAR_STRING` | `latin1_swedish_ci` | 128 | `PART_KEY` |
| `RESERVED` | `LONG` | `binary` | 11 | `NUM` |

The verified MySQL 8.4.9 keyword catalog contains 734 rows, of which 259 rows
have `RESERVED = 1`. The first five rows from `SELECT * FROM
INFORMATION_SCHEMA.KEYWORDS LIMIT 5` were:

| WORD | RESERVED |
| --- | ---: |
| `ACCESSIBLE` | 1 |
| `ACCOUNT` | 0 |
| `ACTION` | 0 |
| `ACTIVE` | 0 |
| `ADD` | 1 |

Representative observed rows:

| WORD | RESERVED |
| --- | ---: |
| `ACCOUNT` | 0 |
| `ADD` | 1 |
| `JSON_TABLE` | 1 |
| `SELECT` | 1 |
| `SYSTEM` | 1 |
| `VISIBLE` | 0 |
| `WINDOW` | 1 |

MySQL supports normal query processing over this table. For example,
`SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS` returns `734`, and
`SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1` returns
`259` in the verified runtime. MyLite intentionally defers those non-wildcard,
filtered, and aggregate forms in this first slice.

## Syntax

MyLite owns the grammar below. It describes the accepted executable slice for
this feature and is authored for MyLite's Lemon parser:

```lemon
select_statement ::= SELECT STAR FROM information_schema_keywords_name.

information_schema_keywords_name ::= identifier DOT identifier.
```

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.KEYWORDS`. Broader projections, filters, aliases, ordering,
limits, and aggregates are handled by the composable information-schema
system-view path where the corresponding `SELECT` feature is implemented.
Schema and table names match case-insensitively after identifier unquoting.

## Runtime Semantics

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.KEYWORDS
```

Result columns:

- The result set has exactly two columns in the uppercase order documented
  above.
- `WORD` is a non-null uppercase keyword text value from MyLite's lexer keyword
  catalog.
- `RESERVED` is emitted as numeric integer `1` when
  `MYLITE_SQL_KEYWORD_RESERVED` is present in the keyword flags and numeric
  integer `0` otherwise.
- Successful execution is read-only, returns affected rows `-1`, and does not
  mutate diagnostics beyond the existing successful-prepare/step behavior for
  MyLite `SELECT` statements.

Rows must be generated from the shared lexer keyword catalog in
`mylite_lexer.c`. The lookup path must keep its current case-insensitive binary
search behavior. The iteration API should expose the keyword count and indexed
entries without allowing callers to mutate the catalog.

Rows are emitted in lexer catalog order. The current catalog order begins:

| WORD | RESERVED |
| --- | ---: |
| `ACCESSIBLE` | 1 |
| `ACCOUNT` | 0 |
| `ACTION` | 0 |
| `ACTIVE` | 0 |
| `ADD` | 1 |

## INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` must include
`TABLE_SCHEMA='information_schema'`, `TABLE_NAME='KEYWORDS'` as a system-view
row alongside MyLite's existing information-schema system views.

For this row, MyLite uses the same system-view values as the existing narrow
metadata views:

- `TABLE_CATALOG='def'`
- `TABLE_TYPE='SYSTEM VIEW'`
- `ENGINE=NULL`
- `VERSION=10`
- `TABLE_ROWS=0`
- `TABLE_COLLATION=NULL`
- `TABLE_COMMENT=''`

`SHOW TABLES FROM information_schema` should expose `KEYWORDS` through the same
system-view list, including `SHOW FULL TABLES FROM information_schema LIKE
'keywords'` returning `KEYWORDS`, `SYSTEM VIEW`.

## Metadata Limitation

Existing `INFORMATION_SCHEMA` `SELECT` execution prepares SQLite-backed
statements directly and does not attach full MySQL field metadata for any
supported information-schema table. This first slice keeps that behavior
consistent: tests verify column names, values, numeric `RESERVED` values, row
order, and case-insensitive resolution, while exact field descriptors for
`INFORMATION_SCHEMA.KEYWORDS` remain deferred to a unified information-schema
metadata pass.

## Composable Query Forms

The following MySQL-supported forms are covered by the shared system-view
`SELECT` path after the composable information-schema update:

- `SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.KEYWORDS`
- `SELECT ALL * FROM INFORMATION_SCHEMA.KEYWORDS`
- `SELECT * FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1`
- `SELECT * FROM INFORMATION_SCHEMA.KEYWORDS ORDER BY WORD`
- `SELECT * FROM INFORMATION_SCHEMA.KEYWORDS LIMIT 5`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS`
- table aliases
- qualified wildcards such as
  `SELECT INFORMATION_SCHEMA.KEYWORDS.* FROM INFORMATION_SCHEMA.KEYWORDS`
- joins and expressions involving `INFORMATION_SCHEMA.KEYWORDS`

Unqualified `SELECT * FROM KEYWORDS` is not part of the feature and follows the
ordinary selected-schema table-resolution path.

## Runtime And Storage Impact

This feature is read-only and requires no file-format change. Runtime execution
materializes the immutable lexer keyword catalog into a SQLite read statement
with uppercase column aliases for `INFORMATION_SCHEMA.KEYWORDS`. No mutable
process-global state or new dependency is needed.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.KEYWORDS`
- lower-case and mixed-case schema/table names
- quoted schema/table names
- explicit projection and filter forms remain parseable and are executable
  through the shared system-view path

Runtime coverage:

- exact uppercase result column names
- first rows match lexer catalog order
- representative reserved and nonreserved rows expose numeric `RESERVED`
  values
- lower-case, mixed-case, and quoted table references resolve successfully
- explicit projection, `DISTINCT`, `ALL`, `WHERE`, `ORDER BY`, `LIMIT`,
  `COUNT(*)`, table aliases, and qualified wildcards execute through the shared
  system-view path
- `INFORMATION_SCHEMA.TABLES` includes a `KEYWORDS` system-view row with the
  same system-view values as the existing metadata views
- `SHOW TABLES FROM information_schema` exposes `KEYWORDS`
- lexer keyword iteration returns the same entries and flags used by keyword
  lookup

## Known Incompatibilities

- MyLite exposes its current lexer-supported keyword catalog instead of
  claiming exact MySQL 8.4.9 keyword catalog completeness. The catalog may be
  smaller or differ until full MySQL grammar coverage lands.
- Full MySQL field metadata for information-schema `SELECT` statements remains
  deferred.

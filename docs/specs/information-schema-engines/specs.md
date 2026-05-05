# INFORMATION_SCHEMA.ENGINES

## Scope

This feature implements the first executable slice of MySQL's storage-engine
metadata table:

- `SELECT * FROM INFORMATION_SCHEMA.ENGINES`

This slice defines the row source and wildcard row shape for
`INFORMATION_SCHEMA.ENGINES`. Broader MySQL-supported `SELECT` forms are covered
by the composable `information_schema` system-view path when this table is
routed through the regular binder:

- explicit projections
- `WHERE`
- `ORDER BY`
- `LIMIT`
- joins
- aliases
- expressions
- aggregate queries such as `COUNT(*)`

The same immutable storage-engine registry must back this table and
`SHOW [STORAGE] ENGINES`.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `ENGINES` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-engines-table.html
- MySQL 8.4 Reference Manual, `SHOW ENGINES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-engines.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- Runtime observations verified by the parent workflow against MySQL 8.4.9.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.ENGINES` exposes storage-engine status rows. MySQL
documents `SELECT * FROM INFORMATION_SCHEMA.ENGINES` and `SHOW ENGINES` as
equivalent statements for this data.

The verified MySQL 8.4.9 runtime accepts case-insensitive references such as
`information_schema.engines` and quoted forms such as
`` `information_schema`.`ENGINES` ``.

The result has exactly these columns in order:

1. `ENGINE`
2. `SUPPORT`
3. `COMMENT`
4. `TRANSACTIONS`
5. `XA`
6. `SAVEPOINTS`

Column metadata observed for `SELECT * FROM INFORMATION_SCHEMA.ENGINES LIMIT 1`
in MySQL 8.4.9:

| Column | Type | Collation | Length | Nullability |
| --- | --- | --- | ---: | --- |
| `ENGINE` | `VAR_STRING` | `latin1_swedish_ci` | 64 | not null |
| `SUPPORT` | `VAR_STRING` | `latin1_swedish_ci` | 8 | not null |
| `COMMENT` | `VAR_STRING` | `latin1_swedish_ci` | 80 | not null |
| `TRANSACTIONS` | `VAR_STRING` | `latin1_swedish_ci` | 3 | nullable |
| `XA` | `VAR_STRING` | `latin1_swedish_ci` | 3 | nullable |
| `SAVEPOINTS` | `VAR_STRING` | `latin1_swedish_ci` | 3 | nullable |

MySQL supports normal query processing over this table, including projections,
filters, ordering, limits, and aggregates. MyLite intentionally defers those
forms in this first slice.

`SUPPORT` values have the same meaning as `SHOW ENGINES`:

- `DEFAULT`: supported, active, and selected as the default storage engine
- `YES`: supported and active
- `NO`: not supported by the build
- `DISABLED`: supported by the build but disabled by startup/runtime options

Capability columns use text values such as `YES` or `NO` when a storage engine
is supported and `NULL` when the row describes an unsupported engine.

## Syntax

MyLite owns the grammar below. It describes the accepted slice for this feature
and is authored for MyLite's Lemon parser:

```lemon
select_statement ::= SELECT STAR FROM information_schema_engines_name.

information_schema_engines_name ::= identifier DOT identifier.
```

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.ENGINES`. Broader projections, filters, aliases, ordering,
limits, and aggregates are handled by the composable information-schema
system-view path where the corresponding `SELECT` feature is implemented.
Schema and table names match case-insensitively after identifier unquoting.

## Runtime Semantics

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.ENGINES
```

Result columns:

- The result set has exactly six columns in the uppercase order documented
  above.
- The first three columns are non-null text.
- The last three columns are text for supported engines and `NULL` for
  unsupported engines.
- Successful execution is read-only, returns affected rows `-1`, and does not
  mutate diagnostics beyond the existing successful-prepare/step behavior for
  MyLite `SELECT` statements.

MyLite first-slice rows:

| ENGINE | SUPPORT | COMMENT | TRANSACTIONS | XA | SAVEPOINTS |
| --- | --- | --- | --- | --- | --- |
| `InnoDB` | `DEFAULT` | `MyLite SQLite-backed transactional engine facade` | `YES` | `NO` | `YES` |
| `MEMORY` | `NO` | `In-memory tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `MyISAM` | `NO` | `MyISAM tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `FEDERATED` | `NO` | `Federated tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `MRG_MYISAM` | `NO` | `Merge MyISAM tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `BLACKHOLE` | `NO` | `Blackhole tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `CSV` | `NO` | `CSV-backed tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |
| `ARCHIVE` | `NO` | `Archive tables are not supported by MyLite` | `NULL` | `NULL` | `NULL` |

This registry deliberately claims only the `InnoDB` facade as supported.
Unsupported engine rows are probeable metadata only; `CREATE TABLE` must
continue rejecting unsupported engine names.

`InnoDB` is a MyLite compatibility facade over SQLite-backed storage. It does
not imply MySQL InnoDB internals, row locks, foreign-key enforcement,
tablespaces, redo/undo logs, or XA transaction support.

## INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` must include
`TABLE_SCHEMA='information_schema'`, `TABLE_NAME='ENGINES'` as a system-view
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

`SHOW TABLES FROM information_schema` should expose `ENGINES` through the same
system-view list.

## Metadata Limitation

`SHOW ENGINES` currently attaches MySQL-compatible field metadata through
MyLite's custom result metadata path. Existing `INFORMATION_SCHEMA` `SELECT`
execution prepares SQLite-backed statements directly and does not attach full
MySQL field metadata for any supported information-schema table. This first
slice keeps that behavior consistent: tests verify column names, values, row
order, nulls, and case-insensitive resolution, while exact field descriptors for
`INFORMATION_SCHEMA.ENGINES` remain deferred to a unified information-schema
metadata pass.

## Composable Query Forms

The following MySQL-supported forms are covered by the shared system-view
`SELECT` path after the composable information-schema update:

- `SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES`
- `SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'InnoDB'`
- `SELECT * FROM INFORMATION_SCHEMA.ENGINES ORDER BY ENGINE`
- `SELECT * FROM INFORMATION_SCHEMA.ENGINES LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.ENGINES`
- aliases, joins, and expressions involving `INFORMATION_SCHEMA.ENGINES`

Unqualified `SELECT * FROM ENGINES` is not part of the feature and follows the
ordinary selected-schema table-resolution path.

## Runtime And Storage Impact

This feature is read-only and requires no file-format change. Runtime execution
materializes the small immutable storage-engine registry into a SQLite read
statement with different column aliases for `SHOW ENGINES` and
`INFORMATION_SCHEMA.ENGINES`. No mutable process-global state or new dependency
is needed.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.ENGINES`
- lower-case and mixed-case schema/table names
- quoted schema/table names
- explicit projection and filter forms remain parseable and are executable
  through the shared system-view path

Runtime coverage:

- exact uppercase result column names
- exact row values in the shared registry order
- unsupported engines return `NULL` for `TRANSACTIONS`, `XA`, and `SAVEPOINTS`
- lower-case, mixed-case, and quoted table references resolve successfully
- explicit projection, `WHERE`, `ORDER BY`, `LIMIT`, `COUNT(*)`, table aliases,
  and qualified wildcards execute through the shared system-view path
- `INFORMATION_SCHEMA.TABLES` includes an `ENGINES` system-view row with the
  same system-view values as the existing metadata views
- `SHOW TABLES FROM information_schema` exposes `ENGINES`

## Known Incompatibilities

- MyLite exposes a small embedded registry instead of MySQL's full
  build-dependent engine catalog.
- Full MySQL field metadata for information-schema `SELECT` statements remains
  deferred.

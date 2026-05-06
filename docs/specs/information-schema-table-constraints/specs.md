# INFORMATION_SCHEMA.TABLE_CONSTRAINTS

## Scope

This feature implements the first executable slice of MySQL's table-constraint
metadata table:

- `SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS`

This slice defines the row source and wildcard row shape for
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS`. Broader MySQL-supported `SELECT` forms
are covered by the composable `information_schema` system-view path when this
table is routed through the regular binder:

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

This slice exposes primary-key and unique constraints by deriving rows from
`__mylite_index_catalog` and CHECK constraints by deriving rows from
`__mylite_check_constraint_catalog`. MyLite must not create a parallel
constraint catalog for primary and unique metadata. Nonunique indexes are
excluded. Actual foreign-key constraint rows remain deferred until MyLite has
catalogs and runtime semantics for that constraint family.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `TABLE_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- MySQL 8.4 Reference Manual, constraint information-schema tables:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-information-schema.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA`
  `REFERENTIAL_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html
- Runtime observations verified against MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` reports constraints attached to tables.
MySQL includes `UNIQUE`, `PRIMARY KEY`, `FOREIGN KEY`, and `CHECK` rows. The
verified primary-key and unique-key behavior maps to the same logical metadata
reported by `SHOW INDEX` rows whose `Non_unique` value is `0`; CHECK rows map
to MyLite's CHECK catalog.

The verified MySQL 8.4.9 runtime accepts case-insensitive references such as
`information_schema.table_constraints`, mixed-case references, and quoted forms
such as `` `information_schema`.`TABLE_CONSTRAINTS` ``.

The result has exactly these columns in order:

1. `CONSTRAINT_CATALOG`
2. `CONSTRAINT_SCHEMA`
3. `CONSTRAINT_NAME`
4. `TABLE_SCHEMA`
5. `TABLE_NAME`
6. `CONSTRAINT_TYPE`
7. `ENFORCED`

Column metadata observed in MySQL 8.4.9:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `CONSTRAINT_CATALOG` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `CONSTRAINT_SCHEMA` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `CONSTRAINT_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | none |
| `TABLE_SCHEMA` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `TABLE_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `CONSTRAINT_TYPE` | `VAR_STRING` | `latin1_swedish_ci` | 11 | `NOT_NULL BINARY` |
| `ENFORCED` | `VAR_STRING` | `latin1_swedish_ci` | 3 | `NOT_NULL BINARY` |

Runtime probe:

```sql
CREATE TABLE parent (
    id INT PRIMARY KEY,
    code INT UNIQUE,
    name VARCHAR(20),
    UNIQUE KEY uq_name (name),
    KEY idx_name (name)
);
```

Querying `TABLE_CONSTRAINTS` for that schema returned:

| CONSTRAINT_CATALOG | CONSTRAINT_SCHEMA | CONSTRAINT_NAME | TABLE_SCHEMA | TABLE_NAME | CONSTRAINT_TYPE | ENFORCED |
| --- | --- | --- | --- | --- | --- | --- |
| `def` | `mylite_tc_probe` | `code` | `mylite_tc_probe` | `parent` | `UNIQUE` | `YES` |
| `def` | `mylite_tc_probe` | `PRIMARY` | `mylite_tc_probe` | `parent` | `PRIMARY KEY` | `YES` |
| `def` | `mylite_tc_probe` | `uq_name` | `mylite_tc_probe` | `parent` | `UNIQUE` | `YES` |

The nonunique index `idx_name` did not appear. Dropping a unique index removed
its row. Renaming a unique index changed `CONSTRAINT_NAME` to the new index
name.

MySQL supports normal query processing over this table. Projection, filtered,
ordered, limited, alias, join, and aggregate forms therefore work in MySQL, but
MyLite intentionally defers those forms in this first slice.

## Syntax

MyLite owns the grammar below. It describes the accepted executable slice for
this feature and is authored for MyLite's Lemon parser:

```lemon
select_statement ::= SELECT STAR FROM information_schema_table_constraints_name.

information_schema_table_constraints_name ::= identifier DOT identifier.
```

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS`. Broader projections, filters, aliases,
ordering, limits, and aggregates are handled by the composable
information-schema system-view path where the corresponding `SELECT` feature is
implemented. Schema and table names match case-insensitively after identifier
unquoting.

## Runtime Semantics

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
```

Result columns:

- The result set has exactly seven columns in the uppercase order documented
  above.
- `CONSTRAINT_CATALOG` is the literal `def`.
- `CONSTRAINT_SCHEMA` and `TABLE_SCHEMA` are the table schema.
- `TABLE_NAME` is the constrained table name.
- `CONSTRAINT_NAME` is `PRIMARY` for primary-key rows and the index name for
  non-primary unique rows. CHECK rows use the explicit constraint name or the
  generated `<table>_chk_<n>` name.
- `CONSTRAINT_TYPE` is `PRIMARY KEY` for `index_name='PRIMARY'` and `UNIQUE`
  for other unique rows. CHECK rows use `CHECK`.
- `ENFORCED` is `YES` for primary and unique rows. CHECK rows use the
  `ENFORCED` or `NOT ENFORCED` state captured from `CREATE TABLE`.
- Successful execution is read-only, returns affected rows `-1`, and does not
  mutate diagnostics beyond the existing successful-prepare/step behavior for
  MyLite `SELECT` statements.

Primary and unique rows are derived from `__mylite_index_catalog` with
`non_unique = 0`. Each logical index produces at most one table-constraint row,
even when the index has multiple key parts. CHECK rows are derived from
`__mylite_check_constraint_catalog`.

Rows are ordered deterministically by:

1. `TABLE_SCHEMA` using binary collation
2. `TABLE_NAME` using binary collation
3. `CONSTRAINT_NAME` using case-insensitive MySQL-compatible ordering
4. primary-key constraints before unique constraints only as a tie-breaker for
   unusual duplicate catalog names
5. first catalog row for the index or CHECK ordinal position

This ordering keeps `SELECT *` stable without adding general `ORDER BY` support
for information-schema queries and matches the MySQL 8.4.9 ordering observed
for primary and unique constraints.

## INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` must include
`TABLE_SCHEMA='information_schema'`, `TABLE_NAME='TABLE_CONSTRAINTS'` as a
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

`SHOW TABLES FROM information_schema` should expose `TABLE_CONSTRAINTS` through
the same system-view list, including `SHOW FULL TABLES FROM information_schema
LIKE 'table_constraints'` returning `TABLE_CONSTRAINTS`, `SYSTEM VIEW`.

## Metadata Limitation

Existing `INFORMATION_SCHEMA` `SELECT` execution prepares SQLite-backed
statements directly and does not attach full MySQL field metadata for any
supported information-schema table. This first slice keeps that behavior
consistent: tests verify column names, values, row order, DDL side effects, and
case-insensitive resolution, while exact field descriptors for
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` remain deferred to a unified
information-schema metadata pass.

## Composable Query Forms

The following MySQL-supported forms are covered by the shared system-view
`SELECT` path after the composable information-schema update:

- `SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- `SELECT ALL * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- `SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE CONSTRAINT_TYPE = 'UNIQUE'`
- `SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS ORDER BY CONSTRAINT_NAME`
- `SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- table aliases
- qualified wildcards such as
  `SELECT INFORMATION_SCHEMA.TABLE_CONSTRAINTS.* FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- joins and expressions involving `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`

Unqualified `SELECT * FROM TABLE_CONSTRAINTS` is not part of the feature and
follows the ordinary selected-schema table-resolution path.

## Runtime And Storage Impact

This feature is read-only and requires no file-format change. Runtime execution
selects from the existing `__mylite_index_catalog`; DDL that already inserts,
deletes, or renames primary and unique index metadata automatically updates the
visible constraint rows. No mutable process-global state or new dependency is
needed.

The feature intentionally does not add a physical SQLite index or constraint
catalog. Broader optimizer use of indexes, CHECK constraints, foreign keys,
privilege filtering, temporary tables, and user views remain separate features.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- lower-case and mixed-case schema/table names
- quoted schema/table names
- explicit projection and filter forms remain parseable and are executable
  through the shared system-view path

Runtime coverage:

- empty database returns no rows with exact uppercase column names
- primary key, inline unique, named unique, and nonunique index fixture returns
  exactly the primary and unique rows and excludes the nonunique index
- inline and table-level CHECK constraints return CHECK rows with MySQL
  8.4.9-verified generated names and enforcement values
- row order follows the deterministic MyLite ordering documented above
- lower-case, mixed-case, and quoted table references execute
- `INFORMATION_SCHEMA.TABLES` exposes the `TABLE_CONSTRAINTS` system-view row
- `SHOW TABLES FROM information_schema LIKE 'table_constraints'` and
  `SHOW FULL TABLES FROM information_schema LIKE 'table_constraints'` expose
  the system view
- `DROP INDEX` removes the unique constraint row
- `ALTER TABLE ... RENAME INDEX` renames the unique constraint row when that
  DDL is supported by the current runtime
- composable projections, `DISTINCT`/`ALL`, `WHERE`, `ORDER BY`, `LIMIT`,
  `COUNT(*)`, aliases, and qualified wildcard forms are covered by the shared
  system-view SELECT path

## Known Gaps

- Foreign-key constraints are omitted until MyLite has foreign-key catalogs,
  referential actions, enforcement, and catalog-backed
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` rows.
- Key-part ordinals for primary and unique constraints are exposed by
  [INFORMATION_SCHEMA.KEY_COLUMN_USAGE](../information-schema-key-column-usage/specs.md).
- Privilege filtering and exact MySQL field metadata remain deferred. General
  projection, filtering, ordering, limiting, alias, and aggregate behavior is
  covered by the composable information-schema SELECT path.

# INFORMATION_SCHEMA.KEY_COLUMN_USAGE

## Scope

This feature implements the first executable slice of MySQL's key-column
constraint metadata table:

- `SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE`

This slice defines the row source and wildcard row shape for
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE`. Broader MySQL-supported `SELECT` forms
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

This slice exposes primary-key and unique-constraint key parts by deriving rows
from `__mylite_index_catalog`. MyLite must not create a parallel key-column
catalog for primary and unique metadata. Nonunique indexes are excluded.
Functional or expression-only key parts are excluded because this information
schema table reports columns, not expressions. CHECK constraints do not appear
in `KEY_COLUMN_USAGE`. Foreign-key rows remain deferred until MyLite has
foreign-key catalogs and runtime semantics; `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
currently exposes only the empty MySQL-compatible system-view shape.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `KEY_COLUMN_USAGE` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
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

`INFORMATION_SCHEMA.KEY_COLUMN_USAGE` reports which table columns participate in
constraints. MySQL returns one row per constrained column position. Functional
key parts are omitted because they are expressions rather than columns.

For primary-key and unique constraints, MySQL reports:

- `CONSTRAINT_CATALOG='def'`
- `CONSTRAINT_SCHEMA` as the constraint schema
- `CONSTRAINT_NAME='PRIMARY'` for primary keys
- `CONSTRAINT_NAME` as the index name for unique constraints
- `TABLE_CATALOG='def'`
- `TABLE_SCHEMA` and `TABLE_NAME` for the constrained table
- `COLUMN_NAME` for the constrained column
- `ORDINAL_POSITION` as the one-based position within the constraint
- `POSITION_IN_UNIQUE_CONSTRAINT=NULL`
- referenced-table columns as `NULL`

Foreign-key rows populate the referenced-table columns and may populate
`POSITION_IN_UNIQUE_CONSTRAINT`, but foreign keys are outside this first slice.
CHECK constraints do not produce `KEY_COLUMN_USAGE` rows.

The verified MySQL 8.4.9 runtime accepts case-insensitive references such as
`information_schema.key_column_usage`, mixed-case references, and quoted forms
such as `` `information_schema`.`KEY_COLUMN_USAGE` ``.

The result has exactly these columns in order:

1. `CONSTRAINT_CATALOG`
2. `CONSTRAINT_SCHEMA`
3. `CONSTRAINT_NAME`
4. `TABLE_CATALOG`
5. `TABLE_SCHEMA`
6. `TABLE_NAME`
7. `COLUMN_NAME`
8. `ORDINAL_POSITION`
9. `POSITION_IN_UNIQUE_CONSTRAINT`
10. `REFERENCED_TABLE_SCHEMA`
11. `REFERENCED_TABLE_NAME`
12. `REFERENCED_COLUMN_NAME`

Column metadata observed in MySQL 8.4.9:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `CONSTRAINT_CATALOG` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `CONSTRAINT_SCHEMA` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `CONSTRAINT_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | none |
| `TABLE_CATALOG` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `TABLE_SCHEMA` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `TABLE_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE` |
| `COLUMN_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | none |
| `ORDINAL_POSITION` | `LONG` | `binary` | 10 | `NOT_NULL UNSIGNED NUM` |
| `POSITION_IN_UNIQUE_CONSTRAINT` | `LONG` | `binary` | 10 | `UNSIGNED NUM` |
| `REFERENCED_TABLE_SCHEMA` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `BINARY` |
| `REFERENCED_TABLE_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `BINARY` |
| `REFERENCED_COLUMN_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | none |

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

Querying `KEY_COLUMN_USAGE` for that schema returned:

| CONSTRAINT_CATALOG | CONSTRAINT_SCHEMA | CONSTRAINT_NAME | TABLE_CATALOG | TABLE_SCHEMA | TABLE_NAME | COLUMN_NAME | ORDINAL_POSITION | POSITION_IN_UNIQUE_CONSTRAINT | REFERENCED_TABLE_SCHEMA | REFERENCED_TABLE_NAME | REFERENCED_COLUMN_NAME |
| --- | --- | --- | --- | --- | --- | --- | ---: | --- | --- | --- | --- |
| `def` | `mylite_kcu_probe` | `code` | `def` | `mylite_kcu_probe` | `parent` | `code` | 1 | `NULL` | `NULL` | `NULL` | `NULL` |
| `def` | `mylite_kcu_probe` | `PRIMARY` | `def` | `mylite_kcu_probe` | `parent` | `id` | 1 | `NULL` | `NULL` | `NULL` | `NULL` |
| `def` | `mylite_kcu_probe` | `uq_name` | `def` | `mylite_kcu_probe` | `parent` | `name` | 1 | `NULL` | `NULL` | `NULL` | `NULL` |

The nonunique index `idx_name` did not appear.

Composite primary and unique keys returned one row per key part:

| CONSTRAINT_NAME | COLUMN_NAME | ORDINAL_POSITION | POSITION_IN_UNIQUE_CONSTRAINT |
| --- | --- | ---: | --- |
| `PRIMARY` | `a` | 1 | `NULL` |
| `PRIMARY` | `b` | 2 | `NULL` |
| `uq_bc` | `b` | 1 | `NULL` |
| `uq_bc` | `c` | 2 | `NULL` |
| `uq_prefix` | `c` | 1 | `NULL` |

For unique constraints, MySQL uses the index name in `CONSTRAINT_NAME`. In the
verified statement

```sql
CONSTRAINT con_e UNIQUE KEY unique_e (e)
```

MySQL reported `CONSTRAINT_NAME='unique_e'`, not the constraint symbol
`con_e`. Prefix lengths and descending key-part markers did not change
`KEY_COLUMN_USAGE`; a unique key over `s(4), t DESC` reported rows for `s` and
`t` with ordinal positions `1` and `2`.

Dropping a unique index removes its key-column rows. Renaming a unique index
changes `CONSTRAINT_NAME` for its key-column rows. Dropping a primary key
removes the primary key-column rows.

MySQL supports normal query processing over this table. Projection, filtered,
ordered, limited, alias, join, and aggregate forms therefore work in MySQL, but
MyLite intentionally defers those forms in this first slice.

## Syntax

MyLite owns the grammar below. It describes the accepted executable slice for
this feature and is authored for MyLite's Lemon parser:

```lemon
select_statement ::= SELECT STAR FROM information_schema_key_column_usage_name.

information_schema_key_column_usage_name ::= identifier DOT identifier.
```

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE`. Broader projections, filters, aliases,
ordering, limits, and aggregates are handled by the composable
information-schema system-view path where the corresponding `SELECT` feature is
implemented. Schema and table names match case-insensitively after identifier
unquoting.

## Runtime Semantics

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
```

Result columns:

- The result set has exactly twelve columns in the uppercase order documented
  above.
- `CONSTRAINT_CATALOG` is the literal `def`.
- `CONSTRAINT_SCHEMA` is `__mylite_index_catalog.table_schema`.
- `CONSTRAINT_NAME` is `PRIMARY` for primary-key rows and
  `__mylite_index_catalog.index_name` for non-primary unique rows.
- `TABLE_CATALOG` is the literal `def`.
- `TABLE_SCHEMA` and `TABLE_NAME` come from `__mylite_index_catalog`.
- `COLUMN_NAME` is `__mylite_index_catalog.column_name`.
- `ORDINAL_POSITION` is `__mylite_index_catalog.seq_in_index`.
- `POSITION_IN_UNIQUE_CONSTRAINT` is `NULL` for every row in this slice.
- `REFERENCED_TABLE_SCHEMA`, `REFERENCED_TABLE_NAME`, and
  `REFERENCED_COLUMN_NAME` are `NULL` for every row in this slice.
- Successful execution is read-only, returns affected rows `-1`, and does not
  mutate diagnostics beyond the existing successful-prepare/step behavior for
  MyLite `SELECT` statements.

Rows are derived from `__mylite_index_catalog` with `non_unique = 0` and
`column_name IS NOT NULL`. This produces one row per primary-key or unique-key
column part. Nonunique indexes and expression-only key parts are excluded.

Rows are ordered deterministically by:

1. `TABLE_SCHEMA` using binary collation
2. `TABLE_NAME` using binary collation
3. primary-key constraints before unique constraints
4. first catalog row for the logical index
5. `ORDINAL_POSITION`

This ordering keeps `SELECT *` stable without adding general `ORDER BY` support
for information-schema queries.

## INFORMATION_SCHEMA.TABLES

`INFORMATION_SCHEMA.TABLES` must include
`TABLE_SCHEMA='information_schema'`, `TABLE_NAME='KEY_COLUMN_USAGE'` as a
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

`SHOW TABLES FROM information_schema` should expose `KEY_COLUMN_USAGE` through
the same system-view list, including `SHOW FULL TABLES FROM information_schema
LIKE 'key_column_usage'` returning `KEY_COLUMN_USAGE`, `SYSTEM VIEW`.

## Metadata Limitation

Existing `INFORMATION_SCHEMA` `SELECT` execution prepares SQLite-backed
statements directly and does not attach full MySQL field metadata for any
supported information-schema table. This first slice keeps that behavior
consistent: tests verify column names, values, row order, DDL side effects, and
case-insensitive resolution, while exact field descriptors for
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE` remain deferred to a unified
information-schema metadata pass.

## Composable Query Forms

The following MySQL-supported forms are covered by the shared system-view
`SELECT` path after the composable information-schema update:

- `SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- `SELECT ALL * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- `SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE CONSTRAINT_NAME = 'PRIMARY'`
- `SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE ORDER BY CONSTRAINT_NAME`
- `SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- table aliases
- qualified wildcards such as
  `SELECT INFORMATION_SCHEMA.KEY_COLUMN_USAGE.* FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- joins and expressions involving `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`

Unqualified `SELECT * FROM KEY_COLUMN_USAGE` is not part of the feature and
follows the ordinary selected-schema table-resolution path.

## Runtime And Storage Impact

This feature is read-only and requires no file-format change. Runtime execution
selects from the existing `__mylite_index_catalog`; DDL that already inserts,
deletes, or renames primary and unique index metadata automatically updates the
visible key-column rows. No mutable process-global state or new dependency is
needed.

The feature intentionally does not add a physical SQLite index or key-column
catalog. Broader optimizer use of indexes, CHECK constraints, foreign keys,
privilege filtering, temporary tables, and user views remain separate features.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- lower-case and mixed-case schema/table names
- quoted schema/table names
- explicit projection and filter forms remain parseable and are executable
  through the shared system-view path

Runtime coverage:

- empty database returns no rows with exact uppercase column names
- composite primary key, inline unique, named composite unique,
  prefix/descending unique, and nonunique index fixture returns exactly the
  primary and unique key-part rows and excludes the nonunique index
- row order follows the deterministic MyLite ordering documented above
- lower-case, mixed-case, and quoted table references execute
- `INFORMATION_SCHEMA.TABLES` exposes the `KEY_COLUMN_USAGE` system-view row
- `SHOW TABLES FROM information_schema LIKE 'key_column_usage'` and
  `SHOW FULL TABLES FROM information_schema LIKE 'key_column_usage'` expose the
  system view
- `DROP INDEX` removes unique key-column rows
- `ALTER TABLE ... RENAME INDEX` renames unique key-column rows when that DDL
  is supported by the current runtime
- `ALTER TABLE ... DROP PRIMARY KEY` removes primary key-column rows when that
  DDL is supported by the current runtime
- composable projections, `DISTINCT`/`ALL`, `WHERE`, `ORDER BY`, `LIMIT`,
  `COUNT(*)`, aliases, and qualified wildcard forms are covered by the shared
  system-view SELECT path

## Known Gaps

- CHECK constraints are omitted. MySQL does not expose CHECK constraints in
  `KEY_COLUMN_USAGE`.
- Foreign-key rows are omitted until MyLite has foreign-key catalogs,
  referential actions, enforcement, and catalog-backed
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` rows.
- Expression-only key parts are omitted until MyLite has functional index
  runtime/catalog support; even then `KEY_COLUMN_USAGE` should remain
  column-only.
- Privilege filtering and exact MySQL field metadata remain deferred. General
  projection, filtering, ordering, limiting, alias, and aggregate behavior is
  covered by the composable information-schema SELECT path.

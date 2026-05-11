# Baseline INFORMATION_SCHEMA Constraints

## Summary

This phase adds the next foundational metadata slice for common application and
ORM introspection: limited descriptor-backed rows for
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` and
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE`.

The supported rows describe only constraints MyLite already owns as catalog
descriptors:

- current single-column primary-key descriptors;
- current supported single-column unique-index descriptors.

The slice deliberately does not add new DDL, foreign keys, check constraints,
nonunique index rows, composite constraints, privilege filtering, or InnoDB
internal metadata. It exposes existing MyLite descriptors through MySQL-shaped
system views.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_constraints_expectations.sh`.

This specification is independently authored from the public documentation and
runtime observations above. It does not copy MySQL grammar or implementation
sources.

## Ownership Boundaries

- Public API: no new ABI. Applications use existing `mylite_execute()` and
  result accessors.
- Statement context: no new session state. Existing diagnostics and selected
  schema behavior for information-schema queries remain unchanged.
- Parser/AST: no grammar changes. Existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` parsing is reused.
- Analyzer/planner: the existing limited information-schema query planner
  resolves projections, aliases, predicates, ordering, and limits against
  synthetic table definitions.
- Catalog module: MyLite catalog descriptors remain authoritative. Rows are
  built from `mylite_catalog_index_descriptor`,
  `mylite_catalog_index_column_descriptor`, table descriptors, schema
  descriptors, and column descriptors. SQLite schema text is not consulted.
- Result builder: emits MySQL-shaped column labels and text/`NULL` values
  through the existing `mylite_result` conventions.
- Storage/VFS: no `.mylite` file-format or VFS change.
- SQLite physical storage: no SQLite fork patch or extension hook is required.
  This is a MyLite-owned synthetic metadata view.

## Supported Query Surface

This phase adds the two table names to the existing limited
information-schema query engine:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]

SELECT select_list
FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE [AS alias]
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
  information-schema support, grouping, aggregation beyond `COUNT(*)`, or
  mutation.

Predicates use the existing MyLite information-schema column collation policy.
MySQL 8.4.9 reports table/schema/catalog name columns in these two views with
binary metadata collation, while constraint and constrained-column name columns
use the case-insensitive `utf8mb3_tolower_ci` metadata collation.

No new Lemon grammar is required for this phase.

## `TABLE_CONSTRAINTS` Columns

`TABLE_CONSTRAINTS` has seven columns in this order:

| Column | MyLite value for supported rows |
| --- | --- |
| `CONSTRAINT_CATALOG` | `def` |
| `CONSTRAINT_SCHEMA` | logical schema name |
| `CONSTRAINT_NAME` | logical index/constraint name |
| `TABLE_SCHEMA` | logical schema name |
| `TABLE_NAME` | logical table name |
| `CONSTRAINT_TYPE` | `PRIMARY KEY` for primary descriptors, `UNIQUE` for unique secondary descriptors |
| `ENFORCED` | `YES` |

Column metadata follows MySQL 8.4.9 runtime observations:

- catalog/schema/table name columns are `varchar(64)`;
- `CONSTRAINT_NAME` is nullable metadata with `utf8mb3_tolower_ci`;
- `CONSTRAINT_TYPE` is `varchar(11)`, non-null, default empty string;
- `ENFORCED` is `varchar(3)`, non-null, default empty string.

Rows are emitted only for descriptor-owned primary and unique constraints on
persistent base tables. Nonunique secondary indexes have no rows.

## `KEY_COLUMN_USAGE` Columns

`KEY_COLUMN_USAGE` has twelve columns in this order:

| Column | MyLite value for supported rows |
| --- | --- |
| `CONSTRAINT_CATALOG` | `def` |
| `CONSTRAINT_SCHEMA` | logical schema name |
| `CONSTRAINT_NAME` | logical index/constraint name |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | logical schema name |
| `TABLE_NAME` | logical table name |
| `COLUMN_NAME` | constrained descriptor column name |
| `ORDINAL_POSITION` | `1` |
| `POSITION_IN_UNIQUE_CONSTRAINT` | `NULL` |
| `REFERENCED_TABLE_SCHEMA` | `NULL` |
| `REFERENCED_TABLE_NAME` | `NULL` |
| `REFERENCED_COLUMN_NAME` | `NULL` |

Column metadata follows MySQL 8.4.9 runtime observations:

- catalog/schema/table/name columns are `varchar(64)`;
- `CONSTRAINT_NAME`, `COLUMN_NAME`, and `REFERENCED_COLUMN_NAME` use
  `utf8mb3_tolower_ci` metadata;
- `ORDINAL_POSITION` and `POSITION_IN_UNIQUE_CONSTRAINT` are `int unsigned`;
- referenced table columns are nullable `varchar(64)`.

Rows are emitted only for descriptor-owned primary and unique constraints on
persistent base tables. Because this baseline admits only single-column primary
and unique descriptors, `ORDINAL_POSITION` is always `1`.

## Descriptor Semantics

Primary-key descriptors are exposed as:

- `CONSTRAINT_NAME = 'PRIMARY'`;
- `CONSTRAINT_TYPE = 'PRIMARY KEY'`;
- one `KEY_COLUMN_USAGE` row for the primary-key column.

Unique-index descriptors are exposed as:

- `CONSTRAINT_NAME = index.name`;
- `CONSTRAINT_TYPE = 'UNIQUE'`;
- one `KEY_COLUMN_USAGE` row for the unique-index column.

Nonunique secondary indexes are omitted. `STATISTICS` remains the metadata
surface for nonunique indexes.

The existing descriptor lifecycle determines visibility:

- `CREATE TABLE` primary and unique descriptors appear immediately after
  successful DDL;
- `CREATE TABLE ... LIKE` cloned primary and unique descriptors appear for the
  clone using the cloned logical names and target table/column descriptors;
- `CREATE TABLE ... SELECT` produces no key descriptors and therefore no rows;
- `RENAME TABLE` updates the logical table name visible through these views;
- `DROP TABLE` and `TRUNCATE TABLE` preserve existing descriptor behavior:
  dropped tables disappear, truncated tables retain constraints;
- close/reopen preserves rows through catalog descriptors;
- independent file-backed handles expose only their own descriptors.

## Information-Schema System Rows

Adding these table definitions also expands MyLite's existing
`information_schema` system-view metadata:

- `INFORMATION_SCHEMA.TABLES` includes `TABLE_CONSTRAINTS` and
  `KEY_COLUMN_USAGE` as `SYSTEM VIEW` rows;
- `INFORMATION_SCHEMA.COLUMNS` includes rows for each column in both new
  system views;
- system rows use the same fixed placeholders as existing MyLite
  information-schema system views.

This phase does not add `TABLE_CONSTRAINTS_EXTENSIONS`,
`CHECK_CONSTRAINTS`, `REFERENTIAL_CONSTRAINTS`, or InnoDB internal constraint
views.

## Ordering

No natural row order is claimed. Tests and compatibility expectations must use
explicit `ORDER BY` for user-visible row ordering.

When MyLite internally iterates descriptors, it may use catalog order, but that
is not part of the compatibility contract. Runtime tests should assert content
through explicit ordering by schema, table, constraint, and ordinal position.

## Diagnostics

The feature reuses existing information-schema diagnostics:

- unknown information-schema table: MySQL-compatible `1109 / 42S02`;
- unknown projected column: MySQL-compatible `1054 / 42S22`;
- unknown `WHERE` or `ORDER BY` column: existing context-specific `1054 /
  42S22`;
- unsupported query shape: existing deterministic MyLite unsupported syntax or
  runtime diagnostic;
- allocation failure: `MYLITE_NOMEM` with handle diagnostics;
- descriptor corruption or stale index-column mapping: deterministic runtime
  error.

No warnings are introduced by successful queries.

## Performance

Rows are synthesized from MyLite descriptors on demand. This is appropriate for
baseline metadata because catalog descriptor sets are expected to be small and
the existing information-schema engine already materializes rows before
projection/filtering.

The implementation must not inspect `sqlite_schema`, `PRAGMA` output, or
physical index SQL. It should reuse existing descriptor loaders and keep all
physical names internal.

## Tests

Add a focused runtime test, preferably
`libmylite.runtime.information_schema_constraints`, covering:

- exact `TABLE_CONSTRAINTS` rows for primary and unique descriptors;
- exact `KEY_COLUMN_USAGE` rows for primary and unique descriptors;
- nonunique secondary indexes omitted from both views;
- `COUNT(*)`, explicit projection, aliases, case-insensitive metadata
  predicates, numeric predicate coercion, `ORDER BY`, and `LIMIT` through the
  existing information-schema query engine;
- system `TABLES` and `COLUMNS` rows for the new information-schema views;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, rename, truncate, drop,
  reopen, and independent file-backed handles;
- unknown table and unknown column diagnostics;
- no row result warnings for successful queries.

Add and run a MySQL 8.4.9 expectation script for the same user-visible rows and
diagnostics.

## Compatibility Gaps

Deferred:

- foreign-key rows and referenced-column metadata;
- check-constraint rows and `ENFORCED` variants for checks;
- composite primary/unique constraints;
- named constraints distinct from index names;
- constraints on string unique keys, because string unique keys are deferred;
- temporary tables, views, privileges, hidden generated columns, storage-engine
  internal views, and Performance Schema/sys-schema alternatives;
- complete information-schema SQL support beyond the existing limited query
  engine.

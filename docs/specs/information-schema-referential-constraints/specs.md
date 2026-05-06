# INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS

## Scope

This feature adds the first executable MyLite slice for MySQL's foreign-key
constraint metadata table:

- `SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`

MyLite does not yet have executable foreign-key DDL, a foreign-key catalog,
referential-action storage, or referential enforcement. This slice therefore
exposes the MySQL-compatible `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` table
shape as a read-only system view and returns zero rows for every current MyLite
database. Rows for actual foreign keys are intentionally deferred until MyLite
has foreign-key DDL, catalog storage, supporting-index dependency checks,
referential validation, cascading actions, and DML enforcement.

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`. Broader projections, filters,
aliases, ordering, limits, and aggregates are handled by the composable
information-schema system-view path where the corresponding `SELECT` feature is
implemented.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA`
  `REFERENTIAL_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- MySQL 8.4 Reference Manual, constraint information-schema tables:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-information-schema.html
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` reports foreign-key constraints.
The table has exactly these columns in order:

1. `CONSTRAINT_CATALOG`
2. `CONSTRAINT_SCHEMA`
3. `CONSTRAINT_NAME`
4. `UNIQUE_CONSTRAINT_CATALOG`
5. `UNIQUE_CONSTRAINT_SCHEMA`
6. `UNIQUE_CONSTRAINT_NAME`
7. `MATCH_OPTION`
8. `UPDATE_RULE`
9. `DELETE_RULE`
10. `TABLE_NAME`
11. `REFERENCED_TABLE_NAME`

For a foreign key, MySQL reports the constraint catalog as `def`, the
constraint schema and table name for the child table, the referenced unique
constraint catalog as `def`, the referenced unique constraint schema and name,
the match option, the update and delete rules, and the referenced table name.

Column metadata observed in MySQL 8.4.9 for an empty result:

| Column | Type | Collation | Length | Flags |
| --- | --- | --- | ---: | --- |
| `CONSTRAINT_CATALOG` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL UNIQUE_KEY BINARY NO_DEFAULT_VALUE PART_KEY` |
| `CONSTRAINT_SCHEMA` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE PART_KEY` |
| `CONSTRAINT_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | none |
| `UNIQUE_CONSTRAINT_CATALOG` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL MULTIPLE_KEY BINARY NO_DEFAULT_VALUE PART_KEY` |
| `UNIQUE_CONSTRAINT_SCHEMA` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE PART_KEY` |
| `UNIQUE_CONSTRAINT_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | none |
| `MATCH_OPTION` | `STRING` | `latin1_swedish_ci` | 7 | `NOT_NULL BINARY ENUM NO_DEFAULT_VALUE` |
| `UPDATE_RULE` | `STRING` | `latin1_swedish_ci` | 11 | `NOT_NULL BINARY ENUM NO_DEFAULT_VALUE` |
| `DELETE_RULE` | `STRING` | `latin1_swedish_ci` | 11 | `NOT_NULL BINARY ENUM NO_DEFAULT_VALUE` |
| `TABLE_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE PART_KEY` |
| `REFERENCED_TABLE_NAME` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL BINARY NO_DEFAULT_VALUE PART_KEY` |

Runtime probes:

- A schema containing parent and child tables without a foreign key returned
  zero rows for `REFERENTIAL_CONSTRAINTS`.
- `SHOW FULL TABLES FROM information_schema LIKE 'referential_constraints'`
  returned `REFERENTIAL_CONSTRAINTS`, `SYSTEM VIEW`.
- A table with
  `CONSTRAINT fk_parent FOREIGN KEY (parent_id) REFERENCES parent(id) ON UPDATE CASCADE ON DELETE SET NULL`
  returned one `REFERENTIAL_CONSTRAINTS` row:

| CONSTRAINT_CATALOG | CONSTRAINT_SCHEMA | CONSTRAINT_NAME | UNIQUE_CONSTRAINT_CATALOG | UNIQUE_CONSTRAINT_SCHEMA | UNIQUE_CONSTRAINT_NAME | MATCH_OPTION | UPDATE_RULE | DELETE_RULE | TABLE_NAME | REFERENCED_TABLE_NAME |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `def` | `mylite_ref_rows_codex` | `fk_parent` | `def` | `mylite_ref_rows_codex` | `PRIMARY` | `NONE` | `CASCADE` | `SET NULL` | `child` | `parent` |

The actual foreign-key row above is future MyLite behavior, not part of this
first slice.

## MyLite Behavior

Supported executable query:

```sql
SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
```

The schema and table identifiers are resolved case-insensitively, including
quoted qualified forms such as:

```sql
SELECT * FROM `information_schema`.`REFERENTIAL_CONSTRAINTS`
```

The result columns are exactly:

1. `CONSTRAINT_CATALOG`
2. `CONSTRAINT_SCHEMA`
3. `CONSTRAINT_NAME`
4. `UNIQUE_CONSTRAINT_CATALOG`
5. `UNIQUE_CONSTRAINT_SCHEMA`
6. `UNIQUE_CONSTRAINT_NAME`
7. `MATCH_OPTION`
8. `UPDATE_RULE`
9. `DELETE_RULE`
10. `TABLE_NAME`
11. `REFERENCED_TABLE_NAME`

The query returns no rows because no foreign-key catalog exists yet. This is a
deliberate empty static system view, not a placeholder catalog and not fake
metadata.

`INFORMATION_SCHEMA.TABLES` exposes `REFERENTIAL_CONSTRAINTS` with
`TABLE_SCHEMA='information_schema'`,
`TABLE_NAME='REFERENTIAL_CONSTRAINTS'`, and `TABLE_TYPE='SYSTEM VIEW'`. The
existing `SHOW TABLES FROM information_schema` and
`SHOW FULL TABLES FROM information_schema` inventory also exposes
`REFERENTIAL_CONSTRAINTS`; `SHOW FULL TABLES FROM information_schema LIKE
'referential_constraints'` returns `REFERENTIAL_CONSTRAINTS`, `SYSTEM VIEW`.

### Composable Query Shapes

The following forms are covered by the shared system-view `SELECT` path after
the composable information-schema update:

- `SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
- `SELECT ALL * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
- `SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS WHERE CONSTRAINT_NAME = 'fk_parent'`
- `SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS ORDER BY CONSTRAINT_NAME`
- `SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
- `SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS AS rc`
- `SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS rc`
- `SELECT INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS.* FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
- joins involving `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`

Unqualified `SELECT * FROM REFERENTIAL_CONSTRAINTS` is not part of this
feature.

## Grammar

No new SELECT grammar is needed. The existing qualified table-reference grammar
must continue accepting ordinary, mixed-case, and quoted identifiers for this
information-schema table. The supported surface can be described with this
MyLite-authored Lemon-style snippet:

```lemon
information_schema_wildcard_select ::= SELECT STAR FROM qualified_table_name.

qualified_table_name ::= identifier DOT identifier.
```

Runtime validation narrows the accepted parsed statement to
`information_schema.referential_constraints`, an unqualified wildcard
projection, no explicit duplicate modifier, no alias, and no additional SELECT
clauses.

## Storage And Runtime

This feature must not add `__mylite_foreign_key_catalog`,
`__mylite_referential_constraint_catalog`, or any other foreign-key metadata
table. Runtime lowering should use a static SQLite statement with the correct
column aliases and an always-false row predicate.

When foreign-key support is implemented later, the foreign-key catalog should
be designed once for DDL validation, dependency checks, DML enforcement,
referential actions, `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`, foreign-key
rows in `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and referenced-column rows in
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE`. This empty static view should then be
replaced by a catalog-backed query.

Existing `INFORMATION_SCHEMA` `SELECT` execution prepares SQLite-backed
statements directly and does not attach full MySQL field metadata for any
supported information-schema table. This slice keeps that behavior consistent:
tests verify column names, zero-row behavior, case-insensitive resolution, and
system-view inventory while exact field descriptors remain deferred to a
unified information-schema metadata pass.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
- lower-case and mixed-case qualified names
- quoted qualified name
- projection and `WHERE` forms parse successfully for runtime rejection

Runtime coverage:

- empty database returns zero rows with the exact eleven uppercase column names
- creating normal parent and child tables without foreign keys still returns
  zero rows
- lower-case, mixed-case, and quoted table references execute
- `INFORMATION_SCHEMA.TABLES` exposes the `REFERENTIAL_CONSTRAINTS`
  system-view row
- `SHOW TABLES FROM information_schema LIKE 'referential_constraints'` exposes
  the system view
- `SHOW FULL TABLES FROM information_schema LIKE 'referential_constraints'`
  returns `REFERENTIAL_CONSTRAINTS`, `SYSTEM VIEW`
- composable projections, `DISTINCT`/`ALL`, `WHERE`, `ORDER BY`, `LIMIT`,
  `COUNT(*)`, aliases, and qualified wildcard forms are covered by the shared
  system-view SELECT path
- unsupported table-level `CREATE TABLE ... FOREIGN KEY ...` DDL parses but
  must not create `REFERENTIAL_CONSTRAINTS` rows
- inline `CREATE TABLE` column `REFERENCES` clauses parse and are ignored like
  the verified MySQL 8.4.9 behavior; they must not create foreign-key metadata

## Known Gaps

- Actual foreign-key rows are omitted until MyLite has foreign-key DDL, a
  foreign-key catalog, referenced-index validation, dependency checks,
  referential actions, and DML enforcement.
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` still omits `FOREIGN KEY` rows until
  the same underlying foreign-key catalog exists.
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` still omits foreign-key referenced
  table and column rows until the same underlying foreign-key catalog exists.
- Privilege filtering and exact MySQL field metadata remain deferred. General
  projection, filtering, ordering, limiting, alias, and aggregate behavior is
  covered by the composable information-schema SELECT path.

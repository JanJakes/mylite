# Foreign Key Support

## Scope

This feature implements MySQL-compatible foreign-key definition, metadata,
validation, dependency checks, and referential enforcement for supported
MyLite base tables.

The work is intentionally split into reviewable slices:

- persistent catalog storage shared by DDL, metadata, and DML enforcement
- table-level `CREATE TABLE ... FOREIGN KEY` definition support
- `ALTER TABLE ... ADD/DROP FOREIGN KEY`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `REFERENTIAL_CONSTRAINTS` catalog-backed rows
- `SHOW CREATE TABLE` rendering
- insert/update child-row checks
- update/delete parent-row checks and referential actions
- DDL dependency restrictions for parent indexes, parent tables, renames, and
  truncation

Out of scope for the first implementation slices:

- views and triggers
- partitioned tables
- generated-column restrictions beyond existing column metadata
- storage-engine internals that are not observable through MySQL SQL/API
  behavior
- privilege filtering

## Sources

- MySQL 8.4 Reference Manual, Foreign Key Constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

Observed table-level definition:

```sql
CREATE TABLE parent(id INT PRIMARY KEY, code INT UNIQUE, payload VARCHAR(20));
CREATE TABLE child(
  id INT PRIMARY KEY,
  parent_id INT,
  code_ref INT,
  CONSTRAINT fk_parent
    FOREIGN KEY (parent_id) REFERENCES parent(id)
    ON UPDATE CASCADE ON DELETE SET NULL,
  FOREIGN KEY idx_code_ref (code_ref) REFERENCES parent(code)
    ON DELETE RESTRICT ON UPDATE NO ACTION
);
```

MySQL creates:

- `TABLE_CONSTRAINTS` rows for `fk_parent`, the generated
  `child_ibfk_1`, primary keys, and the parent unique key
- `KEY_COLUMN_USAGE` rows for the FK child columns with
  `POSITION_IN_UNIQUE_CONSTRAINT = 1` and referenced table/column names
- `REFERENTIAL_CONSTRAINTS` rows with `MATCH_OPTION = NONE`, `UPDATE_RULE`,
  `DELETE_RULE`, `UNIQUE_CONSTRAINT_NAME`, child table, and referenced table
- supporting child indexes named `fk_parent` and `idx_code_ref`
- `SHOW CREATE TABLE` lines for the child indexes followed by the two
  `CONSTRAINT ... FOREIGN KEY ... REFERENCES ...` lines

Observed rule and error details:

- A successful `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY ...` over two
  existing matching child rows reported affected rows `2`.
- Adding a foreign key over existing unmatched child rows with
  `foreign_key_checks = 1` failed with error 1452 / SQLSTATE `23000` and did
  not add metadata.
- Adding the same shape with `foreign_key_checks = 0` succeeded without
  scanning existing rows and reported affected rows `0`.
- Re-enabling `foreign_key_checks` does not retroactively validate old rows,
  but later unmatched child inserts fail with 1452.
- Dropping a foreign key reports affected rows `0` and leaves the supporting
  child index in place.
- Updating or deleting parent rows that are referenced by `RESTRICT` or
  `NO ACTION` constraints fails with error 1451 / SQLSTATE `23000`.
- `CASCADE`, `SET NULL`, and default `NO ACTION` rules are reported in
  `REFERENTIAL_CONSTRAINTS`; `SHOW CREATE TABLE` omits explicit default
  `NO ACTION`.

## MyLite Design

MyLite stores one catalog row per foreign-key column part. The same catalog
must drive metadata, validation, and enforcement so DDL and DML cannot drift.

Required catalog fields:

- constraint catalog/schema/name
- child table schema/name
- child column name and ordinal position
- supporting child index name
- referenced table schema/name
- referenced column name and position in the referenced unique constraint
- referenced unique constraint name
- match option
- update and delete rules

Foreign keys apply only to supported persistent base tables in the first
runtime slices. Temporary table behavior must be verified separately and
implemented with explicit MySQL-compatible diagnostics before it is marked
supported.

## DDL Semantics

Table-level `CREATE TABLE ... FOREIGN KEY` should:

- copy the source-complete AST into the create-table plan
- generate the MySQL-style constraint name when omitted
- create or reuse the supporting child index
- validate child columns
- validate the parent table and referenced columns when `foreign_key_checks`
  is enabled
- record metadata atomically with the table, columns, and indexes

`ALTER TABLE ... ADD FOREIGN KEY` should additionally validate existing child
rows when `foreign_key_checks` is enabled and use MySQL affected-row semantics
for the verified rebuild shape.

`ALTER TABLE ... DROP FOREIGN KEY` removes only the foreign-key catalog rows.
It must not remove the supporting child index.

## DML Semantics

When `foreign_key_checks` is enabled:

- inserting or updating a child row with non-`NULL` FK columns requires a
  matching parent unique row
- updating referenced parent key columns must apply the FK's `ON UPDATE` rule
- deleting parent rows must apply the FK's `ON DELETE` rule
- `RESTRICT` and `NO ACTION` reject the parent mutation when matching child
  rows exist
- `CASCADE` updates or deletes matching child rows
- `SET NULL` sets all child FK columns to `NULL`
- `SET DEFAULT` remains unsupported until MySQL-observed behavior is designed
  against MyLite default-expression support

When `foreign_key_checks` is disabled, MySQL skips foreign-key validation and
referential actions for covered DML. Existing rows are not retroactively
validated when checks are re-enabled.

## Metadata

The foreign-key catalog backs:

- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` `FOREIGN KEY` rows
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` referenced-table and referenced-column
  rows
- `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` rows
- `SHOW CREATE TABLE` foreign-key lines
- dependency checks for parent unique indexes

Ordering must follow observed MySQL behavior for each metadata surface. Existing
primary-key, unique-key, and CHECK rows must retain their ordering.

## Diagnostics

Required diagnostics include:

| Case | Error |
| --- | --- |
| child-row insert/update violation | 1452 / `23000` |
| parent-row update/delete violation | 1451 / `23000` |
| missing dropped foreign key | 1091 / `42000` |
| dropping an index needed by a foreign key | 1553 / `HY000` |
| malformed or invalid FK definition | MySQL-compatible validation error where verified |

Exact messages should include the schema, table, constraint name, child columns,
referenced table, referenced columns, and referential actions where MySQL
exposes them.

## Test Requirements

Each completed slice must add runtime tests whose expected behavior was
verified against MySQL 8.4.9. Tests must cover result rows, result metadata
where available, diagnostics, affected rows, row side effects, metadata side
effects, and `foreign_key_checks` interactions.


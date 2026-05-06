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
- DDL dependency restrictions for child/parent indexes, parent/child tables,
  renames, truncation, and FK columns

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
- Dropping a child supporting index, a referenced parent `PRIMARY` index, or a
  referenced parent unique index while the foreign key exists fails with error
  1553, including when `foreign_key_checks=0`; an `AUTO_INCREMENT` primary key
  still fails first with error 1075.
- Dropping a parent table referenced by a surviving child table fails with
  error 3730 while `foreign_key_checks=1`. Dropping parent and child together
  succeeds, and dropping the parent while `foreign_key_checks=0` leaves child
  foreign-key metadata in place.
- Renaming a parent table rewrites the referenced table schema/name in child
  foreign-key metadata. Renaming a child table rewrites the child table
  schema/name and regenerates constraint names that match `old_table_ibfk_N`.
- Updating or deleting parent rows that are referenced by `RESTRICT` or
  `NO ACTION` constraints fails with error 1451 / SQLSTATE `23000`.
- `CASCADE`, `SET NULL`, and default `NO ACTION` rules are reported in
  `REFERENTIAL_CONSTRAINTS`; `SHOW CREATE TABLE` omits explicit default
  `NO ACTION`.
- `SET DEFAULT` rules are accepted and reported in `SHOW CREATE TABLE` and
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`, but parent updates/deletes are
  rejected with error 1451 in the observed InnoDB behavior rather than applying
  child-column defaults.
- Foreign keys on temporary tables are rejected with error 1215, `Cannot add
  foreign key constraint`. A persistent table that references a temporary table
  does not resolve that temporary table as a parent and fails with error 1824,
  `Failed to open the referenced table 'name'`.

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

The first implemented runtime slices record table-level `CREATE TABLE`
foreign-key definitions for persistent base tables and enforce child-row
checks for supported insert-like DML. Definition support copies the constraint
shape into the create-table plan, generates MySQL-style unnamed constraint
names, creates or reuses supporting child indexes, validates the referenced
unique or primary key when the parent table exists, allows missing referenced
tables only while `foreign_key_checks=0`, and writes catalog-backed
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, `REFERENTIAL_CONSTRAINTS`, and
`SHOW CREATE TABLE` output. Insert enforcement covers `INSERT ... VALUES`,
`INSERT ... SET`, `INSERT ... SELECT FROM DUAL`, insert and update branches of
`ON DUPLICATE KEY UPDATE`, `INSERT IGNORE`, and `REPLACE`. It skips checks when
`foreign_key_checks=0`,
does not retroactively validate old rows when checks are re-enabled, accepts
rows where any child foreign-key column is `NULL`, treats simple self-references
to the candidate row as satisfied, and demotes unmatched child rows to warning
1452 for `INSERT IGNORE`. Child-row update enforcement covers supported
single-table and joined `UPDATE` paths, validates only constraints whose child
columns changed, and allows unrelated updates to orphaned child rows that were
created while `foreign_key_checks=0`. Parent-side `RESTRICT`, `NO ACTION`, and
`SET DEFAULT` enforcement rejects supported parent key updates, parent deletes,
multi-table deletes, self-referential parent mutations, and `REPLACE` conflict
deletes with error 1451 while `foreign_key_checks` is enabled. Direct parent
updates apply `ON UPDATE CASCADE` and `ON UPDATE SET NULL` to matching child
rows for supported single-table and joined `UPDATE` paths. Direct parent
deletes and `REPLACE` conflict deletes apply `ON DELETE CASCADE` and
`ON DELETE SET NULL` to matching child rows for supported paths. Covered
parent update and delete actions are skipped while `foreign_key_checks=0`.
Temporary child foreign-key definitions are rejected with a deterministic
`Cannot add foreign key constraint` diagnostic, and persistent child
definitions do not resolve temporary parent tables.
FK-only `ALTER TABLE ... ADD FOREIGN KEY` persists catalog metadata, creates or
reuses the supporting child index, validates existing child rows while
`foreign_key_checks=1`, skips existing-row validation while
`foreign_key_checks=0`, and permits missing referenced tables only while checks
are disabled. FK-only `ALTER TABLE ... DROP FOREIGN KEY` removes only
foreign-key metadata and leaves the supporting child index in place.
`DROP INDEX` and `ALTER TABLE ... DROP INDEX` / `DROP PRIMARY KEY` reject
child supporting indexes and parent primary/unique indexes required by foreign
keys with error 1553, even while `foreign_key_checks=0`. `RENAME TABLE` and
`ALTER TABLE ... RENAME` rewrite foreign-key catalog metadata for child and
parent tables, including cross-schema moves and MySQL-style regenerated
`old_table_ibfk_N` child constraint names. Recursive referential actions,
executable mixed-action ALTER FK operations, and exact parent-update optimizer
ordering remain follow-up slices. `DROP TABLE`
rejects parent tables referenced by surviving child tables while
`foreign_key_checks=1`, permits multi-table drops that remove the parent and
all referencing children together, and allows checks-off parent drops while
preserving child foreign-key metadata.

## DDL Semantics

Table-level `CREATE TABLE ... FOREIGN KEY` should:

- copy the source-complete AST into the create-table plan
- generate the MySQL-style constraint name when omitted
- create or reuse the supporting child index
- validate child columns
- validate the parent table and referenced columns when `foreign_key_checks`
  is enabled
- record metadata atomically with the table, columns, and indexes

FK-only `ALTER TABLE ... ADD FOREIGN KEY` additionally validates existing child
rows when `foreign_key_checks` is enabled, skips that scan when checks are
disabled, and reports the child table row count as affected rows for supported
metadata-only shapes.
Mixed FK actions with supported column/index actions are rejected before
mutation, so no partial column, index, or foreign-key metadata changes survive.
Executable mixed FK actions remain deferred until FK catalog rewrites
participate in the shadow-table ALTER model.

`ALTER TABLE ... DROP FOREIGN KEY` removes only the foreign-key catalog rows.
It must not remove the supporting child index.

`DROP INDEX` and `ALTER TABLE ... DROP INDEX` / `DROP PRIMARY KEY` must reject
indexes that are still required by a foreign key. MyLite enforces this for
child supporting indexes and parent primary/unique indexes recorded in the
foreign-key catalog.

`DROP TABLE` validates parent-table dependencies before mutating storage when
`foreign_key_checks` is enabled. A referenced parent table can be dropped only
when every cataloged child table that references it is also an existing
persistent target of the same `DROP TABLE` statement. When checks are disabled,
MyLite follows MySQL by allowing the parent drop and keeping the child
foreign-key catalog rows, so future checked child writes still fail until a
matching parent table is restored or the foreign key is dropped.

`TRUNCATE TABLE` validates parent-table dependencies before mutating rows when
`foreign_key_checks` is enabled. A referenced parent table is rejected with
error 1701 unless the only referencing foreign key is self-referential.
Truncating a child table succeeds. When checks are disabled, MyLite follows
MySQL by allowing referenced parent truncation while keeping child rows and
foreign-key metadata, so future checked child writes still fail until matching
parent rows are restored.

`ALTER TABLE` column operations validate foreign-key dependencies before a
shadow-table rebuild becomes visible. Dropping a child foreign-key column fails
with error 1828, and dropping a referenced parent column fails with error 1829.
Renaming a child or referenced parent column through `RENAME COLUMN` or a
type-compatible `CHANGE COLUMN` rewrites `column_name` and
`referenced_column_name` in the foreign-key catalog. `CHANGE` and `MODIFY`
forms that would make the child and parent column definitions incompatible fail
with error 3780, even while `foreign_key_checks=0`.

`RENAME TABLE` and `ALTER TABLE ... RENAME` rewrite catalog-backed foreign-key
metadata in the same statement-atomic transaction as the physical table and
table/index metadata rename. Child renames update `constraint_schema`,
`table_schema`, `table_name`, and generated-pattern constraint names. Parent
renames update `unique_constraint_schema`, `referenced_table_schema`, and
`referenced_table_name`.

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
- `SET DEFAULT` is metadata-preserved and rejects matching parent mutations
  with error 1451, matching MySQL 8.4.9 InnoDB behavior observed for covered
  parent update/delete paths
- self-referential `ON UPDATE CASCADE` and `ON UPDATE SET NULL` parent key
  mutations reject with 1451 in the covered MySQL 8.4.9-observed shape rather
  than recursively applying the action

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
- dependency checks for child supporting indexes and parent primary/unique
  indexes

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
| dropping a child foreign-key column | 1828 / `HY000` |
| dropping a referenced parent column | 1829 / `HY000` |
| truncating a parent table referenced by a non-self foreign key | 1701 / `42000` |
| dropping a parent table referenced by a surviving child | 3730 / `HY000` |
| incompatible child/parent foreign-key column definitions | 3780 / `HY000` |
| malformed or invalid FK definition | MySQL-compatible validation error where verified |

Exact messages should include the schema, table, constraint name, child columns,
referenced table, referenced columns, and referential actions where MySQL
exposes them.

## Test Requirements

Each completed slice must add runtime tests whose expected behavior was
verified against MySQL 8.4.9. Tests must cover result rows, result metadata
where available, diagnostics, affected rows, row side effects, metadata side
effects, and `foreign_key_checks` interactions.

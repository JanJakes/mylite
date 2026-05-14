# Baseline Foreign Key Constraints

## Summary

This phase adds the first descriptor-owned foreign-key slice for persistent
MyLite base tables. It targets common schema DDL and basic referential
integrity:

```sql
CREATE TABLE child (
  id INT NOT NULL PRIMARY KEY,
  parent_id INT,
  CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) REFERENCES parent(id)
);
```

The slice is deliberately narrow. It supports one-column integer-family foreign
keys with default `RESTRICT` / `NO ACTION` behavior, metadata, and enforcement
for the current descriptor-driven DML paths that can safely participate. It
does not add cascades, `SET NULL`, composite keys, mutable
`foreign_key_checks`, temporary-table foreign keys, self-referential edge cases,
or arbitrary `ALTER TABLE` combinations.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Existing primary-key, unique-index, nonunique-index, and information-schema
  constraint implementations in `packages/libmylite/src/runtime/`
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_foreign_key_constraints_expectations.sh`
records runtime probes for this feature. Observed behavior that shapes this
slice:

- A foreign key requires a unique referenced parent key. A nonunique parent
  index is rejected with `6125 / HY000`.
- Referencing and referenced columns must be compatible. `INT` referencing
  `BIGINT` is rejected with `3780 / HY000`.
- Nullable child values are accepted when the child key part is `NULL`.
- MySQL creates or reuses a child-side index. If an explicit child index exists
  on the key column, it is reused. Otherwise, MySQL creates a nonunique child
  index using the constraint name for named constraints or the column name for
  unnamed constraints.
- Unnamed `CREATE TABLE` foreign keys get generated constraint names like
  `<table>_ibfk_1`.
- `SHOW CREATE TABLE` renders child indexes before `CONSTRAINT ... FOREIGN KEY`
  lines.
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes `FOREIGN KEY` rows with
  `ENFORCED = 'YES'`.
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes the child column plus
  `POSITION_IN_UNIQUE_CONSTRAINT = 1` and referenced schema/table/column names.
- `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` exposes
  `UNIQUE_CONSTRAINT_NAME`, `MATCH_OPTION = 'NONE'`, and default
  `UPDATE_RULE = DELETE_RULE = 'NO ACTION'`.
- Inserting or updating a child row to a missing non-`NULL` parent value fails
  with `1452 / 23000`.
- `INSERT IGNORE` on a child row with a missing non-`NULL` parent value skips
  that row, reports `ROW_COUNT() = 0`, and records warning `1452 / 23000`.
- Deleting or updating a referenced parent key value while matching child rows
  exist fails with `1451 / 23000`.
- `ON DELETE CASCADE` and `ON DELETE SET NULL` are valid MySQL forms, but they
  have additional row-write semantics that are deferred by this MyLite slice.

## Scope

Supported DDL:

- persistent base tables only;
- table-level `FOREIGN KEY (child_column) REFERENCES parent_table(parent_column)`
  in `CREATE TABLE`;
- optional `CONSTRAINT constraint_name` before the table-level foreign key;
- single-action `ALTER TABLE child ADD CONSTRAINT constraint_name FOREIGN KEY
  (child_column) REFERENCES parent_table(parent_column)`;
- single-column child and parent key parts only;
- child and parent columns must be existing descriptor-owned integer-family
  columns with exactly compatible logical integer type and signedness;
- parent column must be the first and only part of an existing primary or unique
  descriptor key with no prefix length;
- child column may be nullable;
- omitted child index creates a descriptor-owned nonunique secondary index;
- existing child index whose first key part is the child column is reused;
- default `MATCH SIMPLE` behavior only, represented as `MATCH_OPTION = 'NONE'`;
- default `RESTRICT` / `NO ACTION` behavior only, represented in metadata as
  `NO ACTION`.

Supported metadata:

- `SHOW CREATE TABLE` renders foreign-key lines from MyLite descriptors;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` adds `FOREIGN KEY` rows;
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` fills referenced columns for FK rows;
- `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` becomes queryable for supported
  FK descriptors;
- `CREATE TABLE ... LIKE` follows the current MyLite clone policy and does not
  copy FK descriptors;
- `CREATE TABLE ... SELECT` does not copy foreign keys.

Supported enforcement:

- child-side `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE child SET child_column = value`
  check non-`NULL` child values against the parent descriptor table before
  completing;
- `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` skip child rows whose
  non-`NULL` FK value is missing from the parent key and append a
  MySQL-compatible warning;
- parent-side single-table `DELETE` and `UPDATE parent SET parent_column = ...`
  reject changes that would leave matching child rows orphaned;
- `TRUNCATE` rejects parent tables referenced by FK descriptors and child
  tables with FK descriptors until MySQL's full truncate dependency behavior is
  separately specified;
- `DROP TABLE` rejects referenced parent tables while child FK descriptors
  exist, and drops child FK descriptors when dropping the child table;
- `DROP PRIMARY KEY`, `DROP INDEX`, and column-rebuild `ALTER TABLE` paths
  reject operations that would remove or change a referenced parent key or a
  child key column;
- ordinary statement enforcement is implemented with set-based SQLite probes
  generated by MyLite; `INSERT IGNORE` uses descriptor-built parent-key probes
  per proposed row so violating rows can be skipped with MySQL-compatible
  warnings.

Out of scope:

- inline column `REFERENCES`;
- unnamed `ALTER TABLE ... ADD FOREIGN KEY` forms;
- `ALTER TABLE ... ADD FOREIGN KEY` without `CONSTRAINT name`;
- `ALTER TABLE ... DROP FOREIGN KEY`;
- `ON UPDATE` / `ON DELETE` actions other than the default;
- `CASCADE`, `SET NULL`, `SET DEFAULT`, action timing, and trigger-like effects;
- `MATCH` clauses, composite keys, prefix keys, functional keys, descending
  keys, generated columns, invisible columns, and non-integer keys;
- self-referential foreign keys;
- references across schemas in this first slice;
- temporary tables, views, partitioned tables, storage-engine variants, and
  privilege semantics;
- mutable `foreign_key_checks` and `SET foreign_key_checks = 0`;
- `INSERT ... SELECT`, `REPLACE ... SELECT`,
  `INSERT ... ON DUPLICATE KEY UPDATE`, key-bearing `REPLACE`, and multi-table
  DML on FK-related tables until their FK semantics are specified;
- native SQLite foreign-key enforcement as the user-visible authority.

## Ownership Boundary

- Public API: no public ABI changes. Applications continue through
  `mylite_execute()` and existing result handles.
- Statement context: owns diagnostics, warning counts, affected rows,
  `ROW_COUNT()`, and failure cleanup for FK DDL/DML.
- Lexer/parser/AST: admits the narrow table-level FK syntax and carries the
  child columns, parent table, parent columns, optional constraint name, and
  action tokens. It does not inspect descriptors or SQLite schema text.
- Analyzer/planner: resolves schemas, tables, columns, parent keys, child
  indexes, and dependency checks from MyLite descriptors. It rejects unsupported
  forms before any physical SQLite SQL is generated.
- Catalog: owns durable foreign-key descriptors, descriptor versions,
  generation, and cache invalidation. FK descriptors are MyLite metadata, not
  reflections of SQLite `PRAGMA foreign_key_list`.
- Result and introspection builders: render `SHOW CREATE TABLE` and
  information-schema rows from descriptors only.
- SQLite physical storage: stores user rows and generated child indexes.
  MyLite uses prepared SQLite statements for set-based FK checks and binds
  values as parameters.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Syntax

MyLite Lemon-syntax sketch for the admitted grammar:

```lemon
create_table_item ::= column_definition.
create_table_item ::= primary_key_definition.
create_table_item ::= secondary_index_definition.
create_table_item ::= unique_index_definition.
create_table_item ::= foreign_key_definition.

foreign_key_definition ::=
    constraint_name_opt FOREIGN KEY LPAREN foreign_key_part_list RPAREN
    REFERENCES table_name LPAREN foreign_key_part_list RPAREN.

constraint_name_opt ::= .
constraint_name_opt ::= CONSTRAINT identifier.

foreign_key_part_list ::= identifier.

alter_table_add_foreign_key_statement ::=
    ALTER TABLE table_name ADD CONSTRAINT identifier FOREIGN KEY
    LPAREN foreign_key_part_list RPAREN REFERENCES table_name
    LPAREN foreign_key_part_list RPAREN.
```

Runtime rejects admitted-but-deferred forms such as multiple key parts, missing
explicit constraint names in `ALTER TABLE`, reference actions, `MATCH` clauses,
and schema-qualified parent references outside the current schema.

## Catalog Design

The catalog schema advances to the next schema version and adds two descriptor
tables:

```sql
_mylite_catalog_foreign_keys(
  foreign_key_id INTEGER PRIMARY KEY,
  child_table_id INTEGER NOT NULL,
  parent_table_id INTEGER NOT NULL,
  name TEXT NOT NULL,
  parent_index_id INTEGER NOT NULL,
  child_index_id INTEGER NOT NULL,
  update_rule TEXT NOT NULL,
  delete_rule TEXT NOT NULL,
  match_option TEXT NOT NULL,
  descriptor_version INTEGER NOT NULL,
  created_catalog_generation INTEGER NOT NULL,
  updated_catalog_generation INTEGER NOT NULL,
  UNIQUE(child_table_id, name)
)

_mylite_catalog_foreign_key_columns(
  foreign_key_column_id INTEGER PRIMARY KEY,
  foreign_key_id INTEGER NOT NULL,
  child_table_id INTEGER NOT NULL,
  parent_table_id INTEGER NOT NULL,
  child_column_id INTEGER NOT NULL,
  parent_column_id INTEGER NOT NULL,
  ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),
  position_in_unique_constraint INTEGER NOT NULL CHECK(position_in_unique_constraint > 0),
  descriptor_version INTEGER NOT NULL,
  created_catalog_generation INTEGER NOT NULL,
  updated_catalog_generation INTEGER NOT NULL,
  UNIQUE(foreign_key_id, ordinal_position)
)
```

The first slice always stores one FK column row with ordinal and parent-position
`1`. `update_rule`, `delete_rule`, and `match_option` store the rendered MySQL
metadata strings: `NO ACTION`, `NO ACTION`, and `NONE`.

Existing index descriptors remain authoritative for primary, unique, and
nonunique indexes. FK descriptors reference index IDs but are not themselves
indexes. If a child-side supporting index must be auto-created, MyLite creates
a normal nonunique secondary index descriptor and generated physical SQLite
index in the same catalog mutation.

## Name Resolution

- Child target table resolution follows the existing `CREATE TABLE` or
  `ALTER TABLE` selected/default schema policy.
- The parent table must resolve in the same schema as the child for this slice.
- Reserved `_mylite_*` names are rejected before physical SQL generation.
- Parent and child columns resolve from descriptors or planned create-table
  columns, not SQLite metadata.
- Constraint names use current MyLite identifier length and reserved-name
  policy. Names are unique per child table.
- Unnamed `CREATE TABLE` constraints use `<child_table>_ibfk_N`, with the first
  available positive suffix.
- Existing child indexes are reusable only when their first key part is the
  child column and the part has no prefix length.
- Parent keys are accepted only when the parent index is primary or unique,
  has exactly one key part, and that part is the referenced parent column.

## Conversion and Enforcement

The slice reuses the current descriptor conversion code. FK checks operate on
stored physical values:

- `NULL` child values pass without a parent lookup;
- non-`NULL` child values must have an equal parent key row;
- parent deletes and parent key updates fail if any child row stores the old
  non-`NULL` parent key value;
- integer compatibility uses exact descriptor logical type and signedness for
  this baseline, avoiding lossy cross-type comparison semantics.

Generated SQLite probe shapes are descriptor-built and parameterized. Examples:

```sql
SELECT 1
FROM "_mylite_user_table_<parent_id>"
WHERE "id" = ?1
LIMIT 1
```

```sql
SELECT 1
FROM "_mylite_user_table_<child_id>"
WHERE "parent_id" = ?1
LIMIT 1
```

For statement-level probes after set-based updates or inserts, MyLite may use
anti-join forms:

```sql
SELECT 1
FROM "_mylite_user_table_<child_id>" AS c
WHERE c."parent_id" IS NOT NULL
  AND NOT EXISTS (
    SELECT 1 FROM "_mylite_user_table_<parent_id>" AS p
    WHERE p."id" = c."parent_id"
  )
LIMIT 1
```

Every generated identifier is quoted. Literal values are bound through prepared
statements. MyLite does not use SQLite native FK diagnostics as the public
compatibility surface.

For `INSERT IGNORE`, a missing parent value appends warning `1452 / 23000` and
skips the proposed row. Supported in-range non-ignored FK DDL and DML produces
`warning_count == 0`.

## Diagnostics

Diagnostics for the supported subset:

- missing default schema, unknown schema, unknown table, duplicate table, and
  reserved names reuse existing MyLite diagnostics;
- unknown child or parent column uses MySQL-shaped missing-column diagnostics;
- missing parent unique key returns a MySQL-shaped `6125 / HY000` diagnostic;
- incompatible child/parent descriptors return a MySQL-shaped
  `3780 / HY000` diagnostic;
- duplicate constraint names return a deterministic duplicate-key/constraint
  diagnostic verified against MySQL 8.4.9 for the admitted subset;
- child-side violations return `1452 / 23000`;
- parent-side delete/update violations return `1451 / 23000`;
- unsupported FK grammar or action clauses return deterministic unsupported
  diagnostics;
- unsupported DML/DDL on FK-related tables returns deterministic unsupported
  diagnostics before mutation;
- allocation and physical SQLite failures use existing runtime conventions.

## Introspection

`SHOW CREATE TABLE` renders foreign-key constraints after columns and supported
key lines:

```sql
KEY `fk_child_parent` (`parent_id`),
CONSTRAINT `fk_child_parent` FOREIGN KEY (`parent_id`) REFERENCES `parent` (`id`)
```

`TABLE_CONSTRAINTS` adds one row per FK descriptor:

| Column | Value |
| --- | --- |
| `CONSTRAINT_CATALOG` | `def` |
| `CONSTRAINT_SCHEMA` | child schema |
| `CONSTRAINT_NAME` | FK name |
| `TABLE_SCHEMA` | child schema |
| `TABLE_NAME` | child table |
| `CONSTRAINT_TYPE` | `FOREIGN KEY` |
| `ENFORCED` | `YES` |

`KEY_COLUMN_USAGE` adds one row per FK column:

| Column | Value |
| --- | --- |
| `CONSTRAINT_CATALOG` | `def` |
| `CONSTRAINT_SCHEMA` | child schema |
| `CONSTRAINT_NAME` | FK name |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | child schema |
| `TABLE_NAME` | child table |
| `COLUMN_NAME` | child column |
| `ORDINAL_POSITION` | `1` |
| `POSITION_IN_UNIQUE_CONSTRAINT` | `1` |
| `REFERENCED_TABLE_SCHEMA` | parent schema |
| `REFERENCED_TABLE_NAME` | parent table |
| `REFERENCED_COLUMN_NAME` | parent column |

`REFERENTIAL_CONSTRAINTS` becomes a limited queryable information-schema view
with MySQL's columns for supported FK rows. For this slice:

- catalog columns are `def`;
- `CONSTRAINT_SCHEMA`, `TABLE_NAME`, and `REFERENCED_TABLE_NAME` come from
  descriptors;
- `UNIQUE_CONSTRAINT_SCHEMA` is the parent schema;
- `UNIQUE_CONSTRAINT_NAME` is the referenced primary/unique key name;
- `MATCH_OPTION` is `NONE`;
- `UPDATE_RULE` and `DELETE_RULE` are `NO ACTION`.

## Performance

Foreign-key enforcement must stay set-based. The implementation may check
single literal row values before insertion or assignment when that is already
the execution shape, but multi-row and parent-side checks must be expressed as
SQLite `EXISTS` / `NOT EXISTS` probes over physical tables. MyLite must not
materialize child or parent tables in memory to enforce constraints.

Child-side auto-created indexes are ordinary SQLite indexes, so parent-side
dependency checks can use the same physical execution path as other indexed
queries. MyLite does not expose optimizer guarantees.

## Tests

Add MySQL-runtime-verified expectations and fast C runtime tests for:

- named `CREATE TABLE` FK over integer parent primary key;
- unnamed `CREATE TABLE` FK name generation;
- parent unique-key references;
- existing child index reuse and child index auto-creation metadata;
- `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY`;
- `SHOW CREATE TABLE`, `SHOW INDEX`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, and `REFERENTIAL_CONSTRAINTS`;
- child `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` valid values,
  `NULL` values, and missing-parent violations;
- parent `DELETE` and parent key `UPDATE` violations;
- reopen persistence and independent file-backed handles;
- unsupported action clauses and unsupported FK-related DML/DDL paths;
- unknown columns, missing parent key, incompatible types, duplicate names, and
  missing parent table diagnostics;
- existing primary-key, unique-index, nonunique-index, information-schema,
  insert, update, delete, truncate, drop/rename table, and full workflow tests
  still passing.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-indexes-constraints.md`,
`docs/compatibility/sql-table-ddl.md`, `docs/compatibility/sql-table-dml.md`,
`docs/compatibility/metadata-information-schema.md`, and `docs/compatibility/
runtime-system-variables.md` only for the exact supported FK subset.

Do not claim full InnoDB foreign keys, cascading actions, mutable
`foreign_key_checks`, composite/non-integer references, cross-schema
references, or optimizer behavior.

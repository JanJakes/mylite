# Baseline Composite Foreign Key Constraints

## Summary

This phase expands MyLite's descriptor-owned foreign-key support from one
integer key part to ordered composite integer-family key parts:

```sql
CREATE TABLE child (
  id INT PRIMARY KEY,
  a INT,
  b INT,
  CONSTRAINT fk_child_parent FOREIGN KEY (a, b) REFERENCES parent (a, b)
);
```

The goal is to close the next schema-compatibility gap now that MyLite has
descriptor-owned composite primary keys, composite unique keys, multi-part
secondary indexes, and one-column foreign keys. The slice remains narrower than
MySQL: it supports persistent same-schema base tables, compatible integer-family
columns, default `NO ACTION` rules, and descriptor-driven metadata/enforcement.
It does not add cascades, non-integer foreign keys, cross-schema references,
mutable `foreign_key_checks`, or full InnoDB dependency behavior.

## Sources

- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_composite_foreign_key_constraints_expectations.sh`.

This specification is independently authored from the public documentation and
runtime observations above. It does not copy MySQL grammar, MySQL
implementation sources, MariaDB sources, Percona sources, or SQLite internals.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `FOREIGN KEY (a, b) REFERENCES parent(a, b)` succeeds when the referenced
  parent columns are the complete ordered key parts of a composite primary key
  or unique key.
- A named composite foreign key without an existing child index creates a
  nonunique child index with the constraint name. An unnamed create-time
  composite foreign key creates a child index named after the first child
  column and a constraint named `<child_table>_ibfk_N`.
- An existing child index is reused when its leftmost full key parts match the
  child foreign-key columns in order, even if the index has additional trailing
  parts.
- `SHOW CREATE TABLE` renders one child key line and one constraint line:
  ``KEY `fk` (`a`,`b`)`` and
  ``CONSTRAINT `fk` FOREIGN KEY (`a`, `b`) REFERENCES `parent` (`a`, `b`)``.
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` expose one child-index row
  per foreign-key part with `Seq_in_index` / `SEQ_IN_INDEX` values starting at
  `1`.
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` has one `FOREIGN KEY` row per
  constraint with `ENFORCED = 'YES'`.
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` has one row per child column with
  matching `ORDINAL_POSITION` and `POSITION_IN_UNIQUE_CONSTRAINT`; referenced
  table and column names are populated.
- `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` has one row per foreign key,
  with `UNIQUE_CONSTRAINT_NAME` set to the parent primary or unique key name,
  `MATCH_OPTION = 'NONE'`, and default `UPDATE_RULE` / `DELETE_RULE` values of
  `NO ACTION`.
- Composite foreign keys use `MATCH SIMPLE` behavior: a child row is not
  checked when any child key part is `NULL`. Fully non-`NULL` child tuples must
  match a parent key tuple.
- MySQL accepts nullable parent unique keys as referenced keys. A fully
  non-`NULL` child tuple still needs a matching parent tuple; child tuples with
  any `NULL` part pass without lookup.
- Missing fully non-`NULL` parent tuples fail `INSERT` / `UPDATE` with
  `1452 / 23000`. `INSERT IGNORE` skips the row, reports `ROW_COUNT() = 0`,
  and records warning `1452`.
- Deleting or updating a referenced parent tuple fails with `1451 / 23000`.
  Parent rows whose referenced key tuple contains `NULL` are not blocked by
  child rows that contain `NULL` in any corresponding part.
- Mismatched child/reference column counts fail with `1239 / 42000`.
- Missing parent unique keys fail with `6125 / HY000`.
- Incompatible child and parent column descriptors fail with `3780 / HY000`.
- Duplicate foreign-key names in the same child table fail with
  `1826 / HY000`.
- MySQL accepts composite action clauses such as `ON DELETE CASCADE`, but
  cascade and `SET NULL` row-write semantics are deferred by this MyLite slice.

## Ownership Boundaries

- Public API: no ABI change. Applications continue through `mylite_execute()`
  and existing result/diagnostic accessors.
- Statement context: owns diagnostic reset, warning counts, affected rows,
  `ROW_COUNT()`, statement atomicity, and cleanup on failure.
- Parser/AST: the existing foreign-key part-list grammar already preserves
  ordered child and referenced column lists. Parser code remains descriptor
  agnostic.
- Analyzer/planner: resolves child and parent schema/table names, column lists,
  parent key eligibility, child index reuse/creation, and diagnostics from
  MyLite descriptors before generated SQLite SQL is produced.
- Catalog: existing `_mylite_catalog_foreign_keys` and
  `_mylite_catalog_foreign_key_columns` rows are authoritative. Composite
  foreign keys use one foreign-key descriptor plus one ordered column descriptor
  per key part; no catalog schema migration is required.
- Result builders: `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
  `INFORMATION_SCHEMA` rows render from MyLite descriptors, not SQLite schema
  text or `PRAGMA` output.
- Storage/VFS: no `.mylite` preamble, file-format, or shifted SQLite payload
  change.
- SQLite physical storage: MyLite uses ordinary generated SQLite indexes for
  child lookup support and descriptor-built prepared statements for checks.
  Native SQLite foreign-key enforcement is not the public compatibility
  surface, and no SQLite fork patch is required.

## Supported SQL Subset

Supported persistent-base-table forms:

```sql
CREATE TABLE child (
  ...,
  [CONSTRAINT constraint_name]
  FOREIGN KEY (child_column, child_column[, ...])
  REFERENCES parent_table (parent_column, parent_column[, ...])
)

ALTER TABLE child
  ADD CONSTRAINT constraint_name
  FOREIGN KEY (child_column, child_column[, ...])
  REFERENCES parent_table (parent_column, parent_column[, ...])
```

This phase extends the existing one-column FK subset to two-or-more key parts
while preserving the one-column path. Supported key parts:

- unqualified child and parent column names;
- one or more full descriptor columns, with this phase focused on composite
  integer-family lists;
- child and parent lists with the same length;
- exactly compatible integer-family logical descriptors and signedness for each
  child/parent pair;
- same-schema parent tables only;
- parent keys that are primary or unique descriptors whose ordered full key
  parts exactly match the referenced parent column list;
- child indexes whose leftmost full key parts match the child column list, or
  an auto-created nonunique child index when none exists;
- default `MATCH SIMPLE` behavior only, exposed as `MATCH_OPTION = 'NONE'`;
- default `ON UPDATE NO ACTION` and `ON DELETE NO ACTION` behavior only.

Deferred:

- cross-schema references;
- non-integer foreign-key columns, including string, decimal, temporal, binary,
  `BIT`, `ENUM`, `SET`, and `JSON`;
- prefix, descending, expression, functional, generated, or invisible key
  parts;
- inline column `REFERENCES`;
- unnamed `ALTER TABLE ... ADD FOREIGN KEY`;
- action clauses other than default `NO ACTION`, including `CASCADE`,
  `SET NULL`, `SET DEFAULT`, and timing subtleties;
- `MATCH` clauses beyond the default simple behavior;
- self-referential composite FKs;
- temporary tables, views, partitions, privilege checks, metadata locks, and
  storage-engine variants;
- mutable `foreign_key_checks = 0`;
- `INSERT ... SELECT`, `REPLACE ... SELECT`, key-bearing `REPLACE`, ODKU,
  multi-table DML, and joined DML on FK-related tables unless already rejected
  by the existing one-column FK policy.

## MyLite Grammar Snippets

The parser already admits ordered lists for the existing FK grammar. The
intended MyLite Lemon-style grammar is:

```lemon
create_table_item ::= foreign_key_definition.

foreign_key_definition ::=
    constraint_name_opt FOREIGN KEY
    LPAREN foreign_key_part_list RPAREN
    REFERENCES table_name LPAREN foreign_key_part_list RPAREN.

foreign_key_part_list ::= identifier.
foreign_key_part_list ::= foreign_key_part_list COMMA identifier.

alter_table_add_foreign_key_statement ::=
    ALTER TABLE table_name ADD CONSTRAINT identifier FOREIGN KEY
    LPAREN foreign_key_part_list RPAREN
    REFERENCES table_name LPAREN foreign_key_part_list RPAREN.
```

Semantic analysis, not parsing, decides whether a multi-part list is supported
for the current descriptor families.

## Resolution and Validation

Child target resolution follows existing policy:

- unqualified child table names use the selected/default schema;
- schema-qualified child table names use the explicit schema and do not require
  a selected default schema;
- missing default schema fails with `1046 / 3D000`;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` logical names are rejected before physical SQL is
  generated.

Parent resolution:

- parent table names are resolved in the child schema for this phase;
- schema-qualified parent references outside that schema remain unsupported;
- parent objects must be persistent MyLite base tables.

Column and key validation:

- child and parent list lengths must match; mismatches use
  `1239 / 42000`;
- child and parent columns must exist in MyLite descriptors;
- duplicate child columns or duplicate parent columns are rejected
  deterministically before mutation;
- every child/parent pair must have exactly compatible integer-family
  descriptors and signedness; incompatibilities use `3780 / HY000`;
- the referenced parent key must be a primary or unique descriptor, must have
  no prefix parts, and its ordered column list must exactly match the
  referenced parent column list; missing keys use `6125 / HY000`;
- child index reuse requires an existing index whose leftmost key parts are the
  child FK columns in order, with no prefix parts;
- absent child indexes are auto-created as nonunique secondary index
  descriptors in the same catalog mutation;
- duplicate constraint names in the same child table use
  `1826 / HY000`.

Constraint names:

- explicit names remain table-local and ASCII case-insensitive under the
  current descriptor policy;
- unnamed create-time constraints use `<child_table>_ibfk_N`;
- named auto-created child indexes use the constraint name;
- unnamed auto-created child indexes use the first child column name and the
  existing generated-index collision policy.

## Enforcement Semantics

Composite FK checks use MySQL's default simple matching:

- if any child key part is `NULL`, the row passes without a parent lookup;
- if every child key part is non-`NULL`, the tuple must match a parent row on
  every referenced parent key part;
- parent deletes and parent-key updates are blocked only when a child row has a
  fully non-`NULL` tuple equal to the old parent tuple;
- supported successful FK DDL and DML produce `warning_count == 0` unless an
  existing supported warning path such as `INSERT IGNORE` applies.

Generated check SQL is descriptor-built and uses prepared statements. Example
single-row child lookup:

```sql
SELECT 1
FROM "_mylite_user_table_<parent_table_id>"
WHERE "a" = ?1 AND "b" = ?2
LIMIT 1
```

Example set-based child validation:

```sql
SELECT 1
FROM "_mylite_user_table_<child_table_id>" AS c
WHERE c."a" IS NOT NULL
  AND c."b" IS NOT NULL
  AND NOT EXISTS (
    SELECT 1
    FROM "_mylite_user_table_<parent_table_id>" AS p
    WHERE p."a" = c."a" AND p."b" = c."b"
  )
LIMIT 1
```

Example parent-side blocker:

```sql
SELECT 1
FROM "_mylite_user_table_<child_table_id>"
WHERE "a" = ?1 AND "b" = ?2
LIMIT 1
```

Every generated identifier is quoted. Values are bound as SQLite parameters.
MyLite must not materialize parent or child tables in memory to enforce
composite FKs.

## Metadata

`SHOW CREATE TABLE` renders child indexes before FK constraints. Composite FK
constraints render one ordered list of child columns and one ordered referenced
list.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` adds one `FOREIGN KEY` row per FK
descriptor with fixed `ENFORCED = 'YES'`.

`INFORMATION_SCHEMA.KEY_COLUMN_USAGE` adds one row per FK column:

- `ORDINAL_POSITION` is the child-column position in the FK definition;
- `POSITION_IN_UNIQUE_CONSTRAINT` is the referenced parent-key position;
- referenced schema/table/column fields are populated from descriptors.

`INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` adds one row per FK descriptor:

- `UNIQUE_CONSTRAINT_NAME` is the parent primary or unique key descriptor name;
- `MATCH_OPTION` is `NONE`;
- `UPDATE_RULE` and `DELETE_RULE` are `NO ACTION`;
- table names and schema names come from descriptors.

`CREATE TABLE ... LIKE` and `CREATE TABLE ... SELECT` continue not to copy FK
descriptors. `DROP FOREIGN KEY` removes all column descriptor rows for the FK
and preserves the child index, matching the existing one-column policy.

## Diagnostics

Supported diagnostics:

- `1046 / 3D000`: missing default schema for unqualified child target;
- `1049 / 42000`: unknown schema;
- `1103 / 42000`: reserved `_mylite_*` logical names where existing policy
  uses incorrect-name diagnostics;
- `1146 / 42S02`: unknown table;
- `1824 / HY000`: missing referenced parent table in create-time forms;
- `1072 / 42000`: unknown child or parent column;
- `1239 / 42000`: child/reference column-count mismatch;
- `1826 / HY000`: duplicate FK name in the child table;
- `3780 / HY000`: incompatible child/parent descriptor pair;
- `6125 / HY000`: referenced parent columns are not covered by a compatible
  primary or unique descriptor;
- `1452 / 23000`: child-side missing parent tuple;
- `1451 / 23000`: parent-side referenced tuple update/delete;
- existing allocation and physical SQLite failures use current runtime
  conventions.

Unsupported grammar or deferred action clauses should fail deterministically
before catalog or row mutation.

## Performance

Composite FK enforcement stays set-based. Single proposed rows may use
parameterized point lookups, but multi-row validation and parent-side checks
must be expressed as SQLite `EXISTS` / `NOT EXISTS` probes over physical tables.
Auto-created child indexes are ordinary generated SQLite indexes so parent-side
checks have an indexed execution path when SQLite chooses it. No SQLite fork
change is required.

## Tests

Add MySQL-runtime-verified expectations and fast C runtime tests for:

- named `CREATE TABLE` composite FK over a composite parent primary key;
- unnamed `CREATE TABLE` composite FK name generation and child-index naming;
- `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY` over composite keys;
- composite FK over a composite unique parent key, including nullable parent
  unique columns and child `MATCH SIMPLE` `NULL` behavior;
- child index auto-creation and reuse of an existing left-prefix child index;
- `SHOW CREATE TABLE`, `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
  `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and `REFERENTIAL_CONSTRAINTS`;
- child `INSERT ... VALUES`, `INSERT ... SET`, `INSERT IGNORE`, and
  `UPDATE` valid values, `NULL` tuples, and missing-parent violations;
- parent `DELETE` and parent key `UPDATE` violations;
- reopen persistence, drop-foreign-key behavior, drop-database cleanup, and
  independent file-backed handles;
- diagnostics for count mismatch, unknown columns, missing parent key,
  incompatible descriptors, duplicate names, missing parent table, and
  unsupported actions;
- existing parser, primary-key, unique-index, secondary-index, FK, DML,
  information-schema, file-format, and full workflow tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-indexes-constraints.md`,
`docs/compatibility/sql-table-ddl.md`, `docs/compatibility/sql-table-dml.md`,
and `docs/compatibility/metadata-information-schema.md` only for the exact
implemented subset.

Do not claim cascades, `SET NULL`, non-integer references, cross-schema
references, mutable `foreign_key_checks`, self-referential composite behavior,
temporary table FKs, optimizer guarantees, or native SQLite FK semantics.

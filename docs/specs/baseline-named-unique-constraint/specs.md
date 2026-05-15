# Baseline Named UNIQUE Constraint

## Goal

Add the narrow `CREATE TABLE` form that real MySQL DDL commonly emits for
named unique constraints:

```sql
CREATE TABLE t (
    name INT,
    CONSTRAINT c UNIQUE (name)
);
```

For this baseline, MyLite treats the admitted named unique constraint as the
same descriptor-owned unique secondary index surface that already powers
`UNIQUE KEY c (name)`. The work is parser and planning coverage over existing
catalog, physical SQLite index, duplicate-check, `SHOW`, `INFORMATION_SCHEMA`,
`CREATE TABLE ... LIKE`, persistence, and `ALTER TABLE ... DROP INDEX` paths.

## Compatibility Authority

This design is based on:

- MySQL 8.4 Reference Manual `CREATE TABLE` documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual `SHOW CREATE TABLE` documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
- MySQL 8.4 Reference Manual `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html>
- MySQL 8.4 Reference Manual `INFORMATION_SCHEMA.STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- MySQL 8.4 Reference Manual `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html>
- Observed MySQL 8.4.9 runtime behavior from the companion expectation script.

The official manual describes `CONSTRAINT symbol` as a way to name constraints,
and describes `UNIQUE` as a unique index that allows multiple `NULL` values for
nullable key parts. Runtime probes on MySQL 8.4.9 show that for unique
constraints, `SHOW CREATE TABLE`, `SHOW INDEX`,
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `INFORMATION_SCHEMA.STATISTICS`, and
`INFORMATION_SCHEMA.KEY_COLUMN_USAGE` expose the resulting index name, not a
separate durable unique-constraint object.

## Supported Syntax

The admitted MyLite grammar is deliberately small and independently authored:

```lemon
create_table_item ::= named_unique_constraint.

named_unique_constraint ::=
    CONSTRAINT identifier UNIQUE unique_index_keyword_opt LPAREN secondary_index_part_list RPAREN.

named_unique_constraint ::=
    CONSTRAINT UNIQUE unique_index_keyword_opt index_name_opt LPAREN secondary_index_part_list RPAREN.

named_unique_constraint ::=
    CONSTRAINT identifier UNIQUE unique_index_keyword_required identifier LPAREN secondary_index_part_list RPAREN.

unique_index_keyword_opt ::= .
unique_index_keyword_opt ::= KEY.
unique_index_keyword_opt ::= INDEX.

unique_index_keyword_required ::= KEY.
unique_index_keyword_required ::= INDEX.

index_name_opt ::= .
index_name_opt ::= identifier.
```

The first form uses the constraint identifier as the unique index name. The
second form covers `CONSTRAINT UNIQUE (...)` and
`CONSTRAINT UNIQUE KEY index_name (...)`. The third form covers
`CONSTRAINT constraint_name UNIQUE KEY index_name (...)` and the matching
`INDEX` spelling. When an explicit index name appears after `KEY` or `INDEX`,
that name is authoritative, matching observed MySQL 8.4.9 behavior. The
existing unprefixed `UNIQUE`, `UNIQUE KEY`, and `UNIQUE INDEX` table-level
forms remain unchanged.

The `secondary_index_part_list` is exactly the currently supported MyLite
unique-index key-part subset:

- unqualified descriptor column names;
- one or more key parts;
- optional positive string prefix lengths where the existing unique-prefix
  index surface admits them;
- optional `ASC` or `DESC` direction metadata.

## Deferred Syntax

This phase does not add:

- `ALTER TABLE ... ADD CONSTRAINT name UNIQUE ...`;
- MySQL's accepted `CONSTRAINT constraint_name UNIQUE index_name (...)`
  shorthand without `KEY` or `INDEX` before the explicit index name;
- named primary-key constraints;
- table-qualified, expression, functional, ordinal, fulltext, spatial, binary,
  JSON, or multi-valued key parts beyond existing unique-index support;
- index options such as `USING`, comments, visibility, parser plugins,
  algorithms, or locks;
- a separate unique-constraint descriptor namespace independent of the
  descriptor-owned unique secondary index.

Unsupported forms continue to use the existing parser or planner diagnostics.

## Semantic Model

### Ownership Boundary

- Public API: no public ABI changes. Callers continue to execute SQL through
  `mylite_execute()` and observe the existing result object conventions.
- Statement context: no new handle state or SQL mode state.
- Parser/AST: accepts the named-constraint syntax and lowers it to the existing
  `MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION` shape. When both a constraint name
  and explicit index name are supplied, the explicit index name becomes the AST
  index name.
- Analyzer/planner: reuses the existing `CREATE TABLE` secondary-index planner.
  It resolves names from the in-flight table descriptor, not SQLite metadata.
- Catalog: no schema change. The unique constraint is stored in
  `_mylite_catalog_indexes` and `_mylite_catalog_index_columns` exactly like
  existing descriptor-owned unique secondary indexes.
- Result builder: successful `CREATE TABLE` and `ALTER TABLE ... DROP INDEX`
  keep their existing non-row result behavior, affected-row counts, and warning
  counts.
- Storage/VFS: no file-format or VFS changes. MyLite still creates one generated
  SQLite unique index for the descriptor and preserves the `.mylite` preamble.
- SQLite: no fork patch. Generated SQLite DDL is produced by existing
  descriptor-driven physical index creation code.

### Name Resolution

The target table follows existing `CREATE TABLE` schema resolution:

- unqualified table names require the selected/default schema;
- schema-qualified table names use the named schema;
- missing default schema, unknown schema, existing table, and reserved
  `_mylite_*` names preserve current diagnostics.

Key-part columns are resolved against columns declared earlier or later in the
same `CREATE TABLE` descriptor plan using existing unique-index rules. Column
names are unqualified and use the current descriptor name-comparison behavior.

### Naming

For `CONSTRAINT c UNIQUE (...)`, MyLite stores `c` as the descriptor-owned unique
index name. For `CONSTRAINT UNIQUE (...)`, MyLite uses the existing generated
unique-index naming policy based on the first key-part column and suffixes for
same-table collisions. For `CONSTRAINT c UNIQUE KEY idx (...)`, `idx` is the
stored index and metadata name; the constraint token name is accepted for MySQL
compatibility but not stored separately.

Duplicate names are checked through the existing unique-index name policy:

- duplicate secondary index names in the same table fail with the existing
  duplicate key-name diagnostic;
- the same unique name may be reused by another table in the same schema;
- unique index names and check or foreign-key constraint names remain separate
  descriptor namespaces, matching the current MyLite catalog model and observed
  MySQL 8.4.9 behavior for checks.

### Metadata

Because the feature lowers to an existing unique-index descriptor,
descriptor-driven metadata updates automatically:

- `SHOW CREATE TABLE` renders `UNIQUE KEY name (key_part[, ...])`;
- `SHOW INDEX` reports `NON_UNIQUE = 0`, `Key_name = name`, ordered key parts,
  `Sub_part`, and `Collation` through existing code;
- `INFORMATION_SCHEMA.STATISTICS` exposes the index rows;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes a `UNIQUE` row named after the
  stored index name;
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` exposes ordered unique key-part rows;
- `CREATE TABLE ... LIKE` clones the descriptor;
- `ALTER TABLE ... DROP INDEX name` and standalone `DROP INDEX name ON table`
  remove the descriptor and generated SQLite index.

### DML Behavior

All existing unique-index enforcement applies:

- duplicate non-`NULL` inserts and updates fail with the existing duplicate key
  diagnostic;
- duplicate nullable key tuples with at least one `NULL` remain allowed;
- `INSERT IGNORE`, limited `ON DUPLICATE KEY UPDATE`, `REPLACE`, and supported
  `UPDATE` paths keep using descriptor-owned key metadata.

### Physical SQLite Handling

Generated SQLite identifiers continue to come only from stable MyLite physical
names such as the table physical name and generated index physical name.
Identifiers are quoted through existing helpers. User literals are not
interpolated by this feature. Unique enforcement uses the existing generated
SQLite unique index plus existing MyLite validation where the descriptor subset
requires MySQL-specific checks.

## Diagnostics

This phase reuses current unique-index diagnostics:

- syntax errors for unsupported grammar;
- missing default schema, unknown schema, unknown table, existing table, and
  reserved names from the surrounding `CREATE TABLE` lifecycle;
- unknown key columns with the existing MySQL-compatible missing-key-column
  diagnostic;
- duplicate same-table index names with the existing duplicate-key-name
  diagnostic;
- unsupported key-part type or prefix forms with existing unique-index
  diagnostics;
- allocation and SQLite physical failures through existing runtime error paths.

No new public API misuse cases are introduced.

## Performance

The feature adds no row materialization. Once parsed, it uses the same planner
and SQLite unique-index execution path as `UNIQUE KEY name (...)`. Metadata
queries remain descriptor-driven. DML duplicate checks continue to use existing
descriptor-built probes and physical unique indexes.

## MySQL 8.4.9 Runtime Evidence

The expectation script verifies these observed behaviors:

- `CONSTRAINT c UNIQUE (name)` is accepted and renders as
  `UNIQUE KEY `c` (`name`)`;
- `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` use `c`;
- `ALTER TABLE t DROP INDEX c` removes the unique key;
- `CONSTRAINT UNIQUE (name)` derives the index name from the first key part;
- `CONSTRAINT c UNIQUE KEY (name)` and
  `CONSTRAINT c UNIQUE INDEX (name)` use `c`;
- `CONSTRAINT c UNIQUE KEY idx (name)` uses `idx`;
- duplicate same-table index names fail with `1061 / 42000`;
- unknown key columns fail with `1072 / 42000`;
- duplicate non-`NULL` values fail with `1062 / 23000`;
- duplicate nullable `NULL` values are allowed.

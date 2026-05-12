# Baseline Composite Primary Key Lifecycle

## Summary

This phase expands create-time primary-key support from one integer-family
column to descriptor-owned composite primary keys over two or more
integer-family columns on persistent base tables.

The slice is deliberately create-time only. It proves the shared ordered
key-part representation used by `SHOW`, limited `INFORMATION_SCHEMA`,
`CREATE TABLE ... LIKE`, duplicate-key DML diagnostics, and physical SQLite
unique indexes. `ALTER TABLE ... ADD PRIMARY KEY (a, b)`, string primary keys,
key options, and auto-increment-in-composite-key behavior are deferred to
separate phases.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline primary key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Baseline ALTER TABLE ADD PRIMARY KEY:
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`
- Baseline secondary index lifecycle:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`
- Baseline unique index lifecycle:
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline information schema constraints:
  `docs/specs/baseline-information-schema-constraints/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, primary-key and unique constraints:
  <https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE ... LIKE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table-like.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_composite_primary_key_lifecycle_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase show:

- `CREATE TABLE t (a INT, b INT, PRIMARY KEY (a,b))` succeeds, makes both key
  columns `NOT NULL`, renders `PRI` for both columns in `SHOW COLUMNS`,
  renders two `SHOW INDEX` rows with `Seq_in_index = 1` and `2`, and renders
  `PRIMARY KEY (`a`,`b`)` in `SHOW CREATE TABLE`.
- Duplicate composite-key writes fail with `1062 / 23000` and a message shaped
  like `Duplicate entry '1-2' for key 't.PRIMARY'`.
- `INSERT IGNORE` skips duplicate composite-key rows, records warning `1062`,
  continues with nonconflicting rows, and reports affected rows for rows
  actually inserted.
- `NULL` in any primary-key part fails with `1048 / 23000` for DML.
- Explicit `NULL` nullability on any key part in a table-level composite
  primary key fails with `1171 / 42000`.
- `DEFAULT NULL` on a table-level composite primary-key part also fails with
  `1171 / 42000`. A non-`NULL` integer default is preserved and can be used by
  omitted-column `INSERT`.
- Repeated key parts fail with `1060 / 42S21` using duplicate-column
  diagnostics. Unknown key parts fail with `1072 / 42000`.
- `CREATE TABLE ... LIKE` clones composite primary-key metadata and physical
  enforcement. `CREATE TABLE ... SELECT` copies descriptor columns and
  nullability but does not copy the primary key.
- MySQL accepts wider forms such as string composite primary keys and
  `AUTO_INCREMENT` when the generated column is the first key part. MyLite
  defers those until collation-aware string key comparison and composite
  auto-increment behavior are specified.

## Scope

Supported:

- persistent base tables only;
- table-level create-time primary keys with two or more unqualified key parts:
  `PRIMARY KEY (column_name, column_name[, ...])`;
- all key parts must resolve to existing MyLite integer-family descriptors:
  `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT` / `INTEGER`, `BIGINT`, the
  `INT1` / `INT2` / `INT3` / `INT4` / `INT8` aliases, and admitted signed or
  unsigned forms;
- each key part becomes logically and physically `NOT NULL`;
- key-part order is the declared order and is preserved in catalog
  `ordinal_position`, physical SQLite index order, `SHOW INDEX`, `SHOW CREATE
  TABLE`, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, and
  `INFORMATION_SCHEMA.STATISTICS`;
- duplicate-key enforcement for `INSERT ... VALUES`, `INSERT ... SET`,
  `INSERT IGNORE ... VALUES`, `INSERT IGNORE ... SET`, and single-assignment
  `UPDATE`;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, limited
  `INFORMATION_SCHEMA.COLUMNS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `STATISTICS`;
- `CREATE TABLE ... LIKE` cloning and `CREATE TABLE ... SELECT` key omission
  matching the existing single-column behavior;
- `DROP TABLE`, `RENAME TABLE`, `TRUNCATE`, reopen persistence, independent
  handles, and `.mylite` preamble preservation.

Deferred:

- `ALTER TABLE ... ADD PRIMARY KEY (a,b,...)`;
- `ALTER TABLE ... DROP PRIMARY KEY`;
- column-level multi-column primary keys, which MySQL does not support;
- string, decimal, temporal, `TEXT`, generated, expression, hidden, or
  unsupported descriptor key parts;
- `AUTO_INCREMENT` inside a composite primary key;
- named constraints, `CONSTRAINT symbol`, `KEY` column shorthand, `USING`,
  comments, visibility, `ASC`/`DESC`, prefix lengths, functional key parts,
  storage-engine attributes, algorithms, locks, temporary tables, partitions,
  views, and privileges;
- key-aware `REPLACE`, `INSERT ... SELECT` into key-bearing targets,
  `ON DUPLICATE KEY UPDATE`, foreign keys, cascades, triggers, check
  constraints, optimizer/index-use guarantees, protocol flag changes, and
  SQLite fork patches.

## Ownership Boundaries

- Public API: no ABI change. Applications use existing `mylite_execute()`,
  result accessors, and diagnostics.
- Statement context: owns SQL text lifetime, diagnostics, affected rows,
  warnings, transaction completion, and cleanup on failure.
- Parser/AST: already admits `primary_key_part_list` and preserves key-part
  source spans. It does not inspect descriptors or physical storage.
- Analyzer/planner/runtime: resolves key parts against planned descriptors,
  rejects unsupported shapes, marks key parts `NOT NULL`, plans catalog rows,
  builds quoted physical SQLite index SQL, and maps duplicate-key diagnostics.
- Catalog module: durable table, column, index, and index-column descriptors
  remain authoritative. Ordered key parts are catalog rows, not reflections of
  SQLite schema text or pragmas.
- Result/introspection builders: render primary-key metadata from descriptors.
  SQLite schema text is not a compatibility authority.
- Storage/VFS: owns the `.mylite` preamble and shifted SQLite payload. This
  feature writes only payload catalog/table/index data.
- SQLite physical storage: stores rows and enforces generated composite unique
  indexes. MyLite remains responsible for type conversion, nullability,
  metadata, warnings, and MySQL-shaped diagnostics.

## Supported Grammar

The admitted MyLite SQL subset is:

```sql
CREATE TABLE table_name (
    column_definition,
    ...,
    PRIMARY KEY (column_name, column_name[, ...])
) [supported_table_options]
```

MyLite Lemon-style snippet:

```lemon
create_table_item ::= primary_key_definition.

primary_key_definition ::= PRIMARY KEY LPAREN primary_key_part_list RPAREN.

primary_key_part_list ::= primary_key_part.
primary_key_part_list ::= primary_key_part_list COMMA primary_key_part.

primary_key_part ::= identifier.
primary_key_part ::= qualified_identifier.
```

The parser may continue admitting qualified parts so analysis can return
deterministic unsupported diagnostics. The supported semantic subset requires
unqualified `identifier` parts. A one-part table-level primary key remains
supported by the existing single-column primary-key behavior.

## Name Resolution

The target table follows existing `CREATE TABLE` schema rules:

- unqualified target names use the selected/default schema;
- schema-qualified target names use the explicit schema;
- reserved `_mylite_*` schema, table, and column names are rejected before
  physical SQL generation;
- existing table conflicts and missing default schema diagnostics remain
  unchanged.

Each key part is resolved against the planned column descriptors for the new
table:

- unknown key parts fail with `1072 / 42000`;
- duplicate key parts fail with `1060 / 42S21`;
- explicit `NULL` nullability or `DEFAULT NULL` on any table-level composite
  key part fails with `1171 / 42000`;
- descriptor name matching follows the current MyLite identifier policy.

## Descriptor Model

The catalog schema already stores ordered index-column rows. This phase uses
that shape fully for primary keys:

- one primary index descriptor with logical name `PRIMARY`,
  `kind = PRIMARY`, `is_unique = 1`, and generated physical name
  `_mylite_user_index_<index_id>`;
- one index-column descriptor per key part, with `ordinal_position` starting
  at `1`;
- each referenced column descriptor is stored with `is_nullable = false`;
- non-`NULL` integer defaults on key parts are preserved;
- no prefix length, direction, visibility, comment, expression, or collation
  metadata is stored.

Runtime loaded-index helpers should expose one logical index with an ordered
array of key parts rather than one logical index per key part. This keeps
`SHOW CREATE TABLE`, `SHOW INDEX`, information-schema rows, and duplicate-key
diagnostics aligned with descriptor authority.

## Physical SQLite Handling

Physical table creation keeps rowid-table storage and generated MyLite column
names. The primary key is enforced with a normal SQLite unique index:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("a", "b", ...)
```

Rules:

- quote every identifier;
- generate SQL only from descriptors and stable physical names;
- use SQLite prepared statements and existing schema execution helpers;
- no SQLite `WITHOUT ROWID` tables and no SQLite fork patch;
- if any catalog or physical step fails, the create statement rolls back
  without leaving descriptor rows or physical artifacts.

## DML Semantics

Existing descriptor conversion, default materialization, nullability checks,
and auto-increment handling run before physical row writes.

For supported DML into a composite-primary-key table:

- non-`NULL` duplicate tuples fail with `1062 / 23000`;
- duplicate messages format key values in declared key-part order joined with
  `-`, matching observed MySQL 8.4.9 behavior for admitted integer values;
- `INSERT IGNORE` demotes duplicate tuple rows to warning `1062`, skips the
  conflicting row, and continues with later nonconflicting rows;
- `UPDATE` detects duplicate tuple conflicts even if the statement assigns only
  one key part, by combining the assigned value with current row values for the
  other key parts;
- `NULL` assignment to any key part remains a `1048 / 23000` error before
  duplicate probing;
- successful DML keeps existing public non-row result conventions, affected
  rows, and warning-count behavior.

`INSERT ... SELECT`, `REPLACE`, and `ON DUPLICATE KEY UPDATE` remain deferred
for key-bearing targets.

## Metadata Semantics

`SHOW COLUMNS` renders `PRI` for every composite primary-key column. If a
column has a non-`NULL` descriptor default, it renders normally; otherwise
MySQL's `NULL` default display is preserved even though the column is `NOT
NULL`.

`SHOW CREATE TABLE` renders columns first, then:

```sql
PRIMARY KEY (`a`,`b`)
```

`SHOW INDEX` renders one row per key part. `Key_name` is `PRIMARY`,
`Non_unique` is `0`, `Seq_in_index` is the declared ordinal, `Column_name` is
the logical descriptor column name, and other placeholder fields follow the
existing primary-key and secondary-index baseline.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` renders one `PRIMARY KEY` row for the
constraint. `KEY_COLUMN_USAGE` and `STATISTICS` render one row per key part
with the declared ordinal. `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` renders
`PRI` for each key part.

`CREATE TABLE ... LIKE` clones the composite descriptor to new table/index
physical names. `CREATE TABLE ... SELECT` copies key columns as ordinary `NOT
NULL` columns and does not copy the primary-key descriptor.

## Diagnostics

Supported diagnostics include:

- syntax errors for unsupported grammar that the parser does not admit;
- missing default schema, unknown schema, existing target table, and reserved
  names through existing table DDL diagnostics;
- duplicate column names in table definitions: `1060 / 42S21`;
- duplicate key parts: `1060 / 42S21`;
- unknown key parts: `1072 / 42000`;
- multiple primary keys: `1068 / 42000`;
- explicit `NULL` or `DEFAULT NULL` table-level composite key parts:
  `1171 / 42000`;
- unsupported key part type or shape: deterministic MyLite unsupported
  diagnostics, except `TEXT`/blob-style key parts may use the existing
  MySQL-compatible blob-key-without-length diagnostic;
- duplicate DML tuples: `1062 / 23000`;
- DML `NULL` for a key part: `1048 / 23000`;
- allocation failure: `MYLITE_NOMEM`;
- physical SQLite failures: existing internal/physical row diagnostics.

No warnings are produced by successful in-range composite primary-key DDL or
DML, except existing `INSERT IGNORE` duplicate warnings.

## Performance

MyLite should not materialize full tables to enforce composite primary keys.
SQLite enforces the generated composite unique index. MyLite may run targeted
lookup statements to map duplicate-key diagnostics to MySQL text and to support
`INSERT IGNORE` row skipping, but those lookups must bind converted descriptor
values and use the generated physical index shape.

The shared loaded-index representation should keep memory proportional to the
number of descriptors, not row count.

## Tests

Add MySQL-runtime-verified expectations and C runtime coverage for:

- create-time composite integer primary-key metadata;
- `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.COLUMNS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `STATISTICS`;
- insert, duplicate insert, duplicate update, `INSERT IGNORE`, no-result
  conventions, affected rows, warning count, and warning diagnostics;
- omitted-column inserts with non-`NULL` defaults and no-default errors;
- DML `NULL` errors for every key part;
- signed/unsigned integer-family key parts within existing row-value ranges;
- duplicate key-part diagnostics, unknown key-part diagnostics, unsupported
  qualified parts, unsupported string/decimal/temporal/text parts, named
  constraints, prefixes, directions, key options, and composite auto-increment
  deferral;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, `RENAME TABLE`,
  `TRUNCATE TABLE`, `DROP TABLE`, reopen persistence, independent handles, and
  file preamble preservation;
- zero-initialized cleanup for new owned key-part arrays.

Verification before completion:

1. `cmake --build --preset dev`
2. New and affected parser/runtime CTest entries.
3. `packages/libmylite/tests/mysql_baseline_composite_primary_key_lifecycle_expectations.sh`
4. `cmake --workflow --preset check`
5. Subagent release-gate review and final diff review.

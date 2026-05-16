# Baseline Key-Aware ALTER DROP COLUMN

## Status

This feature extends the existing descriptor-driven
`ALTER TABLE ... DROP [COLUMN]` lifecycle. The original slice drops ordinary
columns on persistent base tables and rejects current primary-key columns. This
phase admits column drops when MyLite can update existing key descriptors
without reconstructing logical metadata from SQLite schema text.

The goal is narrow: primary-key, unique, nonunique, prefix, metadata-only
fulltext, and auto-increment descriptors must remain coherent after a column is
dropped. Supported integer-family foreign-key columns are rejected with
MySQL-compatible dependency diagnostics rather than leaving stale descriptors.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline drop-column lifecycle:
  `docs/specs/baseline-alter-table-drop-column/specs.md`
- Existing key, fulltext, foreign-key, auto-increment, and key-aware alter
  specs under `docs/specs/`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-foreign-key.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_key_aware_alter_drop_column_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase.

- Dropping a column used by a one-column nonunique, unique, prefix, or fulltext
  secondary index succeeds and removes that index from `SHOW INDEX`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.STATISTICS`.
- Dropping one column from a composite nonunique, unique, or primary key
  succeeds when the remaining key parts are valid. The key name is preserved
  and the remaining key parts are compacted to contiguous positions.
- If dropping one column from a composite unique or primary key would create
  duplicate remaining key tuples, MySQL returns duplicate-key error `1062`.
- Dropping a one-column primary-key column succeeds and removes the primary
  key. MySQL reports affected rows equal to the current row count for this
  shape.
- Dropping an `AUTO_INCREMENT` column succeeds when it otherwise fits the
  supported syntax. For the admitted nonunique-key-backed auto-increment probe,
  MySQL reports zero affected rows.
- Dropping a child foreign-key column fails with error `1828`, SQLSTATE
  `HY000`.
- Dropping a referenced parent foreign-key column fails with error `1829`,
  SQLSTATE `HY000`.
- Successful supported drops report `@@warning_count = 0`.

## Scope

Supported:

- persistent MyLite base tables only;
- one existing `ALTER TABLE table_name DROP [COLUMN] column_name` action only;
- the schema, table, column, reserved-name, single-column-table, last-visible
  column, and physical failure behavior from the existing drop-column slice;
- target columns referenced by current primary-key descriptors;
- target columns referenced by supported unique, nonunique, prefix, and
  metadata-only fulltext secondary-index descriptors;
- target columns carrying the current supported nonunique-key-backed
  auto-increment attribute;
- descriptor mutation rules matching MySQL for this slice:
  - one-part keys over the dropped column are removed;
  - composite keys lose the dropped key part and compact remaining key-part
    ordinals;
  - surviving key descriptor ids, names, physical names, uniqueness, sort
    directions, and prefix lengths are preserved;
- duplicate validation before dropping a column when a surviving primary or
  unique key would be narrowed;
- MySQL-compatible rejection when the dropped column participates in a
  supported foreign key as either a child or parent column.

Deferred:

- multiple `ALTER TABLE` actions;
- `ALGORITHM` / `LOCK` modifiers;
- temporary tables, views, generated columns, triggers, stored routines,
  metadata locks, and privileges;
- CHECK-constrained tables, which remain rejected by the current drop-column
  implementation;
- dropping or rewriting foreign-key descriptors in the same statement;
- unsupported future dependency kinds that need expression dependency
  rewriting;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns call validation, public result
  ownership, and failure cleanup.
- Statement context: owns diagnostics, warnings, affected rows, and the
  statement boundary.
- Parser/AST: unchanged. The existing independently authored drop-column
  grammar remains the syntax authority.
- Analyzer/planner: resolves the target table and column from MyLite
  descriptors, loads key and foreign-key dependencies, validates duplicate
  risks for narrowed unique keys, rejects unsupported dependency shapes, and
  builds a fixed physical mutation plan before any SQLite SQL is generated.
- Catalog: remains authoritative for logical metadata. Dropping a column
  deletes that column descriptor, compacts remaining column ordinals, deletes
  or shrinks affected key descriptors, updates descriptor versions/generations,
  and updates table identity exactly once in the mutation.
- Result and introspection builders: render `SHOW COLUMNS`, `SHOW INDEX`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` rows from descriptors, so
  surviving metadata reflects catalog changes without reading SQLite schema
  text.
- SQLite physical storage: owns row storage and physical b-tree indexes. MyLite
  drops affected generated physical indexes, executes public SQLite
  `ALTER TABLE ... DROP COLUMN`, and recreates surviving generated physical
  indexes from descriptors inside the same transaction.
- Storage/VFS: unchanged. The `.mylite` preamble remains untouched.

## Grammar

This feature does not add syntax. It uses the existing MyLite grammar:

```lemon
alter_table_drop_column_statement ::=
    ALTER TABLE table_name DROP column_keyword_opt identifier.

column_keyword_opt ::= .
column_keyword_opt ::= COLUMN.
```

All wider `ALTER TABLE` forms remain governed by the existing parser and
unsupported-syntax diagnostics.

## Dependency Semantics

The dropped column is resolved by the existing case-insensitive descriptor
lookup. Dependency checks use descriptor ids, not SQLite metadata.

For each descriptor-owned key on the target table:

- if no key part references the dropped column, the key is unchanged;
- if the key has exactly one key part and it references the dropped column, the
  key descriptor and its key-part rows are deleted;
- if a composite key references the dropped column, only that key part is
  deleted and later key-part ordinals are compacted by one;
- remaining primary-key parts keep their current `NOT NULL` descriptors;
- metadata-only fulltext keys follow the same descriptor rule but do not
  create or drop a physical SQLite index.

When a narrowed primary or unique key has remaining key parts, MyLite validates
the existing physical rows for duplicate remaining tuples before mutating the
catalog or physical schema. `NULL` handling follows the existing unique-index
validation policy used by descriptor-owned key creation.

If the target column participates in a supported foreign key as a child column,
MyLite rejects the statement with MySQL error `1828`, SQLSTATE `HY000`, and a
message containing the column name and foreign-key name. If it participates as
a referenced parent column, MyLite rejects with MySQL error `1829`, SQLSTATE
`HY000`, and a message containing the column name, foreign-key name, and child
table name. Child-column dependency checks run before parent-column dependency
checks for deterministic diagnostics.

CHECK-constrained tables remain unsupported in this phase because CHECK
expression rewriting needs expression dependency tracking beyond this
column-id/key-id model.

## Physical SQLite Handling

For affected non-fulltext physical indexes, the physical SQL sequence inside
the existing catalog mutation transaction is:

```sql
DROP INDEX "<physical_index_name>";
ALTER TABLE "<physical_table_name>" DROP COLUMN "<column_name>";
CREATE [UNIQUE] INDEX "<physical_index_name>"
    ON "<physical_table_name>" (<remaining_key_part>[, ...]);
```

The recreate step is omitted for one-part dropped keys. Fulltext descriptors
remain metadata-only and have no SQLite index to drop or recreate.

All identifiers are generated from descriptors and quoted with MyLite's SQLite
identifier helper. No user literals are interpolated. MyLite does not read
SQLite schema text back into logical descriptors and does not use writable
schema editing.

If any physical SQL, catalog update, allocation, or duplicate validation fails,
the SQLite transaction rolls back both catalog and physical changes.

## Result Semantics

Successful supported drops return a non-row result:

- result column count is `0`;
- result row count is `0`;
- `warning_count == 0`;
- `affected_rows == current row count` when the statement removes a
  one-column primary key;
- `affected_rows == 0` for other supported successful drops.

The affected-row behavior is intentionally limited to the MySQL 8.4.9 shapes
verified for this slice.

## Diagnostics

Existing drop-column diagnostics are preserved for syntax errors, missing
default schema, unknown schema, unknown table, reserved names, unsupported
object kind, CHECK-constrained tables, unknown dropped column, attempts to drop
all columns, attempts to drop the last visible column, allocation failures, and
physical SQLite failures.

New or expanded diagnostics:

- duplicate narrowed primary/unique key: MySQL error `1062`, SQLSTATE `23000`;
- child foreign-key dependency: MySQL error `1828`, SQLSTATE `HY000`;
- parent foreign-key dependency: MySQL error `1829`, SQLSTATE `HY000`;
- missing unshadowed SQLite rowid alias for duplicate validation remains a
  MyLite-specific unsupported diagnostic, matching existing key-creation
  validation constraints.

Successful supported drops produce no warnings.

## Compatibility Notes

MyLite intentionally remains narrower than MySQL. MySQL accepts combined
actions and algorithm/lock clauses for many drop-column forms; MyLite still
rejects those shapes until multi-action planning and DDL option handling are
specified.

Foreign-key column drops are rejected rather than implementing a combined
foreign-key drop. Applications must explicitly drop the foreign key first,
matching MySQL's dependency requirement.

## Tests

Required coverage:

- MySQL expectation probes for primary-key, secondary, prefix, fulltext,
  auto-increment, duplicate narrowed unique/primary, and foreign-key cases;
- C runtime coverage that one-column secondary, unique, prefix, fulltext, and
  primary keys are deleted when their only key part is dropped;
- C runtime coverage that composite primary, unique, nonunique, and prefix keys
  shrink to remaining descriptor parts, preserve names/ids/metadata, and
  recreate physical indexes where applicable;
- duplicate enforcement before and after narrowed unique/primary key drops;
- child and parent foreign-key dependency diagnostics;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` after drop;
- row readback, DML after drop, reopen persistence, independent handles, and
  `.mylite` preamble preservation through the existing drop-column test binary;
- existing parser, drop-column, primary-key, secondary-index, fulltext,
  foreign-key, auto-increment, and compatibility lifecycle tests continue to
  pass.

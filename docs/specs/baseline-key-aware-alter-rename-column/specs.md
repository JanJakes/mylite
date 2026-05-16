# Baseline Key-Aware ALTER RENAME COLUMN

## Status

This feature extends the existing descriptor-driven
`ALTER TABLE ... RENAME COLUMN` lifecycle. The original slice renamed ordinary
persistent base-table columns and rejected current primary-key columns. This
phase admits column renames when existing MyLite descriptor dependencies can
continue to identify the same logical column by `column_id`.

The goal is narrow: primary-key, unique, nonunique, prefix, metadata-only
fulltext, and supported integer-family foreign-key descriptors should keep
working after a column is renamed. This is not full MySQL `ALTER TABLE`
support and does not add multi-action alter planning, generated columns,
views, triggers, algorithms, locks, or online DDL behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing rename-column lifecycle:
  `docs/specs/baseline-alter-table-rename-column/specs.md`
- Existing key/index/foreign-key lifecycle specs under `docs/specs/`
- Key-aware `CHANGE` / `MODIFY` expansion:
  `docs/specs/baseline-key-aware-alter-change-modify/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE INDEX`:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_key_aware_alter_rename_column_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase.

- `ALTER TABLE keyed RENAME COLUMN id TO pk_id` succeeds when `id` is the
  primary-key column, reports `ROW_COUNT() = 0`, reports no warnings, and
  renders the primary key over `pk_id`.
- Renaming a column used by unique, nonunique, and prefix secondary indexes
  succeeds and preserves the index names, uniqueness, order, and prefix
  lengths while rendering the new column name.
- Renaming a metadata fulltext column succeeds and renders the fulltext key
  over the new column name.
- Renaming a referenced parent key column updates child foreign-key metadata
  to reference the new parent column name.
- Renaming a child foreign-key column updates the child key and foreign-key
  metadata to use the new child column name.
- Existing row values and column ordinal positions are preserved.

## Scope

Supported:

- persistent MyLite base tables only;
- one existing `ALTER TABLE table_name RENAME COLUMN old_col TO new_col`
  action only;
- the schema, table, column, duplicate-name, same-name no-op, case-only
  rename, and reserved-name behavior from the existing rename-column slice;
- target columns referenced by existing primary-key descriptors;
- target columns referenced by supported unique, nonunique, and prefix
  secondary-index descriptors;
- target columns referenced by metadata-only fulltext index descriptors;
- target columns referenced by the current supported integer-family
  foreign-key descriptors, either as child or parent columns;
- descriptor identity preservation: column id, ordinal, type, nullability,
  default metadata, visibility, and generation semantics remain those of the
  existing rename-column slice;
- zero affected rows and zero warnings for successful in-range renames.

Deferred:

- multiple `ALTER TABLE` actions;
- `ALGORITHM` / `LOCK` modifiers;
- temporary tables, views, generated columns, triggers, stored routines,
  metadata locks, and privileges;
- CHECK-constrained tables, which remain rejected by the current
  rename-column implementation;
- unsupported future dependency kinds that cannot safely follow a column id;
- direct physical index rebuilds, writable-schema editing, or SQLite fork
  patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns call validation, public
  result ownership, and failure cleanup.
- Statement context: owns diagnostics, warnings, affected rows, and the
  statement boundary.
- Parser/AST: unchanged. The existing independently authored
  rename-column grammar remains the syntax authority.
- Analyzer/planner: resolves the target table and columns from MyLite
  descriptors, admits key-dependent columns when dependencies identify the
  same descriptor column id, and continues to reject unsupported object kinds
  and CHECK-constrained tables before physical SQLite SQL exists.
- Catalog: remains authoritative for logical metadata. Renaming a column
  updates only the column descriptor name; key, index-column, and foreign-key
  descriptor rows keep their ids and column-id references.
- Result and introspection builders: render `SHOW COLUMNS`, `SHOW INDEX`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` rows from descriptors, so
  dependency metadata naturally follows the renamed descriptor column.
- SQLite physical storage: owns row storage and physical b-tree indexes.
  MyLite uses SQLite's public `ALTER TABLE ... RENAME COLUMN` on the stable
  physical table name; SQLite updates generated physical index schema for
  current MyLite-created rowid tables. SQLite schema text is not
  MySQL-visible metadata authority.
- Storage/VFS: unchanged. The `.mylite` preamble remains untouched.

## Grammar

This feature does not add syntax. It uses the existing MyLite grammar:

```lemon
alter_table_rename_column_statement ::=
    ALTER TABLE table_name RENAME COLUMN identifier TO identifier.
```

All wider `ALTER TABLE` forms remain governed by the existing parser and
unsupported-syntax diagnostics.

## Resolution And Dependency Semantics

The old column is resolved by the existing case-insensitive descriptor lookup.
The new name is checked against existing descriptors before any physical SQL
is generated. Exact same-name renames remain successful no-ops and do not
advance catalog or SQLite schema generations. Case-only renames update the
visible spelling and perform the physical rename.

Existing dependency descriptors continue to reference the same `column_id`.
Because MyLite key and foreign-key descriptors do not store a separate copy of
the column name, no index-column or foreign-key catalog rows are rewritten.
After the rename, descriptor-driven metadata renders the new name wherever the
old column descriptor was referenced.

CHECK-constrained tables remain unsupported in this phase because CHECK
expression rewriting needs expression dependency tracking beyond this narrow
column-id-following model.

## Physical SQLite Handling

The physical SQL shape remains:

```sql
ALTER TABLE "<physical_table_name>" RENAME COLUMN "<old_column_name>" TO "<new_column_name>"
```

All identifiers are generated from descriptors and quoted with MyLite's
SQLite identifier helper. There are no SQL literals or bound values in this
slice.

SQLite public `ALTER TABLE RENAME COLUMN` updates physical index definitions
created by MyLite for the supported primary, unique, nonunique, and prefix
index descriptors. Metadata-only fulltext descriptors have no physical SQLite
index to rebuild. MyLite does not read SQLite schema text back into logical
descriptors.

No SQLite fork patch is required.

## Diagnostics

Existing rename-column diagnostics are preserved for syntax errors, missing
default schema, unknown schema, unknown table, reserved names, unsupported
object kind, CHECK-constrained tables, unknown old column, duplicate new
column, allocation failures, and physical SQLite failures.

This phase removes the former primary-key-column unsupported diagnostic. A
primary-key column rename now succeeds when the statement otherwise fits the
existing rename-column slice.

Successful supported renames produce no warnings.

## Compatibility Notes

MyLite intentionally remains narrower than MySQL. MySQL accepts combined
actions and algorithm/lock clauses for some rename-column forms; MyLite still
rejects those shapes until multi-action planning and DDL option handling are
specified.

Foreign-key rename support is descriptor-based only for the current supported
integer-family foreign-key subset. It does not add new foreign-key actions,
cross-schema references, `SET NULL`, `SET DEFAULT`, recursive cascades, or
broader constraint support.

## Tests

Required coverage:

- MySQL expectation probes for primary-key, secondary, prefix, fulltext, and
  foreign-key renames;
- C runtime coverage that primary-key column renames preserve rows, descriptor
  ids, `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS`;
- C runtime coverage that unique, nonunique, prefix, and fulltext metadata
  follows the renamed column;
- C runtime coverage that foreign-key parent and child column renames update
  descriptor-driven `SHOW CREATE TABLE` and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` output;
- duplicate enforcement and supported foreign-key enforcement after rename;
- reopen persistence and `.mylite` preamble preservation through the existing
  rename-column test binary;
- existing parser, rename-column, primary-key, secondary-index, fulltext, and
  foreign-key lifecycle tests continue to pass.

# Baseline DROP FOREIGN KEY Lifecycle

## Summary

This phase completes the narrow descriptor-owned foreign-key DDL lifecycle for
the current MyLite baseline:

```sql
ALTER TABLE child DROP FOREIGN KEY fk_name
```

The supported operation removes one foreign-key descriptor from a persistent
base table. It preserves the child-side index, row data, primary/unique/secondary
index descriptors, and physical SQLite tables. Enforcement and introspection
then observe the remaining descriptor state.

This is intentionally not full MySQL foreign-key DDL. It does not add cascades,
composite foreign keys, mutable `foreign_key_checks`, multi-action
`ALTER TABLE`, temporary-table foreign keys, or arbitrary table rebuild logic.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline foreign-key constraints:
  `docs/specs/baseline-foreign-key-constraints/specs.md`
- Baseline `ALTER TABLE ... DROP INDEX`:
  `docs/specs/baseline-alter-table-drop-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, foreign-key constraints:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_drop_foreign_key_lifecycle_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE child DROP FOREIGN KEY fk_name` removes the foreign-key
  constraint but leaves the child-side index in `SHOW CREATE TABLE`.
- Successful drops report `ROW_COUNT() == 0` and `@@warning_count == 0`.
- After the drop, child inserts that would previously fail for missing parent
  rows succeed because the FK enforcement metadata is gone.
- `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` no longer report the dropped
  foreign-key row.
- Explicitly named constraints can be dropped case-insensitively, including
  quoted names with different letter case.
- Unnamed `CREATE TABLE` foreign keys can be dropped by their generated
  `<table>_ibfk_N` name.
- Missing default schema fails with `1046 / 3D000`.
- Unknown schema fails with `1049 / 42000`.
- Unknown table fails with `1146 / 42S02`.
- Unknown foreign-key name fails with `1091 / 42000`.
- `IF EXISTS` after `DROP FOREIGN KEY` is a syntax error.
- MySQL accepts wider forms such as multi-action `ALTER TABLE` and
  `ALGORITHM`/`LOCK` options. They remain deferred for this MyLite slice.

## Scope

Supported:

- persistent MyLite base tables only;
- one single-action `ALTER TABLE table_name DROP FOREIGN KEY foreign_key_name`;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- one unqualified or quoted foreign-key name resolving to an existing
  descriptor-owned child-table FK;
- case-insensitive FK-name resolution matching current descriptor-name policy;
- foreign keys created by supported `CREATE TABLE` table-level definitions and
  `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY`;
- descriptor-backed updates to `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, and
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`;
- post-drop DML behavior where child-side and parent-side FK enforcement no
  longer applies to the removed descriptor;
- row preservation, reopen persistence, table rename/drop interaction, and
  `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for successful drops.

Deferred:

- multi-action `ALTER TABLE`;
- `ALGORITHM`, `LOCK`, `ONLINE`, `VALIDATION`, or other table options;
- `DROP FOREIGN KEY IF EXISTS`;
- `DROP CONSTRAINT`;
- `ALTER TABLE ... DROP CHECK`, check constraints, and constraint renaming;
- cascades, `SET NULL`, `SET DEFAULT`, action timing, and trigger-like effects;
- composite, non-integer, cross-schema, self-referential, temporary-table, or
  view foreign keys beyond the existing FK slice;
- mutable `foreign_key_checks`, disabled-enforcement mode, and privilege
  semantics;
- child-index auto-removal. The current slice preserves child indexes like the
  verified MySQL behavior.

## Ownership Boundaries

- Public API: no ABI change. Applications continue through `mylite_execute()`
  and existing result/diagnostic handles.
- Statement context: owns diagnostics reset, affected rows, warning counts,
  `ROW_COUNT()`, and cleanup on failure.
- Parser/AST: admits the narrow single-action grammar and carries only the
  target table and FK-name nodes. It does not inspect descriptors or SQLite
  schema text.
- Analyzer/planner/runtime: resolves schema, table, and foreign-key names from
  MyLite descriptors, rejects unsupported object kinds, and plans catalog
  deletion.
- Catalog: owns durable FK descriptors. The drop removes rows from
  `_mylite_catalog_foreign_key_columns` and `_mylite_catalog_foreign_keys`
  atomically.
- Result and introspection builders: render post-drop metadata from catalog
  descriptors only.
- SQLite physical storage: no user-table or index DDL is required. The child
  index remains a normal generated SQLite index.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Supported Grammar

MyLite admits only a single FK-drop action:

```sql
ALTER TABLE table_name DROP FOREIGN KEY foreign_key_name
```

The table name may be unqualified or schema-qualified. The foreign-key name is
one identifier or quoted identifier. `IF EXISTS`, multiple actions, options,
and `DROP CONSTRAINT` are not admitted by this grammar slice.

MyLite Lemon-style sketch:

```lemon
statement(A) ::= alter_table_drop_foreign_key_statement(B). {
    A = B;
}

alter_table_drop_foreign_key_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP FOREIGN KEY identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_foreign_key_statement(
        state, A1, T, I);
}
```

## Resolution Semantics

Target table resolution follows the existing table policy:

- unqualified table names require a selected/default schema;
- schema-qualified table names use the explicit schema and do not require a
  selected schema;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, and reserved `_mylite_*` schema/table names use existing diagnostics;
- only persistent base-table descriptors are supported.

Foreign-key resolution is descriptor-owned:

- load current child-table FK descriptors from the MyLite catalog;
- match the requested logical FK name case-insensitively against child-table
  descriptor names;
- reject a missing name with MySQL-compatible `1091 / 42000`;
- do not use SQLite `PRAGMA foreign_key_list`, SQLite schema text, or generated
  physical index names as the logical source of truth.

## Descriptor and Catalog Semantics

On success:

- delete all FK-column descriptor rows for the resolved FK;
- delete the FK descriptor row for the resolved child table and FK ID;
- preserve child and parent table descriptors;
- preserve row data and all column, primary-key, unique-index, and secondary
  index descriptors, including the child-side index that supported the FK;
- update the child table descriptor generation so descriptor caches observe
  the FK set change;
- do not increment SQLite schema generation because physical SQLite schema is
  unchanged.

Failed planning or execution must leave catalog descriptors and physical SQLite
state unchanged.

## Physical SQLite Handling

No generated SQLite DDL is required for the supported operation. Execution uses
the catalog mutation transaction only. The implementation must not inspect
SQLite metadata, rebuild tables, drop physical indexes, or materialize rows in
MyLite.

The absence of physical SQLite DDL is deliberate: MyLite foreign keys are
descriptor-owned, and MySQL preserves the child index when only the FK
constraint is dropped.

## Result Semantics

Successful execution returns through existing non-row statement conventions:

- no row result set;
- `affected_rows == 0`;
- `warning_count == 0`;
- statement diagnostics remain clear.

`ROW_COUNT()` after the statement reports `0` through the existing statement
context machinery.

## Diagnostics

Expected diagnostics for the supported subset:

- syntax errors and deferred grammar: parser `1064 / 42000`;
- missing default schema: existing `1046 / 3D000`;
- unknown schema: existing `1049 / 42000`;
- unknown table: existing `1146 / 42S02`;
- reserved `_mylite_*` target names: existing reserved-name diagnostic;
- unsupported object kind or temporary table: deterministic MyLite unsupported
  diagnostic;
- unknown foreign-key name: `1091 / 42000`, "Can't DROP '<name>'; check that
  column/key exists";
- allocation failure: `MYLITE_NOMEM`;
- unexpected catalog or SQLite failure: deterministic runtime error through
  existing diagnostics.

Successful supported drops emit no warnings.

## Tests

Add a focused C runtime test under `packages/libmylite/tests/`, or extend the
FK lifecycle test if the result remains readable. Coverage must include:

- parser acceptance for unqualified, schema-qualified, quoted, and generated
  FK names;
- parser rejection for `IF EXISTS`, multi-action forms, and options;
- successful drop of named and unnamed FKs;
- `SHOW CREATE TABLE` retains child indexes but omits FK constraints;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, and
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` omit dropped FK rows;
- child inserts and parent updates/deletes are no longer constrained by the
  dropped FK;
- the preserved child index can be dropped afterward with the existing
  `DROP INDEX` path;
- missing default schema, unknown schema, unknown table, unknown FK, and
  reserved-name diagnostics;
- rename/drop/reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles observe independent dropped-FK state;
- zero-initialized cleanup for any new planner object.

Run:

1. `cmake --build --preset dev`
2. Focused parser/FK/drop-index/runtime CTest entries.
3. `packages/libmylite/tests/mysql_baseline_drop_foreign_key_lifecycle_expectations.sh`
4. `cmake --workflow --preset check`

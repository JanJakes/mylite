# Baseline DROP INDEX Lifecycle

## Summary

This phase adds the standalone secondary-index removal form for persistent
MyLite base tables:

```sql
DROP INDEX index_name ON table_name
```

The operation is the standalone companion to the existing limited
`CREATE INDEX`, `CREATE UNIQUE INDEX`, and `ALTER TABLE ... DROP INDEX|KEY`
lifecycles. It removes one descriptor-owned secondary index and its generated
SQLite physical index while preserving table rows, column descriptors, primary
keys, other supported indexes, and file-format invariants.

This is intentionally not full MySQL index DDL. Primary-key drops through
``DROP INDEX `PRIMARY` ON t``, `ALGORITHM` / `LOCK` clauses, `IF EXISTS`,
standalone `DROP KEY`, foreign-key dependency handling, and optimizer
guarantees remain separate slices.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline standalone `CREATE INDEX` lifecycle:
  `docs/specs/baseline-create-index-lifecycle/specs.md`
- Baseline `ALTER TABLE ... DROP INDEX|KEY` lifecycle:
  `docs/specs/baseline-alter-table-drop-index-lifecycle/specs.md`
- Baseline secondary and unique index lifecycle specs:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `DROP INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/drop-index.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_drop_index_lifecycle_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `DROP INDEX k ON t` removes a secondary index and reports
  `ROW_COUNT() == 0` and `@@warning_count == 0`.
- Dropping a unique secondary index removes the uniqueness constraint; later
  duplicate non-`NULL` values for that key column are accepted.
- The removed index disappears from `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS`.
- Schema-qualified target tables work without a selected default schema.
- Index-name matching for the tested subset is case-insensitive.
- Missing default schema, unknown schema, unknown table, unknown index, and
  auto-increment last-key violations match the diagnostics already used by
  the `ALTER TABLE ... DROP INDEX|KEY` slice.
- MySQL accepts `DROP INDEX k ON t ALGORITHM=INPLACE LOCK=NONE`.
- MySQL accepts ``DROP INDEX `PRIMARY` ON t`` to drop a primary key. Unquoted
  `DROP INDEX PRIMARY ON t` is a syntax error because `PRIMARY` is reserved.
- MySQL does not admit `DROP INDEX IF EXISTS k ON t`.

## Scope

Supported:

- persistent MyLite base tables only;
- one `DROP INDEX index_name ON table_name` statement;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- one unqualified index name or quoted index name resolving to an existing
  descriptor-owned secondary index;
- secondary descriptors with `kind = SECONDARY`, including both
  nonunique and unique indexes;
- secondary indexes created by `CREATE TABLE`, `CREATE TABLE ... LIKE`,
  `ALTER TABLE ... ADD INDEX|KEY`, `CREATE INDEX`, and
  `CREATE UNIQUE INDEX`;
- auto-increment safety checks that prevent removing the last supported key on
  an auto-increment column;
- descriptor-backed metadata after the drop through `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, `SHOW INDEX`, limited `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`;
- row-value preservation, reopen persistence, table rename/drop interaction,
  independent file-backed handles, and `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for successful supported drops.

Deferred:

- ``DROP INDEX `PRIMARY` ON table_name`` primary-key drops; use the existing
  `ALTER TABLE ... DROP PRIMARY KEY` subset instead;
- unquoted `DROP INDEX PRIMARY ON table_name`, which remains a syntax error;
- `DROP KEY index_name ON table_name`;
- `IF EXISTS`;
- `ALGORITHM` and `LOCK` clauses;
- foreign-key dependency checks, cascades, triggers, temporary tables, views,
  partitions, privileges, online DDL behavior, and implicit-commit emulation;
- prefix, descending, functional, fulltext, spatial, invisible, and expression
  indexes beyond existing descriptor kinds;
- optimizer/index-use guarantees;
- SQLite fork patches.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, statement completion, and cleanup on failure.
- Parser/AST: admits only the narrow standalone statement and preserves the
  logical index-name and table-name nodes. It does not inspect descriptors or
  SQLite schema.
- Analyzer/planner/runtime: resolves schema, table, and index names from
  MyLite descriptors, enforces secondary-only and auto-increment key rules,
  plans catalog deletion, and builds physical SQLite DDL from the descriptor.
- Catalog: MyLite index and index-column descriptors are authoritative for
  logical index state. SQLite schema text is not inspected for logical indexes.
- Result/introspection builders: existing descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, and `INFORMATION_SCHEMA` paths render the
  post-drop descriptor state.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite uses standard SQLite `DROP INDEX` against
  the generated physical index name. No SQLite fork hook is required.

## Supported Grammar

MyLite admits only:

```sql
DROP INDEX index_name ON table_name
```

The table name may be unqualified or schema-qualified. The index name is one
identifier or quoted identifier. The grammar does not admit unquoted
`PRIMARY` as the index name.

MyLite Lemon-style sketch:

```lemon
statement(A) ::= drop_index_statement(B). {
    A = B;
}

drop_index_statement(A) ::= DROP(D) INDEX identifier(I) ON table_name(T). {
    A = mylite_sql_parser_make_drop_index_statement(state, D, I, T);
}
```

The AST keeps this standalone statement distinct from
`ALTER TABLE ... DROP INDEX|KEY`, while both may share descriptor-planning and
physical-execution helpers.

## Resolution Semantics

Target table resolution follows existing table policy:

- unqualified table names require a selected/default schema;
- schema-qualified table names use the explicit schema and do not require a
  selected schema;
- missing default schema fails with `1046 / 3D000`;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL
  is generated;
- only persistent base-table descriptors are supported.

Index resolution is descriptor-owned:

- load current table columns and index descriptors from the MyLite catalog;
- match the requested logical index name case-insensitively against table-local
  descriptor index names;
- reject a missing name with `1091 / 42000`;
- reject a resolved primary-key descriptor as unsupported in this slice, using
  a deterministic MyLite diagnostic that directs callers to
  `ALTER TABLE ... DROP PRIMARY KEY`;
- allow both unique and nonunique secondary indexes;
- do not use SQLite schema metadata to discover, validate, or classify
  indexes.

## Descriptor and Catalog Semantics

On success:

- delete all index-column descriptor rows for the resolved secondary index;
- delete the secondary index descriptor row;
- preserve the table descriptor, column descriptors, row values, primary-key
  descriptor, other secondary descriptors, auto-increment counter, physical
  table name, and schema descriptor;
- update table descriptor identity/generation so descriptor caches observe the
  new index set;
- increment SQLite schema generation because physical SQLite schema changed.

`CREATE TABLE ... LIKE` after the drop clones only remaining descriptors.
`SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` recompute key labels
from remaining descriptors.

Failed planning or execution must leave both catalog descriptors and physical
SQLite indexes unchanged.

## Auto-Increment Interaction

MySQL requires each auto-increment column to remain indexed. For the current
MyLite descriptor subset:

- if the dropped index does not reference the auto-increment column, allow it;
- if the dropped index references the auto-increment column but a primary key
  or another supported secondary index remains on that same column, allow it;
- if the dropped index is the last supported key on the auto-increment column,
  reject with `1075 / 42000`;
- preserve the auto-increment descriptor and counter on successful drops.

The check is descriptor-only and does not scan row data.

## Physical SQLite Handling

Generated SQL shape:

```sql
DROP INDEX "_mylite_user_index_<index_id>"
```

Rules:

- generate SQL only from the resolved index descriptor physical name;
- quote every generated identifier;
- execute physical SQLite `DROP INDEX` inside the same catalog mutation
  transaction used for descriptor deletion;
- roll back the catalog mutation if the physical drop fails;
- surface `MYLITE_NOMEM` for allocation failure and a deterministic physical
  schema diagnostic for unexpected SQLite failures;
- do not materialize table rows in MyLite and do not rebuild physical tables;
- use public SQLite DDL support only. No SQLite fork patch is required.

## Result Semantics

Successful execution returns through existing non-row statement conventions:

- no row result set;
- `affected_rows == 0`;
- `warning_count == 0`;
- statement diagnostics remain clear.

## Diagnostics

The implementation must cover deterministic diagnostics for:

- parser errors and unsupported grammar (`IF EXISTS`, standalone `DROP KEY`,
  options, unquoted `PRIMARY`, multiple statements inside one parse unit when
  not otherwise supported);
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` schema/table names;
- unsupported target object kind once non-base-table descriptors exist;
- unknown index names with MySQL-compatible `1091 / 42000`;
- primary-key descriptor drops through standalone `DROP INDEX`;
- dropping the last key on an auto-increment column with `1075 / 42000`;
- physical SQLite failures;
- allocation failures.

## Test Plan

Add MySQL-runtime expectation coverage and fast C runtime coverage for:

- successful nonunique and unique secondary-index drops;
- index drops created by create-time indexes, `ALTER TABLE ... ADD INDEX`,
  standalone `CREATE INDEX`, standalone `CREATE UNIQUE INDEX`, and
  `CREATE TABLE ... LIKE`;
- metadata updates in `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`;
- unique-index removal allowing later duplicate values;
- schema-qualified and unqualified target resolution;
- case-insensitive index-name matching;
- missing default schema, unknown schema, unknown table, unknown index,
  reserved names, primary-key descriptors, and auto-increment last-key
  diagnostics;
- unsupported syntax for `IF EXISTS`, standalone `DROP KEY`, options,
  unquoted primary-key drops, and multiple/drop variants outside scope;
- reopen persistence, table rename/drop interaction, independent file-backed
  handles, physical SQLite index count, and `.mylite` preamble preservation;
- zero-initialized cleanup of any new planner object.

## Performance and SQLite Integration

The runtime performs descriptor lookups, a small auto-increment metadata check,
one catalog mutation, and one SQLite schema DDL statement. It does not scan or
materialize user rows. The supported path remains close to SQLite's native
index-drop cost while keeping MyLite descriptors authoritative.


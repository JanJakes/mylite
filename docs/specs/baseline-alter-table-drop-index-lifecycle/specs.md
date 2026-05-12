# Baseline ALTER TABLE DROP INDEX Lifecycle

## Summary

This phase completes the narrow secondary-index table-action lifecycle for
persistent MyLite base tables:

```sql
ALTER TABLE table_name DROP INDEX index_name
ALTER TABLE table_name DROP KEY index_name
```

The supported operation removes one descriptor-owned unique or nonunique
secondary index and its generated SQLite physical index. It builds on the
current descriptor catalog, create-time secondary and unique indexes,
`ALTER TABLE ... ADD INDEX`, primary-key and auto-increment rules, descriptor
metadata, row DML, and file-backed `.mylite` storage.

This is intentionally not full MySQL index DDL. Standalone
`DROP INDEX index_name ON table_name` is covered by
`docs/specs/baseline-drop-index-lifecycle/specs.md`; quoted-primary drops,
multi-action `ALTER TABLE`, online DDL options, foreign-key dependencies, and
index renames remain separate slices.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline secondary index lifecycle:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`
- Baseline unique index lifecycle:
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline `ALTER TABLE ... ADD INDEX` lifecycle:
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`
- Baseline `ALTER TABLE ... DROP PRIMARY KEY`:
  `docs/specs/baseline-alter-table-drop-primary-key/specs.md`
- MySQL 8.4 Reference Manual, `DROP INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/drop-index.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_alter_table_drop_index_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t DROP INDEX k` and `ALTER TABLE t DROP KEY k` are synonyms for
  ordinary secondary-index removal.
- Successful secondary-index drops report `ROW_COUNT() == 0` and
  `@@warning_count == 0`.
- The removed index disappears from `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS`.
- Dropping a unique secondary index removes the uniqueness constraint; later
  duplicate values for that column are accepted by MySQL.
- Index-name matching for the tested subset is case-insensitive.
- Dropping an unknown index fails with `1091 / 42000` and a "Can't DROP"
  diagnostic.
- Missing default schema, unknown schema, and unknown table use the existing
  MySQL diagnostics for `ALTER TABLE` target resolution.
- Dropping a secondary index on an `AUTO_INCREMENT` column succeeds while a
  primary key or another supported index still indexes that column.
- Dropping the last key on an `AUTO_INCREMENT` column fails with
  `1075 / 42000`.
- MySQL accepts wider forms such as quoted ``DROP INDEX `PRIMARY``` /
  ``DROP KEY `PRIMARY``` primary-key drops, multi-action `ALTER TABLE`, and
  `ALGORITHM` / `LOCK` clauses. They remain deferred here.

## Scope

Supported:

- persistent MyLite base tables only;
- one `ALTER TABLE table_name DROP INDEX index_name` action;
- one `ALTER TABLE table_name DROP KEY index_name` action;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- one unqualified index name or quoted index name resolving to an existing
  descriptor-owned secondary index;
- supported secondary descriptors with `kind = SECONDARY`, including both
  `is_unique = 0` nonunique indexes and `is_unique = 1` unique indexes;
- current single-column secondary index descriptors created by `CREATE TABLE`,
  `CREATE TABLE ... LIKE`, and `ALTER TABLE ... ADD INDEX`;
- auto-increment safety checks that prevent removing the last supported key on
  an auto-increment column;
- descriptor-backed metadata after the drop through `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, `SHOW INDEX`, limited `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`;
- row-value preservation, reopen persistence, table rename/drop interaction,
  independent file-backed handles, and `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for supported successful secondary-index drops.

Deferred:

- quoted-primary index drops, including ``ALTER TABLE t DROP INDEX `PRIMARY```
  and standalone ``DROP INDEX `PRIMARY` ON t``; use the existing
  `ALTER TABLE ... DROP PRIMARY KEY` subset instead;
- unquoted `DROP INDEX PRIMARY`, which is a MySQL syntax error and remains a
  syntax error;
- multi-action `ALTER TABLE`;
- `DROP FOREIGN KEY`, `DROP CONSTRAINT`, check constraints, generated invisible
  primary keys, foreign-key dependency checks, cascades, triggers, temporary
  tables, views, partitions, privileges, algorithms, locks, and implicit-commit
  emulation;
- prefix, descending, functional, fulltext, spatial, invisible, and expression
  indexes beyond existing descriptor kinds;
- optimizer/index-use guarantees;
- SQLite fork patches.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, statement completion, and cleanup on failure.
- Parser/AST: admits the narrow single-action `ALTER TABLE` drop-index/drop-key
  shape and preserves the target table and logical index-name nodes. It does
  not inspect descriptors or SQLite schema.
- Analyzer/planner/runtime: resolves schema/table/index names from MyLite
  descriptors, enforces auto-increment key rules, plans catalog deletion, and
  builds physical SQLite DDL from descriptor physical names.
- Catalog: MyLite index and index-column descriptors are authoritative for
  logical index state. SQLite schema text is not inspected for logical indexes.
- Result/introspection builders: existing `SHOW` and `INFORMATION_SCHEMA`
  paths render the post-drop descriptor state.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite uses a standard SQLite `DROP INDEX` against
  the generated physical index name. No SQLite fork hook is required.

## Supported Grammar

MyLite admits only a single secondary-index drop action:

```sql
ALTER TABLE table_name DROP INDEX index_name
ALTER TABLE table_name DROP KEY index_name
```

The table name may be unqualified or schema-qualified. The index name is one
identifier or quoted identifier. The grammar does not admit `PRIMARY` as an
unquoted identifier for this action.

MyLite Lemon-style sketch:

```lemon
statement(A) ::= alter_table_drop_index_statement(B). {
    A = B;
}

alter_table_drop_index_statement(A) ::=
    ALTER TABLE table_name(T) DROP drop_index_keyword(K) identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(state, T, K, I);
}

drop_index_keyword ::= INDEX.
drop_index_keyword ::= KEY.
```

The implementation should keep this AST kind distinct from
`ALTER TABLE ... DROP PRIMARY KEY`, even if both eventually share helper code.

## Resolution Semantics

Target table resolution follows existing table policy:

- unqualified table names require a selected/default schema;
- schema-qualified table names use the explicit schema and do not require a
  selected schema;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, and reserved `_mylite_*` schema/table names use existing diagnostics;
- only persistent base-table descriptors are supported.

Index resolution is descriptor-owned:

- load current table columns and index descriptors from the MyLite catalog;
- match the requested logical index name case-insensitively against table-local
  descriptor index names;
- reject a missing name with `1091 / 42000`;
- reject a resolved primary-key descriptor as unsupported in this slice unless
  a later implementation deliberately delegates it to the existing
  `DROP PRIMARY KEY` path with MySQL-compatible row-count semantics;
- allow both unique and nonunique secondary indexes;
- do not use SQLite schema metadata to discover, validate, or classify indexes.

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
from remaining descriptors: `PRI` remains highest precedence, then eligible
unique secondary keys, then nonunique secondary `MUL`.

Failed planning or execution must leave both catalog descriptors and physical
SQLite indexes unchanged.

## Auto-Increment Interaction

MySQL requires each auto-increment column to remain indexed. For the current
MyLite descriptor subset:

- if the dropped index does not reference the auto-increment column, allow it;
- if the dropped index references the auto-increment column but a primary key or
  another supported secondary index remains on that same column, allow it;
- if the dropped index is the last supported key on the auto-increment column,
  reject with `1075 / 42000`;
- preserve the auto-increment descriptor and counter on successful drops.

The check is descriptor-only and should not scan row data.

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

This differs from `ALTER TABLE ... DROP PRIMARY KEY`, whose current supported
path reports the table row count. The secondary-index drop behavior is
separately verified against MySQL 8.4.9.

## Diagnostics

Supported diagnostics:

- syntax errors and unsupported grammar: existing parser diagnostic
  `1064 / 42000`;
- missing default schema: `1046 / 3D000`;
- unknown explicit schema: `1049 / 42000`;
- unknown target table: `1146 / 42S02`;
- reserved `_mylite_*` schema/table names: existing MyLite reserved-name
  diagnostics;
- unsupported object kind: future non-base-table diagnostic matching the
  surrounding `ALTER TABLE` policy;
- unknown index/key name: `1091 / 42000`;
- dropping the last key for an auto-increment column: `1075 / 42000`;
- primary-key drop through `DROP INDEX` / `DROP KEY`: deterministic unsupported
  diagnostic for this slice, while unquoted `PRIMARY` remains a syntax error;
- multi-action `ALTER TABLE`, algorithms, locks, foreign-key and constraint
  forms, partition clauses, temporary tables, views, and other deferred
  syntax: deterministic syntax or unsupported diagnostics;
- physical SQLite failure: deterministic internal/physical schema diagnostic;
- allocation failure: `MYLITE_NOMEM` and handle-owned diagnostics.

Successful admitted forms produce no warnings.

## Performance Boundary

The implementation must stay descriptor-driven and close to SQLite:

- no C-side table row materialization;
- no row scan for ordinary secondary-index removal;
- no physical table rebuild;
- one catalog lookup path to load columns/indexes;
- one SQLite `DROP INDEX` schema statement;
- no optimizer-plan claims beyond removing the physical index object.

## Tests

Add a fast C runtime test, preferably
`packages/libmylite/tests/runtime_alter_table_drop_index_test.c`, registered as
`libmylite.runtime.alter_table_drop_index`.

Coverage must include:

- successful `DROP INDEX` and `DROP KEY`;
- dropping create-time nonunique secondary indexes;
- dropping `ALTER TABLE ... ADD INDEX` secondary indexes;
- dropping unique secondary indexes and allowing later duplicate row values;
- metadata after drop through `SHOW COLUMNS`, `SHOW CREATE TABLE`,
  `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`;
- case-insensitive logical index-name matching;
- schema-qualified and unqualified target table resolution;
- missing default schema, unknown schema, unknown table, unknown index, and
  reserved `_mylite_*` target names;
- auto-increment cases where a primary key remains, another secondary key
  remains, and the dropped index would be the last key;
- row-value preservation, DML after drop, reopen persistence, table rename/drop
  interaction, independent file-backed handles, and `.mylite` preamble safety;
- physical SQLite index count before and after drop;
- unsupported syntax rejected deterministically: quoted-primary drops if
  deferred, unquoted `PRIMARY`, multi-action alter, algorithms, locks,
  `DROP FOREIGN KEY`, `DROP CONSTRAINT`, `RENAME INDEX`, temporary, view,
  partition forms, and index options;
- zero-initialized cleanup for any new planner/result objects;
- existing parser, runtime, catalog, row-values, DML, primary-key,
  secondary-index, unique-index, auto-increment, metadata, storage, and VFS
  tests still pass.

The MySQL comparison script
`packages/libmylite/tests/mysql_baseline_alter_table_drop_index_expectations.sh`
must pass against MySQL 8.4.9 before implementation behavior is claimed.

## Compatibility Documentation

Implementation must update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-indexes-constraints.md`;
- `docs/compatibility/sql-table-ddl.md`;

with limited wording for the exact `ALTER TABLE ... DROP INDEX` / `DROP KEY`
secondary-index subset. Do not claim quoted-primary drop-index forms, full
constraint dependency behavior, foreign keys, renames, visibility, comments,
algorithms, locks, optimizer guarantees, or full metadata parity.

## Verification

Before the feature is complete, run:

1. `cmake --build --preset dev`
2. focused CTest entries for parser, secondary indexes, unique indexes,
   primary keys, auto-increment, information schema, add-index, and the new
   runtime test;
3. `packages/libmylite/tests/mysql_baseline_alter_table_drop_index_expectations.sh`
4. `cmake --workflow --preset check`

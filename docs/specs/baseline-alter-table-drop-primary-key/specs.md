# Baseline ALTER TABLE DROP PRIMARY KEY

## Summary

This phase completes the narrow primary-key DDL lifecycle with
`ALTER TABLE table_name DROP PRIMARY KEY` for persistent MyLite base tables.
It builds on descriptor-owned create-time primary keys, composite primary keys,
`ALTER TABLE ... ADD PRIMARY KEY`, supported secondary indexes, auto-increment
metadata, row-value DML, descriptor-driven `SHOW` and limited
`INFORMATION_SCHEMA` surfaces, file-backed `.mylite` storage, and the existing
statement context.

The supported operation removes the current primary-key descriptor and its
generated SQLite unique index. It does not rebuild columns to become nullable
again, does not change row values, and does not broaden MyLite's index or
constraint model.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline primary key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Baseline composite primary key lifecycle:
  `docs/specs/baseline-composite-primary-key-lifecycle/specs.md`
- Baseline ALTER TABLE ADD PRIMARY KEY:
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`
- Baseline ALTER TABLE ADD composite primary key:
  `docs/specs/baseline-alter-table-add-composite-primary-key/specs.md`
- Baseline auto-increment lifecycle:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- Baseline information schema constraints:
  `docs/specs/baseline-information-schema-constraints/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, primary-key and unique constraints:
  <https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_alter_table_drop_primary_key_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script for this feature records runtime probes for the exact
supported and deferred surface. Observed behavior that defines this slice:

- `ALTER TABLE t DROP PRIMARY KEY` succeeds for tables with a primary key.
- Successful drops report `ROW_COUNT()` equal to the number of rows in the
  table at the time of the operation and `@@warning_count == 0`.
- Dropping the primary key removes `PRI` from `SHOW COLUMNS`, removes
  `PRIMARY` rows from `SHOW INDEX`, removes the `PRIMARY KEY (...)` clause from
  `SHOW CREATE TABLE`, and removes primary-key rows from
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `STATISTICS`.
- Columns that became `NOT NULL` because of the primary key remain `NOT NULL`
  after the primary key is dropped. A previous implicit or explicit `DEFAULT
  NULL` is not restored as a default clause in `SHOW CREATE TABLE`.
- Existing row values are preserved, and duplicate key-part values are allowed
  by later DML after the primary key is removed.
- Supported secondary indexes are preserved.
- `ALTER TABLE t DROP PRIMARY KEY` on a table without a primary key fails with
  `1091 / 42000` and a "Can't DROP 'PRIMARY'" diagnostic.
- Dropping the only key on an `AUTO_INCREMENT` column fails with
  `1075 / 42000`. If another supported secondary index remains on the
  auto-increment column, MySQL accepts the drop and keeps the column
  `auto_increment`.
- MySQL accepts wider forms such as multi-action `ALTER TABLE` statements,
  `DROP CONSTRAINT` where it can resolve the constraint, online DDL options,
  locks, and algorithms; those remain deferred.

## Scope

Supported:

- persistent base tables only;
- one `ALTER TABLE table_name DROP PRIMARY KEY` action;
- unqualified and schema-qualified table names using the existing selected
  schema policy;
- primary keys created by the current `CREATE TABLE` and
  `ALTER TABLE ... ADD PRIMARY KEY` subsets, including ordered integer-family
  composite primary keys;
- descriptor-owned primary-key deletion from MyLite catalog tables;
- generated SQLite physical index removal using the descriptor physical index
  name;
- preservation of table descriptors, column descriptors, physical row values,
  supported secondary indexes, and auto-increment descriptors when MySQL permits
  the drop;
- affected-row reporting equal to the table's current row count;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `SHOW TABLE STATUS`, and limited `INFORMATION_SCHEMA` surfaces after the drop;
- reopen persistence, table rename/drop behavior, independent file-backed
  handles, and `.mylite` preamble preservation.

Deferred:

- multi-action `ALTER TABLE`, including `DROP PRIMARY KEY` combined with
  `ADD`, `DROP`, `MODIFY`, `CHANGE`, table options, or secondary-index actions;
- `DROP CONSTRAINT`, `DROP INDEX PRIMARY`, standalone `DROP INDEX`, and
  secondary-index drops;
- named constraints, foreign keys, cascades, triggers, check constraints,
  temporary tables, views, partitions, privileges, online DDL algorithms, locks,
  and implicit-commit emulation;
- changing column nullability or defaults while dropping the primary key;
- dropping an auto-increment primary key unless a supported secondary index
  keeps the auto-increment column indexed;
- SQLite fork patches.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result and diagnostic accessors.
- Statement context: owns diagnostics, warning count, affected rows,
  transaction completion, and cleanup on failure.
- Parser/AST: admits only the narrow `ALTER TABLE ... DROP PRIMARY KEY` shape
  and source spans. It does not inspect descriptors, rows, SQLite schema, or
  auto-increment state.
- Analyzer/planner/runtime: resolves the target table from MyLite descriptors,
  loads the current primary-key descriptor and key parts, verifies
  auto-increment restrictions, counts physical rows for affected-row reporting,
  plans catalog deletion, and builds physical SQLite SQL from descriptor names.
- Catalog module: owns durable index and index-column descriptor rows,
  descriptor generations, and cache invalidation. Primary-key metadata is not
  read back from SQLite schema text.
- Result/introspection builders: render the post-drop metadata from catalog
  descriptors through existing `SHOW` and `INFORMATION_SCHEMA` paths.
- Storage/VFS: owns the `.mylite` preamble and shifted SQLite payload. This
  feature writes only SQLite payload catalog/index data and must not touch the
  preamble.
- SQLite physical storage: stores rows and generated physical indexes. MyLite
  uses public SQLite APIs to drop the generated index and to count rows; MyLite
  remains responsible for MySQL-compatible diagnostics.

## Supported Grammar

The feature extends the existing single-action `ALTER TABLE` grammar:

```sql
ALTER TABLE table_name DROP PRIMARY KEY
```

MyLite Lemon-style snippet:

```lemon
alter_table_drop_primary_key_statement ::=
    ALTER TABLE table_name DROP PRIMARY KEY.
```

The parser should keep `DROP PRIMARY KEY` separate from unsupported
`DROP INDEX`, `DROP KEY`, `DROP CONSTRAINT`, and multi-action `ALTER TABLE`
forms. Unsupported forms that are not admitted by the parser remain syntax
errors.

## Schema and Name Resolution

The target table follows existing table-name policy:

- unqualified table names require a selected/default schema;
- schema-qualified names use the explicit schema;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, and reserved `_mylite_*` names use existing diagnostics;
- only persistent base-table descriptors are supported.

The primary key is resolved from descriptor-owned index metadata:

- if no primary-key descriptor exists for the table, fail with `1091 / 42000`;
- the descriptor's generated physical index name is the only SQLite index name
  used by generated SQL;
- descriptor name matching follows current MyLite catalog identifier policy.

## Descriptor Semantics

On success:

- delete the primary index-column descriptor rows for the table's primary-key
  index;
- delete the primary index descriptor row;
- preserve every table and column descriptor, including `is_nullable`, default
  kind/value, visibility, physical names, type metadata, and auto-increment
  metadata;
- preserve supported unique and nonunique secondary-index descriptors and their
  physical indexes;
- preserve all row values and table identity.

If any catalog or physical step fails, all catalog and physical changes must
roll back atomically to the pre-statement state.

## Auto-Increment Interaction

MySQL requires an auto-increment column to remain indexed. For the current
MyLite subset:

- if the primary key contains the auto-increment column and no supported
  secondary index on that same column remains, reject with `1075 / 42000`;
- if a supported secondary index on the auto-increment column remains, allow the
  drop, preserve the auto-increment descriptor, and keep generated values
  available to the current `INSERT ... VALUES`, `INSERT ... SET`, and
  `CREATE TABLE ... LIKE` clone paths through the same descriptor-owned
  counter model;
- composite primary keys with auto-increment are not currently admitted by
  MyLite's create/add primary-key subset, so no composite auto-increment drop
  path is included.

## Physical SQLite Handling

This feature uses public SQLite APIs and does not require a SQLite fork patch.

Generated SQL shape:

```sql
DROP INDEX "_mylite_user_index_<index_id>"
```

Rules:

- generate SQL only from descriptors and stable physical names;
- quote every identifier;
- count rows with a descriptor-built `SELECT COUNT(*) FROM
  "_mylite_user_table_<table_id>"` statement before mutating descriptors;
- use SQLite schema execution helpers and normal statement transactions;
- do not modify physical row tables, row values, column declarations, or the
  bundled SQLite fork.

## Result Semantics

Successful execution returns through existing non-row statement result
conventions:

- no row result set;
- `affected_rows == physical row count at drop time`;
- `warning_count == 0`;
- statement diagnostics remain clear.

## Diagnostics

The supported subset uses these diagnostics:

- syntax errors: existing parser diagnostics;
- unsupported grammar or semantic forms: deterministic MyLite unsupported
  diagnostics unless a MySQL-compatible diagnostic already exists locally;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved schema/table names: existing reserved-name diagnostics;
- unsupported object kind: MyLite unsupported diagnostic;
- no primary key to drop: `1091 / 42000`;
- auto-increment column would become unindexed: `1075 / 42000`;
- `sql_require_primary_key` behavior remains bounded by the current fixed
  system-variable surface;
- physical SQLite failure: existing physical-row/schema failure diagnostics;
- allocation failure: `MYLITE_NOMEM` with handle diagnostics.

## Performance Boundary

The implementation must not materialize table rows in C memory. It may run one
SQLite `COUNT(*)` query for affected-row reporting and one `DROP INDEX` schema
statement. It must not rebuild the physical table merely to change column
declarations, and it must not scan rows for duplicate or `NULL` validation
because dropping a primary key removes, rather than adds, a constraint.

No optimizer/index-use claim is made for query planning in this slice.

## Test Plan

- MySQL 8.4.9 expectation script covering successful single-column,
  added-key, and composite-key drops; metadata after drop; affected rows;
  duplicate DML after drop; secondary-index preservation; auto-increment
  restriction and secondary-index allowance; no-primary-key and name-resolution
  diagnostics; and deliberately deferred wider forms.
- C runtime tests extending the existing ALTER ADD PRIMARY KEY test binary or a
  new focused test binary for success, metadata, DML after drop, affected-row
  counts, warnings, auto-increment interaction, schema qualification, reopen
  persistence, rename/drop interactions, independent file-backed handles,
  unsupported syntax, and preamble preservation.
- Parser tests for the supported `DROP PRIMARY KEY` statement and rejected
  unsupported alternatives.
- Existing primary-key, composite primary-key, alter-add-primary-key,
  auto-increment, insert/update/delete, row-value, show/information-schema,
  parser, storage, and file-format tests must keep passing.

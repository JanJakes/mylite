# Baseline Foreign Key SET NULL Actions

## Summary

This phase extends MyLite's descriptor-owned foreign-key action subset with:

```sql
CREATE TABLE child (
  parent_id INT,
  CONSTRAINT fk_child_parent FOREIGN KEY (parent_id)
    REFERENCES parent(id) ON DELETE SET NULL ON UPDATE SET NULL
);
```

The implementation remains a direct, non-recursive, descriptor-driven subset for
same-schema persistent base tables and the existing compatible integer-family FK
descriptors. `SET NULL` is admitted in `CREATE TABLE` and the current
single-action `ALTER TABLE ... ADD FOREIGN KEY` subset, persisted in MyLite FK
descriptors, rendered through existing metadata surfaces, and applied for
supported parent-side `DELETE` and `UPDATE` statements by setting all child FK
columns in each matched tuple to `NULL`.

This phase does not add `SET DEFAULT`, `MATCH`, recursive actions,
self-referential action cycles, non-integer foreign keys, temporary-table
foreign keys, cross-schema references, triggers, mutable `foreign_key_checks`,
or SQLite-native foreign-key authority.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Existing FK baseline:
  `docs/specs/baseline-foreign-key-constraints/specs.md`
- Existing composite FK baseline:
  `docs/specs/baseline-composite-foreign-key-constraints/specs.md`
- Existing FK symbol/action baseline:
  `docs/specs/baseline-foreign-key-symbol-actions/specs.md`
- Existing DROP FOREIGN KEY baseline:
  `docs/specs/baseline-drop-foreign-key-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-referential-constraints-table.html
- Observed MySQL 8.4.9 behavior recorded by
  `packages/libmylite/tests/mysql_baseline_foreign_key_set_null_actions_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ON DELETE SET NULL` and `ON UPDATE SET NULL` are accepted foreign-key
  reference options in `CREATE TABLE` and `ALTER TABLE ... ADD FOREIGN KEY`.
- `SHOW CREATE TABLE` renders `SET NULL` as a non-default action and renders
  `ON DELETE` before `ON UPDATE`.
- `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS.UPDATE_RULE` and `DELETE_RULE`
  expose `SET NULL`.
- A parent `DELETE` covered by `ON DELETE SET NULL` reports the parent deleted
  row count and zero warnings, while matching child FK columns become `NULL`.
- A parent `UPDATE` covered by `ON UPDATE SET NULL` reports the parent changed
  row count and zero warnings, while matching child FK columns become `NULL`.
- For composite child FKs, every child FK column in the matched tuple becomes
  `NULL`. The changed parent part alone is not the only nulled child part.
- `MATCH SIMPLE` behavior remains visible: child rows whose FK tuple already
  contains any `NULL` are not matched by the parent action.
- A child column that participates in a `SET NULL` action cannot be `NOT NULL`.
  MySQL rejects both create-time and alter-added constraints with
  `1830 / HY000`, using a message shaped as:
  `Column 'pid' cannot be NOT NULL: needed in a foreign key constraint 'fk' SET NULL`.
- MySQL 8.4.9 accepts and renders `SET DEFAULT` in FK metadata, but InnoDB
  parent writes covered by `SET DEFAULT` failed with parent-row referenced
  errors in the probes for this slice. MyLite keeps `SET DEFAULT` unsupported
  until that stored-rule-but-restrictive behavior is specified separately.

## Scope

Supported DDL:

- persistent same-schema base tables only;
- table-level `CREATE TABLE` foreign keys supported by the existing FK
  baseline;
- named and unnamed single-action `ALTER TABLE ... ADD FOREIGN KEY` forms
  supported by the existing FK action baseline;
- one-or-more compatible integer-family FK parts already supported by the
  existing single-column and composite FK phases;
- optional `ON DELETE SET NULL` and `ON UPDATE SET NULL`, independently or
  together, in either action-clause order;
- existing `CASCADE`, `RESTRICT`, `NO ACTION`, default action, FK index-symbol,
  generated-name, metadata, and drop-FK behavior unchanged.

Supported DML effects:

- direct non-recursive `ON DELETE SET NULL` child updates before supported
  parent-side single-table `DELETE` statements;
- direct non-recursive `ON UPDATE SET NULL` child updates before supported
  parent-side single-table `UPDATE` statements that change a referenced parent
  key column;
- parent statement affected rows and warning counts follow the parent write,
  not the internal child action write;
- changed child rows persist and are visible through the existing descriptor
  `SELECT` surfaces after close/reopen.

Deferred:

- `SET DEFAULT`;
- recursive actions through child tables that are themselves parents in other
  FK descriptors;
- self-referential `SET NULL` cycles;
- action clauses on unsupported FK shapes such as temporary tables,
  cross-schema references, non-integer references, prefix/expression parts, or
  unsupported object kinds;
- `MATCH` clauses, mutable `foreign_key_checks = 0`, triggers, metadata locks,
  privilege checks, full InnoDB dependency behavior, and native SQLite FK
  enforcement as a user-visible authority.

## Ownership Boundaries

- Public API: no ABI change. Applications continue through `mylite_execute()`
  and existing result/diagnostic accessors.
- Statement context: owns diagnostics, warning counts, affected rows,
  `ROW_COUNT()`, statement transactions, rollback on failure, and action
  failure cleanup.
- Parser/AST: admits `SET NULL` action nodes under existing FK action syntax.
  It does not inspect descriptors or SQLite schema text.
- Analyzer/planner: resolves names from MyLite descriptors, decides generated
  names, records action-rule text, rejects `SET NULL` over `NOT NULL` child
  columns, and decides whether a parent write can use a direct set-based
  action.
- Catalog: existing `_mylite_catalog_foreign_keys.update_rule` and
  `delete_rule` text columns are authoritative. No catalog schema migration is
  required.
- Result builders: render `SHOW CREATE TABLE` and information-schema rows from
  MyLite FK descriptors only.
- SQLite physical storage: stores user rows and generated indexes. MyLite builds
  parameterized SQLite statements for child `SET NULL` updates, parent-target
  subqueries, and validation probes.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Syntax

MyLite Lemon-syntax sketch for the extended grammar:

```lemon
foreign_key_action_clause ::= ON DELETE foreign_key_reference_option.
foreign_key_action_clause ::= ON UPDATE foreign_key_reference_option.

foreign_key_reference_option ::= CASCADE.
foreign_key_reference_option ::= RESTRICT.
foreign_key_reference_option ::= NO ACTION.
foreign_key_reference_option ::= SET NULL.
```

The existing parser shape continues to decide whether an action is a delete or
update action. The analyzer continues to reject duplicate action timings such
as two `ON DELETE` clauses before catalog mutation.

`SET DEFAULT` remains a syntax error in MyLite for this phase.

## Descriptor Rules

Each supported FK descriptor keeps:

- `update_rule`: `CASCADE`, `RESTRICT`, `NO ACTION`, or `SET NULL`;
- `delete_rule`: `CASCADE`, `RESTRICT`, `NO ACTION`, or `SET NULL`;
- `match_option`: still `NONE`;
- ordered child and parent FK part descriptors from the existing FK catalog.

Planner validation rejects a `SET NULL` action when any child FK column for that
constraint is descriptor `NOT NULL`. This applies if either `delete_rule` or
`update_rule` is `SET NULL`. The diagnostic is `1830 / HY000` with the child
column and FK constraint name. The check runs after generated FK names are known
and before catalog rows or physical child indexes are committed.

## Direct SET NULL Actions

Direct `SET NULL` actions run inside the same MyLite statement transaction as
the parent write. They are MyLite wrapper SQL built on public SQLite prepared
statements, not SQLite native FK actions.

### Delete

Before deleting parent rows, MyLite applies each direct `ON DELETE SET NULL`
descriptor whose parent table is the target table:

1. Build a descriptor-owned subquery for the exact parent rows selected by the
   current `DELETE` plan, including admitted `WHERE`, `ORDER BY`, and `LIMIT`
   behavior.
2. Update matching direct child rows with a single SQLite `UPDATE child AS c
   SET child_fk_col = NULL[, ...] WHERE EXISTS (...)` shape.
3. Keep existing `MATCH SIMPLE` guards so child rows with any `NULL` FK part do
   not match.
4. Continue with the parent delete.
5. Run existing parent-side validation for remaining `RESTRICT` / `NO ACTION`
   descriptors and as a safety check.

The internal child update does not change the public affected-row count for the
parent statement.

### Update

Before updating parent rows, MyLite applies each direct `ON UPDATE SET NULL`
descriptor whose referenced parent key columns are changed by the supported
single-table `UPDATE` plan:

1. Build a descriptor-owned parent-row subquery for the exact parent rows that
   will change, including admitted `WHERE`, `ORDER BY`, `LIMIT`, and
   changed-row filtering.
2. Update matching direct child rows with all child FK columns set to `NULL`.
3. Execute the parent update.
4. Validate mutated child tables' own FK descriptors after the parent rows are
   updated, so child-side FK failures roll back both internal child action and
   parent update.
5. Run existing child-side and parent-side validation as a safety check.

This slice follows the existing parent update action limitation: the action
fires for supported parent updates whose assignment list lets the current
planner determine that a referenced parent key part changes. Broader
multi-assignment parent-key updates are deferred with the existing validation
behavior until the update planner grows a general changed-parent-key map.

## Physical SQLite Shape

Generated action SQL uses stable physical table names from descriptors and
quotes every generated SQLite identifier. Literal data comes from existing
descriptor-built predicate and update planners and is bound with prepared
statement parameters. `SET NULL` assignment itself emits SQL `NULL` tokens
because the value is not user input.

The child update shape is set-based. MyLite must not materialize parent or
child tables into process memory to decide action side effects. Existing
generated child indexes remain ordinary SQLite indexes and should support child
lookups where SQLite's planner can use them.

No SQLite fork patch is required for this phase. If later recursive actions or
lower-overhead execution hooks need deeper engine integration, that must be
specified separately as a narrow SQLite extension point.

## Rendering And Metadata

`SHOW CREATE TABLE` renders FK actions from descriptor rules:

- omit `NO ACTION`;
- append `ON DELETE SET NULL` when `delete_rule = 'SET NULL'`;
- append `ON UPDATE SET NULL` when `update_rule = 'SET NULL'`;
- preserve existing `CASCADE` and `RESTRICT` rendering;
- render `ON DELETE` before `ON UPDATE`.

`INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` exposes stored `UPDATE_RULE` and
`DELETE_RULE` text, so descriptor values of `SET NULL` become visible there.
`SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
`KEY_COLUMN_USAGE` continue to render the same index and FK part descriptors.

## Diagnostics

Supported diagnostics:

- `SET NULL` over any `NOT NULL` child FK column returns `1830 / HY000` with a
  MySQL-compatible message naming the column and FK constraint;
- duplicate action timing such as two `ON DELETE` clauses returns the existing
  deterministic analyze error before catalog mutation;
- missing default schema, unknown schema/table, duplicate table, duplicate
  index name, duplicate FK constraint name, unknown child/parent columns,
  incompatible descriptors, and missing parent unique keys reuse existing FK
  diagnostics;
- default, `NO ACTION`, and `RESTRICT` parent write violations return existing
  `1451 / 23000`;
- child-side missing parent rows after direct actions return existing
  `1452 / 23000` if validation finds an inconsistency;
- recursive/self-referential action shapes not yet supported return existing
  deterministic unsupported diagnostics before mutation;
- `SET DEFAULT`, explicit `MATCH`, unsupported object kinds, unsupported FK
  part shapes, and unsupported action recursion remain rejected by existing
  parser/analyzer/runtime policy;
- allocation and physical SQLite failures use existing runtime conventions.

## Tests

Add MySQL-runtime-verified expectation coverage and fast C runtime/parser tests
for:

- parser admission of `ON DELETE SET NULL`, `ON UPDATE SET NULL`, and both
  together;
- parser rejection of `SET DEFAULT`;
- create-time `SET NULL` metadata through `SHOW CREATE TABLE` and
  `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`;
- alter-added `SET NULL` metadata;
- one-column `ON DELETE SET NULL` and `ON UPDATE SET NULL` parent actions;
- composite `SET NULL` actions setting all child FK columns to `NULL`;
- nullable child rows with partial `NULL` tuples remaining unmatched;
- affected-row counts, warning counts, result shape, rollback on failure, and
  persistence after reopen;
- `NOT NULL` child FK column rejection for create-time and alter-added
  constraints;
- interoperation with existing `CASCADE`, `RESTRICT`, `NO ACTION`, generated
  child indexes, drop-FK, rename/drop table, update/delete, file-format, VFS,
  parser, and full workflow tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-indexes-constraints.md`,
`docs/compatibility/sql-table-dml.md`,
`docs/compatibility/metadata-information-schema.md`, and
`docs/compatibility/sql-show-statements.md` for the exact subset.

Do not claim full InnoDB foreign keys, recursive actions, `SET DEFAULT`,
`MATCH`, mutable `foreign_key_checks`, triggers, metadata locks, privileges, or
native SQLite FK enforcement.

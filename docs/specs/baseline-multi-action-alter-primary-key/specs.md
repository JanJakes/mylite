# Baseline Multi-Action ALTER Primary Key

## Summary

This phase extends the descriptor-driven multi-action `ALTER TABLE` baseline so
primary-key actions can participate in the same atomic action list as the
current supported persistent-table column, secondary-index, unique-index, and
catalog default actions.

Supported primary-key forms:

```sql
ALTER TABLE table_name ADD PRIMARY KEY (key_part[, ...]), action ...
ALTER TABLE table_name DROP PRIMARY KEY, action ...
```

The feature is deliberately narrower than full MySQL `ALTER TABLE`. It reuses
the current single-action `ADD PRIMARY KEY` and `DROP PRIMARY KEY` semantic
subsets, applies every action through one catalog mutation, and rolls back the
whole statement on the first failure.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline multi-action ALTER TABLE:
  `docs/specs/baseline-multi-action-alter-table/specs.md`
- Baseline multi-action ALTER default changes:
  `docs/specs/baseline-multi-action-alter-defaults/specs.md`
- Baseline ALTER TABLE ADD PRIMARY KEY:
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`
- Baseline ALTER TABLE DROP PRIMARY KEY:
  `docs/specs/baseline-alter-table-drop-primary-key/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, atomic DDL:
  <https://dev.mysql.com/doc/refman/8.4/en/atomic-ddl.html>
- MySQL 8.4 Reference Manual, primary-key and unique constraints:
  <https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_multi_action_alter_primary_key_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t ADD PRIMARY KEY(id), ADD KEY k_v(v)` succeeds when existing
  `id` values are non-`NULL` and unique. It reports `ROW_COUNT() == 0` and
  `@@warning_count == 0`.
- `ALTER TABLE t DROP PRIMARY KEY, ADD PRIMARY KEY(v)` succeeds when `v`
  values are non-`NULL` and unique. It reports `ROW_COUNT() == 0` and
  `@@warning_count == 0`.
- `ALTER TABLE t ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY(id)` succeeds on
  an empty table. The new column is rendered before the new primary key in
  `SHOW CREATE TABLE`.
- `ALTER TABLE t ADD COLUMN id INT NOT NULL DEFAULT 7, ADD PRIMARY KEY(id)`
  succeeds on an empty table and preserves the explicit default.
- When a later primary-key action fails, earlier actions are rolled back. For
  example, `ADD KEY k_v(v), ADD PRIMARY KEY(id)` over duplicate `id` values
  fails with `1062 / 23000` and does not leave `k_v`.
- `DROP PRIMARY KEY, ADD PRIMARY KEY(v)` over duplicate `v` values fails with
  `1062 / 23000` and leaves the original primary key intact.
- Adding a primary key over existing `NULL` values fails with
  `1138 / 22004` and rolls back earlier actions.
- Adding a second primary key fails with `1068 / 42000` and rolls back later
  actions that would otherwise be valid.
- Dropping a missing primary key fails with `1091 / 42000` and rolls back later
  actions.
- `ALTER TABLE t ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY(id)` on a
  non-empty table fails with duplicate entry `0` for key `t.PRIMARY` and rolls
  back the added column.
- `ALTER TABLE t DROP PRIMARY KEY, ADD KEY k_id(id)` succeeds for an
  `AUTO_INCREMENT` primary-key column. `ADD KEY k_id(id), DROP PRIMARY KEY`
  also succeeds.
- `ALTER TABLE t DROP PRIMARY KEY, ADD KEY k_v(v)` fails with
  `1075 / 42000` when `id` is the only `AUTO_INCREMENT` column and the final
  key set does not index it.
- `ADD PRIMARY KEY USING HASH(...)` in a multi-action statement is accepted by
  MySQL with warning `3502`. MyLite keeps the current multi-action no-warning
  policy and rejects that warning-producing form in this phase.

## Scope

Supported:

- persistent MyLite base tables only;
- two or more comma-separated actions on one target table;
- the existing multi-action table-name resolution and default-schema policy;
- the existing single-action `ADD PRIMARY KEY` subset, including integer-family
  and supported ASCII `CHAR` / `VARCHAR` key parts, composite keys, optional
  key-part `ASC` / `DESC`, duplicate and `NULL` existing-row validation,
  descriptor column nullability updates, key-length validation, generated
  SQLite unique-index creation, `SHOW` metadata, and limited
  `INFORMATION_SCHEMA` metadata;
- the existing single-action `DROP PRIMARY KEY` subset, including descriptor
  primary-key deletion, generated SQLite index removal, foreign-key dependency
  checks, and preservation of rows, columns, secondary indexes, and former
  primary-key column `NOT NULL` state;
- mixing the primary-key actions with the current no-warning multi-action
  `ADD COLUMN`, ordinary `ADD INDEX` / `ADD KEY`, `ADD UNIQUE`,
  `ADD CONSTRAINT ... UNIQUE`, `DROP INDEX` / `DROP KEY`,
  `ALTER ... SET DEFAULT`, and `ALTER ... DROP DEFAULT` subsets;
- later actions resolving descriptor changes made by earlier actions in the
  same uncommitted catalog mutation, including `ADD COLUMN id ..., ADD PRIMARY
  KEY(id)`;
- final-state validation that each `AUTO_INCREMENT` column remains indexed by
  the first key part of some surviving primary or secondary index, including
  cases where the supporting key is added later than `DROP PRIMARY KEY` in the
  same action list;
- rollback of every catalog and physical SQLite change if any admitted action
  fails;
- successful result shape with no result rows, `affected_rows == 0`, and
  `warning_count == 0`.

Deferred:

- temporary tables;
- warning-producing primary-key adds, including `USING HASH`;
- trailing multi-action `ALGORITHM` / `LOCK` option lists;
- `ADD CONSTRAINT name PRIMARY KEY (...)` if not already covered by the
  single-action primary-key grammar;
- primary prefix parts, non-ASCII string key values, expression key parts,
  table-qualified key parts, generated invisible primary keys, and
  auto-increment conversion;
- `DROP CONSTRAINT`, `DROP INDEX PRIMARY`, and standalone primary-key drops
  through `DROP INDEX`;
- multi-action foreign keys, check constraints, rename-index actions, index
  visibility changes, table options, `AUTO_INCREMENT` table options, column
  `MODIFY` / `CHANGE`, column drops, column renames, and partition clauses;
- online DDL scheduling semantics, metadata locks, triggers, privilege
  semantics, optimizer guarantees, and SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. Applications continue to call `mylite_execute()` and
  release result objects through the existing API.
- Statement context: owns diagnostics reset, warning accumulation, affected
  rows, implicit DDL transaction boundaries, and cleanup on failure.
- Parser/AST: owns the independently authored action-list syntax. It stores
  one shared table name and action child nodes without catalog or SQLite
  inspection.
- Analyzer/planner/runtime: reuses existing single-action primary-key planners
  through the shared target-table statement view, validates multi-action
  no-warning scope, performs final auto-increment key validation, and calls
  lower-level helpers that assume an active catalog mutation.
- Catalog module: remains the logical authority. The feature writes durable
  table, column, index, and index-column descriptors through one
  `mylite_catalog_mutation` and never reconstructs logical metadata from
  SQLite schema text.
- Result and introspection builders: existing descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, limited `INFORMATION_SCHEMA`, and DML paths observe
  the committed final descriptor state.
- SQLite physical storage: stores rows and generated physical indexes. MyLite
  emits standard SQLite index DDL from descriptor physical names inside the same
  transaction as catalog writes.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are not modified.

## Grammar

MyLite admits primary-key actions inside the existing multi-action action-list
grammar only when the statement has at least two actions. Single-action
`ALTER TABLE ... ADD PRIMARY KEY ...` and `ALTER TABLE ... DROP PRIMARY KEY`
remain separate statements and keep their existing optional single-action
option tails.

Independent MyLite Lemon-syntax sketch:

```lemon
alter_table_multi_first_action(A) ::= ADD(T) primary_key_definition(P) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_primary_key_statement(
            state, T, NULL, P, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_first_action(A) ::= DROP(T) PRIMARY KEY(K) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_primary_key_statement(
            state, T, NULL, K, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_action(A) ::= ADD(T) primary_key_definition(P). {
    A = mylite_sql_parser_make_alter_table_add_primary_key_statement(
        state, T, NULL, P, mylite_sql_parser_empty_alter_table_options());
}

alter_table_multi_action(A) ::= DROP(T) PRIMARY KEY(K). {
    A = mylite_sql_parser_make_alter_table_drop_primary_key_statement(
        state, T, NULL, K, mylite_sql_parser_empty_alter_table_options());
}
```

The child action nodes intentionally carry no table child. Runtime attaches the
outer target table node through the same statement-view mechanism used by the
existing multi-action implementation.

## Name Resolution

The target table follows existing table-name policy:

- unqualified names require a selected/default schema;
- schema-qualified names use the explicit schema;
- unknown schemas, unknown tables, missing default schema, reserved
  `_mylite_*` names, temporary tables, and non-base object kinds use the
  existing single-action diagnostics unless this spec names a narrower
  multi-action diagnostic;
- every primary-key key part and every mixed action column/index name is
  resolved from MyLite descriptors visible in the current uncommitted mutation.

Descriptor identifier matching follows the current MyLite catalog policy for
case-insensitive MySQL-style identifiers. This phase does not introduce new
collation or identifier-folding behavior.

## Execution Semantics

Execution follows the existing multi-action framework:

1. Create one result object.
2. Begin one `mylite_catalog_mutation`.
3. For each action, attach the shared target table to the child action node and
   run the corresponding single-action planner.
4. Reject warning-producing primary-key adds before mutating descriptors.
5. Execute catalog and physical SQLite changes through mutation-aware helpers.
6. Validate the final auto-increment key rule against the descriptor state
   visible inside the mutation.
7. Commit once. On failure, roll back the catalog mutation and SQLite
   transaction.
8. Increment `sqlite_schema_generation` once if any physical schema changed.
9. Return a non-row result with `affected_rows == 0`.

`ADD PRIMARY KEY` in mutation:

- validates existing rows for `NULL`, duplicates, supported string-key values,
  and key length before writing descriptors;
- allocates a new durable index id and generated SQLite physical index name;
- replaces each key-part column descriptor as `NOT NULL` using the existing
  primary-key default-preservation policy;
- inserts primary-key and index-column descriptors;
- creates the generated SQLite unique index from descriptor physical names;
- updates table identity in the mutation.

`DROP PRIMARY KEY` in mutation:

- loads the current primary-key descriptor and parts;
- enforces supported foreign-key dependency checks before mutation;
- defers auto-increment key preservation to final-state validation so later
  actions in the same statement can supply the supporting index;
- deletes the primary-key descriptor and index-column descriptors;
- drops the generated SQLite physical index;
- updates table identity in the mutation.

## Auto-Increment Final-State Validation

MySQL requires an `AUTO_INCREMENT` column to be indexed. For multi-action
primary-key changes, the relevant state is the final table definition, not only
the state immediately before a `DROP PRIMARY KEY` action. MyLite therefore
validates the final descriptor state before commit:

- load final column descriptors and final index descriptors for the target
  table from the active mutation;
- for each `AUTO_INCREMENT` column, require at least one surviving index whose
  first key part is that column;
- accept primary or secondary indexes that satisfy the rule;
- fail with existing MySQL-shaped `1075 / 42000` diagnostics if no such key
  exists;
- roll back the full action list on failure.

This validation is internal to the multi-action executor. Single-action
`DROP PRIMARY KEY` keeps the existing immediate validation and affected-row
behavior.

## Physical SQLite Handling

The feature uses MyLite wrapper/translation code with public SQLite APIs. It
does not rely on SQLite parsing MySQL syntax and does not require a SQLite fork
patch.

Generated SQL uses:

- stable descriptor-owned physical table names such as
  `_mylite_user_table_<table_id>`;
- generated descriptor-owned physical index names;
- quoted SQLite identifiers;
- standard SQLite `CREATE UNIQUE INDEX` and `DROP INDEX` shapes already used by
  single-action primary-key DDL;
- no physical row materialization except the existing primary-key validation
  probes for `NULL`, duplicate tuples, supported string-key values, and row
  counts where already needed by single-action planners.

All logical metadata remains descriptor-owned. SQLite schema text is never used
as the source of MyLite logical primary-key state.

## Result Semantics

Successful multi-action primary-key statements:

- return through the existing public result object convention for non-query
  statements;
- return no result rows;
- report `affected_rows == 0`, including `DROP PRIMARY KEY` actions that would
  report the table row count when executed as a single-action statement;
- report `warning_count == 0` for admitted in-scope statements;
- update descriptor-driven `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW INDEX`,
  `SHOW TABLE STATUS`, and limited `INFORMATION_SCHEMA.COLUMNS`,
  `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` metadata after commit.

## Diagnostics

Use existing MySQL-compatible diagnostics where available:

- syntax error for unsupported grammar;
- missing default schema: existing `1046 / 3D000`;
- unknown schema: existing `1049 / 42000`;
- unknown table: existing `1146 / 42S02`;
- reserved `_mylite_*` schema, table, column, or index names: existing reserved
  name diagnostic;
- unsupported temporary or non-base table target: multi-action persistent
  base-table unsupported diagnostic;
- unknown primary-key, secondary-index, default, or added-column names:
  existing deterministic name-resolution diagnostics;
- duplicate primary key definition: existing `1068 / 42000`;
- duplicate existing key tuple: existing `1062 / 23000`;
- `NULL` existing primary-key value: existing `1138 / 22004`;
- missing primary key for `DROP PRIMARY KEY`: existing `1091 / 42000`;
- auto-increment column left unindexed in the final descriptor state:
  existing `1075 / 42000`;
- unsupported or warning-producing primary-key options: deterministic
  unsupported diagnostics for the multi-action slice;
- physical SQLite failures: existing physical SQLite row/schema diagnostics;
- allocation failures: existing `MYLITE_NOMEM` / `HY001` behavior.

## Tests

Add or extend fast plain C tests under `packages/libmylite/tests/` and register
no new binary unless the existing multi-action lifecycle test becomes unclear.

Required coverage:

- parser acceptance of `ADD PRIMARY KEY` and `DROP PRIMARY KEY` in first and
  later multi-action positions;
- parser preservation of existing single-action option tails and syntax errors
  for unsupported multi-action trailing option tails;
- successful `ADD PRIMARY KEY, ADD KEY`;
- successful `DROP PRIMARY KEY, ADD PRIMARY KEY`;
- successful `ADD COLUMN, ADD PRIMARY KEY` on an empty table;
- successful `DROP PRIMARY KEY, ADD KEY(id)` over an auto-increment primary key
  and the reversed action order;
- final-state auto-increment failure for `DROP PRIMARY KEY, ADD KEY(v)`;
- rollback when a later primary-key add fails because of duplicates or `NULL`;
- rollback when a primary-key swap fails because the replacement key has
  duplicates;
- rollback when `ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY(id)` fails on a
  non-empty table because the implicit backfill creates duplicate `0` values;
- unknown schema/table/key/column diagnostics through representative cases;
- rejection of temporary table targets and warning-producing `USING HASH`
  primary-key adds in multi-action lists;
- affected rows, warning count, no result rows, `SHOW CREATE TABLE`,
  `SHOW INDEX`, limited `INFORMATION_SCHEMA`, DML enforcement after commit,
  reopen persistence, and `.mylite` preamble preservation;
- existing parser, runtime lifecycle, index, primary-key, default, file-format,
  VFS, and workflow checks remain passing.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-indexes-constraints.md`
to say that the dedicated limited multi-action ALTER slice now includes the
no-warning `ADD PRIMARY KEY` and `DROP PRIMARY KEY` subsets. Do not claim full
multi-action `ALTER TABLE`, constraint syntax, foreign keys, checks, table
options, online DDL semantics, or broader expression/key support.

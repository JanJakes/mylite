# Baseline Multi-Action ALTER Defaults

## Summary

This phase extends the current descriptor-driven multi-action `ALTER TABLE`
slice with catalog-only default changes:

```sql
ALTER TABLE table_name
    ALTER [COLUMN] column_name SET DEFAULT default_value,
    ALTER [COLUMN] column_name DROP DEFAULT
```

The feature remains deliberately narrow. It admits two or more actions on one
persistent base table when each default action targets a column that existed
before the statement began. It reuses the existing single-action default
conversion and metadata rules, executes the action list inside one MyLite
catalog mutation, and rolls the whole list back on the first failure.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing default specs:
  `docs/specs/baseline-alter-column-set-default/specs.md` and
  `docs/specs/baseline-alter-column-drop-default/specs.md`
- Existing multi-action spec:
  `docs/specs/baseline-multi-action-alter-table/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, data type default values:
  <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
- SQLite fork policy: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_multi_action_alter_defaults_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The MySQL manual lists `ALTER [COLUMN] col_name SET DEFAULT ...` and
`ALTER [COLUMN] col_name DROP DEFAULT` as `ALTER TABLE` actions, and lists
comma-separated alter actions after the table name. Runtime probes for this
phase establish:

- `ALTER TABLE t ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT` succeeds with
  `ROW_COUNT() == 0` and `@@warning_count == 0`.
- `SET DEFAULT` preserves existing rows, updates later omitted-column inserts,
  and updates `SHOW COLUMNS`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS`.
- `DROP DEFAULT` preserves existing rows, removes the default clause from
  `SHOW CREATE TABLE`, reports `NULL` in `SHOW COLUMNS`, and makes later
  omitted-column inserts fail with `1364 / HY000`.
- Default actions can be mixed with existing-index actions and with `ADD
  COLUMN` actions that do not become the default action target.
- `ALTER TABLE t ADD COLUMN c INT, ALTER c SET DEFAULT 7` fails with
  `1054 / 42S22`, `Unknown column 'c' in 't'`. This differs from
  `ADD COLUMN c INT, ADD INDEX k_c(c)`, which MySQL accepts.
- A failing later default action rolls back earlier default metadata changes.
- MySQL accepts broader multi-action lists and warning-producing default
  actions outside this phase.

## Scope

Supported:

- persistent MyLite base tables only;
- two or more comma-separated actions on one target table;
- existing supported multi-action `ADD [COLUMN]`, ordinary `ADD INDEX` /
  `ADD KEY`, `ADD UNIQUE` / named unique constraint, and `DROP INDEX` /
  `DROP KEY` actions;
- new `ALTER [COLUMN] column_name SET DEFAULT default_value` actions using the
  current single-action default-value envelope;
- new `ALTER [COLUMN] column_name DROP DEFAULT` actions using the current
  single-action dropped-default descriptor state;
- default action targets that existed in the table descriptor before the
  multi-action statement began;
- later actions observing earlier supported descriptor changes where MySQL
  does, such as index actions over earlier added columns and later inserts
  observing default metadata committed by the action list;
- one catalog mutation, one commit, and rollback of all descriptor and physical
  SQLite changes if any action fails;
- no-row DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for successful supported statements.

Deferred:

- default actions over columns added earlier in the same statement;
- warning-producing default actions, including current zero-temporal default
  warning cases;
- temporary tables;
- trailing multi-action `ALGORITHM` / `LOCK` option tails;
- primary-key, foreign-key, check, rename-index, visibility, table-option,
  column drop/rename/modify/change/default-character-set/order/force actions
  inside these default action lists unless already admitted by the existing
  multi-action slice;
- parenthesized `ADD (...)` lists, partitions, privileges, triggers, storage
  engine scheduling, or metadata-lock semantics;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. Applications keep using `mylite_execute()` and the
  existing result APIs.
- Statement context: owns diagnostics reset, warning accounting, affected-row
  reporting, and cleanup.
- Parser/AST: owns only the independently authored action-list syntax. Child
  action nodes use the existing single-action statement node kinds and carry no
  table node; the outer multi-action statement carries the shared target table.
- Analyzer/planner/runtime: reuses single-action default planners through a
  temporary shared-target AST view, enforces the "pre-existing default target"
  rule, and applies lower-level catalog mutation helpers.
- Catalog: remains the logical authority for defaults. The action list updates
  column descriptors inside one `mylite_catalog_mutation`.
- Result and introspection builders: continue reading descriptor defaults for
  `SHOW`, `DESCRIBE`, `SHOW CREATE TABLE`, `CREATE TABLE ... LIKE`, and
  `INFORMATION_SCHEMA`.
- SQLite physical row storage: unchanged for default-only actions. Existing
  add/drop index or add-column actions continue to use descriptor-built
  physical SQLite DDL through the existing multi-action path.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are preserved.

## Grammar

Independent MyLite Lemon-syntax sketch:

```lemon
alter_table_multi_first_action(A) ::=
    ALTER(T) column_keyword_opt identifier(C) SET DEFAULT(D) NULL(N) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_set_default_statement(
            state, T, NULL, C, mylite_sql_parser_make_column_default_null(state, D, N)));
}

alter_table_multi_first_action(A) ::=
    ALTER(T) column_keyword_opt identifier(C) SET DEFAULT(D) column_default_value(V) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_set_default_statement(
            state, T, NULL, C, mylite_sql_parser_make_column_default_value(state, D, V)));
}

alter_table_multi_first_action(A) ::=
    ALTER(T) column_keyword_opt identifier(C) DROP DEFAULT(D) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_default_statement(state, T, NULL, C, D));
}

alter_table_multi_action(A) ::=
    ALTER(T) column_keyword_opt identifier(C) SET DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, T, NULL, C, mylite_sql_parser_make_column_default_null(state, D, N));
}

alter_table_multi_action(A) ::=
    ALTER(T) column_keyword_opt identifier(C) SET DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, T, NULL, C, mylite_sql_parser_make_column_default_value(state, D, V));
}

alter_table_multi_action(A) ::=
    ALTER(T) column_keyword_opt identifier(C) DROP DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_drop_default_statement(state, T, NULL, C, D);
}
```

The grammar deliberately keeps table-qualified default targets as syntax
errors, matching the current single-action default grammar.

## Execution Semantics

At statement start, MyLite records the columns that are added by already
executed actions in the current action list. Before planning a default action,
the runtime checks the default target against that added-column set. If the
target was added earlier in the same statement, MyLite returns the same
unknown-column diagnostic used for absent descriptor columns, matching the
tested MySQL behavior for this edge.

For each admitted default action:

1. Build the shared-target statement view from the outer table node and child
   action node.
2. Run the existing single-action `SET DEFAULT` or `DROP DEFAULT` planner.
3. Reject non-base targets and warning-producing `SET DEFAULT` plans for this
   multi-action phase.
4. Apply the descriptor change through a lower-level helper that assumes the
   outer catalog mutation is active.

`SET DEFAULT` keeps the existing single-action conversion envelope for admitted
descriptor types: integer-family, decimal, approximate, `CHAR`, `VARCHAR`,
`BINARY`, `VARBINARY`, limited `ENUM`, limited `SET`, `BIT`, `YEAR`, `DATE`,
`TIME`, `DATETIME`, and `TIMESTAMP`, including the currently supported
literal, parenthesized-expression, and current-date/time/timestamp subsets.

`DROP DEFAULT` writes the existing durable no-explicit-default descriptor
state.

Successful default-only action lists do not change physical SQLite schema and
must not increment `sqlite_schema_generation`. Mixed lists increment it only
when an admitted non-default action changes the physical schema.

## Diagnostics

Existing single-action diagnostics are preserved for missing default schema,
unknown schema, unknown table, reserved names, unsupported object kind,
unknown target column, invalid or unsupported default values, integer and
type-specific range failures, `NULL` into incompatible columns, allocation
failures, and catalog mutation failures.

New or explicitly preserved multi-action diagnostics:

- default action targeting a column added earlier in the same statement:
  `1054 / 42S22` unknown column;
- warning-producing `SET DEFAULT` inside a multi-action statement:
  deterministic unsupported-feature diagnostic;
- temporary-table targets remain unsupported by the multi-action phase;
- unsupported action kinds remain rejected by the existing multi-action
  unsupported diagnostic;
- any later failure rolls back earlier descriptor and physical changes.

## SQLite Integration

Default actions are MyLite catalog mutations. No generated SQLite DDL or DML is
needed for default-only actions, and no SQLite fork patch or optional SQLite
syntax is needed. Mixed action lists reuse the existing public-SQLite DDL path
for add-column and secondary-index physical changes.

## Tests

Tests must cover:

- parser success for multi-action `SET DEFAULT`, `DROP DEFAULT`, and mixed
  default/index/add-column forms;
- parser rejection for table-qualified default targets and unsupported tails;
- successful `SET DEFAULT` plus `DROP DEFAULT` in one statement;
- future inserts, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  reflecting committed default metadata;
- rollback of earlier default changes when a later default action fails;
- rollback of earlier physical changes when a later default action fails;
- `ADD COLUMN c, ALTER c SET DEFAULT ...` rejected as unknown column;
- schema-qualified target names, missing default schema, unknown schema,
  unknown table, unknown default column, reserved target names, and temporary
  table targets;
- warning-producing `SET DEFAULT` rejected in multi-action;
- affected rows, warning count, and absence of result rows;
- reopen persistence and `.mylite` preamble preservation;
- existing parser, default, DDL, DML, introspection, and multi-action tests.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` to include
the limited multi-action default-change subset without claiming full
multi-action `ALTER TABLE`, temporary tables, warning-producing default
changes, default actions over same-statement added columns, or online-DDL
semantics.


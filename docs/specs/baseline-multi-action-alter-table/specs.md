# Baseline Multi-Action ALTER TABLE

## Summary

This phase admits the first descriptor-driven multi-action `ALTER TABLE` slice
for persistent MyLite base tables. The supported form combines two or more
already-supported column and secondary-index actions in one statement:

```sql
ALTER TABLE table_name
    ADD [COLUMN] column_definition,
    ADD INDEX|KEY [index_name] (key_part[, ...]),
    ADD UNIQUE [INDEX|KEY] [index_name] (key_part[, ...]),
    ADD CONSTRAINT [symbol] UNIQUE [INDEX|KEY] [index_name] (key_part[, ...]),
    DROP INDEX|KEY index_name
```

The goal is not full MySQL `ALTER TABLE`. The goal is an atomic migration
building block: MyLite plans each admitted action from logical descriptors,
applies catalog and physical SQLite changes inside one MyLite catalog mutation,
commits once on success, and rolls back the complete action list on the first
failure.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing single-action specs:
  `docs/specs/baseline-alter-table-add-column/specs.md`,
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-add-unique-lifecycle/specs.md`, and
  `docs/specs/baseline-alter-table-drop-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, atomic DDL:
  <https://dev.mysql.com/doc/refman/8.4/en/atomic-ddl.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_multi_action_alter_table_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t ADD COLUMN a INT DEFAULT 7, ADD COLUMN b INT NOT NULL DEFAULT 8`
  succeeds, reports `ROW_COUNT() == 0`, leaves `@@warning_count == 0`, and
  backfills existing rows with the same values as the corresponding
  single-action add-column forms.
- `ALTER TABLE t ADD COLUMN c INT, ADD INDEX k_c (c)` succeeds and the new
  index can refer to the earlier column added by the same statement.
- `ALTER TABLE t DROP INDEX k_v, ADD INDEX k_v2 (v)` succeeds and updates
  `SHOW INDEX` atomically.
- `ALTER TABLE t ADD INDEX k_v(v), ADD UNIQUE u_a(a)` rolls the full statement
  back when the unique add fails because of duplicate existing values; the
  preceding nonunique add is not visible afterwards.
- `ALTER TABLE t ADD COLUMN ok_col INT, ADD UNIQUE u_v(v)` rolls the added
  column back when the unique add fails.
- Supported successful multi-action forms in the probes report
  `ROW_COUNT() == 0` and `@@warning_count == 0`.
- MySQL accepts broader multi-action lists, including primary keys, foreign
  keys, check constraints, default changes, table options, and trailing
  `ALGORITHM` / `LOCK` options. Those remain outside this phase unless listed
  in scope below.

## Scope

Supported:

- persistent MyLite base tables only;
- two or more comma-separated actions on one target table;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- the existing single-action `ADD [COLUMN] column_definition` subset for
  persistent base tables, including its type, default, positioning, row-size,
  row-backfill, warning, and diagnostic rules;
- the existing persistent-table ordinary `ADD INDEX` / `ADD KEY` subset,
  including composite key parts, prefix key parts, `ASC` / `DESC`, admitted
  no-warning index options, omitted-name generation, and physical-index
  generation;
- the existing persistent-table `ADD UNIQUE` and
  `ADD CONSTRAINT ... UNIQUE` subset, including duplicate validation before
  mutation and generated physical unique indexes where applicable;
- the existing persistent-table `DROP INDEX` / `DROP KEY` subset, including
  auto-increment and foreign-key dependency checks;
- later actions resolving against earlier successful actions in the same
  uncommitted catalog mutation;
- rollback of every catalog and physical SQLite change if any admitted action
  fails;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for successful supported statements;
- implicit user-transaction commit behavior matching the existing DDL boundary.

Deferred:

- temporary tables;
- standalone option-only alters and trailing multi-action `ALGORITHM` / `LOCK`
  option lists;
- warning-producing multi-action forms, including `USING HASH`, fulltext adds,
  spatial adds, and zero-temporal add-column warning cases;
- `ADD PRIMARY KEY`, `DROP PRIMARY KEY`, `ADD FOREIGN KEY`, `DROP FOREIGN KEY`,
  `ADD CHECK`, `DROP CHECK`, `ALTER CHECK`, `RENAME INDEX`, `ALTER INDEX`
  visibility changes, table comment/default charset/collation changes, table
  options, `AUTO_INCREMENT`, `FORCE`, `ORDER BY`, `CONVERT TO CHARACTER SET`,
  `DROP COLUMN`, `RENAME COLUMN`, `MODIFY`, and `CHANGE` inside multi-action
  lists;
- parenthesized `ADD (column_definition[, ...])` lists;
- partition clauses, `DISABLE/ENABLE KEYS`, validation clauses, online DDL
  scheduling semantics, metadata locks, triggers, privilege semantics, and
  optimizer guarantees;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. Applications continue to call `mylite_execute()` and
  release result objects with the existing API.
- Statement context: owns diagnostics reset, warning accumulation,
  `ROW_COUNT()` / affected rows, and statement cleanup.
- Parser/AST: owns the independently authored action-list syntax and stores
  the shared table name plus child action nodes. It does not inspect the
  catalog or SQLite schema.
- Analyzer/planner/runtime: reuses existing single-action planners for each
  action through a shared target-table AST view, validates the multi-action
  scope, and calls lower-level execution helpers that assume an active catalog
  mutation.
- Catalog: remains the logical authority. Multi-action execution uses one
  `mylite_catalog_mutation` and updates table identity after each admitted
  action that changes descriptors.
- Result and introspection builders: existing descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, `INFORMATION_SCHEMA`, and DML paths observe the
  committed descriptor state. They never reconstruct logical metadata from
  SQLite schema text.
- SQLite physical storage: owns generated physical tables and indexes. MyLite
  emits standard SQLite DDL from descriptor physical names inside the same
  transaction as catalog writes.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are not modified by this feature.

## Grammar

MyLite admits a multi-action statement only when the action list contains at
least two supported actions. Existing single-action grammar remains the
authority for one action plus optional single-action `ALGORITHM` / `LOCK`
tails.

Independent MyLite Lemon-syntax sketch:

```lemon
statement(A) ::= alter_table_multi_action_statement(B). {
    A = B;
}

alter_table_multi_action_statement(A) ::=
    ALTER(T) TABLE table_name(N) alter_table_multi_action_list(L). {
    A = mylite_sql_parser_make_alter_table_multi_action_statement(state, T, N, L);
}

alter_table_multi_action_list(A) ::=
    alter_table_multi_first_action(L) alter_table_multi_action(N). {
    A = mylite_sql_parser_append_alter_table_action(state, L, N);
}

alter_table_multi_action_list(A) ::=
    alter_table_multi_action_list(L) COMMA alter_table_multi_action(N). {
    A = mylite_sql_parser_append_alter_table_action(state, L, N);
}

alter_table_multi_first_action(A) ::=
    ADD(T) column_keyword_opt column_definition(C) column_position_opt(P) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_column_statement(
            state, T, NULL, C, P, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_first_action(A) ::= ADD(T) secondary_index_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state, T, NULL, I, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_first_action(A) ::= ADD(T) unique_index_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state, T, NULL, I, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_first_action(A) ::= ADD(T) named_unique_constraint_definition(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_add_index_statement(
            state, T, NULL, I, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_first_action(A) ::= DROP(T) INDEX identifier(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_index_statement(
            state, T, NULL, I, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_first_action(A) ::= DROP(T) KEY identifier(I) COMMA. {
    A = mylite_sql_parser_make_alter_table_action_list(
        state,
        mylite_sql_parser_make_alter_table_drop_index_statement(
            state, T, NULL, I, mylite_sql_parser_empty_alter_table_options()));
}

alter_table_multi_action(A) ::=
    ADD(T) column_keyword_opt column_definition(C) column_position_opt(P). {
    A = mylite_sql_parser_make_alter_table_add_column_statement(
        state, T, NULL, C, P, mylite_sql_parser_empty_alter_table_options());
}

alter_table_multi_action(A) ::= ADD(T) secondary_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state, T, NULL, I, mylite_sql_parser_empty_alter_table_options());
}

alter_table_multi_action(A) ::= ADD(T) unique_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state, T, NULL, I, mylite_sql_parser_empty_alter_table_options());
}

alter_table_multi_action(A) ::= ADD(T) named_unique_constraint_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state, T, NULL, I, mylite_sql_parser_empty_alter_table_options());
}

alter_table_multi_action(A) ::= DROP(T) INDEX identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(
        state, T, NULL, I, mylite_sql_parser_empty_alter_table_options());
}

alter_table_multi_action(A) ::= DROP(T) KEY identifier(I). {
    A = mylite_sql_parser_make_alter_table_drop_index_statement(
        state, T, NULL, I, mylite_sql_parser_empty_alter_table_options());
}
```

The shared target table node is attached only to the outer multi-action
statement. Child action nodes intentionally carry no table child in the AST.

## Execution Semantics

The runtime creates one result object and starts one catalog mutation before
executing the first action. For each action:

1. Build a temporary statement view that combines the shared target table node
   with the child action node.
2. Run the existing single-action planner against that view.
3. Reject temporary or non-base targets for this multi-action phase.
4. Execute the planned action through an internal helper that assumes the
   catalog mutation is already active.
5. Deinitialize the plan before moving to the next action.

The planner for every action reads MyLite descriptors through the same
connection that owns the mutation, so later actions can resolve descriptors
inserted, deleted, or renamed by earlier supported actions. This is required
for shapes such as `ADD COLUMN c INT, ADD INDEX k_c(c)`.

If any planner, validator, catalog write, physical SQLite DDL, allocation, or
commit fails, MyLite rolls back the active catalog mutation and returns the
original diagnostic when one exists. The public result object is discarded on
failure.

Successful execution commits once, increments the connection-local SQLite
schema generation once when any physical schema changed, and returns a
non-row result. Supported multi-action statements report `affected_rows == 0`
and `warning_count == 0`.

## Physical SQLite Handling

All generated SQLite SQL is built from descriptors and stable physical names:

- add-column uses the generated physical table name and descriptor-built column
  definition from the existing add-column path;
- add-index allocates a new catalog index id, derives the generated physical
  index name from that id, and emits a standard SQLite `CREATE INDEX` or
  `CREATE UNIQUE INDEX` for non-fulltext/non-spatial descriptors;
- drop-index emits a standard SQLite `DROP INDEX` for descriptors with a
  physical index.

Identifiers are quoted by MyLite's SQLite identifier helpers. User literals
are not interpolated into generated index DDL. Existing row backfill and
default materialization rules are reused from the admitted add-column path.

No SQLite optional `ALTER TABLE` extensions or fork patches are required.

## Diagnostics

Existing single-action diagnostics are preserved for each admitted action,
including syntax errors, missing default schema, unknown schema, unknown table,
reserved names, unsupported object kind, duplicate/unknown columns, duplicate
or unknown indexes, unsupported key parts, duplicate-key validation failures,
foreign-key dependency checks, auto-increment key checks, allocation failures,
and physical SQLite failures.

New multi-action-specific diagnostics:

- a parsed action node with an unsupported kind is rejected with a deterministic
  MyLite unsupported-feature diagnostic;
- temporary-table targets are rejected for this phase even when the underlying
  single-action form supports them;
- warning-producing add-index or add-column forms are rejected for this phase;
- multi-action trailing `ALGORITHM` / `LOCK` tails are syntax errors until the
  online-DDL option matrix is specified for action lists.

## Compatibility Notes

This slice reduces migration friction for common table-evolution statements
that add columns and indexes together. It remains partial because MySQL's full
multi-action `ALTER TABLE` surface combines many action-specific semantics,
online DDL options, and table rebuild behaviors. Later phases should add more
actions only when their single-action implementation can be safely factored
into an in-mutation helper and MySQL 8.4.9 behavior is verified for the action
combination.

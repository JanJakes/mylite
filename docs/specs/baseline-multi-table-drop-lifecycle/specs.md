# Baseline Multi-Table DROP Lifecycle

## Status

This feature specifies the next narrow table-lifecycle slice for file-backed
`.mylite` handles. It extends the already supported persistent base-table
`DROP TABLE` and `DROP TABLE IF EXISTS` surface from one target to a
comma-separated target list.

The feature is intentionally not full MySQL `DROP TABLE` support. It keeps the
current persistent base-table, MyLite-catalog, warning, result, and generated
SQLite DDL boundaries. It does not implement temporary tables, views, triggers,
foreign keys, privileges, implicit commit behavior, or `RESTRICT` / `CASCADE`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table IF EXISTS lifecycle:
  `docs/specs/baseline-table-if-exists-lifecycle/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `DROP TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/drop-table.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for `DROP TABLE table_name [, table_name] ...`;
- parser and AST support for `DROP TABLE IF EXISTS table_name [, table_name]
  ...`;
- unqualified and schema-qualified table-name resolution through the existing
  selected-schema policy;
- descriptor-driven planning of every target before any catalog row or physical
  SQLite table is mutated;
- all-or-nothing behavior for non-`IF EXISTS` multi-table drops;
- successful mixed existing/missing behavior for `IF EXISTS`, dropping existing
  persistent base tables and recording one `Note 1051` per missing target;
- deterministic duplicate-target diagnostics before mutation;
- one catalog mutation spanning all descriptor deletes and physical table drops;
- one `sqlite_schema_generation` increment for a successful statement that
  drops at least one physical table;
- no mutation for all-missing `IF EXISTS` statements;
- result behavior for successful DDL: no rows, `affected_rows == 0`,
  `ROW_COUNT() == 0`, and warning count matching MySQL 8.4.9 for this subset.

## Non-Goals

This feature must not implement:

- `DROP TEMPORARY TABLE`;
- `DROP TABLE ... RESTRICT` or `DROP TABLE ... CASCADE`;
- views, temporary tables, triggers, cascades, foreign keys, partition cleanup,
  privileges, or implicit commit behavior;
- object-kind checks beyond currently known MyLite persistent base-table
  descriptors;
- arbitrary SQLite SQL pass-through;
- changing descriptor catalog case-sensitivity behavior;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, diagnostics snapshots, and
  failure cleanup.
- Statement context owns diagnostics reset, warning collection, warning count,
  affected rows, row-count state, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves each table name, selected schemas, explicit
  schemas, reserved `_mylite_*` names, duplicates, `IF EXISTS` missing-target
  policy, and base-table descriptor lookups before generated SQLite SQL.
- The catalog module remains authoritative for schemas, tables, columns,
  descriptor versions, and generation advancement. SQLite schema text is not a
  MySQL-visible metadata authority.
- SQLite owns durable b-tree row storage and physical table removal for real
  drops. MyLite generates stable physical table names such as
  `_mylite_user_table_<table_id>` from descriptors and quotes every generated
  identifier.
- The result builder owns the empty DDL result and copies the statement warning
  count from diagnostics.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  No-op `IF EXISTS` statements must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits these table-drop forms:

```sql
DROP TABLE table_name [, table_name] ...
DROP TABLE IF EXISTS table_name [, table_name] ...

table_name:
    identifier
  | identifier.identifier
```

`table_name` uses the same identifier spelling and quoting support as the
current table lifecycle. A one-target `DROP TABLE` remains supported through the
same grammar.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
drop_table_statement ::=
    DROP TABLE drop_if_exists_opt table_name_list.

drop_if_exists_opt ::= .
drop_if_exists_opt ::= IF EXISTS.

table_name_list ::= table_name.
table_name_list ::= table_name_list COMMA table_name.
```

The AST should represent the target list structurally. A dedicated
table-name-list node is preferred over reusing `identifier_list`, because each
target may be a qualified identifier and the list represents table names, not
column identifiers.

## Name Resolution

Unqualified targets use the selected schema. If any target is unqualified and
there is no selected schema, MyLite returns MySQL error `1046`, SQLSTATE
`3D000`, and message `No database selected` before dropping any table. This
applies even when other targets in the same statement are schema-qualified and
even when `IF EXISTS` is present.

Schema-qualified targets use the explicit schema and do not require a selected
schema.

For non-`IF EXISTS` drops, unknown explicit schemas are reported as unknown
tables, matching MySQL 8.4.9's observed `1051` behavior for
`DROP TABLE missing_schema.t`.

For `IF EXISTS` drops, unknown explicit schemas are not errors. They produce the
same `Note 1051` shape as missing tables in existing schemas.

Reserved `_mylite_*` schema and table names remain MyLite-owned internals and
must be rejected before generated SQLite SQL.

Descriptor lookup keeps the current MyLite catalog name matching behavior. This
slice does not introduce a new collation or case-sensitivity policy. Duplicate
target detection must use the same effective logical target identity MyLite uses
for descriptor resolution, so a repeated table is rejected before mutation.

## Drop Semantics

For `DROP TABLE` without `IF EXISTS`, MyLite plans every target before mutation:

- resolve all target schemas;
- reject reserved names;
- reject duplicate logical targets with MySQL error `1066`, SQLSTATE `42000`,
  and message `Not unique table/alias: '<table>'`;
- resolve every persistent base-table descriptor;
- if one or more targets are missing, fail with MySQL error `1051`, SQLSTATE
  `42S02`, and a deterministic unknown-table message, without deleting any
  descriptor or physical table.

When all targets resolve, MyLite deletes all descriptor rows and physical tables
inside one catalog mutation, commits the mutation, increments
`sqlite_schema_generation` once, and returns an empty result with
`affected_rows == 0`, `warning_count == 0`, and `ROW_COUNT() == 0`.

For `DROP TABLE IF EXISTS`, MyLite also plans every target before mutation:

- resolve all target schemas that are required for resolution;
- reject reserved names;
- reject duplicate logical targets with `1066`, even if `IF EXISTS` is present;
- collect existing persistent base-table descriptors;
- collect missing targets and append one `Note 1051` per missing target in
  statement order;
- if at least one existing table is present, delete those descriptors and
  physical tables inside one catalog mutation;
- if no existing table is present, skip catalog mutation and generated SQLite
  SQL.

Successful `IF EXISTS` statements return an empty result with
`affected_rows == 0`, `ROW_COUNT() == 0`, and warning count equal to the number
of missing targets.

The warning shape is:

- level: `Note`
- code: `1051`
- SQLSTATE: `42S02`
- message: `Unknown table '<schema>.<table>'`

For unqualified missing targets, `<schema>` is the selected schema. For missing
explicit schemas, `<schema>` is the explicit schema spelling.

## Diagnostics And Result Behavior

`SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`, `@@warning_count`, and
`@@error_count` observe notes and errors through the existing diagnostics
snapshot policy.

Successful multi-table drops return through the existing public non-row result
conventions. `DROP TABLE` reports `ROW_COUNT() == 0` for existing, mixed
`IF EXISTS`, and all-missing `IF EXISTS` statements, matching MySQL 8.4.9
runtime observations.

Unsupported syntax uses deterministic parser or unsupported-statement
diagnostics according to the existing MyLite policy. This includes
`DROP TEMPORARY TABLE`, `DROP TABLE ... RESTRICT`, and
`DROP TABLE ... CASCADE` for this slice, even though MySQL accepts those forms.

Allocation failures, catalog read failures, physical SQLite failures, and public
API misuse use existing diagnostics policy unless explicitly overridden above.

## Physical SQLite Handling

No user SQL is passed through to SQLite. MyLite resolves descriptors and builds
one generated SQLite `DROP TABLE "<physical_name>"` statement per existing
target. Every identifier is quoted by the existing dynamic-string helper.

The implementation must not rely on SQLite supporting MySQL's multi-table
`DROP TABLE` syntax. SQLite receives only one physical table name at a time,
inside the surrounding MyLite catalog mutation.

The implementation must not add SQLite fork patches. Public SQLite APIs and the
existing MyLite wrapper/translation layer are sufficient.

## MySQL 8.4.9 Runtime Observations

The companion script
`packages/libmylite/tests/mysql_baseline_multi_table_drop_expectations.sh`
records these MySQL 8.4.9 observations:

- `DROP TABLE a, b` drops both existing tables and leaves
  `ROW_COUNT() == 0`, `@@warning_count == 0`.
- `DROP TABLE db1.a, db2.b` works without a selected schema.
- If any non-`IF EXISTS` target is missing, MySQL returns `1051` and leaves all
  existing tables unchanged.
- If more than one non-`IF EXISTS` target is missing, MySQL lists missing
  targets in one `1051` message separated by commas.
- `DROP TABLE IF EXISTS a, missing_one, missing_two` drops existing targets,
  returns `ROW_COUNT() == 0`, and records one `Note 1051` per missing target.
- `DROP TABLE IF EXISTS missing_schema.t` succeeds with `Note 1051`; it does
  not return unknown-database error.
- Repeating the same logical table target, including `a, a` and `a, db.a` when
  `db` is selected, fails with `1066` before mutation. `IF EXISTS` does not
  change this duplicate-target behavior.
- If any target is unqualified and no default schema is selected, MySQL returns
  `1046` before mutation, even when other targets are schema-qualified and even
  with `IF EXISTS`.
- MySQL accepts wider forms outside this slice, including `TEMPORARY`,
  `RESTRICT`, and `CASCADE`.

## Compatibility Documentation

After implementation:

- `COMPATIBILITY.md` should update the `DROP TABLE` row to mention limited
  multi-table target-list support.
- `docs/compatibility/sql-table-ddl.md` should make the same exact-scope update.
- Diagnostics docs should change only if the implementation alters existing
  warning-count or `SHOW WARNINGS` policy beyond the `Note 1051` rows introduced
  here.

Do not overclaim temporary tables, views, triggers, partitions, privileges,
implicit commit behavior, `RESTRICT`, `CASCADE`, or non-base object cleanup.

## Test Plan

Add fast plain C tests under `packages/libmylite/tests/`, preferably by
expanding the existing table lifecycle tests unless a new binary keeps the
coverage clearer. Cover:

- parser success for one-target and multi-target `DROP TABLE` with and without
  `IF EXISTS`;
- AST target-list shape for unqualified and schema-qualified targets;
- parser or unsupported diagnostics for `DROP TEMPORARY TABLE`,
  `DROP TABLE ... RESTRICT`, and `DROP TABLE ... CASCADE`;
- successful full multi-table drop over unqualified selected-schema targets;
- successful schema-qualified multi-table drop without a selected schema;
- missing target without `IF EXISTS` fails atomically and leaves all existing
  descriptors and physical tables intact;
- multiple missing targets without `IF EXISTS` produce deterministic unknown
  table diagnostics;
- mixed existing/missing `IF EXISTS` drops existing targets, records ordered
  notes for missing targets, and reports the correct public warning count;
- all-missing `IF EXISTS` records notes and does not mutate catalog generation,
  SQLite schema generation, row data, or the `.mylite` preamble;
- missing explicit schema with `IF EXISTS`;
- no selected schema with at least one unqualified target, with and without
  `IF EXISTS`;
- duplicate logical targets, with and without `IF EXISTS`;
- reserved `_mylite_*` schema/table names;
- row-count state, warning count, absence of result rows, and remaining tables
  after each relevant statement;
- reopen persistence after dropping multiple tables;
- update after table rename and after drop where applicable to ensure dropped
  targets are no longer readable or mutable;
- independent file-backed handles with independent table state;
- zero-initialized cleanup for new planner objects;
- existing parser, diagnostics, table lifecycle, table IF EXISTS, schema
  lifecycle, row-values, select, delete, update, alter, file-format, VFS, and
  public API misuse tests still pass.

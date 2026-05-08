# Baseline Table IF EXISTS Lifecycle

## Status

This feature specifies the next narrow table-lifecycle slice for file-backed
`.mylite` handles. It adds optional existence clauses to the already supported
persistent base-table lifecycle:

- `CREATE TABLE IF NOT EXISTS table_name (...)`
- `DROP TABLE IF EXISTS table_name`

The feature is intentionally not full MySQL table lifecycle support. It keeps
the current single-table, persistent base-table, integer descriptor, option, and
diagnostic boundaries. It does not implement temporary tables, multi-table
drop, `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, defaults, keys,
constraints, generated columns, auto-increment, privilege semantics, or implicit
commit behavior.

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
- Baseline show warnings diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- Baseline diagnostics count variables:
  `docs/specs/baseline-diagnostics-count-variables/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
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

- parser and AST support for `IF NOT EXISTS` after `CREATE TABLE`;
- parser and AST support for `IF EXISTS` after `DROP TABLE`;
- unqualified and schema-qualified table-name resolution through the existing
  selected-schema policy;
- existing limited `CREATE TABLE` descriptor validation and physical table
  creation for non-existing targets;
- existing limited `DROP TABLE` descriptor deletion and physical table removal
  for existing targets;
- successful no-op behavior with one warning when `CREATE TABLE IF NOT EXISTS`
  targets an existing table;
- successful no-op behavior with one warning when `DROP TABLE IF EXISTS`
  targets a missing table or a table in a missing explicit schema;
- MySQL-compatible warning count, `SHOW WARNINGS`, and `@@warning_count`
  behavior for the admitted no-op cases;
- no catalog generation, descriptor cache, SQLite schema generation, or physical
  file changes for no-op existing-create and missing-drop statements;
- result behavior for successful DDL: no rows, `affected_rows == 0`, and
  statement warning count matching MySQL 8.4.9 for this subset.

## Non-Goals

This feature must not implement:

- temporary tables;
- multi-table `DROP TABLE`;
- `DROP TABLE ... RESTRICT` or `DROP TABLE ... CASCADE`;
- `CREATE TABLE ... LIKE`;
- `CREATE TABLE ... SELECT`;
- `CREATE TABLE` defaults, indexes, keys, constraints, generated columns,
  invisible columns, auto-increment, comments, storage options, partitions, or
  unsupported table options;
- `CREATE DATABASE IF NOT EXISTS` or `DROP DATABASE IF EXISTS`;
- object kind checks beyond the currently supported persistent base-table
  descriptors;
- privilege semantics or implicit commit behavior;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, diagnostics snapshots, and
  failure cleanup.
- Statement context owns diagnostics reset, warning collection, warning count,
  affected rows, row-count state, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves table names, selected schemas, explicit
  schemas, reserved `_mylite_*` names, and existence policy before generated
  SQLite SQL.
- The catalog module remains authoritative for schemas, tables, and columns.
  Existing `CREATE TABLE` and `DROP TABLE` catalog mutations are reused only
  when a real create/drop occurs.
- SQLite owns durable b-tree row storage and physical table creation/removal for
  real create/drop statements. SQLite schema text and `PRAGMA` output remain
  physical implementation details.
- The result builder owns the empty DDL result and warning count copied from
  current diagnostics.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  No-op existence statements must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits these extensions to the already supported table lifecycle:

```sql
CREATE TABLE IF NOT EXISTS table_name (
    column_definition [, column_definition] ...
) [table_option ...]

DROP TABLE IF EXISTS table_name
```

`column_definition` and `table_option` stay exactly as supported by the current
basic table lifecycle and table-option slices.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
create_table_statement ::=
    CREATE TABLE create_if_not_exists_opt table_name LP column_definition_list RP table_options_opt.

create_if_not_exists_opt ::= .
create_if_not_exists_opt ::= IF NOT EXISTS.

drop_table_statement ::=
    DROP TABLE drop_if_exists_opt table_name.

drop_if_exists_opt ::= .
drop_if_exists_opt ::= IF EXISTS.
```

The AST should represent the existence policy explicitly, preferably as a small
child marker or statement flag equivalent that keeps runtime checks out of
source-text string parsing.

## Schema And Table Resolution

Unqualified names use the selected schema. Without a selected schema, both
`CREATE TABLE IF NOT EXISTS table_name (...)` and
`DROP TABLE IF EXISTS table_name` return MySQL error `1046`, SQLSTATE `3D000`,
and message `No database selected`.

Schema-qualified create targets use the explicit schema and do not require a
selected schema. Unknown explicit schemas for `CREATE TABLE IF NOT EXISTS`
return MySQL error `1049`, SQLSTATE `42000`, and message
`Unknown database '<schema>'`.

Schema-qualified drop targets use the explicit schema and do not require a
selected schema. With `IF EXISTS`, a missing explicit schema is not an error in
MySQL 8.4.9. MyLite returns success with one warning, using the same missing
table warning policy as a missing table in an existing schema.

Reserved `_mylite_*` schema and table names remain MyLite-owned internals and
must be rejected before generated SQLite SQL.

## Create Semantics

If the target table does not exist, `CREATE TABLE IF NOT EXISTS` follows the
existing limited `CREATE TABLE` path:

- resolve the target schema;
- validate the column list, integer-family types, nullability, duplicate names,
  and supported table options;
- create authoritative catalog descriptors;
- create the stable physical SQLite table generated from descriptors;
- advance catalog generation and SQLite schema generation exactly like ordinary
  `CREATE TABLE`;
- return an empty result with `affected_rows == 0` and no warnings.

If the target table already exists, MyLite must not verify that the existing
descriptor matches the submitted definition. The statement succeeds as a no-op,
records one warning, returns an empty result, leaves `affected_rows == 0`, and
does not mutate catalog rows, descriptor versions, catalog generation,
descriptor caches, SQLite schema generation, physical SQLite schema, row data,
or the `.mylite` preamble.

The warning is a MySQL-compatible note-level condition:

- level: `Note`
- code: `1050`
- SQLSTATE: `42S01`
- message: `Table '<table>' already exists`

The message uses MySQL's observed table-name spelling for this subset: the
unqualified target table name, not a schema-qualified name.

## Drop Semantics

If the target table exists, `DROP TABLE IF EXISTS` follows the existing limited
`DROP TABLE` path:

- resolve the target schema;
- resolve the persistent base-table descriptor;
- delete catalog descriptors;
- drop the generated physical SQLite table;
- advance catalog generation and SQLite schema generation exactly like ordinary
  `DROP TABLE`;
- return an empty result with `affected_rows == 0` and no warnings.

If the target table is missing, MyLite succeeds as a no-op with one warning.
For schema-qualified targets, this includes missing explicit schemas. The
statement must not mutate catalog rows, descriptor versions, catalog generation,
descriptor caches, SQLite schema generation, physical SQLite schema, row data,
or the `.mylite` preamble.

The warning is a MySQL-compatible note-level condition:

- level: `Note`
- code: `1051`
- SQLSTATE: `42S02`
- message: `Unknown table '<schema>.<table>'`

For unqualified targets, `<schema>` is the selected schema.

## Diagnostics And Result Behavior

Successful new creates and existing drops return through the existing public
non-row result conventions with warning count `0`.

Successful existing creates and missing drops return through the same non-row
result conventions with warning count `1`. `SHOW WARNINGS`,
`SHOW COUNT(*) WARNINGS`, `@@warning_count`, and `@@error_count` must observe
the new warnings according to the existing diagnostics snapshot policy.

`ROW_COUNT()` returns `0` for successful admitted create/drop existence
statements. `SHOW WARNINGS` remains a diagnostic statement and may change a
subsequent `ROW_COUNT()` result according to the existing diagnostics behavior.

Errors, including syntax errors, missing selected schema for unqualified names,
unknown explicit create schema, reserved names, unsupported create options,
unsupported object kinds, allocation failures, physical SQLite failures, and
public API misuse use the existing diagnostics policy unless explicitly
overridden above.

## Physical SQLite Handling

No SQLite SQL is generated for existing-create no-ops or missing-drop no-ops.
Real create/drop statements reuse the existing descriptor-generated physical SQL
shapes and stable physical table names such as `_mylite_user_table_<table_id>`.
Every generated SQLite identifier remains quoted, and no user SQL is passed
through to SQLite.

The implementation must not add SQLite fork patches. Public SQLite APIs and the
existing MyLite wrapper/translation layer are sufficient.

## MySQL 8.4.9 Runtime Observations

The companion script
`packages/libmylite/tests/mysql_baseline_table_if_exists_expectations.sh`
records these MySQL 8.4.9 observations:

- `CREATE TABLE IF NOT EXISTS db.t (...)` creates a missing table with
  `ROW_COUNT() == 0` and `@@warning_count == 0`.
- Repeating `CREATE TABLE IF NOT EXISTS db.t (...)` succeeds with
  `ROW_COUNT() == 0`, `@@warning_count == 1`, and `SHOW WARNINGS` row
  `Note 1050 Table 't' already exists`.
- The existing table definition is not compared with the submitted definition.
- `DROP TABLE IF EXISTS db.t` drops an existing table with `ROW_COUNT() == 0`
  and `@@warning_count == 0`.
- Repeating `DROP TABLE IF EXISTS db.t` succeeds with `ROW_COUNT() == 0`,
  `@@warning_count == 1`, and `SHOW WARNINGS` row
  `Note 1051 Unknown table '<db>.t'`.
- `DROP TABLE IF EXISTS missing_schema.t` succeeds with the same `1051` warning
  shape rather than returning unknown-database error.
- Unqualified create/drop existence statements without a selected schema return
  `1046` / `3D000`.
- Schema-qualified create against an unknown schema returns `1049` / `42000`.
- MySQL accepts wider forms outside this slice, including temporary tables,
  multi-table drops, `CREATE TABLE ... LIKE`, and `CREATE TABLE ... SELECT`.

## Compatibility Documentation

After implementation:

- `COMPATIBILITY.md` should update the `CREATE TABLE` and `DROP TABLE` rows to
  mention the limited `IF NOT EXISTS` / `IF EXISTS` support.
- `docs/compatibility/sql-table-ddl.md` should make the same exact-scope update.
- Diagnostics docs should only change if the implementation alters the existing
  warning-count or `SHOW WARNINGS` surface beyond the warnings introduced here.

Do not overclaim temporary tables, multi-table drop, `LIKE`, `SELECT`, defaults,
keys, constraints, generated columns, auto-increment, privileges, or implicit
commit behavior.

## Test Plan

Add a focused runtime C test, preferably
`runtime_table_if_exists_lifecycle_test.c`, covering:

- `CREATE TABLE IF NOT EXISTS` creates a missing table and preserves the
  existing descriptor-driven create behavior;
- existing-create no-op warning count, `SHOW WARNINGS`, `SHOW COUNT(*)
  WARNINGS`, `@@warning_count`, result warning count, and row count;
- existing-create does not compare or replace a different submitted definition;
- existing-create no-op leaves catalog and SQLite schema generations unchanged;
- `DROP TABLE IF EXISTS` drops an existing table and preserves the existing
  descriptor-driven drop behavior;
- missing-drop no-op warning count and warning row, including missing explicit
  schema;
- unqualified create/drop existence statements without selected schema;
- unknown explicit schema for create;
- reserved schema/table names;
- unsupported syntax rejected deterministically, including temporary tables,
  multi-table drop, `CREATE TABLE ... LIKE`, and `CREATE TABLE ... SELECT`;
- reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles with independent warning and table state;
- existing parser, diagnostics, warning, table lifecycle, row-values, DML, and
  file-format tests still pass.

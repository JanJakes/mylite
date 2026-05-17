# Baseline ALTER TABLE ADD COLUMN Positioning

## Status

This feature extends the existing descriptor-driven
`ALTER TABLE ... ADD [COLUMN]` lifecycle with the MySQL column-position clauses
needed by common application DDL:

- `FIRST`;
- `AFTER column_name`.

It does not widen the existing add-column type, default, constraint, key,
multi-action, algorithm, lock, temporary-table, or view surface. The only new
user-visible behavior is logical placement of one added column in a persistent
base table.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing add-column lifecycle:
  `docs/specs/baseline-alter-table-add-column/specs.md`
- Existing change/modify positioning lifecycle:
  `docs/specs/baseline-alter-change-modify-temporal-positioning/specs.md`
- Existing parser, catalog, result, storage, and runtime lifecycle specs under
  `docs/specs/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_alter_table_add_column_positioning_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase.

- A single `ADD [COLUMN] col definition FIRST` places the new column at ordinal
  position `1`.
- A single `ADD [COLUMN] col definition AFTER existing_col` places the new
  column immediately after the existing descriptor named by `existing_col`.
- Omitted positioning appends as before.
- Successful positioned add-column statements report `ROW_COUNT() = 0` and
  `@@warning_count = 0`.
- The new order is visible in `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SELECT *`,
  implicit row-value `INSERT`, and `INFORMATION_SCHEMA.COLUMNS.ORDINAL_POSITION`.
- `AFTER missing_col` reports `1054 / 42S22` with
  `Unknown column '<name>' in '<table>'`.
- `AFTER` cannot name the new column being added unless an existing column with
  that name already exists. When no existing descriptor matches, MySQL reports
  the same unknown-column diagnostic.
- Duplicate added-column names continue to report `1060 / 42S21` before
  position resolution.
- Schema-qualified targets do not require a selected default schema. Unqualified
  targets without a selected schema still report `1046 / 3D000`.

## Scope

Supported:

- persistent MyLite base tables only;
- one `ALTER TABLE table_name ADD [COLUMN] column_definition [FIRST|AFTER name]`
  action only;
- all column definitions already supported by the current
  `ALTER TABLE ... ADD [COLUMN]` implementation;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- unqualified added-column names;
- unqualified `AFTER` column names resolved from MyLite descriptors;
- descriptor-driven duplicate detection and row-size validation;
- descriptor ordering reflected by `SELECT *`, implicit row-value DML,
  `SHOW COLUMNS`, `SHOW CREATE TABLE`, and limited
  `INFORMATION_SCHEMA.COLUMNS`;
- no-row DDL results with `affected_rows == 0` and `warning_count == 0` for
  successful statements.

Deferred:

- multiple alter actions or parenthesized add lists;
- table-qualified added-column names or table-qualified `AFTER` operands;
- adding keys or constraints in the same action;
- generated, invisible, or auto-increment column additions beyond the existing
  add-column rejection policy;
- algorithms, locks, partition operations, metadata locks, privilege semantics,
  temporary tables, views, triggers, cascades, or SQLite fork changes.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to own public misuse
  validation, result ownership, and failure cleanup.
- Statement context: owns diagnostics, warning count, affected rows, backend
  status, and the statement boundary.
- Lexer/parser/AST: admits only the explicit syntax subset and stores an
  optional column-position node. It does not resolve descriptors or build
  SQLite SQL.
- Analyzer/planner: resolves schema, table, added column, duplicate names,
  supported column definition, optional `AFTER` target, and final ordinal from
  MyLite descriptors before generated SQLite SQL exists.
- Catalog: remains the durable authority for logical column order. This feature
  inserts the new column descriptor and updates shifted descriptor ordinals
  inside the same catalog mutation.
- SQLite physical storage: stores rows in MyLite-generated rowid tables. The
  physical SQLite `ALTER TABLE ... ADD COLUMN` operation always appends the new
  physical column. MyLite does not treat SQLite physical column order or
  `sqlite_schema` text as MySQL-visible metadata.
- Result and introspection builders: render descriptor order.
- Storage/VFS: unchanged. The feature writes only inside the shifted SQLite
  payload and must not touch the `.mylite` preamble.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippet:

```lemon
alter_table_add_column_statement ::=
    ALTER TABLE table_name ADD column_keyword_opt column_definition column_position_opt.

column_keyword_opt ::= .
column_keyword_opt ::= COLUMN.

column_position_opt ::= .
column_position_opt ::= FIRST.
column_position_opt ::= AFTER identifier.
```

The existing `column_definition` grammar supplies the admitted type,
nullability, default, charset/collation, and temporal attributes. This feature
only adds the optional trailing position clause to the add-column statement.

## Resolution And Ordering

Unqualified target tables use the selected schema. Schema-qualified targets use
the explicit schema and do not require a selected schema. Missing selected
schema, unknown schema, unknown table, reserved `_mylite_*` schema/table names,
and unsupported object kinds keep the existing add-column diagnostics.

The added column name is resolved against existing descriptors for duplicate
detection before `AFTER` resolution. Reserved `_mylite_*` column names are
rejected before generated SQLite SQL.

`FIRST` sets the new descriptor's target ordinal to `1`.

`AFTER after_column_name` resolves `after_column_name` against existing column
descriptors in the target table. If no existing descriptor matches, MyLite
reports MySQL error `1054`, SQLSTATE `42S22`, and message
`Unknown column '<after_column_name>' in '<table>'`.

All existing columns keep their relative order. Existing columns whose ordinal
changes receive updated descriptor versions and catalog generations through the
same reorder helper already used by positioned `CHANGE`/`MODIFY` statements.

The current descriptor catalog comparison policy remains case-insensitive for
name lookup and duplicate detection.

## Catalog And Physical Handling

Successful positioned add-column execution:

1. resolves the target and column definition;
2. computes the final descriptor ordinal from `FIRST`, `AFTER`, or append;
3. begins one catalog mutation;
4. inserts the new column descriptor at the physical append ordinal;
5. when the target ordinal is not append, reorders catalog descriptors into the
   target logical order;
6. executes SQLite `ALTER TABLE "<physical_table>" ADD COLUMN "<column>" ...`
   with quoted identifiers and descriptor-built physical type/default text;
7. updates table identity metadata and commits the catalog mutation;
8. increments connection-local `sqlite_schema_generation` once.

The physical SQLite add remains append-only because SQLite has no public
column-insert operation. MyLite-generated DML and query SQL use explicit
descriptor-built column lists, so physical column order is not observable
through the supported MySQL surface. A later table rebuild may realign physical
order as an internal detail, but this feature does not require one.

On allocation, catalog, prepare, SQLite execution, or commit failure, MyLite
rolls back and leaves both logical descriptors and physical schema unchanged.

## Result Semantics

Successful statements return through existing non-row DDL result conventions:

- result column count `0`;
- result row count `0`;
- affected rows `0`;
- warning count `0`;
- `ROW_COUNT()` returns `0` after the statement.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- public API misuse through existing `mylite_execute()` behavior;
- missing default schema;
- unknown explicit schema;
- unknown table;
- reserved target schema, table, added column, or `AFTER` column names;
- unsupported object kind;
- duplicate added column;
- unknown `AFTER` column;
- unsupported type, default, generated, invisible, key, constraint, option,
  partition, multiple-action, parenthesized-list, or combined-action syntax;
- table-qualified added-column names and table-qualified `AFTER` operands;
- physical SQLite failures;
- allocation failures.

Supported successful positioned add-column statements produce no warnings.

## Tests

Fast C tests must cover:

- parser acceptance for `ADD COLUMN ... FIRST` and `ADD COLUMN ... AFTER name`;
- parser rejection for table-qualified `AFTER` operands, parenthesized lists,
  and multiple actions;
- successful `FIRST`, `AFTER middle_column`, and `AFTER last_column`;
- existing-row backfill, row-value `INSERT` without a column list, explicit
  `SELECT`, `SELECT *`, `UPDATE`, `SHOW COLUMNS`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS.ORDINAL_POSITION` after positioned additions;
- descriptor ordinal updates, table descriptor version changes, catalog
  generation changes, and SQLite schema generation changes;
- unknown `AFTER` column, `AFTER` self, duplicate added column with a position,
  reserved `AFTER` name, schema-qualified target without selected schema,
  missing default schema, unknown schema, and unknown table;
- reopen persistence, independent file-backed handles, rollback on physical
  failure, and preamble preservation through existing add-column lifecycle
  coverage.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only for
the exact `FIRST`/`AFTER` add-column extension. Do not overclaim full
`ALTER TABLE`, multiple actions, parenthesized lists, key or constraint
additions, algorithms, locks, temporary tables, views, privilege semantics, or
physical column-order guarantees.

## Verification

Before marking the implementation complete:

1. `cmake --build --preset dev`
2. Run the add-column parser/runtime CTest entries and adjacent alter lifecycle
   entries.
3. Run
   `packages/libmylite/tests/mysql_baseline_alter_table_add_column_positioning_expectations.sh`.
4. `cmake --workflow --preset check`

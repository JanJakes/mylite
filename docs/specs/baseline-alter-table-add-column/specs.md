# Baseline ALTER TABLE ADD COLUMN Lifecycle

## Status

This feature specifies the next narrow table-lifecycle slice for file-backed
`.mylite` handles. It adds descriptor-driven `ALTER TABLE ... ADD [COLUMN]`
execution on top of `mylite_execute()`, statement context, the MyLite parser
scaffold, shifted `.mylite` storage, durable catalog descriptors, table
create/drop/rename/truncate lifecycle, integer/`NULL` row storage, descriptor
`SELECT`, and descriptor DML.

The feature is intentionally not full MySQL `ALTER TABLE` support. It supports
one append-only integer column action for persistent base tables and the same
single-action add-column shape for visible session temporary tables. It does
not implement defaults, positioning, multiple actions, rebuild algorithms,
locks, inline indexes/constraints on temporary targets, views, metadata locks,
or implicit commit behavior.

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
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline `ALTER TABLE ... RENAME` lifecycle:
  `docs/specs/baseline-alter-table-rename-to/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, integer types:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, statements that cause implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- SQLite `ALTER TABLE ADD COLUMN`:
  https://www.sqlite.org/lang_altertable.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited
  `ALTER TABLE table_name ADD [COLUMN] column_definition` statement;
- one action and one new column only;
- append-only placement at the logical end of the table;
- visible persistent MyLite base-table descriptors and shadowing session
  temporary table descriptors;
- unqualified and schema-qualified target table resolution;
- `INT`, `INTEGER`, and `BIGINT`, each optionally `UNSIGNED`;
- optional `NULL` and `NOT NULL`; omitted nullability means nullable;
- descriptor-driven duplicate-column detection;
- existing-row backfill with `NULL` for nullable columns and integer `0` for
  `NOT NULL` integer columns, matching observed MySQL 8.4.9 behavior;
- descriptor and physical SQLite schema mutation inside one MyLite catalog
  mutation and SQLite transaction boundary;
- result behavior for successful DDL: no rows, `affected_rows == 0`, and
  `warning_count == 0`;
- MySQL 8.4.9 runtime-verified expectations for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- general `ALTER TABLE`;
- `ALTER ONLINE TABLE`, `WAIT`, `NOWAIT`, `ALGORITHM`, or `LOCK`;
- `ADD COLUMN` positioning with `FIRST` or `AFTER`;
- `ADD (col ...)`, multiple add clauses, or combined alter actions;
- `DEFAULT`, generated, invisible, auto-increment, primary key, unique,
  foreign key, check, comment, collation, charset, storage, or option clauses;
- non-integer column types;
- `DROP COLUMN`, `RENAME COLUMN`, `CHANGE COLUMN`, `MODIFY COLUMN`, index/key
  actions, constraints, partition actions, or table-option changes;
- views, triggers, privileges, metadata locks, foreign keys, cascades,
  routines, events, or `INFORMATION_SCHEMA`;
- inline primary-key or unique-key add-column forms on temporary tables;
- multi-action `ALTER TABLE` on temporary tables;
- reconstructing descriptors from SQLite schema text;
- SQLite fork patches.

MySQL accepts several wider forms, including defaults, positioning, multiple
actions, parenthesized add lists, and non-integer types. MyLite rejects them in
this phase because they require later descriptor metadata, assignment
conversion, table rebuild, or type work.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  backend status, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the visible target table and new column
  descriptor, preferring a shadowing session temporary table before the
  persistent descriptor, rejects unsupported scope, and builds a fixed
  add-column plan.
- The catalog module owns `_mylite_catalog_*` tables, table descriptor version
  changes, column descriptor insertion, catalog generation advancement, and
  descriptor-cache invalidation.
- The temporary catalog owns session-local column descriptor insertion and
  cleanup for temporary add-column targets.
- SQLite owns durable b-tree row storage and the physical schema change for the
  generated physical table. SQLite schema text and `PRAGMA` output remain
  physical implementation details, not MySQL-visible metadata authority.
- The result builder owns the empty DDL result.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call:

```sql
ALTER TABLE table_name ADD column_definition
ALTER TABLE table_name ADD COLUMN column_definition

column_definition:
    column_name integer_type [NULL | NOT NULL]

integer_type:
    INT
  | INTEGER
  | BIGINT
  | INT UNSIGNED
  | INTEGER UNSIGNED
  | BIGINT UNSIGNED

table_name:
    identifier
  | identifier.identifier
```

Only unqualified column names are admitted. The parser rejects unsupported
clauses after the admitted column definition.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= alter_table_add_column_statement.

alter_table_add_column_statement ::=
    ALTER TABLE table_name ADD column_keyword_opt column_definition.

column_keyword_opt ::= .
column_keyword_opt ::= COLUMN.

column_definition ::= identifier integer_type nullability_opt.

integer_type ::= INT.
integer_type ::= INTEGER.
integer_type ::= BIGINT.
integer_type ::= INT UNSIGNED.
integer_type ::= INTEGER UNSIGNED.
integer_type ::= BIGINT UNSIGNED.

nullability_opt ::= .
nullability_opt ::= NULL.
nullability_opt ::= NOT NULL.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply for qualified table names.

## Schema And Table Resolution

Unqualified target names use the selected schema. If no schema is selected,
MyLite returns MySQL error `1046`, SQLSTATE `3D000`, and message
`No database selected`.

Schema-qualified targets use the explicit schema and do not require a selected
schema. Unknown explicit schemas return MySQL error `1049`, SQLSTATE `42000`,
and message `Unknown database '<schema>'`. Unknown tables return MySQL error
`1146`, SQLSTATE `42S02`, and message `Table '<schema>.<table>' doesn't exist`.

The target table descriptor is resolved by logical schema name and table name,
preferring a visible session temporary table over a persistent table of the
same schema/name. The current catalog comparison policy remains
case-insensitive for name lookup and duplicate detection.
`MYLITE_CATALOG_TABLE_KIND_BASE` and `MYLITE_CATALOG_TABLE_KIND_TEMPORARY` are
supported. Views or other object kinds must be rejected before any physical
SQLite SQL is generated.

Reserved `_mylite_*` schema, table, and column names are MyLite-owned internals
and must be rejected before generated SQLite SQL.

## Column Descriptor Semantics

The new column is appended with ordinal position `existing_column_count + 1`.
Its logical type and physical type use the existing create-table mapping:

| SQL type | Logical descriptor | Physical descriptor |
| --- | --- | --- |
| `INT` | `INT` | `INTEGER` |
| `INTEGER` | `INT` | `INTEGER` |
| `BIGINT` | `BIGINT` | `INTEGER` |
| `INT UNSIGNED` | `INT UNSIGNED` | `INTEGER` |
| `INTEGER UNSIGNED` | `INT UNSIGNED` | `INTEGER` |
| `BIGINT UNSIGNED` | `BIGINT UNSIGNED` | `INTEGER` |

Omitted nullability and explicit `NULL` create a nullable descriptor. Explicit
`NOT NULL` creates a non-null descriptor. No logical default descriptor is
created by this phase, even when MySQL uses the integer implicit default for
existing rows.

Duplicate new column names are detected against MyLite descriptors before
physical SQL and fail with MySQL error `1060`, SQLSTATE `42S21`, and message
`Duplicate column name '<column>'`.

## Existing Row Backfill

Observed MySQL 8.4.9 behavior for this subset:

- adding a nullable integer column to a table with existing rows stores `NULL`
  in the new column for those rows;
- adding a `NOT NULL` integer column without an explicit default stores integer
  `0` in the new column for existing rows;
- successful `ALTER TABLE ... ADD COLUMN` reports `ROW_COUNT() = 0` and
  `@@warning_count = 0`.

MyLite implements this by choosing the physical SQLite column definition from
the logical descriptor:

```sql
ALTER TABLE "<physical_table>" ADD COLUMN "<column_name>" INTEGER
ALTER TABLE "<physical_table>" ADD COLUMN "<column_name>" INTEGER NOT NULL DEFAULT 0
```

The physical `DEFAULT 0` is an internal backfill mechanism for `NOT NULL`
integer columns. MyLite descriptors remain authoritative and do not expose a
logical default. Later inserts that omit this non-null column must still fail
through MyLite's descriptor-driven no-default policy before SQLite execution.

## Catalog And Physical Mutation

Successful add-column execution:

- preserves `table_id`, logical table name, schema id, physical table name, and
  created generation;
- increments the table descriptor version by `1`;
- updates the table's updated catalog generation to the mutation generation;
- inserts one column descriptor with descriptor version `1`;
- advances durable catalog generation by `1`;
- refreshes the connection-local catalog generation and invalidates descriptor
  caches;
- increments connection-local `sqlite_schema_generation` by `1`;
- preserves existing rows and stores the new physical column values described
  above;
- preserves selected schema.

The catalog row insertion and physical SQLite `ALTER TABLE` run inside the same
MyLite catalog mutation. On catalog, allocation, prepare, SQLite execution, or
commit failure, MyLite rolls back and leaves both logical descriptors and
physical schema unchanged.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- public API misuse through existing `mylite_execute()` behavior;
- missing default schema;
- unknown explicit schema;
- unknown table;
- reserved target schema, table, or column names;
- unsupported object kind;
- duplicate column;
- unsupported type, default, positioning, generated, invisible, key,
  constraint, option, partition, multiple-action, parenthesized-list, or
  combined-action syntax;
- physical SQLite failures;
- allocation failures.

Supported successful add-column statements produce no warnings.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_alter_table_add_column_expectations.sh`
records the MySQL 8.4.9 comparison cases. Observed baseline behavior:

| Case | Observed MySQL 8.4.9 behavior |
| --- | --- |
| `ALTER TABLE t ADD COLUMN n INT NULL` with rows | succeeds; `ROW_COUNT() = 0`, warnings `0`, existing values are `NULL` |
| `ALTER TABLE t ADD n INT` | same as nullable add; `COLUMN` keyword is optional |
| `ALTER TABLE t ADD COLUMN nn INT NOT NULL` with rows | succeeds; `ROW_COUNT() = 0`, warnings `0`, existing values are `0` |
| schema-qualified target without selected schema | succeeds |
| unqualified target without selected schema | error `1046` / `3D000`, `No database selected` |
| unknown explicit schema | error `1049` / `42000`, `Unknown database '<schema>'` |
| unknown table | error `1146` / `42S02`, `Table '<schema>.<table>' doesn't exist` |
| duplicate column | error `1060` / `42S21`, `Duplicate column name '<column>'` |
| omitted nullable added column on later insert | stores `NULL` |
| omitted non-null added column on later insert | error `1364` / `HY000`, no default |
| one-value `INSERT ... VALUES` after a two-column table | error `1136` / `21S01` |
| defaults, positioning, multiple actions, parenthesized add lists, non-integer types | accepted by MySQL but outside this MyLite slice |
| table-qualified new column name | syntax error `1064` / `42000` |

## Tests

Add a fast C runtime lifecycle test under `packages/libmylite/tests/`, with a
dotted CTest name such as `libmylite.runtime.alter_table_add_column`.

The test suite must cover:

- parser acceptance for `ADD` and `ADD COLUMN` with admitted integer families;
- parser rejection for defaults, positioning, parenthesized lists, multiple
  actions, combined actions, qualified column names, non-integer types, keys,
  constraints, generated/invisible columns, and table options;
- successful nullable and non-null appends on nonempty and empty tables;
- `INT`, `INTEGER`, `BIGINT`, and unsigned variants;
- selected-schema and schema-qualified target resolution, including qualified
  execution without selected schema;
- missing default schema, unknown schema, unknown table, duplicate column, and
  reserved-name diagnostics;
- `SELECT *`, explicit `SELECT`, `SHOW COLUMNS`, `SHOW CREATE TABLE`, `INSERT`
  with and without column lists, `UPDATE`, `DELETE`, `TRUNCATE`, and rename/drop
  interaction after adding columns;
- affected rows, warning count, result row absence, `ROW_COUNT()`, and
  `@@warning_count`;
- descriptor version/generation, column ordinal, physical SQLite schema
  generation, rollback on physical failure, and descriptor-cache invalidation;
- session temporary table shadowing, temporary descriptor metadata, and
  persistent table preservation after dropping the temporary table;
- reopen persistence, independent file-backed handles, and preamble safety;
- zero-initialized cleanup for new planner objects.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md` and
`docs/compatibility/sql-table-ddl.md` only for the exact limited appended
integer-column subset. Do not overclaim full `ALTER TABLE`, defaults,
positioning, metadata locks, algorithms, keys, constraints, temporary inline
keys, temporary multi-action ALTER, views, table rebuilds, arbitrary SQLite
pass-through, or non-integer types.

## Verification

Before marking the implementation complete:

1. `cmake --build --preset dev`
2. Run the new CTest entry and existing parser/basic-table/rename/alter-rename/
   row-values/select/update/delete/truncate lifecycle entries.
3. Run
   `packages/libmylite/tests/mysql_baseline_alter_table_add_column_expectations.sh`.
4. `cmake --workflow --preset check`

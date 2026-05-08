# Baseline ALTER TABLE DROP COLUMN Lifecycle

## Status

This feature specifies the next narrow table-lifecycle slice for file-backed
`.mylite` handles. It adds descriptor-driven `ALTER TABLE ... DROP [COLUMN]`
execution on top of `mylite_execute()`, statement context, the MyLite parser
scaffold, shifted `.mylite` storage, durable catalog descriptors, create/drop/
rename/truncate table lifecycle, add-column lifecycle, integer/`NULL` row
storage, descriptor `SELECT`, descriptor DML, and descriptor table
introspection.

The feature is intentionally not full MySQL `ALTER TABLE` support. It supports
one column drop action for persistent base tables whose descriptors contain at
least two columns before the statement. It does not implement multiple actions,
indexes, constraints, temporary tables, views, metadata locks, algorithms,
locks, privilege checks, dependency invalidation for unsupported object kinds,
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
- Baseline `ALTER TABLE ... ADD COLUMN` lifecycle:
  `docs/specs/baseline-alter-table-add-column/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, statements that cause implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited
  `ALTER TABLE table_name DROP [COLUMN] column_name` statement;
- one action and one dropped column only;
- persistent MyLite base-table descriptors only;
- unqualified and schema-qualified target table resolution;
- unqualified dropped column names only;
- descriptor-driven dropped-column lookup and missing-column diagnostics;
- a MySQL-compatible error for attempts to drop the only remaining column;
- descriptor ordinal compaction for the remaining columns;
- descriptor version, catalog generation, descriptor-cache invalidation, and
  SQLite schema generation updates after successful physical schema mutation;
- generated physical SQLite `ALTER TABLE ... DROP COLUMN` only from
  descriptors and stable physical table names;
- result behavior for successful DDL: no rows, `affected_rows == 0`, and
  `warning_count == 0`;
- MySQL 8.4.9 runtime-verified expectations for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- general `ALTER TABLE`;
- `ALTER ONLINE TABLE`, `WAIT`, `NOWAIT`, `ALGORITHM`, or `LOCK`;
- multiple `DROP` clauses or combined alter actions;
- `DROP PRIMARY KEY`, `DROP INDEX`, `DROP KEY`, `DROP FOREIGN KEY`,
  `DROP CHECK`, `DROP CONSTRAINT`, partitions, or table options;
- table-qualified dropped column names;
- temporary tables, views, triggers, privileges, metadata locks, foreign keys,
  cascades, generated columns, invisible columns, routines, events, or
  `INFORMATION_SCHEMA` dependency maintenance;
- dropping columns from tables with one descriptor column;
- reconstructing descriptors from SQLite schema text;
- generalized table rebuilds or SQLite fork patches.

MySQL accepts several wider forms, including multiple drop actions and
algorithm/lock modifiers. MyLite rejects them in this phase because they require
multi-action planning, dependency checks, metadata-lock semantics, or later
descriptor surfaces.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  backend status, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target table, resolves the dropped column
  from MyLite descriptors, rejects unsupported scope, and builds a fixed
  drop-column plan.
- The catalog module owns `_mylite_catalog_*` tables, table descriptor version
  changes, column descriptor deletion, remaining-column ordinal compaction,
  catalog generation advancement, and descriptor-cache invalidation.
- SQLite owns durable b-tree row storage and the physical schema change for the
  generated physical table. SQLite schema text and `PRAGMA` output remain
  physical implementation details, not MySQL-visible metadata authority.
- The result builder owns the empty DDL result.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call:

```sql
ALTER TABLE table_name DROP column_name
ALTER TABLE table_name DROP COLUMN column_name

table_name:
    identifier
  | identifier.identifier
```

Only unqualified column names are admitted. The parser rejects unsupported
clauses after the admitted column name.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= alter_table_drop_column_statement.

alter_table_drop_column_statement ::=
    ALTER TABLE table_name DROP column_keyword_opt identifier.

column_keyword_opt ::= .
column_keyword_opt ::= COLUMN.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply for qualified table names.

## Schema, Table, And Column Resolution

Unqualified target names use the selected schema. If no schema is selected,
MyLite returns MySQL error `1046`, SQLSTATE `3D000`, and message
`No database selected`.

Schema-qualified targets use the explicit schema and do not require a selected
schema. Unknown explicit schemas return MySQL error `1049`, SQLSTATE `42000`,
and message `Unknown database '<schema>'`. Unknown tables return MySQL error
`1146`, SQLSTATE `42S02`, and message `Table '<schema>.<table>' doesn't exist`.

The target table descriptor is resolved by logical schema id and logical table
name. The dropped column is resolved from the table's column descriptors by
logical column name. The current catalog comparison policy remains
case-insensitive for name lookup. Only `MYLITE_CATALOG_TABLE_KIND_BASE` is
supported. Later temporary tables, views, or other object kinds must be
rejected before any physical SQLite SQL is generated.

Reserved `_mylite_*` schema and table names are MyLite-owned internals and must
be rejected before generated SQLite SQL. Reserved `_mylite_*` column names
cannot be created through supported DDL; if encountered as a drop target, they
must be rejected before generated SQLite SQL.

Unknown dropped columns return MySQL error `1091`, SQLSTATE `42000`, and a
message containing `Can't DROP '<column>'; check that column/key exists`.

If the descriptor has exactly one column before the drop, MyLite returns MySQL
error `1090`, SQLSTATE `42000`, and message
`You can't delete all columns with ALTER TABLE; use DROP TABLE instead`.

## Descriptor Semantics

Dropping a column removes exactly one MyLite column descriptor from the target
table. Remaining column descriptors keep their existing `column_id`, logical
name, logical type, physical type, nullability, and creation generation. Their
visible order is compacted to contiguous ordinal positions starting at `1`,
matching MySQL's visible column order after a drop.

The table descriptor version increments once. The catalog generation advances
once. The dropped column's descriptor is no longer visible through
descriptor-driven `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `SHOW COLUMNS`,
`SHOW CREATE TABLE`, `DESCRIBE`, `SHOW TABLE STATUS`, or related planned
surfaces. The table id and stable physical table name remain unchanged.

No data conversion is performed for remaining integer/`NULL` values. Existing
row values for remaining columns are preserved in their logical order.

## Runtime Semantics

Successful `ALTER TABLE ... DROP [COLUMN]` returns a non-row result:

- result column count is `0`;
- result row count is `0`;
- `affected_rows == 0`;
- `warning_count == 0`;
- `ROW_COUNT()` for the next statement returns `0`;
- `@@warning_count` reports `0` for the supported successful drop.

The statement changes table metadata and physical SQLite schema, but it does
not otherwise mutate the selected schema. It must preserve `.mylite` preamble
bytes and the shifted SQLite payload invariant.

MySQL treats `ALTER TABLE` as DDL and documents implicit commit behavior. User
transactions and implicit commit boundaries are not implemented in this
baseline, so this slice does not claim transaction-boundary equivalence.

## Physical SQLite Handling

The runtime builds SQLite SQL only from descriptors:

```sql
ALTER TABLE "<physical_table_name>" DROP COLUMN "<column_name>"
```

The physical table name is the stable descriptor value such as
`_mylite_user_table_<table_id>`, quoted as a SQLite identifier. The dropped
column identifier is the resolved descriptor column name, quoted as a SQLite
identifier. No user literal or unresolved identifier is interpolated.

MyLite uses SQLite 3.53.0 from `third_party/sqlite/`, whose public SQL surface
includes native `ALTER TABLE ... DROP COLUMN`. This slice must not depend on
optional SQLite compile-time `UPDATE ... LIMIT`, writable-schema editing,
reconstructing schema text by hand, or SQLite fork hooks.

The catalog mutation and physical SQLite schema change must commit or roll back
as one statement. The preferred order is:

1. start a MyLite catalog mutation;
2. stage the catalog column deletion and ordinal compaction;
3. execute generated physical SQLite `ALTER TABLE ... DROP COLUMN`;
4. bump table descriptor identity/version inside the mutation;
5. commit the mutation;
6. increment connection-local `sqlite_schema_generation`.

If allocation, catalog mutation, generated SQL construction, physical SQLite
execution, or commit fails, MyLite rolls back the mutation, leaves descriptors
unchanged, does not increment schema generation, and reports a deterministic
diagnostic. The physical failure path must not leave a partially updated
catalog.

## Diagnostics

The implementation must preserve existing public API misuse behavior. For SQL
execution, this slice requires:

| Condition | Expected behavior |
| --- | --- |
| syntax outside admitted grammar | MyLite parse error using current parser diagnostic policy |
| no selected schema for unqualified target | `1046` / `3D000`, `No database selected` |
| unknown explicit schema | `1049` / `42000`, `Unknown database '<schema>'` |
| unknown target table | `1146` / `42S02`, `Table '<schema>.<table>' doesn't exist` |
| reserved `_mylite_*` schema/table name | deterministic MyLite reserved-name diagnostic |
| reserved `_mylite_*` column target | deterministic MyLite reserved-name diagnostic |
| unsupported object kind | deterministic MyLite unsupported-object diagnostic before SQLite SQL |
| unknown dropped column | `1091` / `42000`, `Can't DROP '<column>'; check that column/key exists` |
| dropping the only column | `1090` / `42000`, cannot delete all columns message |
| table-qualified column target | syntax error for this grammar |
| multiple drop actions | syntax error for this grammar |
| algorithm/lock/table options | syntax error for this grammar |
| physical SQLite failure | MyLite internal/physical SQLite schema diagnostic with catalog rollback |
| allocation failure | `MYLITE_NOMEM` and existing out-of-memory diagnostic policy |

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_alter_table_drop_column_expectations.sh`
records the runtime observations for this slice. It verifies:

- `ALTER TABLE t DROP c` and `ALTER TABLE t DROP COLUMN c` both succeed;
- a successful drop reports `ROW_COUNT() == 0` and `@@warning_count == 0`;
- remaining row values are preserved after dropping first, middle, and last
  non-only columns;
- `SHOW COLUMNS` and `SHOW CREATE TABLE` expose only remaining columns;
- schema-qualified drops work without a selected schema;
- no selected schema, unknown schema, unknown table, unknown column, and
  dropping the only column return the diagnostics listed above;
- table-qualified column targets are syntax errors;
- multiple drop actions and algorithm modifiers are accepted by MySQL but
  outside this MyLite slice;
- `DROP COLUMN IF EXISTS`, parenthesized drop lists, and positioning tokens are
  rejected by MySQL itself.

The probes were run against the local `mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --skip-column-names
```

## Tests

Add a fast C runtime lifecycle test under `packages/libmylite/tests/`, with a
dotted CTest name such as `libmylite.runtime.alter_table_drop_column`.

The test suite must cover:

- parser acceptance for `DROP` and `DROP COLUMN`;
- parser rejection for qualified dropped column names, multiple actions,
  algorithms, locks, `DROP PRIMARY KEY`, `DROP INDEX`, `DROP FOREIGN KEY`,
  `IF EXISTS`, parenthesized forms, and combined actions;
- selected-schema and schema-qualified target resolution, including qualified
  execution without selected schema;
- missing default schema, unknown schema, unknown table, unknown column,
  reserved target names, and dropping the only column;
- dropping first, middle, and last non-only columns from descriptor integer
  families, including nullable and `NOT NULL` columns;
- descriptor ordinal compaction, descriptor version/generation,
  descriptor-cache invalidation, and SQLite schema generation;
- row preservation for remaining columns;
- `SELECT *`, explicit `SELECT`, `SHOW COLUMNS`, `SHOW CREATE TABLE`, `INSERT`,
  `UPDATE`, `DELETE`, `TRUNCATE`, and rename/drop interaction after dropping
  columns;
- affected rows, warning count, result row absence, `ROW_COUNT()`, and
  `@@warning_count`;
- rollback on physical failure and unchanged catalog/schema generation after
  failure;
- reopen persistence, independent file-backed handles, and preamble safety;
- zero-initialized cleanup for new planner objects.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md` and
`docs/compatibility/sql-table-ddl.md` only for the exact limited single-column
drop subset. Do not overclaim full `ALTER TABLE`, multiple drop actions,
indexes, constraints, table rebuilds, algorithms, locks, temporary tables,
views, dependency maintenance, implicit commits, or arbitrary SQLite
pass-through.

## Verification

Before marking this feature done:

1. `cmake --build --preset dev`
2. Run the new CTest entry and existing parser/basic/rename/alter-rename/
   add-column/row-values/select/update/delete/truncate/show-column/show-create
   lifecycle entries.
3. `./packages/libmylite/tests/mysql_baseline_alter_table_drop_column_expectations.sh`
4. `cmake --workflow --preset check`

Review the final diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor-driven physical schema mutation, ordinal compaction,
file-format safety, VFS preservation, zero-init safety, cleanup on failure,
scope control, compatibility accuracy, and test relevance.

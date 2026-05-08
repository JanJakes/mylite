# Baseline ALTER TABLE RENAME COLUMN Lifecycle

## Status

This feature specifies the next narrow table-lifecycle slice for file-backed
`.mylite` handles. It adds descriptor-driven
`ALTER TABLE ... RENAME COLUMN old_col TO new_col` execution on top of
`mylite_execute()`, statement context, the MyLite parser scaffold, shifted
`.mylite` storage, durable catalog descriptors, create/drop/rename/truncate
table lifecycle, add/drop-column lifecycle, integer/`NULL` row storage,
descriptor `SELECT`, descriptor DML, and descriptor table introspection.

The feature is intentionally not full MySQL `ALTER TABLE` support. It supports
one column-rename action for persistent base tables. It does not implement
multiple actions, `CHANGE COLUMN`, `MODIFY COLUMN`, indexes, constraints,
temporary tables, views, metadata locks, algorithms, locks, privilege checks,
dependency invalidation for unsupported object kinds, or implicit commit
behavior.

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
- Baseline `ALTER TABLE ... DROP COLUMN` lifecycle:
  `docs/specs/baseline-alter-table-drop-column/specs.md`
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
- SQLite `ALTER TABLE RENAME COLUMN`:
  https://www.sqlite.org/lang_altertable.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited
  `ALTER TABLE table_name RENAME COLUMN old_col TO new_col` statement;
- one action and one renamed column only;
- persistent MyLite base-table descriptors only;
- unqualified and schema-qualified target table resolution;
- unqualified old and new column names only;
- descriptor-driven old-column lookup and duplicate-new-column detection;
- exact-name no-op handling compatible with MySQL 8.4.9;
- case-only rename support, preserving the new displayed column name;
- descriptor name update while preserving column id, ordinal position, type,
  nullability, and row values;
- descriptor version, catalog generation, descriptor-cache invalidation, and
  SQLite schema generation updates after successful physical schema mutation;
- generated physical SQLite `ALTER TABLE ... RENAME COLUMN ... TO ...` only
  from descriptors and stable physical table names;
- result behavior for successful DDL: no rows, `affected_rows == 0`, and
  `warning_count == 0`;
- MySQL 8.4.9 runtime-verified expectations for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- general `ALTER TABLE`;
- `ALTER ONLINE TABLE`, `WAIT`, `NOWAIT`, `ALGORITHM`, or `LOCK`;
- multiple `RENAME COLUMN` clauses or combined alter actions;
- `CHANGE COLUMN`, `MODIFY COLUMN`, or type/attribute changes;
- `RENAME INDEX`, `RENAME KEY`, table options, partitions, or positioning;
- table-qualified old or new column names;
- temporary tables, views, triggers, privileges, metadata locks, foreign keys,
  cascades, generated columns, invisible columns, routines, events, or
  `INFORMATION_SCHEMA` dependency maintenance;
- reconstructing descriptors from SQLite schema text;
- generalized table rebuilds or SQLite fork patches.

MySQL accepts several wider forms, including multiple rename actions and
algorithm/lock modifiers. MyLite rejects them in this phase because they
require multi-action planning, dependency checks, metadata-lock semantics, or
later descriptor surfaces.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  backend status, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target table, resolves old and new column
  names from MyLite descriptors, rejects unsupported scope, and builds a fixed
  rename-column plan.
- The catalog module owns `_mylite_catalog_*` tables, table descriptor version
  changes, column descriptor name updates, catalog generation advancement, and
  descriptor-cache invalidation.
- SQLite owns durable b-tree row storage and the physical schema change for the
  generated physical table. SQLite schema text and `PRAGMA` output remain
  physical implementation details, not MySQL-visible metadata authority.
- The result builder owns the empty DDL result.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call:

```sql
ALTER TABLE table_name RENAME COLUMN old_col_name TO new_col_name

table_name:
    identifier
  | identifier.identifier
```

Only unqualified old and new column names are admitted. `COLUMN` is required
for column rename. The parser rejects unsupported clauses after the admitted
new column name.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= alter_table_rename_column_statement.

alter_table_rename_column_statement ::=
    ALTER TABLE table_name RENAME COLUMN identifier TO identifier.

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
name. The old column is resolved from the table's column descriptors by
logical column name. The new column name is checked against descriptors before
any physical SQLite SQL is generated. The current catalog comparison policy
remains case-insensitive for name lookup. Only
`MYLITE_CATALOG_TABLE_KIND_BASE` is supported. Later temporary tables, views,
or other object kinds must be rejected before any physical SQLite SQL is
generated.

Reserved `_mylite_*` schema and table names are MyLite-owned internals and must
be rejected before generated SQLite SQL. Reserved `_mylite_*` old or new column
names must be rejected before generated SQLite SQL.

Unknown old columns return MySQL error `1054`, SQLSTATE `42S22`, and message
`Unknown column '<column>' in '<table>'`. New names that collide with a
different existing descriptor column return MySQL error `1060`, SQLSTATE
`42S21`, and message `Duplicate column name '<column>'`.

If the old and new names are byte-for-byte equal, MyLite returns a successful
no-op result and does not mutate catalog rows, descriptor versions, catalog
generation, physical SQLite schema, or `sqlite_schema_generation`. MySQL 8.4.9
reports `ROW_COUNT() == 0` and `@@warning_count == 0` for this case.

If the old and new names match case-insensitively but differ byte-for-byte,
MyLite updates the descriptor to the new spelling and performs the physical
rename. MySQL 8.4.9 updates the visible column name in this case.

## Descriptor Semantics

Renaming a column updates exactly one MyLite column descriptor name. The column
descriptor keeps its existing `column_id`, `ordinal_position`, logical type,
physical type, nullability, and creation generation. Column order does not
change.

The table descriptor version increments once for non-no-op renames. The catalog
generation advances once. The renamed column is visible under its new name
through descriptor-driven `SELECT`, `INSERT`, `UPDATE`, `DELETE`,
`SHOW COLUMNS`, `SHOW CREATE TABLE`, `DESCRIBE`, `SHOW TABLE STATUS`, and
related planned surfaces. The old name becomes unknown after a successful
rename. The table id and stable physical table name remain unchanged.

No data conversion is performed for integer/`NULL` values. Existing row values
for the renamed column and all other columns are preserved.

## Runtime Semantics

Successful `ALTER TABLE ... RENAME COLUMN` returns a non-row result:

- result column count is `0`;
- result row count is `0`;
- `affected_rows == 0`;
- `warning_count == 0`;
- `ROW_COUNT()` observes `0`;
- `@@warning_count` observes `0`.

The operation is statement-atomic within MyLite's current single-statement
boundary. If catalog mutation, allocation, physical SQLite execution, or commit
fails, the catalog mutation is rolled back and the public result is an error.

## Physical SQLite Handling

Generated SQL has this shape:

```sql
ALTER TABLE "<physical_table_name>" RENAME COLUMN "<old_column_name>" TO "<new_column_name>"
```

The physical table name comes from the resolved MyLite table descriptor. The
old physical column name comes from the resolved MyLite column descriptor. The
new physical column name is the validated new logical column name. Every
generated SQLite identifier must be double-quoted with the existing MyLite
identifier quoting helper.

No SQL literals are interpolated. There are no expression values to bind for
this slice. The implementation must not depend on SQLite metadata as MySQL
metadata authority, and it must not use writable-schema editing, arbitrary
SQLite pass-through, generalized table rebuilds, or SQLite fork patches.

SQLite public `ALTER TABLE RENAME COLUMN` is sufficient for the physical row
storage MyLite currently generates. Existing generated MyLite user tables are
ordinary rowid tables, but rename-column behavior must not expose or depend on
rowid semantics.

The implementation order for non-no-op renames is:

1. begin MyLite catalog mutation;
2. update the MyLite column descriptor name and generation;
3. execute generated physical SQLite `ALTER TABLE ... RENAME COLUMN`;
4. update the table descriptor version;
5. commit the catalog mutation;
6. increment the connection-local `sqlite_schema_generation`.

If any step fails before commit, the catalog mutation must roll back. The
`.mylite` preamble must remain unchanged.

## Diagnostics

Supported diagnostics:

- syntax errors and unsupported grammar: existing parse error `1064`, SQLSTATE
  `42000`;
- missing default schema for unqualified target: `1046`, `3D000`,
  `No database selected`;
- unknown explicit schema: `1049`, `42000`,
  `Unknown database '<schema>'`;
- unknown table: `1146`, `42S02`,
  `Table '<schema>.<table>' doesn't exist`;
- reserved target schema/table/column names: existing MyLite reserved-name
  diagnostics before generated SQLite SQL;
- unsupported object kind: MyLite unsupported-feature diagnostic before
  generated SQLite SQL;
- unknown old column: `1054`, `42S22`,
  `Unknown column '<column>' in '<table>'`;
- duplicate new column: `1060`, `42S21`,
  `Duplicate column name '<column>'`;
- table-qualified old or new column names, missing `COLUMN`, missing `TO`,
  multiple actions, algorithms, locks, and positioning tokens: syntax error
  for this slice;
- physical SQLite failures: internal SQLite schema-operation diagnostic;
- allocation failures: existing out-of-memory diagnostic;
- public API misuse: unchanged existing public execution/result misuse
  behavior.

Supported in-range renames produce no warnings.

## MySQL 8.4.9 Runtime Observations

The reproducible expectation script is
`packages/libmylite/tests/mysql_baseline_alter_table_rename_column_expectations.sh`.
It checks that the runtime is MySQL `8.4.9` before evaluating expectations.

Observed behavior:

- `ALTER TABLE t RENAME COLUMN a TO b` succeeds for a persistent base table;
- success reports `ROW_COUNT() == 0` and `@@warning_count == 0`;
- row values and column ordinal positions are preserved;
- schema-qualified targets work without a selected default schema;
- no selected schema for unqualified targets reports `1046/3D000`;
- unknown explicit schema reports `1049/42000`;
- unknown table reports `1146/42S02`;
- unknown old column reports `1054/42S22`;
- duplicate new column reports `1060/42S21`;
- exact same-name rename succeeds with zero row count and no warnings;
- case-only rename succeeds and updates the visible column spelling;
- table-qualified old and new column names are syntax errors;
- missing `COLUMN`, missing `TO`, and positioning tokens are syntax errors;
- multiple rename actions and `ALGORITHM`/`LOCK` modifiers are accepted by
  MySQL but remain outside this MyLite slice.

## Tests

Implementation must add fast plain C tests under `packages/libmylite/tests/`,
registered with a dotted CTest name such as
`libmylite.runtime.alter_table_rename_column_lifecycle`.

Required C coverage:

- parser acceptance for the supported statement;
- parser rejection for missing `COLUMN`, missing `TO`, table-qualified column
  names, multiple actions, algorithms, locks, and positioning tokens;
- successful selected-schema and schema-qualified renames;
- first, middle, and last column renames preserving ordinals and row values;
- exact same-name no-op preserving descriptor versions, catalog generation, and
  SQLite schema generation;
- case-only rename updating descriptor and physical name;
- DML and descriptor introspection after rename;
- reopen persistence;
- rename after table rename and after drop where applicable;
- unknown schema, unknown table, unknown old column, duplicate new column, and
  reserved `_mylite_*` names;
- physical SQLite failure rollback preserving catalog descriptors;
- `.mylite` preamble preservation;
- independent file-backed handles with independent renamed state;
- zero-initialized cleanup for new statement/planner/result objects.

Existing lexer, parser, runtime handle, diagnostics, statement context, result
metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
foundation, create/drop/rename/truncate lifecycle, add/drop-column lifecycle,
row values, select, DML, client-data, and registration tests must continue to
pass.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only for
the exact supported subset. Do not overclaim full `ALTER TABLE`, `CHANGE
COLUMN`, `MODIFY COLUMN`, multiple rename actions, aliases, partitions, views,
dependency updates, generated columns, invisible columns, keys, constraints,
algorithms, locks, metadata locks, privileges, or implicit commit semantics.

## Verification

Before marking the feature done:

1. `cmake --build --preset dev`
2. Run the new CTest entry and existing parser/basic/rename/alter-rename/
   add-column/drop-column/row-values/select/update/delete/truncate/show-column/
   show-create lifecycle entries.
3. `./packages/libmylite/tests/mysql_baseline_alter_table_rename_column_expectations.sh`
4. `cmake --workflow --preset check`
5. Review the final diff for architecture boundaries, public ABI stability,
   independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
   authority, descriptor-driven physical schema mutation, no-op handling,
   case-only rename behavior, file-format safety, VFS preservation, zero-init
   safety, cleanup on failure, scope control, compatibility accuracy, and test
   relevance.

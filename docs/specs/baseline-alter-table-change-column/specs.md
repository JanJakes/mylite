# Baseline ALTER TABLE CHANGE COLUMN Lifecycle

## Status

This feature specifies the next narrow table-lifecycle slice for file-backed
`.mylite` handles. It adds descriptor-driven
`ALTER TABLE ... CHANGE [COLUMN] old_column new_column_definition` execution on
top of the existing MyLite public execution API, statement context, parser
scaffold, shifted `.mylite` storage, durable catalog descriptors, create/drop/
rename/truncate table lifecycle, add/drop/rename/modify-column lifecycle,
integer/`NULL` row storage, descriptor `SELECT`, descriptor DML, and descriptor
table introspection.

The feature is intentionally not full MySQL `ALTER TABLE` support. It supports
descriptor-driven column changes for persistent base tables and the currently
admitted descriptor families. The dedicated multi-action ALTER slice may
combine this same admitted no-warning change subset with other supported
actions. This feature does not implement temporary tables, views, metadata
locks, broad online-DDL behavior, privilege checks, dependency invalidation for
unsupported object kinds, or full implicit-commit behavior.

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
- Baseline `ALTER TABLE ... RENAME COLUMN` lifecycle:
  `docs/specs/baseline-alter-table-rename-column/specs.md`
- Baseline `ALTER TABLE ... MODIFY COLUMN` lifecycle:
  `docs/specs/baseline-alter-table-modify-column/specs.md`
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
- SQLite `ALTER TABLE`:
  https://www.sqlite.org/lang_altertable.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited
  `ALTER TABLE table_name CHANGE [COLUMN] old_column column_definition`
  statement;
- one changed column per `CHANGE` action;
- persistent MyLite base-table descriptors only;
- unqualified and schema-qualified target table resolution;
- unqualified old and replacement column names only;
- `INT`, `INTEGER`, and `BIGINT`, each optionally `UNSIGNED`;
- optional `NULL` and `NOT NULL`; omitted nullability means nullable because
  MySQL `CHANGE` replaces the column definition rather than preserving omitted
  attributes;
- descriptor-driven old-column lookup and replacement-name duplicate checks;
- case-only spelling updates for the changed column;
- descriptor name, type, and nullability replacement while preserving column id,
  ordinal position, table id, table name, physical table name, and row values;
- validation that existing non-`NULL` row values fit the target integer range;
- validation that existing `NULL` row values are rejected when the target
  definition is `NOT NULL`;
- descriptor version, catalog generation, descriptor-cache invalidation, and
  SQLite schema generation updates after successful physical schema mutation;
- descriptor-driven physical rename for pure metadata renames and
  descriptor-driven physical table rebuild for type or nullability changes;
- result behavior for successful DDL: no rows, `warning_count == 0`, and
  `affected_rows` matching MySQL 8.4.9 for this subset;
- MySQL 8.4.9 runtime-verified expectations for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- general `ALTER TABLE`;
- `ALTER ONLINE TABLE`, `WAIT`, `NOWAIT`, `ALGORITHM`, or `LOCK`;
- multiple `CHANGE` clauses or combined alter actions outside the dedicated
  limited multi-action ALTER slice;
- `MODIFY COLUMN`, `RENAME COLUMN`, or table rename in this `CHANGE` path;
- table-qualified old or replacement column names;
- non-admitted `DEFAULT`, generated, invisible, auto-increment, primary key, unique,
  foreign key, check, comment, collation, charset, storage, or option clauses;
- non-integer column types;
- indexes, keys, constraints, table options, or partitions;
- temporary tables, views, triggers, privileges, metadata locks, foreign keys,
  cascades, generated columns, invisible columns, routines, events, or
  `INFORMATION_SCHEMA` dependency maintenance;
- reconstructing descriptors from SQLite schema text;
- SQLite fork patches.

MySQL accepts several wider forms. MyLite only admits the replacement
descriptors, positioning, and no-warning multi-action combinations that are
covered by the current descriptor model and dedicated ALTER slices.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  backend status, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target table, resolves the old column
  from MyLite descriptors, validates the replacement descriptor, checks
  duplicate replacement names, validates existing row compatibility, rejects
  unsupported scope, and builds a fixed change plan.
- The catalog module owns `_mylite_catalog_*` tables, table descriptor version
  changes, column descriptor name/type/nullability updates, catalog generation
  advancement, and descriptor-cache invalidation.
- SQLite owns durable b-tree row storage and physical table replacement or
  native physical column rename. SQLite schema text and `PRAGMA` output remain
  physical implementation details, not MySQL-visible metadata authority.
- The result builder owns the empty DDL result and affected-row count.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call:

```sql
ALTER TABLE table_name CHANGE old_column_name column_definition
ALTER TABLE table_name CHANGE COLUMN old_column_name column_definition

column_definition:
    new_column_name integer_type [NULL | NOT NULL]

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

Only unqualified old and replacement column names are admitted. The parser
rejects unsupported clauses after the admitted replacement column definition.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= alter_table_change_column_statement.

alter_table_change_column_statement ::=
    ALTER TABLE table_name CHANGE column_keyword_opt identifier column_definition.

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

## Schema, Table, And Column Resolution

Unqualified target names use the selected schema. If no schema is selected,
MyLite returns MySQL error `1046`, SQLSTATE `3D000`, and message
`No database selected`.

Schema-qualified targets use the explicit schema and do not require a selected
schema. Unknown explicit schemas return MySQL error `1049`, SQLSTATE `42000`,
and message `Unknown database '<schema>'`. Unknown tables return MySQL error
`1146`, SQLSTATE `42S02`, and message `Table '<schema>.<table>' doesn't exist`.

The target table descriptor is resolved by logical schema id and logical table
name. The old column is resolved from the table's column descriptors using the
current case-insensitive catalog comparison policy. If no old descriptor
matches, MyLite returns MySQL error `1054`, SQLSTATE `42S22`, and message
`Unknown column '<column>' in '<table>'`.

The replacement column name is checked against all descriptors before generated
SQLite SQL. If it collides case-insensitively with a different descriptor
column, MyLite returns MySQL error `1060`, SQLSTATE `42S21`, and message
`Duplicate column name '<column>'`, using the conflicting descriptor spelling.
If the old and replacement names match case-insensitively, the statement
changes that descriptor and may update only its visible spelling.

Only `MYLITE_CATALOG_TABLE_KIND_BASE` is supported. Later temporary tables,
views, or other object kinds must be rejected before any physical SQLite SQL is
generated.

Reserved `_mylite_*` schema and table names are MyLite-owned internals and must
be rejected before generated SQLite SQL. Reserved `_mylite_*` old or
replacement column names must be rejected before generated SQLite SQL.

## Descriptor Semantics

`CHANGE` updates exactly one MyLite column descriptor:

- `name` becomes the replacement definition name;
- `logical_type` becomes the replacement integer logical type;
- `physical_type` remains the current integer physical type for the supported
  subset;
- `is_nullable` becomes the replacement nullability, with omitted nullability
  treated as nullable.

The descriptor keeps its existing `column_id`, `ordinal_position`, and creation
generation. Column order does not change in this slice. The table descriptor
version increments once for non-no-op mutations. The table id and stable
physical table name remain unchanged.

The changed column is visible under its replacement spelling through
descriptor-driven `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `SHOW COLUMNS`,
`SHOW CREATE TABLE`, `DESCRIBE`, `SHOW TABLE STATUS`, and related planned
surfaces. Case-insensitive column lookup continues to resolve alternate
spellings according to the current catalog policy.

## Existing Row Validation

For type or nullability changes, MyLite validates every existing row against
the target descriptor using MyLite-owned conversion rules after starting the
catalog mutation, before descriptor replacement, and before any physical table
rebuild SQL. The mutation uses MyLite's `BEGIN IMMEDIATE` catalog transaction,
so same-file writers cannot add or change physical rows between validation and
copy. `NULL` row values remain valid only when the target is nullable. A
`NULL` value in a target `NOT NULL` column fails with MySQL error `1265`,
SQLSTATE `01000`, and message
`Data truncated for column '<column>' at row <n>`. The failing row number is
one-based in ascending physical rowid scan order for this phase.

Non-`NULL` integer row values must fit the target logical type:

| Logical type | Supported existing-value range in this phase |
| --- | --- |
| `INT` / `INTEGER` | `-2147483648` through `2147483647` |
| `INT UNSIGNED` / `INTEGER UNSIGNED` | `0` through `4294967295` |
| `BIGINT` | `-9223372036854775808` through `9223372036854775807` |
| `BIGINT UNSIGNED` | `0` through `9223372036854775807` |

MySQL supports `BIGINT UNSIGNED` values above `9223372036854775807`, but the
current MyLite physical row model stores SQLite signed 64-bit integers only.
This slice must not claim support for `BIGINT UNSIGNED` values above that
physical range.

An out-of-range existing row value fails with MySQL error `1264`, SQLSTATE
`22003`, and message `Out of range value for column '<column>' at row <n>`.
The catalog and physical table remain unchanged.

## Result Semantics

Successful `ALTER TABLE ... CHANGE [COLUMN]` returns a non-row result:

- result column count is `0`;
- result row count is `0`;
- `warning_count == 0`;
- `ROW_COUNT()` observes the affected-row count described below;
- `@@warning_count` observes `0`.

For byte-for-byte identical descriptor definitions with the same visible name,
MyLite returns a successful no-op result and does not mutate catalog rows,
descriptor versions, catalog generation, physical SQLite schema, or
`sqlite_schema_generation`. `affected_rows == 0`.

For pure renames and case-only spelling changes with unchanged type and
nullability, MyLite reports `affected_rows == 0`, matching observed MySQL 8.4.9
behavior.

For nullability-only changes with the same logical type, with or without a
rename, MyLite reports `affected_rows == 0`, matching observed MySQL 8.4.9
behavior. For type changes, with or without rename/nullability changes, MyLite
reports the number of rows copied through the descriptor rebuild. Empty tables
report `0`; non-empty tables report the current physical row count. This
matches observed MySQL 8.4.9 behavior for the admitted integer subset.

## Physical SQLite Handling

SQLite has no public `ALTER TABLE CHANGE COLUMN` operation. MyLite must use a
descriptor-driven wrapper path, not arbitrary SQLite pass-through and not
SQLite schema-text editing.

For exact no-op changes, no generated SQLite SQL is required. For pure
name-spelling changes with unchanged type and nullability, a native
`ALTER TABLE ... RENAME COLUMN ... TO ...` is acceptable because it is
semantically equivalent to the physical part of this metadata-only change; the
operation must still update MyLite descriptors as the metadata authority.

For type or nullability changes, generated SQL must be built from descriptors
and stable physical names only:

1. validate target descriptor, old column, duplicate replacement name, and
   target column list;
2. begin a MyLite catalog mutation;
3. validate existing rows inside the mutation before descriptor replacement;
4. update the catalog column descriptor;
5. create a temporary physical table with the target descriptor, quoted
   identifiers, and a MyLite-owned temporary physical name;
6. copy rows from the old physical table into the temporary table by explicit
   descriptor column list, preserving column order and reading the old physical
   column name for the changed column;
7. drop the old physical table;
8. rename the temporary physical table to the stable physical table name;
9. update the table identity inside the same mutation;
10. commit the mutation and increment connection-local SQLite schema
    generation.

The validation and copy query need a deterministic row identity while
preserving MyLite's descriptor column order. This slice uses SQLite rowid
tables generated by MyLite and requires at least one unshadowed SQLite rowid
alias (`rowid`, `_rowid_`, or `oid`) for rebuilds. If all aliases are shadowed
by user column names, MyLite rejects the rebuild before mutation with an
explicit unsupported diagnostic. Later storage work can remove this limitation
by introducing a hidden MyLite row identity or another descriptor-owned copy
key.

The rebuild path must be statement-atomic for MyLite's current single-statement
boundary. If allocation, validation, generated SQL preparation, physical table
creation, row copy, drop, rename, catalog update, or commit fails, the catalog
mutation is rolled back and the physical table must be left in the old
descriptor-compatible state or restored before returning.

Every generated SQLite identifier must be double-quoted with the existing
MyLite identifier quoting helper. No user SQL literal text may be interpolated
into generated SQL.

This feature must preserve the `.mylite` preamble and shifted SQLite payload
invariants and must not patch the SQLite fork.

## Diagnostics

The implementation must use deterministic diagnostics for:

- syntax errors and unsupported grammar: existing parser syntax diagnostic,
  MySQL error `1064`, SQLSTATE `42000`;
- missing default schema: MySQL error `1046`, SQLSTATE `3D000`;
- unknown schema: MySQL error `1049`, SQLSTATE `42000`;
- unknown table: MySQL error `1146`, SQLSTATE `42S02`;
- reserved target schema, table, old column, or replacement column:
  MyLite reserved-name diagnostic;
- unsupported object kind: MyLite unsupported diagnostic before generated
  SQLite SQL;
- unknown old column: MySQL error `1054`, SQLSTATE `42S22`;
- duplicate replacement column: MySQL error `1060`, SQLSTATE `42S21`;
- unsupported replacement expression, type, default, positioning, action,
  algorithm, lock, or option: existing parser syntax diagnostic or explicitly
  unsupported runtime diagnostic if a parsed surface reaches runtime later;
- out-of-range existing row values: MySQL error `1264`, SQLSTATE `22003`;
- `NULL` into target `NOT NULL`: MySQL error `1265`, SQLSTATE `01000`;
- physical SQLite failures: existing internal/SQLite failure policy;
- allocation failures: `MYLITE_NOMEM` and the existing no-memory diagnostic;
- public API misuse: no new public surface; preserve existing behavior.

Supported in-range changes produce no warnings.

## MySQL 8.4.9 Runtime Observations

The expectation script for this feature verifies these observed MySQL 8.4.9
behaviors:

- `CHANGE` and `CHANGE COLUMN` are accepted.
- Schema-qualified targets work without a selected default schema.
- Unknown old columns report `1054` / `42S22`.
- Replacement names that collide with another column report `1060` / `42S21`.
- Table-qualified old or replacement column names are syntax errors.
- Omitted nullability makes the replacement descriptor nullable.
- Pure renames and same-definition no-ops report `ROW_COUNT() == 0` and
  `@@warning_count == 0`.
- Nullability-only changes report `ROW_COUNT() == 0` when validation succeeds.
- Type changes report copied rows for non-empty tables.
- Existing `NULL` values rejected by target `NOT NULL` report `1265` /
  `01000`.
- Narrowing and signed-to-unsigned range failures report `1264` / `22003`.
- MySQL accepts broader defaults, non-integer target types, positioning,
  multiple `CHANGE` actions, algorithms, and locks. MyLite admits only the
  verified descriptor subsets recorded in the compatibility matrix and the
  dedicated multi-action ALTER slice.

## Compatibility Documentation

After implementation, update:

- `COMPATIBILITY.md`
  - `CHANGE COLUMN`: limited persistent base-table subset with dedicated
    multi-action participation for current no-warning replacements;
  - integer type rows if needed to mention descriptor replacement through
    `CHANGE`;
  - assignment/conversion rows only if the implementation changes the shared
    conversion surface beyond modify-column behavior.
- `docs/compatibility/sql-table-ddl.md`
  - `ALTER TABLE` and ALTER action tables for the limited `CHANGE [COLUMN]`
    subset and rowid-alias rebuild limitation.
- `docs/compatibility/type-system-literals-conversion.md`
  - only if the phase changes documented integer conversion behavior.

Do not overclaim full `CHANGE COLUMN`, defaults, non-integer types,
positioning, multi-action forms outside the dedicated limited multi-action
ALTER slice, algorithms, locks, indexes, keys, constraints, dependencies,
temporary tables, views, privilege semantics, or implicit commit behavior.

## Test Plan

Coverage must include:

- parser acceptance for `CHANGE` and `CHANGE COLUMN`;
- parser rejection for table-qualified old/replacement column names, missing
  replacement type, non-admitted defaults, non-admitted types, algorithms,
  locks, and combined actions outside the dedicated limited multi-action ALTER
  slice;
- successful pure rename, type replacement, nullability-only replacement, and
  rename-plus-type replacement across `INT`, `INTEGER`, `BIGINT`, and unsigned
  variants within the supported physical range;
- omitted nullability replacing an existing `NOT NULL` descriptor with
  nullable;
- explicit `NOT NULL` replacing nullable when existing rows contain no `NULL`;
- deterministic `1265` diagnostics when existing rows contain `NULL`;
- deterministic `1264` diagnostics for negative signed-to-unsigned and
  `BIGINT`-to-`INT` out-of-range existing values;
- same-definition no-op preserving catalog generation and SQLite schema
  generation;
- case-only spelling change updating descriptor-visible and physical-visible
  column names with zero affected rows;
- duplicate replacement-name and unknown old-column diagnostics;
- schema-qualified and unqualified target resolution, including missing default
  schema, unknown schema, unknown table, and reserved `_mylite_*` target names;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, descriptor DML, filtered/ordered
  `SELECT`, `UPDATE`, `DELETE`, and `INSERT` after change;
- change after table rename, column rename, add column, and drop column where
  applicable;
- reopen persistence;
- physical SQLite payload behavior without touching the MyLite preamble;
- independent file-backed handles with independent changed descriptors and row
  state;
- rowid-alias rebuild rejection when all aliases are shadowed;
- rollback on validation and physical rebuild failures;
- zero-initialized cleanup for new planner/rebuild objects.

Verification before marking done:

1. `cmake --build --preset dev`
2. Run the new CTest entry and existing parser/basic/rename/alter-rename/
   add-column/drop-column/rename-column/modify-column/row-values/select/update/
   delete/truncate/show-column/show-create lifecycle entries.
3. `./packages/libmylite/tests/mysql_baseline_alter_table_change_column_expectations.sh`
4. `cmake --workflow --preset check`

# Baseline ALTER COLUMN Visibility

## Status

This feature specifies a narrow descriptor-driven column visibility slice:
`ALTER TABLE table_name ALTER [COLUMN] column_name SET VISIBLE` and
`ALTER TABLE table_name ALTER [COLUMN] column_name SET INVISIBLE` for
persistent base tables with existing MyLite integer-family descriptors.

This is not general invisible-column support. It does not add `INVISIBLE` or
`VISIBLE` to `CREATE TABLE`, `ADD COLUMN`, `MODIFY COLUMN`, or `CHANGE COLUMN`
column definitions; it only toggles visibility for existing descriptor columns.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing table, row, default, DML, and introspection baseline specs under
  `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, invisible columns:
  https://dev.mysql.com/doc/refman/8.4/en/invisible-columns.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Evidence

The MySQL 8.4 manual lists `ALTER [COLUMN] col_name SET {VISIBLE | INVISIBLE}`
as an `ALTER TABLE` action, and documents that invisible columns are omitted
from `SELECT *` while remaining explicitly addressable. Runtime probes against
MySQL 8.4.9 verify the following baseline behavior:

- `ALTER TABLE t ALTER c SET INVISIBLE`,
  `ALTER TABLE t ALTER COLUMN c SET INVISIBLE`,
  `ALTER TABLE t ALTER c SET VISIBLE`, and
  `ALTER TABLE t ALTER COLUMN c SET VISIBLE` are accepted.
- The statement reports `ROW_COUNT() == 0` and `@@warning_count == 0`,
  including no-op visibility changes.
- Existing rows are not rewritten.
- `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` show `INVISIBLE` in `Extra`
  for invisible columns and empty `Extra` for visible columns.
- `SHOW CREATE TABLE` renders invisible columns with
  `/*!80023 INVISIBLE */` after the column's type/default/nullability text.
- `SELECT *` omits invisible columns. Explicit `SELECT column`, `WHERE`,
  `ORDER BY`, `UPDATE`, and `DELETE` references can still use invisible
  columns.
- `INSERT INTO t VALUES (...)` without a column list expects values only for
  visible columns. Omitted invisible columns receive their descriptor default,
  implicit nullable `NULL`, or the same no-default error that MySQL reports for
  omitted required columns.
- `INSERT ... SET` and `INSERT ... (column list)` can explicitly assign an
  invisible column.
- MySQL rejects making the last visible column invisible with error `4028`,
  SQLSTATE `HY000`, message
  `A table must have at least one visible column.`
- Missing default schema, unknown schema, unknown table, and unknown column use
  the existing MySQL diagnostics for those name-resolution failures.
- MySQL accepts broader forms outside this slice, including multi-action
  `ALTER TABLE` visibility changes and `INVISIBLE` column definitions.

The script
`packages/libmylite/tests/mysql_baseline_alter_column_visibility_expectations.sh`
records these runtime expectations.

## Scope

The implementation must add:

- parser and AST support for one `ALTER TABLE ... ALTER [COLUMN] ... SET
  VISIBLE|INVISIBLE` action;
- unqualified and schema-qualified table-name resolution through the existing
  selected/default schema policy;
- unqualified target column resolution against MyLite descriptors;
- durable descriptor visibility metadata with existing columns migrated to
  visible;
- catalog-only descriptor mutation that preserves row storage and physical
  SQLite schema;
- enforcement that at least one descriptor column remains visible;
- `SELECT *` projection filtering based on descriptor visibility;
- implicit `INSERT ... VALUES` column mapping based on visible descriptor
  columns when no explicit column list is provided;
- continued explicit access to invisible columns in supported `SELECT` column
  lists, `WHERE`, `ORDER BY`, `INSERT`, `UPDATE`, and `DELETE` forms;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  visibility metadata.

## Non-Goals

This feature must not implement:

- `INVISIBLE` or `VISIBLE` in `CREATE TABLE`, `ADD COLUMN`, `MODIFY COLUMN`,
  or `CHANGE COLUMN` column definitions;
- generated invisible primary keys or `@@sql_generate_invisible_primary_key`
  effects beyond the existing fixed scalar read;
- table-qualified column targets;
- multiple `ALTER TABLE` actions;
- `ALTER INDEX VISIBLE|INVISIBLE`;
- visible/invisible index metadata, optimizer behavior, protocol flag
  metadata, `INFORMATION_SCHEMA.COLUMNS`, `TABLE` statement behavior, views,
  temporary tables, privileges, or binary logging;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged.
- Lexer/parser/AST own only the admitted syntax and source spans.
- Analyzer/planner owns table and column resolution, reserved-name checks,
  unsupported-object rejection, and last-visible-column validation.
- Catalog owns durable logical visibility metadata. SQLite schema text remains
  non-authoritative.
- Result builder reports a normal non-row DDL result with `affected_rows == 0`
  and `warning_count == 0` for supported successful statements.
- Query planning decides whether `SELECT *` expands all descriptor columns or
  only visible descriptor columns. Explicit column references continue to
  resolve against all descriptor columns.
- Insert planning decides whether an omitted column list means visible
  descriptor columns only. Explicit insert column lists continue to resolve
  against all descriptor columns.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
- SQLite owns physical row storage. This feature does not require user-table
  SQL because only descriptor metadata changes.

## Supported SQL Grammar

The admitted grammar is deliberately small:

```sql
alter_table_column_visibility:
    ALTER TABLE table_name ALTER column_name SET VISIBLE
  | ALTER TABLE table_name ALTER COLUMN column_name SET VISIBLE
  | ALTER TABLE table_name ALTER column_name SET INVISIBLE
  | ALTER TABLE table_name ALTER COLUMN column_name SET INVISIBLE
```

`table_name` may be unqualified or schema-qualified. `column_name` must be a
single unqualified identifier.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
alter_table_column_visibility_statement ::=
    ALTER TABLE table_name ALTER column_keyword_opt identifier SET
    column_visibility.

column_keyword_opt ::= .
column_keyword_opt ::= COLUMN.

column_visibility ::= VISIBLE.
column_visibility ::= INVISIBLE.
```

`VISIBLE` and `INVISIBLE` are nonreserved MySQL keywords, so MyLite must still
allow them as ordinary identifiers outside the admitted visibility action.

## Resolution Semantics

Unqualified table names require the currently selected schema. Schema-qualified
table names use the explicit schema and do not require a selected schema.
Unknown schemas and unknown tables use the existing MySQL-compatible
diagnostics from the table lifecycle and alter-table slices.

Target schemas, tables, and columns with reserved `_mylite_*` names are rejected
before any catalog mutation. Only persistent base-table descriptors are
supported. Once non-base object descriptors exist, this statement must reject
them with a deterministic unsupported-object diagnostic.

Column resolution is descriptor-driven and case-insensitive according to the
current catalog identifier policy. Physical SQLite schema text is not consulted
to decide whether a column exists or whether it is visible. Unknown columns use
MySQL-compatible error `1054` for the supported subset.

## Catalog Semantics

Column descriptors gain a durable visibility state:

| State | Meaning |
| --- | --- |
| visible | The column participates in `SELECT *` and implicit no-column-list inserts. |
| invisible | The column is omitted from `SELECT *` and implicit no-column-list inserts, but explicit references are valid. |

Existing catalogs migrate every descriptor column to visible. New columns from
the existing supported `CREATE TABLE` and `ALTER TABLE ... ADD COLUMN` surfaces
are visible because this slice does not admit invisible column definitions.

The catalog schema must advance to the next version. Version-3 catalogs must
migrate to the new shape without changing existing descriptor semantics. Earlier
catalogs still migrate sequentially through the prior versions.

Successful visibility mutation updates only the target column descriptor's
visibility state and the containing table descriptor version/generation. Column
id, ordinal position, name, type, nullability, default state, physical table
name, physical column name, and existing row values are preserved.

The catalog generation increments. `sqlite_schema_generation` does not
increment because the physical SQLite schema is unchanged.

No-op visibility changes are accepted and report zero affected rows and
warnings.

## Last Visible Column Rule

Before changing a visible column to invisible, MyLite must verify that at least
one other descriptor column in the table remains visible. If not, it must reject
the statement with MySQL error `4028`, SQLSTATE `HY000`, and message
`A table must have at least one visible column.`

Changing an already invisible column to invisible is a no-op and does not need
to re-count as a last-visible removal. Changing an invisible column to visible
is always allowed.

## DML Semantics

`SELECT *` from a descriptor-backed table expands only visible descriptor
columns, in ordinal order. If an invisible column is explicitly named in the
select list, predicate, or order clause, it resolves and reads normally.

`INSERT INTO table VALUES (...)` without an explicit column list maps supplied
values to visible descriptor columns only, in ordinal order. Invisible columns
are treated as omitted columns and receive their descriptor integer default,
implicit nullable `NULL`, or existing dropped/no-default error behavior.

Explicit insert column lists and `INSERT ... SET` assignment targets can name
visible or invisible descriptor columns. Existing duplicate-column,
unknown-column, conversion, range, nullability, default, affected-row, and
atomicity rules continue to apply.

Supported `UPDATE` and `DELETE` statements may explicitly reference invisible
columns in assignment, predicate, and order positions exactly as visible columns
are handled today.

## Introspection Semantics

For invisible columns:

- `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` report `INVISIBLE` in
  `Extra`;
- visible columns continue to report an empty `Extra` value;
- `SHOW CREATE TABLE` includes the column in descriptor ordinal order and
  appends ` /*!80023 INVISIBLE */` after the type/default/nullability text.

Examples:

```sql
`v` int DEFAULT '7' /*!80023 INVISIBLE */
`nullable_i` int /*!80023 INVISIBLE */
`nn` int NOT NULL /*!80023 INVISIBLE */
```

## Diagnostics

The implementation must cover deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` schema, table, or column names;
- unsupported object kind;
- unknown target column;
- table-qualified target column;
- attempting to make every column invisible;
- omitted insert into invisible columns that have no effective default;
- allocation failures;
- catalog migration and mutation failures;
- public API misuse if any public surface is touched.

## Compatibility Matrix

| Behavior | MySQL 8.4.9 | MyLite in this slice |
| --- | --- | --- |
| `ALTER TABLE t ALTER c SET INVISIBLE` | accepted | supported |
| `ALTER TABLE t ALTER COLUMN c SET INVISIBLE` | accepted | supported |
| `ALTER TABLE t ALTER c SET VISIBLE` | accepted | supported |
| schema-qualified target | accepted | supported |
| existing row values | unchanged | unchanged |
| row count and warnings | `0`, `0` | `0`, `0` |
| last visible column made invisible | error 4028 | error 4028 |
| `SHOW COLUMNS` extra | `INVISIBLE` | `INVISIBLE` |
| `SHOW CREATE TABLE` visibility | version comment | version comment |
| `SELECT *` | visible columns only | visible columns only |
| explicit invisible column read/write | accepted | supported where the explicit operation is already supported |
| implicit no-column-list insert | visible columns only | visible columns only |
| `CREATE TABLE ... INVISIBLE` | accepted by MySQL | rejected |
| multi-action `ALTER TABLE` | accepted by MySQL | rejected |
| table-qualified column target | syntax error | rejected |

## SQLite Integration

This is a MyLite catalog mutation and catalog-schema migration. It uses public
SQLite APIs through the existing catalog layer. No SQLite fork patch, optional
SQLite user-table DDL, virtual table, function, collation, trigger, or new
extension point is needed.

## Tests

Tests must cover:

- parser success for `SET VISIBLE` and `SET INVISIBLE`, with and without
  `COLUMN`;
- parser/runtime rejection for table-qualified columns, multi-action `ALTER`,
  `CREATE TABLE ... INVISIBLE`, unsupported options, and unsupported visibility
  tokens;
- missing default schema, unknown schema, unknown table, unknown column, and
  reserved names;
- successful visibility changes for nullable, `NOT NULL`, integer, unsigned
  integer, `BIGINT UNSIGNED` within current storage limits, and `BOOL` /
  `BOOLEAN` descriptor columns;
- no-op visible->visible and invisible->invisible changes;
- last-visible-column rejection;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  visibility metadata;
- `SELECT *` omitting invisible columns while explicit select, predicate, and
  order references still work;
- implicit no-column-list `INSERT ... VALUES` mapping to visible columns only;
- explicit `INSERT` and `INSERT ... SET` assignment to invisible columns;
- omitted invisible columns receiving integer defaults, implicit nullable
  `NULL`, dropped-default/no-default diagnostics, and `NOT NULL` diagnostics;
- explicit `UPDATE` assignment/predicate/order behavior for invisible columns;
- explicit `DELETE` predicate/order behavior for invisible columns;
- affected rows, warning count, and absence of result rows;
- reopen persistence, table rename interaction, drop-table failure after
  visibility mutation, file preamble preservation, independent handles, and
  catalog migration from the previous version;
- zero-initialized cleanup for new statement/planner objects.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md`,
`docs/compatibility/sql-table-ddl.md`,
`docs/compatibility/sql-table-dml.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-show-statements.md` only for the exact supported
visibility subset. Do not overclaim generated invisible primary keys,
`CREATE TABLE ... INVISIBLE`, full invisible-column metadata, protocol flags,
views, indexes, or general expression support.

## Verification

Before implementation is complete, run:

1. `cmake --build --preset dev`
2. Focused parser/runtime CTest entries for column visibility plus existing
   row-values, select, update, delete, default, show-columns, show-create,
   alter-table, catalog, storage, and file-format lifecycle coverage.
3. `packages/libmylite/tests/mysql_baseline_alter_column_visibility_expectations.sh`
4. `cmake --workflow --preset check`

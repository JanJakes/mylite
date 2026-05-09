# Baseline ALTER COLUMN DROP DEFAULT

## Status

This feature specifies the narrow follow-up to
`baseline-alter-column-set-default`: `ALTER TABLE table_name ALTER [COLUMN]
column_name DROP DEFAULT` for persistent base tables and supported
descriptor-backed integer-family columns.

This is not general default handling. It does not add expression defaults, DML
`DEFAULT` values, table-qualified columns, multi-action `ALTER TABLE`, or
visibility changes.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline integer/default and alter-table specs under `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Evidence

The MySQL 8.4 manual lists `ALTER [COLUMN] col_name DROP DEFAULT` as an
`ALTER TABLE` action. Runtime probes against MySQL 8.4.9 verify the following
baseline behavior:

- `ALTER TABLE t ALTER c DROP DEFAULT` and
  `ALTER TABLE t ALTER COLUMN c DROP DEFAULT` are accepted.
- The statement reports `ROW_COUNT() == 0` and `@@warning_count == 0`.
- Existing rows are not rewritten.
- `SHOW COLUMNS` reports `NULL` in the `Default` column for dropped defaults,
  both nullable and `NOT NULL`.
- `SHOW CREATE TABLE` omits any `DEFAULT` clause for the dropped column. This is
  visibly different from a nullable column's implicit `DEFAULT NULL`.
- Later omitted-column `INSERT` statements fail in strict mode with error
  `1364`, SQLSTATE `HY000`, message
  `Field '<column>' doesn't have a default value`, even when the column is
  nullable.
- `ALTER ... SET DEFAULT ...` after a prior drop restores an explicit default
  and later omitted inserts use it.
- Missing default schema, unknown schema, unknown table, and unknown column use
  the existing MySQL diagnostics for those name-resolution failures.
- MySQL accepts broader forms outside this slice, including multi-action
  `ALTER TABLE` and online/lock options.

The script
`packages/libmylite/tests/mysql_baseline_alter_column_drop_default_expectations.sh`
records these runtime expectations.

## Scope

The implementation must add:

- parser and AST support for one `ALTER TABLE ... ALTER [COLUMN] ... DROP
  DEFAULT` action;
- unqualified and schema-qualified table-name resolution through the existing
  selected/default schema policy;
- unqualified target column resolution against MyLite descriptors;
- a durable descriptor state for "no explicit default" distinct from the
  existing implicit nullable `DEFAULT NULL` state;
- catalog schema migration from the previous default metadata shape;
- catalog-only descriptor mutation that preserves row storage and physical
  SQLite schema;
- omitted-column `INSERT` rejection for dropped-default columns;
- `SHOW COLUMNS` and `SHOW CREATE TABLE` reporting that matches the dropped
  default state.

## Non-Goals

This feature must not implement:

- `DEFAULT(col_name)` or DML `DEFAULT` keyword values;
- expression, string, decimal, float, hex, bit, temporal, JSON, spatial, or
  binary defaults;
- table-qualified column targets;
- multiple `ALTER TABLE` actions;
- `ALTER COLUMN SET VISIBLE` or `SET INVISIBLE`;
- indexes, constraints, auto-increment, generated columns, invisible columns,
  triggers, privileges, protocol-grade metadata, or SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged.
- Lexer/parser/AST own only the admitted syntax and spans.
- Analyzer/planner owns table and column resolution, reserved-name checks, and
  selection of the dropped-default descriptor state.
- Catalog owns durable default metadata. SQLite schema text remains
  non-authoritative.
- Result builder reports a normal non-row DDL result with `affected_rows == 0`
  and `warning_count == 0`.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
- SQLite owns physical row storage. This feature does not require user-table SQL
  because only descriptor metadata changes.

## Supported SQL Grammar

```sql
alter_table_drop_default:
    ALTER TABLE table_name ALTER column_name DROP DEFAULT
  | ALTER TABLE table_name ALTER COLUMN column_name DROP DEFAULT
```

`table_name` may be unqualified or schema-qualified. `column_name` must be a
single unqualified identifier.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
alter_table_drop_default_statement ::=
    ALTER TABLE table_name ALTER column_keyword_opt identifier DROP DEFAULT.
```

## Catalog Semantics

Column descriptors use three default states after this slice:

| Kind | Meaning |
| --- | --- |
| none | No explicit non-`NULL` default is stored. Nullable columns have the implicit SQL `DEFAULT NULL`; `NOT NULL` columns have no default. |
| integer | A descriptor-owned signed 64-bit integer default. |
| no explicit default | `DROP DEFAULT` was applied. `SHOW COLUMNS` reports `NULL`, `SHOW CREATE TABLE` emits no default clause, and omitted inserts fail. |

The catalog schema must advance to the next version because existing catalog
tables constrain `default_kind` to the previous value set. Version-2 catalogs
must migrate to the new shape without changing any existing descriptor
semantics: `none` remains `none`, integer defaults remain integer defaults.
Only later `DROP DEFAULT` statements write the new state.

## Runtime Semantics

Successful execution updates only the target column descriptor's default state.
Column id, ordinal position, name, type, nullability, physical table name,
physical column name, and existing row values are preserved.

The catalog generation increments. `sqlite_schema_generation` does not
increment because the physical SQLite schema is unchanged.

Dropping an already dropped default or dropping a `NOT NULL` no-default column
is accepted as a metadata-only no-op from the user's perspective and reports
zero affected rows and warnings, matching MySQL's tested DDL row-count
convention.

`ALTER COLUMN SET DEFAULT` after `DROP DEFAULT` must replace the dropped state
with the explicit descriptor default from the set-default slice.

## Omitted Insert Semantics

When a supported `INSERT ... VALUES` or `INSERT ... SET` omits a column whose
descriptor is in the dropped-default state, MyLite must reject the statement
with the existing field-no-default diagnostic before physical row insertion.
This applies even if the column is nullable.

## Introspection Semantics

For dropped-default columns:

- `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` report `NULL` in the
  `Default` column;
- `SHOW CREATE TABLE` emits the column definition without `DEFAULT NULL` or
  `DEFAULT '<value>'`.

For ordinary nullable descriptors that have not had `DROP DEFAULT` applied,
`SHOW CREATE TABLE` continues to emit `DEFAULT NULL`.

## Diagnostics

The implementation must cover deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` schema, table, or column names;
- unsupported object kind;
- unknown target column;
- omitted insert into dropped-default column;
- allocation failures;
- catalog migration and mutation failures;
- public API misuse if any public surface is touched.

## Compatibility Matrix

| Behavior | MySQL 8.4.9 | MyLite in this slice |
| --- | --- | --- |
| `ALTER TABLE t ALTER c DROP DEFAULT` | accepted | supported for descriptor integer columns |
| `ALTER TABLE t ALTER COLUMN c DROP DEFAULT` | accepted | supported |
| schema-qualified target | accepted | supported |
| existing row values | unchanged | unchanged |
| row count and warnings | `0`, `0` | `0`, `0` |
| `SHOW COLUMNS` default | `NULL` | `NULL` |
| `SHOW CREATE TABLE` default | no default clause | no default clause |
| later omitted insert | error 1364 | error 1364 |
| later `SET DEFAULT` | restores explicit default | supported |
| multi-action `ALTER TABLE` | accepted by MySQL | rejected |
| table-qualified column target | not in this slice | rejected |

## SQLite Integration

This is a MyLite catalog mutation and catalog-schema migration. It uses public
SQLite APIs through the existing catalog layer. No SQLite fork patch, optional
SQLite `ALTER TABLE` user-table syntax, virtual table, function, collation,
trigger, or new extension point is needed.

## Tests

Tests must cover:

- parser success for both `ALTER` forms;
- parser/runtime rejection for table-qualified columns, multi-action `ALTER`,
  visibility actions, and unsupported options;
- successful default drop for nullable, `NOT NULL`, integer, unsigned integer,
  `BIGINT UNSIGNED` within current storage limits, and `BOOL` / `BOOLEAN`
  descriptor columns;
- unchanged existing rows and failed later omitted inserts;
- `SET DEFAULT` after a prior drop;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  differences between implicit nullable default and dropped default;
- missing default schema, unknown schema, unknown table, unknown column, and
  reserved names;
- affected rows, warning count, and absence of result rows;
- reopen persistence, table rename interaction, drop-table failure after drop,
  and independent file-backed handles;
- `.mylite` preamble preservation and unchanged physical SQLite schema;
- migration from the previous catalog version;
- existing parser, runtime, catalog, default, insert, introspection, and
  alter-table tests.

## Documentation Updates

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` to mark the
limited `ALTER COLUMN DROP DEFAULT` subset as supported. Update default wording
without overclaiming expression defaults, DML defaults, full `ALTER TABLE`,
generated columns, or protocol metadata.

## Review Checklist

- MySQL 8.4.9 evidence and official docs are recorded.
- The new descriptor state is durable and migrates safely.
- Descriptor catalog remains authoritative.
- Catalog-only mutation does not touch row storage or SQLite schema generation.
- Omitted inserts fail only for the dropped-default state.
- Introspection distinguishes dropped defaults from implicit nullable defaults.
- Compatibility docs do not overclaim full default support.
- No SQLite fork patch or new dependency is introduced.

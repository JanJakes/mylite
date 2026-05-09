# Baseline Explicit DEFAULT NULL

## Status

This feature specifies a narrow default-value slice for existing descriptor-backed
integer table lifecycle paths. It accepts explicit `DEFAULT NULL` on nullable
integer-family column definitions for persistent base tables and keeps MyLite's
current no-general-default policy everywhere else.

The feature is intentionally not full MySQL default support. It does not add a
catalog default-expression field, expression evaluation, default use in DML,
`ALTER COLUMN SET/DROP DEFAULT`, or non-`NULL` defaults.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline table and alter-table lifecycle specs under `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/mysql/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  https://dev.mysql.com/doc/mysql/en/show-columns.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Evidence

The MySQL 8.4 manual documents that `DEFAULT` specifies a column default and
that, when no explicit default is given, nullable columns are defined with
`DEFAULT NULL` while non-null columns are defined with no explicit default.
`SHOW COLUMNS` reports `NULL` both for explicit `DEFAULT NULL` and for no
explicit default on a nullable column.

Runtime probes against MySQL 8.4.9 verify:

- `CREATE TABLE t (a INT DEFAULT NULL)` succeeds and `SHOW CREATE TABLE`
  renders `` `a` int DEFAULT NULL ``.
- `CREATE TABLE t (a INT NULL DEFAULT NULL)` succeeds and renders the same
  column definition.
- A nullable column with no `DEFAULT` clause also renders as `DEFAULT NULL`.
- `CREATE TABLE t (a INT NOT NULL DEFAULT NULL)` fails with error `1067`,
  SQLSTATE `42000`, message `Invalid default value for 'a'`.
- `ALTER TABLE t ADD f INT DEFAULT NULL` succeeds, reports
  `ROW_COUNT() == 0`, warning count `0`, and existing rows read `NULL` for
  the new column.
- `ALTER TABLE t ADD f INT NOT NULL DEFAULT NULL` fails with `1067` /
  `42000`.
- `ALTER TABLE t MODIFY f BIGINT DEFAULT NULL` and
  `ALTER TABLE t CHANGE f h BIGINT DEFAULT NULL` are accepted for nullable
  columns and preserve row values. A type-domain widening rebuild reports the
  table row count as affected rows; a pure same-domain rename reports zero
  affected rows.
- MySQL accepts wider syntax such as `INT DEFAULT NULL NULL`,
  non-`NULL` literal defaults, expression defaults, and default values in DML.
  Those forms remain outside this MyLite slice.

The script
`packages/libmylite/tests/mysql_baseline_explicit_default_null_expectations.sh`
records the runtime expectations.

## Scope

The implementation must add:

- parser and AST support for optional explicit `DEFAULT NULL` after the
  existing nullability slot in column definitions;
- support in `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`,
  `ALTER TABLE ... MODIFY [COLUMN]`, and
  `ALTER TABLE ... CHANGE [COLUMN]` because they already share
  `column_definition`;
- nullable integer-family columns only, including existing `TINYINT`,
  `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`, `INT1`/`INT2`/`INT3`/
  `INT4`/`INT8`, and bare `BOOL`/`BOOLEAN` aliases;
- `DEFAULT NULL` with omitted nullability and with explicit `NULL`;
- MySQL-compatible `1067` / `42000` diagnostics for `NOT NULL DEFAULT NULL`;
- unchanged descriptor storage: no logical default field is introduced;
- unchanged physical SQLite storage: generated table columns remain nullable or
  non-null according to MyLite descriptors, not SQLite default clauses;
- unchanged DML behavior: omitted nullable values store `NULL`, omitted
  non-null values still fail with the existing no-default diagnostic, and the
  `DEFAULT` keyword in `INSERT` or `UPDATE` remains unsupported;
- unchanged introspection for nullable columns, which already renders
  `DEFAULT NULL` in `SHOW CREATE TABLE` and SQL `NULL` in `SHOW COLUMNS`.

## Non-Goals

This feature must not implement:

- non-`NULL` literal defaults, including numeric, boolean, string, hex, bit,
  temporal, or decimal defaults;
- expression defaults, parenthesized defaults, functions, casts, arithmetic, or
  generated default values;
- `DEFAULT` keyword values in `INSERT`, `INSERT ... SET`, `UPDATE`, predicates,
  projection expressions, or scalar expressions;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` or `DROP DEFAULT`;
- `DEFAULT NULL` before nullability, repeated nullability attributes, repeated
  defaults, or broader MySQL column-attribute ordering;
- default metadata in MyLite catalog descriptors;
- compact physical storage changes, SQLite fork patches, indexes, constraints,
  auto-increment, generated columns, invisible columns, triggers, privileges,
  or protocol-grade metadata.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own public
  misuse handling, diagnostics reset, and result ownership.
- Lexer/parser/AST own the admitted syntax and source spans for the optional
  `DEFAULT NULL` clause.
- Analyzer/planner owns validating that explicit `DEFAULT NULL` is compatible
  with the resolved nullability of the descriptor-backed column definition.
- Catalog remains the authority for logical schema/table/column descriptors.
  This slice intentionally stores no default-expression descriptor because
  explicit `DEFAULT NULL` is equivalent to the existing nullable no-default
  descriptor state in MyLite's current model.
- Result builder continues to report empty DDL results with existing affected
  row and warning-count behavior for each DDL path.
- SQLite owns physical row storage. MyLite still generates physical SQLite DDL
  from descriptors and does not rely on SQLite default clauses for MySQL-visible
  semantics.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The new grammar is deliberately small:

```sql
column_definition:
    column_name integer_type [NULL | NOT NULL] [DEFAULT NULL]
```

`DEFAULT NULL` may appear after omitted nullability or after explicit `NULL` /
`NOT NULL`. When it follows `NOT NULL`, parsing succeeds and analysis reports
MySQL error `1067`.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
column_definition ::=
    identifier integer_type nullability_opt column_default_null_opt.

nullability_opt ::= .
nullability_opt ::= NULL.
nullability_opt ::= NOT NULL.

column_default_null_opt ::= .
column_default_null_opt ::= DEFAULT NULL.
```

## Semantics

Explicit `DEFAULT NULL` is a syntactic promise that the column's default is SQL
`NULL`. For nullable columns, this is the same logical state MySQL assigns when
no explicit default is written. MyLite therefore records no new catalog field
and relies on the existing nullable descriptor behavior:

- `CREATE TABLE` creates the same descriptor as the nullable form without
  `DEFAULT NULL`.
- `ALTER TABLE ... ADD` appends the same descriptor as the nullable form
  without `DEFAULT NULL` and backfills existing rows with SQL `NULL`.
- `ALTER TABLE ... MODIFY` and `CHANGE` apply the same type/nullability
  replacement and row validation they already perform for nullable definitions.
- `SHOW CREATE TABLE` continues to render nullable descriptors as
  `DEFAULT NULL`.
- `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` continue to report SQL `NULL`
  in the `Default` column for nullable columns.

`NOT NULL DEFAULT NULL` is invalid. MyLite must reject it before catalog or
physical SQLite mutation. No result handle is returned for the failing
statement.

## Diagnostics

| Condition | Result |
| --- | --- |
| `DEFAULT NULL` on nullable supported integer-family column | Success, warning count from the surrounding DDL path, no extra warnings |
| `NOT NULL DEFAULT NULL` | Error `1067`, SQLSTATE `42000`, `Invalid default value for '<column>'` |
| non-`NULL` default literal or expression | Syntax error or deterministic unsupported diagnostic, depending on parser admission |
| `DEFAULT` in DML value position | Existing syntax/unsupported behavior preserved |
| default metadata allocation failure | `MYLITE_NOMEM` with allocation diagnostic |
| physical SQLite/catalog failure | Existing internal/SQLite failure policy for the surrounding DDL path |

## SQLite Handling

No SQLite fork patch or optional SQLite syntax is needed. The generated SQLite
physical table definitions remain descriptor-driven. MyLite does not emit
physical `DEFAULT NULL` clauses as a semantic dependency, because nullable
SQLite columns already store `NULL` for omitted values and MyLite binds every
supported row value explicitly in DML.

## Tests

Add MySQL-runtime-verified expectations and plain C tests covering:

- `CREATE TABLE` with omitted nullability plus `DEFAULT NULL`;
- `CREATE TABLE` with explicit `NULL DEFAULT NULL`;
- existing integer families and aliases, including `BOOL`/`BOOLEAN`;
- `NOT NULL DEFAULT NULL` diagnostics;
- `ALTER TABLE ... ADD [COLUMN] ... DEFAULT NULL` existing-row backfill,
  later insert behavior, `SHOW CREATE TABLE`, `SHOW COLUMNS`, and reopen
  persistence;
- `ALTER TABLE ... MODIFY [COLUMN] ... DEFAULT NULL` and
  `CHANGE [COLUMN] ... DEFAULT NULL` preserving rows and metadata;
- parser rejection for non-`NULL` defaults, expression defaults, default
  keyword in DML, default-before-nullability ordering, repeated defaults, and
  wider MySQL forms intentionally deferred;
- reserved-name, unknown schema/table/column, duplicate column, descriptor
  generation, `.mylite` preamble, and independent handle behavior through the
  existing lifecycle tests where relevant;
- no regression in existing parser, table lifecycle, row values, alter-column,
  DML, introspection, storage, and full check workflow.

## Compatibility Documentation

Update the compatibility matrix and `docs/compatibility/sql-table-ddl.md` to
state that explicit `DEFAULT NULL` is accepted only for the current integer
column-definition subset and does not imply full default support. Update
`docs/compatibility/type-system-literals-conversion.md` only to clarify that
`NULL` is admitted in this column-default syntactic position; do not claim
general default expressions or the `DEFAULT` keyword in DML.

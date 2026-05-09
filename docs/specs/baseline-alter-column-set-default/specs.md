# Baseline ALTER COLUMN SET DEFAULT

## Status

This feature specifies a narrow descriptor-driven default mutation slice:
`ALTER TABLE table_name ALTER [COLUMN] column_name SET DEFAULT value` for
persistent base tables and supported integer-family descriptor columns.

This is not general `ALTER TABLE` default support. It does not add
`DROP DEFAULT`, expression defaults, string/decimal/float/hex/bit defaults,
multi-action `ALTER TABLE`, or DML `DEFAULT` values.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Catalog, table lifecycle, row-values, insert-set, integer-family, default,
  and alter-table specs under `docs/specs/`
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

The MySQL 8.4 manual lists `ALTER [COLUMN] col_name SET DEFAULT {literal |
(expr)}` and `DROP DEFAULT` as `ALTER TABLE` actions, and documents that
explicit defaults may be literal constants or parenthesized expressions.
Runtime probes against MySQL 8.4.9 verify the following behavior for this
baseline:

- `ALTER TABLE t ALTER c SET DEFAULT n` and
  `ALTER TABLE t ALTER COLUMN c SET DEFAULT n` are accepted.
- The statement reports `ROW_COUNT() == 0` and `@@warning_count == 0` for the
  supported in-range cases.
- Existing rows are not rewritten; later omitted-column `INSERT` statements use
  the replacement default.
- `SHOW COLUMNS` and `SHOW CREATE TABLE` reflect the new default.
- `SET DEFAULT NULL` is valid for nullable columns and invalid for `NOT NULL`
  columns with error `1067`, SQLSTATE `42000`, message
  `Invalid default value for '<column>'`.
- Negative defaults for unsigned integer descriptors and out-of-range defaults
  fail with the same invalid-default diagnostic.
- Missing default schema, unknown schema, unknown table, and unknown column use
  the existing MySQL diagnostics for those name-resolution failures.
- MySQL accepts broader forms outside this slice, including string-coerced
  defaults, decimal literals, parenthesized expression defaults, multi-action
  `ALTER TABLE`, and `DROP DEFAULT`.
- `DROP DEFAULT` on nullable columns is intentionally deferred. MySQL shows
  `NULL` in `SHOW COLUMNS`, omits `DEFAULT NULL` in `SHOW CREATE TABLE`, and
  rejects later omitted-column inserts in strict mode. MyLite currently needs a
  distinct durable "no explicit default" state before it can model that
  behavior correctly.

The script
`packages/libmylite/tests/mysql_baseline_alter_column_set_default_expectations.sh`
records these runtime expectations.

## Scope

The implementation must add:

- parser and AST support for one `ALTER TABLE ... ALTER [COLUMN] ... SET
  DEFAULT ...` action;
- unqualified and schema-qualified table-name resolution through the existing
  selected/default schema policy;
- unqualified assignment target column resolution against MyLite column
  descriptors;
- supported default values limited to `NULL`, decimal integer literals with
  optional unary `+` or `-`, `TRUE`, and `FALSE`;
- descriptor-owned default conversion for the supported integer-family and
  alias descriptors within MyLite's current signed-64 physical storage range;
- catalog descriptor replacement for default metadata only;
- preservation of physical SQLite table shape and row values;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and later
  omitted-column `INSERT` behavior through the already descriptor-owned default
  readers.

## Non-Goals

This feature must not implement:

- `ALTER TABLE ... ALTER [COLUMN] column_name DROP DEFAULT`;
- default expressions, parenthesized defaults, functions, column references,
  arithmetic, casts, parameters, variables, or subqueries;
- string, decimal, float, hex, bit, temporal, JSON, spatial, or binary
  defaults;
- `DEFAULT(col_name)` or DML `DEFAULT` keyword values;
- table-qualified column targets;
- multiple `ALTER TABLE` actions;
- `ALTER COLUMN SET VISIBLE` or `SET INVISIBLE`;
- indexes, constraints, auto-increment, generated columns, invisible columns,
  triggers, privileges, protocol-grade metadata, or SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own public
  misuse handling, diagnostics reset, result allocation, and result ownership.
- Lexer/parser/AST own the admitted syntax and source spans.
- Analyzer/planner owns table and column resolution, reserved-name checks, and
  default conversion against MyLite descriptors.
- Catalog owns durable logical default metadata. SQLite schema text remains
  non-authoritative for MySQL-visible defaults.
- Result builder reports a normal non-row DDL result with `affected_rows == 0`
  and `warning_count == 0` for supported successful statements.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.
- SQLite owns physical row storage. This feature does not require generated
  SQLite DDL or DML against user tables because only descriptor metadata
  changes.

## Supported SQL Grammar

The admitted grammar is deliberately small:

```sql
alter_table_set_default:
    ALTER TABLE table_name ALTER column_name SET DEFAULT alter_default_value
  | ALTER TABLE table_name ALTER COLUMN column_name SET DEFAULT alter_default_value

alter_default_value:
    NULL
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
```

`table_name` may be unqualified or schema-qualified. `column_name` must be a
single unqualified identifier.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
alter_table_set_default_statement ::=
    ALTER TABLE table_name ALTER column_keyword_opt identifier SET DEFAULT
    alter_column_default_value.

column_keyword_opt ::= .
column_keyword_opt ::= COLUMN.

alter_column_default_value ::= NULL.
alter_column_default_value ::= INTEGER.
alter_column_default_value ::= PLUS INTEGER.
alter_column_default_value ::= MINUS INTEGER.
alter_column_default_value ::= TRUE.
alter_column_default_value ::= FALSE.
```

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
current catalog identifier policy. The physical SQLite schema is not consulted
to decide whether a column exists or what its default is. Unknown columns use
MySQL-compatible error `1054` for the supported subset.

## Conversion Semantics

Supported default conversion reuses MyLite-owned integer default conversion:

- `NULL` stores the existing effective nullable default state and is valid only
  for nullable columns;
- decimal integer literals with optional unary sign are parsed by MyLite;
- `TRUE` converts to integer `1`;
- `FALSE` converts to integer `0`;
- the converted integer must fit the target logical descriptor range;
- unsigned descriptor defaults must be nonnegative;
- `BIGINT UNSIGNED` remains capped at `0..9223372036854775807` in this slice
  because MyLite currently stores integer rows in SQLite signed-64 values.

The supported integer-family ranges are:

| Logical type | Supported default range |
| --- | --- |
| signed `TINYINT` / `INT1` / `BOOL` / `BOOLEAN` | `-128..127` |
| unsigned `TINYINT` / `INT1` | `0..255` |
| signed `SMALLINT` / `INT2` | `-32768..32767` |
| unsigned `SMALLINT` / `INT2` | `0..65535` |
| signed `MEDIUMINT` / `INT3` | `-8388608..8388607` |
| unsigned `MEDIUMINT` / `INT3` | `0..16777215` |
| signed `INT` / `INTEGER` / `INT4` | `-2147483648..2147483647` |
| unsigned `INT` / `INTEGER` / `INT4` | `0..4294967295` |
| signed `BIGINT` / `INT8` | `-9223372036854775808..9223372036854775807` |
| unsigned `BIGINT` / `INT8` | `0..9223372036854775807` |

Out-of-range defaults, negative defaults for unsigned descriptors, and `NULL`
for `NOT NULL` descriptors fail before catalog mutation.

## Catalog and Runtime Semantics

Successful execution updates only the target column descriptor's default fields
using a catalog mutation. The column id, ordinal position, name, type,
nullability, physical table name, physical column name, and all existing row
values are preserved.

The catalog generation increments because descriptor metadata changed.
`sqlite_schema_generation` does not increment because the physical SQLite schema
is unchanged. No SQLite user-table SQL is generated for the metadata-only
default replacement.

Later supported omitted-column `INSERT` paths use the new descriptor default.
Introspection paths read descriptor metadata, so no separate work is needed in
SQLite schema text.

## Result Semantics

Successful `ALTER COLUMN SET DEFAULT` returns through the existing non-row
result conventions:

- no result-set columns;
- no result rows;
- `affected_rows == 0`;
- `warning_count == 0`.

## Diagnostics

The implementation must cover deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` schema, table, or column names;
- unsupported object kind;
- unknown target column;
- unsupported default expression or literal kind;
- integer literal outside MyLite's supported target descriptor range;
- `NULL` default on a `NOT NULL` descriptor;
- allocation failures;
- catalog mutation failures;
- public API misuse if any public surface is touched.

Physical SQLite row failures are not expected for the successful path because
no user-table SQLite SQL is generated. Unexpected SQLite/catalog failures must
still surface through the existing internal/physical diagnostics policy.

## Compatibility Matrix

| Behavior | MySQL 8.4.9 | MyLite in this slice |
| --- | --- | --- |
| `ALTER TABLE t ALTER c SET DEFAULT 1` | accepted | supported for descriptor integer columns |
| `ALTER TABLE t ALTER COLUMN c SET DEFAULT 1` | accepted | supported |
| schema-qualified target | accepted | supported |
| `SET DEFAULT NULL` on nullable column | accepted | supported |
| `SET DEFAULT NULL` on `NOT NULL` column | error 1067 | error |
| existing row values | unchanged | unchanged |
| future omitted inserts | use new default | use new descriptor default |
| row count and warnings | `0`, `0` | `0`, `0` |
| parenthesized expression default | accepted by MySQL | rejected as unsupported |
| string/decimal/hex/bit defaults | often accepted/coerced by MySQL | rejected as unsupported |
| multi-action `ALTER TABLE` | accepted by MySQL | rejected |
| `DROP DEFAULT` | accepted by MySQL | deferred |

## SQLite Integration

This is a MyLite catalog mutation. It uses public SQLite APIs only through the
existing catalog layer. No SQLite fork patch, optional SQLite `ALTER TABLE`
syntax, virtual table, function, collation, trigger, or new extension point is
needed.

## Tests

Tests must cover:

- parser success for both `ALTER` forms and each admitted default value;
- parser or runtime rejection for `DROP DEFAULT`, table-qualified columns,
  multi-action `ALTER`, expression defaults, string/decimal/float/hex/bit
  defaults, functions, parameters, and visibility actions;
- successful default replacement across signed and unsigned integer-family
  descriptors, aliases, and `BOOL` / `BOOLEAN` within MyLite's physical range;
- `NULL` default replacement for nullable columns;
- deterministic invalid-default errors for `NULL` into `NOT NULL`, negative
  unsigned defaults, and out-of-range values;
- unqualified and schema-qualified table resolution, including missing default
  schema, unknown schema, unknown table, unknown column, and reserved names;
- affected rows, warning count, and absence of result rows;
- existing rows remaining unchanged and later omitted inserts using the new
  default;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  reflecting the new descriptor default;
- reopen persistence, table rename interaction, and drop-table failure after
  drop where applicable;
- independent file-backed handles with independent defaults;
- `.mylite` preamble preservation and unchanged physical SQLite schema;
- zero-initialized cleanup for any new statement/planner objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, bootstrap, VFS, catalog, table lifecycle, row-values,
  select/delete/update, integer-family, default, and alter-table tests.

## Documentation Updates

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` to mark the
limited `ALTER COLUMN SET DEFAULT` subset as supported. Update default
expression wording to include this mutation path without overclaiming
`DROP DEFAULT`, expression defaults, string/default coercion, generated
defaults, or general expression evaluation.

## Review Checklist

- MySQL 8.4.9 evidence and official docs are recorded.
- Grammar text is independently authored and narrower than MySQL.
- Descriptor catalog remains authoritative.
- Catalog-only default mutation does not touch row storage or SQLite schema.
- Conversion matches existing integer/default range policy.
- Nullable and `NOT NULL` default behavior is tested.
- Result metadata, affected rows, and warnings match MySQL for the supported
  subset.
- Compatibility docs do not overclaim full default mutation support.
- No SQLite fork patch or new dependency is introduced.

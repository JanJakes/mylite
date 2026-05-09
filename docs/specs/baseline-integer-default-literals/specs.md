# Baseline Integer Default Literals

## Status

This feature specifies the next narrow default-value slice for
descriptor-backed integer tables. It admits non-`NULL` integer-family column
defaults in the existing persistent base-table lifecycle and uses those defaults
when supported `INSERT` forms omit a column.

This is not general MySQL default support. It does not add default expressions,
string/decimal/float/hex/bit defaults, DML `DEFAULT` keyword values,
`DEFAULT(col_name)`, `ALTER COLUMN SET/DROP DEFAULT`, generated values, or
SQLite physical default dependency.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing default-null slice:
  `docs/specs/baseline-explicit-default-null/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values, insert-set, alter-table, and integer-family specs under
  `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Evidence

The MySQL 8.4 manual documents that a `DEFAULT` clause explicitly assigns a
column default and that omitted `INSERT` columns are filled from explicit or
implicit defaults. Runtime probes against MySQL 8.4.9 verify the following
baseline behavior:

- `DEFAULT` accepts signed decimal integer literals for supported integer
  families. `DEFAULT +9` is normalized to `9`.
- `TRUE` and `FALSE` defaults on `BOOL` / `BOOLEAN` normalize to `1` and `0`
  because those aliases render as signed `tinyint(1)`.
- `SHOW CREATE TABLE` renders non-`NULL` integer defaults as single-quoted
  canonical decimal strings, for example `` `a` int DEFAULT '5' `` and
  `` `b` int NOT NULL DEFAULT '-7' ``.
- `SHOW COLUMNS` reports the unquoted canonical decimal default text in the
  `Default` column.
- `INSERT ... VALUES` with a column list and `INSERT ... SET` fill omitted
  defaulted columns with their descriptor defaults; explicitly inserted `NULL`
  into a nullable column remains `NULL`.
- `ALTER TABLE ... ADD [COLUMN] ... DEFAULT n` backfills existing rows with
  `n`, reports `ROW_COUNT() == 0`, and uses `n` for later omitted inserts.
- `ALTER TABLE ... ADD [COLUMN] ... NOT NULL DEFAULT n` also backfills existing
  rows with `n`.
- `ALTER TABLE ... MODIFY [COLUMN] ... DEFAULT n` and
  `CHANGE [COLUMN] ... DEFAULT n` update the default for future rows without
  rewriting existing values when type/nullability do not otherwise require a
  rebuild; they report `ROW_COUNT() == 0`.
- Omitting a default in `MODIFY` or `CHANGE` replaces the previous default with
  the default state implied by the replacement definition: nullable columns show
  `DEFAULT NULL`, while `NOT NULL` columns have no explicit default.
- Out-of-range defaults for the declared integer family fail with error `1067`,
  SQLSTATE `42000`, message `Invalid default value for '<column>'`.
- A negative default for an unsigned integer column fails with the same
  diagnostic.
- `CREATE TABLE IF NOT EXISTS existing_table (a INT DEFAULT out_of_range)`
  takes the existing-table no-op path and reports only the table-exists note;
  the out-of-range literal is not validated because no table is created.
- MySQL accepts broader default forms outside this slice, including strings
  coerced to integers, decimal literals, hex literals, expression defaults, and
  `BIGINT UNSIGNED` defaults above MyLite's current signed-64 physical range.

The script
`packages/libmylite/tests/mysql_baseline_integer_default_literals_expectations.sh`
records the runtime expectations.

## Scope

The implementation must add:

- parser and AST support for optional `DEFAULT` followed by one default value
  after the existing nullability slot in column definitions;
- default values limited to `NULL`, decimal integer literals with optional unary
  `+` or `-`, `TRUE`, and `FALSE`;
- support in `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`,
  `ALTER TABLE ... MODIFY [COLUMN]`, and
  `ALTER TABLE ... CHANGE [COLUMN]`, because they share `column_definition`;
- durable MyLite catalog metadata for descriptor defaults;
- descriptor-driven default conversion for existing integer families and
  aliases within MyLite's current physical signed-64 storage range;
- omitted-column default filling in supported `INSERT ... VALUES` and
  `INSERT ... SET` forms;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW COLUMNS`, `DESCRIBE`, and
  `EXPLAIN table` default reporting;
- file-backed reopen persistence and a catalog schema migration path from the
  previous no-default-metadata catalog.

## Non-Goals

This feature must not implement:

- default expressions, parenthesized defaults, functions, column references,
  arithmetic, casts, parameters, variables, subqueries, or generated values;
- string, decimal, float, hex, bit, temporal, JSON, spatial, or binary
  defaults;
- `DEFAULT` keyword values in `INSERT`, `INSERT ... SET`, `UPDATE`, predicates,
  projection expressions, or scalar expressions;
- `DEFAULT(col_name)`;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` or `DROP DEFAULT`;
- broader MySQL column-attribute ordering, repeated defaults, repeated
  nullability attributes, or `DEFAULT` before nullability;
- physical SQLite default clauses as the source of MySQL-visible semantics;
- indexes, constraints, auto-increment, generated columns, invisible columns,
  triggers, privileges, protocol-grade metadata, or SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own public
  misuse handling, diagnostics reset, and result ownership.
- Lexer/parser/AST own the admitted default syntax and source spans.
- Analyzer/planner owns default conversion against MyLite descriptors,
  including nullability and integer-family range checks.
- Catalog owns durable default metadata as part of the logical column
  descriptor. SQLite schema text remains non-authoritative.
- Result builder reports empty DDL/DML results using the existing public result
  object conventions, affected rows, and warning counts.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.
- SQLite owns physical row storage and statement execution. MyLite binds
  effective default values in generated DML instead of relying on SQLite
  physical default clauses.

## Supported SQL Grammar

The admitted grammar is deliberately small:

```sql
column_definition:
    column_name integer_type [NULL | NOT NULL] [DEFAULT default_value]

default_value:
    NULL
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
```

`DEFAULT` may appear only after omitted or explicit nullability. `DEFAULT NULL`
keeps the behavior from `baseline-explicit-default-null`: it is valid only for
nullable definitions and invalid for `NOT NULL` definitions.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
column_definition ::=
    identifier integer_type nullability_opt column_default_opt.

column_default_opt ::= .
column_default_opt ::= DEFAULT column_default_value.

column_default_value ::= NULL.
column_default_value ::= INTEGER.
column_default_value ::= PLUS INTEGER.
column_default_value ::= MINUS INTEGER.
column_default_value ::= TRUE.
column_default_value ::= FALSE.
```

## Catalog Semantics

Column descriptors gain an effective-default state:

| Kind | Meaning |
| --- | --- |
| none | No explicit non-`NULL` default is stored. Nullable columns still have an effective SQL `NULL` default; `NOT NULL` columns have no default. |
| integer | A descriptor-owned signed 64-bit integer default. |

The explicit `DEFAULT NULL` surface does not require a distinct durable state in
this baseline. It is equivalent to the existing nullable/no-non-null-default
descriptor state for all current MyLite behavior.

The catalog schema is advanced to the next schema version by adding default
metadata to `_mylite_catalog_columns`. Existing version-1 catalogs must migrate
to the new shape by assigning `none` defaults to all existing columns before the
runtime accepts descriptor reads. The migration must update catalog state inside
the shifted SQLite payload only and must not alter the `.mylite` preamble.

## Conversion Semantics

Supported default conversion reuses MyLite-owned integer assignment conversion:

- decimal integer literals with optional unary sign are parsed without relying
  on SQLite;
- `TRUE` converts to integer `1`;
- `FALSE` converts to integer `0`;
- `NULL` converts to the effective SQL `NULL` default only for nullable
  columns;
- the converted integer must fit the declared MyLite integer-family range:
  signed and unsigned `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT` / `INTEGER`,
  `BIGINT`, `INT1` / `INT2` / `INT3` / `INT4` / `INT8`, and signed
  `BOOL` / `BOOLEAN`;
- `BIGINT UNSIGNED` remains capped at `0..9223372036854775807` in this slice
  because MyLite currently stores integer rows in SQLite signed-64 values.

Out-of-range defaults, negative defaults for unsigned descriptors, and
`DEFAULT NULL` on `NOT NULL` descriptors fail with MySQL-compatible invalid
default diagnostics before catalog or physical SQLite mutation.

For `CREATE TABLE IF NOT EXISTS`, MyLite must preserve the diagnostic ordering
already specified for explicit `DEFAULT NULL`: invalid `NOT NULL DEFAULT NULL`
is rejected before an existing-table no-op. Non-`NULL` integer default range
conversion happens only when a table is actually created; MySQL 8.4.9 does not
validate out-of-range integer defaults on an existing-table no-op.

## DDL Semantics

`CREATE TABLE` stores descriptor default metadata and creates the same physical
SQLite rowid table shape as before. Generated SQLite DDL must not rely on
physical `DEFAULT` clauses for MySQL-visible behavior.

`ALTER TABLE ... ADD [COLUMN]` stores the new descriptor default. Existing rows
are backfilled as follows:

- integer default: bind and write that integer for each existing row;
- effective SQL `NULL` default on a nullable column: write `NULL`;
- no default on a `NOT NULL` integer column: preserve the current baseline
  MyLite behavior and backfill `0`.

`ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` replace the descriptor
default from the replacement definition. Existing row values are not rewritten
only because a default changed. Existing type/nullability validation and any
required physical rebuild behavior remain owned by the alter-table lifecycle.

If a replacement definition omits `DEFAULT`, the previous non-`NULL` default is
removed. Nullable replacement definitions then have an effective SQL `NULL`
default; `NOT NULL` replacement definitions have no default.

## DML Semantics

Supported `INSERT ... VALUES` and `INSERT ... SET` continue to generate a
descriptor-driven SQLite `INSERT` with every physical column listed and every
value bound. During planning, MyLite fills omitted columns with their effective
defaults:

- integer default: bind the stored integer default;
- effective SQL `NULL` default: bind `NULL`;
- no default on a `NOT NULL` column: fail with the existing `1364` /
  `HY000`, `Field '<column>' doesn't have a default value` diagnostic.

Explicitly provided values keep existing row-values semantics. For example,
explicit `NULL` into a nullable column with an integer default stores `NULL`;
explicit `NULL` into a `NOT NULL` column fails with the existing bad-null
diagnostic.

`UPDATE ... SET column = DEFAULT`, `INSERT ... VALUES(DEFAULT)`,
`INSERT ... SET column = DEFAULT`, and `DEFAULT(col_name)` remain unsupported.

## Introspection

`SHOW CREATE TABLE` renders descriptor defaults from catalog metadata:

- integer default: `DEFAULT '<canonical_decimal>'`;
- effective nullable SQL `NULL` default: `DEFAULT NULL`;
- `NOT NULL` with no default: no default clause.

`SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` report:

- canonical decimal text for integer defaults;
- SQL `NULL` for effective SQL `NULL` defaults and for `NOT NULL` columns with
  no explicit default, matching MySQL's result shape.

## Diagnostics

| Condition | Result |
| --- | --- |
| supported in-range integer default | Success, warning count from the surrounding DDL path, no extra warnings |
| `DEFAULT TRUE` / `DEFAULT FALSE` on supported integer-family column | Success as `1` / `0` if in range |
| `DEFAULT NULL` on nullable supported integer-family column | Success with effective SQL `NULL` default |
| `DEFAULT NULL` on `NOT NULL` column | Error `1067`, SQLSTATE `42000`, `Invalid default value for '<column>'` |
| integer default outside MyLite descriptor range | Error `1067`, SQLSTATE `42000`, `Invalid default value for '<column>'` |
| negative default on unsigned descriptor | Error `1067`, SQLSTATE `42000`, `Invalid default value for '<column>'` |
| unsupported default literal or expression | Syntax error or deterministic unsupported diagnostic, depending on parser admission |
| omitted `INSERT` column with integer default | Success, default bound into physical row |
| omitted `INSERT` column with effective SQL `NULL` default | Success, `NULL` bound into physical row |
| omitted `INSERT` `NOT NULL` column with no default | Existing `1364` / `HY000` no-default diagnostic |
| catalog migration/allocation failure | `MYLITE_ERROR` or `MYLITE_NOMEM` with existing catalog/allocation diagnostics |
| physical SQLite/catalog failure | Existing internal/SQLite failure policy for the surrounding statement |

## SQLite Handling

No SQLite fork patch or optional SQLite syntax is needed. Physical table
definitions remain descriptor-driven rowid tables with quoted identifiers and
without SQLite default clauses as semantic dependencies. MyLite binds default
values in generated `INSERT` and backfill statements, so SQLite executes storage
work while MyLite retains MySQL default semantics.

## Tests

Add MySQL-runtime-verified expectations and plain C tests covering:

- `CREATE TABLE` defaults for supported signed and unsigned integer families
  and `BOOL` / `BOOLEAN`;
- boundary in-range defaults and out-of-range diagnostics for `TINYINT`,
  `SMALLINT`, `MEDIUMINT`, `INT`, `INTEGER`, `BIGINT`, aliases, and unsigned
  forms within MyLite's physical range;
- `DEFAULT TRUE`, `DEFAULT FALSE`, `DEFAULT +n`, and `DEFAULT -n` normalization;
- `SHOW CREATE TABLE`, `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table`
  metadata;
- omitted-column defaults in supported `INSERT ... VALUES` and
  `INSERT ... SET`;
- explicit `NULL` preserving `NULL` for nullable columns even when a non-`NULL`
  default exists;
- no-default `NOT NULL` omitted-column diagnostics remaining unchanged;
- `ALTER TABLE ... ADD [COLUMN] ... DEFAULT n` backfill and later insert
  behavior;
- `ALTER TABLE ... MODIFY [COLUMN] ... DEFAULT n` and
  `CHANGE [COLUMN] ... DEFAULT n` updating future-row defaults without
  rewriting existing rows when no type/nullability rebuild is needed;
- omitted default in `MODIFY` / `CHANGE` removing a previous non-`NULL`
  default;
- `CREATE TABLE IF NOT EXISTS` existing-table no-op behavior for admitted
  integer defaults;
- unsupported default syntax rejected deterministically: strings, decimals,
  floats, hex, bit, parenthesized expressions, arithmetic expressions,
  functions, parameters, repeated defaults, `DEFAULT` before nullability, and
  DML `DEFAULT` keyword values;
- reopen persistence, independent file-backed handles, and `.mylite` preamble
  preservation;
- migration from a catalog without default metadata to the new descriptor shape;
- zero-initialized cleanup for new planner/catalog objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  catalog foundation, row values, insert-set, alter-table, select, update,
  delete, file-backed opening, VFS, and storage tests still pass.

## Compatibility Documentation

Update `COMPATIBILITY.md` and detail docs only for this limited integer default
surface. The documentation must not claim support for full default expressions,
string/decimal/float/hex/bit defaults, generated defaults, DML `DEFAULT`,
`DEFAULT(col_name)`, full `ALTER COLUMN SET/DROP DEFAULT`, full column
attributes, or protocol-grade metadata.

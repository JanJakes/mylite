# Baseline BOOL And BOOLEAN Aliases

## Status

This feature specifies a narrow column-type alias slice for MySQL `BOOL` and
`BOOLEAN`. In MySQL 8.4.9 these names are accepted as type aliases and render
through table introspection as `tinyint(1)`.

This slice intentionally implements only descriptor-backed table column syntax.
It does not add expression truth semantics, boolean result metadata, standalone
casts, `TRUE` / `FALSE` assignment support, or any broader expression engine.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline small integer types:
  `docs/specs/baseline-small-integer-types/specs.md`
- Baseline integer `SIGNED` attribute:
  `docs/specs/baseline-integer-signed-attribute/specs.md`
- Baseline integer type aliases:
  `docs/specs/baseline-integer-type-aliases/specs.md`
- Baseline integer display width:
  `docs/specs/baseline-integer-display-width/specs.md`
- Current DDL, row-value, select, delete, update, alter, and show specs under
  `docs/specs/`
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, using data types from other database engines:
  https://dev.mysql.com/doc/refman/8.4/en/other-vendor-data-types.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Evidence Summary

Observed MySQL 8.4.9 behavior:

- `BOOL` and `BOOLEAN` are accepted as column data types in `CREATE TABLE`,
  `ALTER TABLE ... ADD`, `ALTER TABLE ... MODIFY`, and
  `ALTER TABLE ... CHANGE`.
- `BOOL` and `BOOLEAN` remain available as nonreserved unquoted identifiers in
  positions where MyLite currently admits nonreserved identifiers.
- Both aliases render as `tinyint(1)` in `SHOW COLUMNS`, `DESCRIBE`,
  `EXPLAIN table`, and `SHOW CREATE TABLE`.
- Creating a `BOOL` or `BOOLEAN` column emits no display-width deprecation
  warning even though the rendered metadata includes `(1)`.
- The value domain is the signed `TINYINT` range, `-128..127`.
- Integer and `NULL` inserts and updates follow the existing signed
  `TINYINT(1)` descriptor behavior.
- `BOOL NOT NULL` rejects `NULL` with the same diagnostic as other current
  not-null integer descriptors.
- Altering between `TINYINT(1)`, `BOOL`, and `BOOLEAN` is metadata-equivalent
  when name and nullability do not change.
- `BOOL(1)`, `BOOLEAN(1)`, `BOOL UNSIGNED`, `BOOL SIGNED`, `BOOL ZEROFILL`,
  and combined signedness forms are syntax errors in MySQL 8.4.9.
- `TRUE` and `FALSE` are expression literals in MySQL, but this slice does not
  admit them as row-value or update-assignment inputs.

The executable evidence script is
`packages/libmylite/tests/mysql_baseline_bool_boolean_aliases_expectations.sh`.

## Scope

The implementation must add:

- parser support for `BOOL` and `BOOLEAN` in the same supported
  column-definition positions as current integer-family descriptors;
- preservation of `BOOL` and `BOOLEAN` as unquoted identifiers outside the
  alias type position where the current parser admits nonreserved identifiers;
- AST representation that records the aliases as signed `TINYINT(1)`-like
  descriptor inputs without treating the `(1)` as explicit display-width
  syntax;
- descriptor-driven `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`,
  `ALTER TABLE ... MODIFY [COLUMN]`, and
  `ALTER TABLE ... CHANGE [COLUMN]` support;
- durable logical descriptors that normalize both aliases to `TINYINT(1)`;
- existing signed `TINYINT(1)` range conversion, nullability behavior,
  predicate conversion, ordering, DML execution, introspection, persistence,
  and preamble preservation;
- no warning for accepted `BOOL` or `BOOLEAN` column definitions;
- deterministic syntax rejection for the unsupported alias forms listed in this
  spec.

## Non-Goals

This feature must not implement:

- expression truth semantics for `BOOL` or `BOOLEAN`;
- `TRUE` or `FALSE` as `INSERT` / `UPDATE` assignment values;
- `BOOL(1)`, `BOOLEAN(1)`, or any display-width syntax after the alias;
- `SIGNED`, `UNSIGNED`, or `ZEROFILL` after the alias;
- repeated or combined alias attributes;
- `SERIAL`, `BIT`, `DECIMAL`, `NUMERIC`, `FLOAT`, `DOUBLE`, string, temporal,
  binary, enum, set, JSON, or spatial types;
- casts, function argument type syntax, parameters, defaults, generated
  columns, expression indexes, protocol-grade result metadata, compact storage,
  or SQLite fork changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to own result
  allocation, diagnostics exposure, affected-row reporting, and public misuse
  behavior.
- Statement context owns per-statement diagnostics snapshots. Accepted
  `BOOL` / `BOOLEAN` syntax must not append display-width warnings.
- Lexer/parser/AST own syntax admission and source spans for alias names. They
  do not consult descriptors, storage, or SQLite.
- Analyzer/planner maps the aliases to the same logical descriptor currently
  used for preserved signed `TINYINT(1)`.
- Catalog descriptors remain authoritative. The original alias spelling is not
  stored because MySQL 8.4.9 reports equivalent MySQL type text through
  introspection.
- Result and introspection builders continue to render descriptor logical
  types to lower-case MySQL-style normalized type text.
- SQLite remains the physical row storage and execution engine. Generated user
  tables still use SQLite `INTEGER` columns and prepared statements; this
  feature requires no SQLite fork patch, extension point, file-format change,
  or custom type support.

## Supported SQL Grammar

This feature expands the current column-definition type grammar only for bare
alias names:

```sql
BOOL
BOOLEAN
```

The aliases may be followed by the existing `NULL` / `NOT NULL` column
nullability grammar because nullability belongs to the column definition, not
to the type alias.

Supported examples:

```sql
CREATE TABLE t (b BOOL, c BOOLEAN NOT NULL)
ALTER TABLE t ADD flag BOOL NOT NULL
ALTER TABLE t MODIFY flag BOOLEAN NULL
ALTER TABLE t CHANGE flag enabled BOOL NOT NULL
```

Unsupported examples:

```sql
BOOL(1)
BOOLEAN(1)
BOOL SIGNED
BOOL UNSIGNED
BOOL ZEROFILL
BOOL SIGNED UNSIGNED
```

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
integer_type ::= integer_family_name display_width_opt integer_signedness_opt.
integer_type ::= boolean_alias_type.

boolean_alias_type ::= BOOL.
boolean_alias_type ::= BOOLEAN.
```

`boolean_alias_type` maps to a signed `TINYINT(1)` descriptor input. It does
not reuse `display_width_opt`, because MySQL 8.4.9 does not admit explicit
width syntax after `BOOL` or `BOOLEAN`, and the implicit `(1)` must not emit a
display-width deprecation warning.

## Descriptor Semantics

MyLite must store both aliases as `TINYINT(1)` logical descriptors:

| SQL input | Stored descriptor | Rendered type |
| --- | --- | --- |
| `BOOL` | `TINYINT(1)` | `tinyint(1)` |
| `BOOLEAN` | `TINYINT(1)` | `tinyint(1)` |

The descriptor has the same signed range and physical representation as the
current `TINYINT(1)` descriptor:

- minimum accepted integer: `-128`;
- maximum accepted integer: `127`;
- physical SQLite type: `INTEGER`;
- selected values render through the existing integer/`NULL` result path;
- predicates, ordering, updates, deletes, inserts, and alter validation reuse
  existing descriptor-driven `TINYINT(1)` behavior.

Changing between `BOOL`, `BOOLEAN`, and `TINYINT(1)` is a metadata-equivalent
type replacement when name, nullability, physical type, and integer value
domain are unchanged. Name changes still require the existing physical column
rename path.

## Diagnostics And Warnings

Accepted `BOOL` and `BOOLEAN` forms produce no warnings.

Unsupported alias syntax should fail deterministically through the existing
parser syntax diagnostic, including:

- explicit alias display width;
- `SIGNED`, `UNSIGNED`, or `ZEROFILL` after the alias;
- combined or repeated signedness attributes;
- unsupported type aliases such as `SERIAL`.

Assignment and existing-row validation diagnostics are inherited from
`TINYINT(1)`:

- out-of-range integer values use current `1264` / `22003` behavior;
- assigning or backfilling `NULL` into a `NOT NULL` alias column uses current
  `1048` / `23000` behavior;
- allocation and physical SQLite failures use existing MyLite failure paths.

## Physical SQLite Handling

The generated physical table remains a MyLite-owned SQLite rowid table with an
`INTEGER` column for each alias-backed descriptor column. The SQLite schema text
is internal and not user-visible authority. MyLite must continue to quote every
generated identifier and bind values through prepared statements in existing
DML paths.

No query should be materialized in MyLite only because a column was declared
with `BOOL` or `BOOLEAN`. SQLite continues to execute scans, filters, sorts,
limits, inserts, deletes, and updates for the generated physical SQL shapes.

## Test Plan

Implementation tests must cover:

- parser acceptance for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
  `CHANGE` alias forms;
- parser rejection for `BOOL(1)`, `BOOLEAN(1)`, `BOOL SIGNED`,
  `BOOL UNSIGNED`, `BOOL ZEROFILL`, and combined/repeated signedness forms;
- `CREATE TABLE` descriptors for nullable and not-null alias columns;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  rendering as `tinyint(1)`;
- zero warnings for accepted alias definitions;
- integer and `NULL` insert/update behavior using existing row-value and update
  support;
- signed `TINYINT(1)` range boundaries and out-of-range diagnostics;
- nullability diagnostics for `BOOL NOT NULL`;
- `WHERE`, `ORDER BY`, `LIMIT`, `DELETE`, and `UPDATE` reuse through
  descriptor-driven `TINYINT(1)` behavior;
- `ALTER TABLE ADD`, `MODIFY`, and `CHANGE` behavior, including
  metadata-equivalent changes between `BOOL`, `BOOLEAN`, and `TINYINT(1)`;
- reopen persistence and `.mylite` preamble preservation;
- no SQLite fork patch, no public ABI change, and no file-format change;
- the MySQL expectation script above, focused parser/runtime CTest entries, and
  the full check workflow.

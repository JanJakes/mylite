# Baseline TIME Type

## Status

This feature specifies the first `TIME` row-value slice for persistent
`.mylite` handles. It follows the existing descriptor-owned temporal type
architecture used by the `DATE`, `DATETIME`, and `TIMESTAMP` slices while
preserving MySQL `TIME` ordering for negative values and hours above 23.

The supported behavior is intentionally narrow: bare second-precision `TIME`
descriptors, canonical string values in MySQL display shape, descriptor-driven
DML, metadata, secondary indexes, and ordering. This feature does not implement
fractional precision, relaxed temporal conversion, numeric time conversion,
standard `TIME 'str'` literals, temporal functions, mutable SQL modes, casts,
or protocol-grade temporal metadata.

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
- Existing catalog, DDL, DML, type, metadata, index, and temporal feature specs
  under `docs/specs/`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, date and time data type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-type-syntax.html
- MySQL 8.4 Reference Manual, date and time literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, data type storage requirements:
  https://dev.mysql.com/doc/refman/8.4/en/storage-requirements.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_time_type_expectations.sh` records the
runtime probes for this feature. Observed behavior that defines this slice:

- The local compatibility runtime reports MySQL `8.4.9` and default strict SQL
  mode including `STRICT_TRANS_TABLES`, `NO_ZERO_IN_DATE`, and `NO_ZERO_DATE`.
- MySQL renders a bare `TIME` descriptor as `time` in `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = time`,
  `COLUMN_TYPE = time`, and `DATETIME_PRECISION = 0` for bare `TIME`.
  Character and numeric metadata columns are `NULL`.
- `TIME` values display as `hh:mm:ss` or `-hh:mm:ss`. Hours may exceed 23.
  The normal admitted range is `-838:59:59` through `838:59:59`.
- MySQL canonicalizes relaxed forms such as `1:2:3` to `01:02:03` and accepts
  `-0:0:1` as `-00:00:01`. MyLite defers those relaxed inputs and admits only
  the canonical display forms listed below.
- With default strict SQL mode, direct canonical strings outside the range,
  invalid minutes or seconds, and unparseable time strings fail with error
  `1292` / SQLSTATE `22007` and an `Incorrect time value` message.
- `INSERT IGNORE` demotes out-of-range time values, invalid time values, and
  explicit `NULL` into a `NOT NULL` time column to warnings. Positive
  out-of-range values clip to `838:59:59`, negative out-of-range values clip to
  `-838:59:59`, other invalid time values store `00:00:00`, and explicit
  `NULL` into `NOT NULL` stores `00:00:00`.
- Omitted or explicit `DEFAULT` for a `TIME NOT NULL` column with no explicit
  default fails with error `1364`; `INSERT IGNORE` demotes that condition to
  warning `1364` and stores `00:00:00`.
- `TIME DEFAULT 'hh:mm:ss'` renders as a quoted default in `SHOW CREATE TABLE`
  and as unquoted canonical text in `SHOW COLUMNS` and
  `INFORMATION_SCHEMA.COLUMNS`. Invalid and out-of-range defaults fail with
  error `1067`.
- `ALTER TABLE ... ADD COLUMN t TIME` backfills existing rows with `NULL`.
  `ALTER TABLE ... ADD COLUMN t TIME NOT NULL DEFAULT 'hh:mm:ss'` backfills
  with that default. `ALTER TABLE ... ADD COLUMN t TIME NOT NULL` succeeds on
  nonempty tables and backfills `00:00:00` while keeping no explicit default
  in metadata.
- Single-table `UPDATE` reports changed-row affected counts after canonical
  time conversion. Assigning a time column to its already-stored canonical text
  reports zero changed rows.
- `TIME` comparisons against canonical string constants, `BETWEEN`, `IN`,
  `IS NULL`, and `IS NOT NULL` work as time-context predicates. `ORDER BY`
  defaults to ascending order, orders `NULL` before non-`NULL` values in
  ascending order, and orders `NULL` after non-`NULL` values in descending
  order.
- MySQL accepts wider behavior that MyLite defers here: `TIME(fsp)`,
  fractional input, standard `TIME 'str'` literals, nondelimited string and
  numeric time inputs, relaxed delimiters, and temporal functions.

## Scope

The implementation must add:

- parser and AST support for bare `TIME` column types;
- descriptor-owned logical type text `TIME`;
- physical SQLite type text `TEXT` for admitted time descriptors;
- `CREATE TABLE` support for persistent base tables containing time columns,
  including nullable and not-null columns plus explicit `DEFAULT NULL` and
  non-`NULL` canonical time defaults;
- `ALTER TABLE ... ADD [COLUMN]` support for time columns, including nullable
  existing-row backfill with `NULL`, explicit-default backfill with the
  canonical default, empty-table `TIME NOT NULL` metadata, and nonempty-table
  `TIME NOT NULL` zero-time backfill;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` and `DROP DEFAULT` support
  for time descriptors;
- `CREATE TABLE ... LIKE` descriptor cloning for time columns, defaults,
  indexes, visibility, and no-default state;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source values are already compatible time
  descriptor values;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for time descriptors,
  including `DATETIME_PRECISION = 0`;
- canonical string literal, `NULL`, and `DEFAULT` values for
  `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignments into time columns;
- MyLite-owned time conversion before SQLite binding: exact canonical
  display-shape syntax, minute and second validation, range checking, strict
  error `1292`, invalid-default error `1067`, supported `INSERT IGNORE`
  warning adjustment, and canonical text output;
- descriptor-backed `WHERE` predicates for time columns using canonical string
  right operands for comparison operators, `BETWEEN` bounds, and `IN` list
  values, with `NULL` preserved in admitted `IN` lists;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  time columns;
- descriptor-backed `ORDER BY` for one time column in `SELECT`, `DELETE`, and
  single-table `UPDATE`, reusing the existing single-key `ASC`/`DESC` and
  `LIMIT` paths;
- supported nonunique and unique secondary-index declaration, duplicate
  enforcement, copying, metadata, and row-update checks for time columns;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted time data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `TIME(fsp)`, fractional-second storage, fractional-second metadata above
  zero, `YEAR`, intervals, temporal arithmetic, casts, or temporal functions
  such as `CURRENT_TIME()`, `TIME()`, `TIME_TO_SEC()`, `SEC_TO_TIME()`,
  `ADDTIME()`, `SUBTIME()`, `TIMEDIFF()`, or `MAKETIME()`;
- standard `TIME 'str'` literals, ODBC temporal literals, relaxed time string
  conversion, nondelimited time strings, numeric time conversion, parameters,
  user variables, functions, arbitrary expressions, column-to-column
  assignments, or `DEFAULT(col_name)`;
- mutable `sql_mode`, mode-dependent invalid-value conversion, or warning
  demotion outside the explicitly tested `INSERT IGNORE` subset;
- time primary keys, composite time indexes, prefix indexes, descending key
  parts, optimizer/index-use guarantees for predicates or ordering, generated
  columns, foreign keys, triggers, or cascades;
- protocol-grade type metadata, field flags, binary protocol values, or origin
  metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and cleanup on failure.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported `INSERT IGNORE` time adjustments record
  warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for bare `TIME` type names and string
  literal default/DML/predicate values. They preserve source spans but do not
  resolve descriptors or convert times.
- Analyzer/planner code maps `TIME` AST nodes to durable descriptors, resolves
  schemas/tables/columns through MyLite catalog descriptors, converts admitted
  time values, rejects unsupported temporal operations, and produces
  descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, default kind, default text, column order,
  index membership, and auto-increment attributes. SQLite schema text is not
  metadata authority.
- Result and introspection builders render logical descriptors and descriptor
  defaults to MySQL-shaped text.
- SQLite owns physical row storage and row mutation for generated prepared
  statements. Time values bind as canonical `TEXT`; MyLite never stores them
  through SQLite numeric affinity.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

This feature extends the existing limited column definition and row-value
grammar:

```sql
column_type:
    existing_integer_type
  | existing_decimal_type
  | existing_string_type
  | existing_temporal_type
  | TIME

column_default_value:
    existing_default_value
  | string_literal

insert_value:
    existing_insert_value
  | string_literal

update_value:
    existing_update_value
  | string_literal

predicate_value:
    existing_predicate_value
  | string_literal
```

MyLite Lemon-syntax sketch:

```lemon
column_type(A) ::= TIME_KW(T). {
  A = mylite_parser_make_time_type(pParser, &T);
}

literal_value(A) ::= STRING_LITERAL(T). {
  A = mylite_parser_make_string_literal(pParser, &T);
}
```

`TIME_KW` is only admitted as a bare type in this slice. `TIME(0)`,
`TIME(6)`, `TIME '12:00:00'`, and function-call uses of `TIME(...)` are not
part of this feature.

## Canonical TIME Values

MyLite admits only canonical display-shape string literals:

- `HH:MM:SS` for nonnegative values where `HH` is `00` through `99`;
- `HHH:MM:SS` for nonnegative values where `HHH` is `100` through `838`;
- `-HH:MM:SS` for negative values where `HH` is `00` through `99`;
- `-HHH:MM:SS` for negative values where `HHH` is `100` through `838`.

Minutes and seconds must be `00` through `59`. The absolute value must be no
greater than `838:59:59`. `-00:00:00` is not canonical and is rejected in strict
paths for this slice; callers should use `00:00:00`.

Accepted examples:

- `00:00:00`
- `01:02:03`
- `24:00:00`
- `100:00:00`
- `838:59:59`
- `-00:00:01`
- `-24:00:00`
- `-838:59:59`

Deferred examples:

- `1:2:3`
- `-0:0:1`
- `101112`
- `TIME '01:02:03'`
- `01:02:03.123456`
- `838:59:59.000000`

The stored physical value is the canonical display text. MyLite separately
computes a signed total-second value when it needs MySQL numeric time ordering
or range comparison semantics.

## Conversion and Diagnostics

Strict row-value paths (`INSERT`, `REPLACE`, and `UPDATE` without `IGNORE`)
accept only canonical time strings, `NULL`, and `DEFAULT` where those values
are already admitted by the statement grammar and target descriptor. Conversion
must happen before SQLite binding.

Diagnostics:

- Invalid row values: `1292 / 22007` with an `Incorrect time value` message for
  the target column.
- Invalid defaults: `1067 / 42000` with `Invalid default value for '<column>'`.
- Explicit `NULL` into `TIME NOT NULL`: existing `1048 / 23000`.
- Omitted or explicit `DEFAULT` for a `TIME NOT NULL` column with no explicit
  default: existing `1364 / HY000`.
- Unsupported non-string values in time contexts: deterministic MyLite
  unsupported diagnostics matching current temporal type style.
- Unsupported `TIME(fsp)`: deterministic parser/runtime unsupported diagnostic
  for this slice, even though MySQL accepts `fsp` values `0..6`.

`INSERT IGNORE` adjustment for admitted statement shapes:

- Positive out-of-range canonical-shaped values store `838:59:59` and warn
  with code `1264`.
- Negative out-of-range canonical-shaped values store `-838:59:59` and warn
  with code `1264`.
- Other invalid canonical-shaped values store `00:00:00` and warn with code
  `1264`.
- Explicit `NULL` into `TIME NOT NULL` stores `00:00:00` and warns with code
  `1048`.
- Omitted or explicit `DEFAULT` for `TIME NOT NULL` with no explicit default
  stores `00:00:00` and warns with code `1364`.

The supported strict paths produce `warning_count == 0`.

## Ordering and Predicates

Physical canonical time text does not sort like MySQL for all admitted values:
`-838:59:59` must sort before `-00:00:01`, and `100:00:00` must sort after
`24:00:00`. Therefore MyLite must not rely on plain SQLite text ordering for
`TIME` comparisons or `ORDER BY`.

For descriptor-backed `TIME` columns, generated SQLite SQL must use a
MyLite-built signed-total-seconds expression over the physical text for:

- comparison operators other than equality when the right operand is a
  canonical time literal;
- `BETWEEN` bounds;
- non-`NULL` `IN` list value matching if the implementation chooses numeric
  comparison for all list entries;
- `ORDER BY` in `SELECT`, ordered `DELETE`, and ordered `UPDATE`.

Equality and uniqueness may compare canonical physical text because conversion
normalizes all supported inputs before binding. `NULL` ordering follows MySQL:
ascending sorts `NULL` before non-`NULL`; descending sorts `NULL` after
non-`NULL`.

The generated expression must be built from descriptors and quoted physical
identifiers, not from user SQL text. Bound right-side time literals should be
converted to signed seconds before binding for numeric comparisons, or to
canonical text before binding for equality.

## Metadata and Introspection

`TIME` columns render as:

- `SHOW COLUMNS`: type `time`, default text unquoted, nullable/key/extra fields
  following existing descriptor rules;
- `SHOW CREATE TABLE`: type `time`, string defaults quoted with existing SQL
  string escaping;
- `INFORMATION_SCHEMA.COLUMNS`: `DATA_TYPE = time`, `COLUMN_TYPE = time`,
  `DATETIME_PRECISION = 0`, character and numeric metadata `NULL`, and
  descriptor-owned `IS_NULLABLE`, `COLUMN_DEFAULT`, `COLUMN_KEY`, `EXTRA`, and
  ordinal metadata;
- `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`: existing secondary-index metadata behavior for supported
  time unique and nonunique indexes.

## Physical Storage and SQLite

No SQLite fork patch is required. The implementation uses public SQLite
prepared statements and generated SQL in the MyLite runtime layer.

Generated physical column type is `TEXT`. Generated SQLite identifiers must be
quoted. User values must be bound parameters. MyLite-owned time conversion must
run before binding. Catalog descriptors remain the source of truth for type and
metadata; SQLite schema text remains an implementation detail.

For row-size validation, bare `TIME` contributes 3 bytes to the descriptor-owned
MySQL row-size envelope, matching second-precision `TIME` storage requirements.

## Tests

Add MySQL-runtime expectations and fast C tests covering:

- descriptor rendering in `SHOW COLUMNS`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS`;
- accepted canonical values at `-838:59:59`, `-00:00:01`, `00:00:00`,
  `24:00:00`, `100:00:00`, and `838:59:59`;
- strict invalid values, invalid defaults, out-of-range values, non-string
  assignments, and unsupported `TIME(fsp)` / temporal literal / numeric input
  forms;
- `INSERT IGNORE` adjustment for invalid, positive out-of-range, negative
  out-of-range, explicit `NULL`, and missing/default no-default values;
- `UPDATE` changed-row semantics and `DEFAULT` keyword behavior;
- `WHERE` comparison, `BETWEEN`, `IN`, `IS NULL`, and `IS NOT NULL` predicates;
- `ORDER BY` default, `ASC`, `DESC`, nullable ordering, negative values, and
  hours above 23;
- ordered/limited `DELETE` and `UPDATE` behavior over time columns;
- supported nonunique and unique secondary indexes on time columns, duplicate
  checks, and metadata;
- `ALTER TABLE ... ADD COLUMN`, `MODIFY`, `CHANGE`, default changes,
  visibility, drop/rename column interactions where current type slices cover
  those operations;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, `INSERT ... SELECT`,
  `REPLACE ... SELECT`, reopen persistence, table rename/drop, file preamble
  preservation, and independent file-backed handles;
- existing parser, runtime, catalog, DML, temporal, index, introspection, VFS,
  and full workflow checks.

## Completion Criteria

- `docs/specs/baseline-time-type/specs.md` and `tasks.md` exist.
- MySQL 8.4.9 expectation script passes against the configured runtime.
- Parser, runtime, catalog, docs, and tests implement exactly the supported
  subset.
- Focused CTest entries and `cmake --workflow --preset check` pass.
- Final review confirms descriptor authority, MySQL evidence, signed time
  ordering, no public ABI changes, no SQLite fork patch, file-format safety,
  compatibility docs accuracy, cleanup on failure, and scope control.

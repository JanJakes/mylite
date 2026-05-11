# Baseline DATETIME Type

## Status

This feature specifies the first `DATETIME` row-value slice for persistent
`.mylite` handles. It extends the existing descriptor-owned type system on top
of the `DATE`, string, decimal, integer, DML, index, result, and introspection
paths.

The feature is intentionally not full MySQL temporal conversion. It stores and
returns canonical second-precision datetime text and supports canonical string
literals in datetime contexts. It does not implement fractional seconds,
`DATETIME(fsp)` syntax, `TIME`, `TIMESTAMP`, `YEAR`, temporal functions,
standard `TIMESTAMP '...'` literals, relaxed delimiters, `T` separators, numeric
datetime conversion, time-zone behavior, mutable SQL modes, generated defaults,
casts, arithmetic, or protocol-grade temporal metadata.

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
- Baseline catalog, table lifecycle, DML, primary-key, auto-increment,
  select, update, string, decimal, and `DATE` specs under `docs/specs/`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `DATE`, `DATETIME`, and `TIMESTAMP` types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, date and time literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_datetime_type_expectations.sh` records
the runtime probes for this feature. Observed behavior that shapes this slice:

- MySQL renders a bare `DATETIME` descriptor as `datetime` in `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = datetime`,
  `COLUMN_TYPE = datetime`, and `DATETIME_PRECISION = 0` for bare `DATETIME`.
  Character and numeric metadata columns are `NULL`.
- `DATETIME` values display as `YYYY-MM-DD hh:mm:ss`. The documented supported
  normal range is `1000-01-01 00:00:00` through `9999-12-31 23:59:59`.
- With MySQL 8.4.9 default SQL mode
  `ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
  canonical valid datetime strings store and read back unchanged, while direct
  invalid dates, invalid times, partial-zero datetimes, and direct zero
  datetimes fail with error `1292` / SQLSTATE `22007` and an
  `Incorrect datetime value` message.
- `INSERT IGNORE` demotes invalid canonical-shaped datetime inputs, zero
  datetime inputs, and explicit `NULL` into a `NOT NULL` datetime column to
  warnings and stores `0000-00-00 00:00:00`. The observed invalid/zero warning
  is `1264`; the observed explicit-`NULL` warning is `1048`.
- Omitted or explicit `DEFAULT` for a `DATETIME NOT NULL` column with no
  explicit default fails with error `1364`; `INSERT IGNORE` demotes that
  condition to warning `1364` and stores `0000-00-00 00:00:00`.
- `DATETIME DEFAULT 'YYYY-MM-DD hh:mm:ss'` renders as a quoted default in
  `SHOW CREATE TABLE` and as unquoted canonical text in `SHOW COLUMNS` and
  `INFORMATION_SCHEMA.COLUMNS`. Invalid and zero-datetime defaults fail with
  error `1067`.
- `ALTER TABLE ... ADD COLUMN d DATETIME` backfills existing rows with `NULL`.
  `ALTER TABLE ... ADD COLUMN d DATETIME NOT NULL DEFAULT 'YYYY-MM-DD hh:mm:ss'`
  backfills existing rows with that default. `ALTER TABLE ... ADD COLUMN d
  DATETIME NOT NULL` succeeds on an empty table but fails on a nonempty table
  with error `1292` because MySQL would have to fill `0000-00-00 00:00:00`
  under the default SQL mode.
- Single-table `UPDATE` reports changed-row affected counts after canonical
  datetime conversion. Assigning a datetime column to its already-stored
  canonical text reports zero changed rows.
- `DATETIME` comparisons against canonical string constants, `BETWEEN`, `IN`,
  `IS NULL`, and `IS NOT NULL` work as datetime-context predicates.
  `ORDER BY` defaults to ascending order, orders `NULL` before non-`NULL`
  values in ascending order, and orders `NULL` after non-`NULL` values in
  descending order.
- MySQL accepts wider temporal input forms, including `T` separators,
  fractional seconds, `DATETIME(fsp)` descriptors, `TIMESTAMP 'str'` literals,
  numeric datetime values, relaxed delimiters, and `CURRENT_TIMESTAMP` defaults.
  MyLite defers those until the temporal conversion, expression, default, and
  SQL-mode layers are broadened.

## Scope

The implementation must add:

- parser and AST support for bare `DATETIME` column types;
- string literal defaults in column definitions, with `DATETIME` validation
  limited to canonical second-precision datetime text;
- descriptor-owned logical type text `DATETIME`;
- physical SQLite type text `TEXT` for admitted datetime descriptors;
- `CREATE TABLE` support for persistent base tables containing datetime
  columns, including nullable and not-null columns plus explicit
  `DEFAULT NULL` and non-`NULL` canonical datetime defaults;
- `ALTER TABLE ... ADD [COLUMN]` support for datetime columns, including
  nullable existing-row backfill with `NULL`, explicit-default backfill with the
  canonical default, empty-table `DATETIME NOT NULL` no-explicit-default
  metadata, and nonempty-table rejection for `DATETIME NOT NULL` without an
  explicit default;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` and `DROP DEFAULT` support
  for datetime descriptors;
- `CREATE TABLE ... LIKE` descriptor cloning for datetime columns and their
  defaults;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source values are already compatible
  datetime descriptors;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and limited
  `INFORMATION_SCHEMA.COLUMNS` rendering for datetime descriptors, including
  `DATETIME_PRECISION = 0`;
- canonical string literal, `NULL`, and `DEFAULT` values for
  `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignments into datetime
  columns;
- MyLite-owned datetime conversion before SQLite binding: exact canonical
  `YYYY-MM-DD HH:MM:SS` syntax, leap-year validation, time validation, normal
  supported range checking, direct zero-datetime rejection under the fixed
  default SQL mode, strict error `1292`, invalid-default error `1067`, supported
  `INSERT IGNORE` warning adjustment of canonical-shaped invalid or zero
  datetimes to `0000-00-00 00:00:00`, and canonical text output;
- descriptor-backed `WHERE` predicates for datetime columns using canonical
  string right operands for comparison operators, `BETWEEN` bounds, and `IN`
  list values, with `NULL` preserved in admitted `IN` lists;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  datetime columns;
- descriptor-backed `ORDER BY` for one datetime column in `SELECT`, `DELETE`,
  and single-table `UPDATE`, reusing the existing single-key `ASC`/`DESC` and
  `LIMIT` paths;
- supported nonunique and unique secondary-index declaration, duplicate
  enforcement, copying, metadata, and row-update checks for datetime columns,
  using canonical fixed-width text order and equality;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted datetime data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `DATETIME(fsp)`, fractional-second storage, fractional-second metadata above
  zero, `TIME`, `TIMESTAMP`, `YEAR`, temporal intervals, or temporal functions
  such as `NOW()` or `CURRENT_TIMESTAMP`;
- standard `TIMESTAMP 'str'` literals, ODBC temporal literals, relaxed
  delimiter conversion, `T` separators, nondelimited datetime strings, two-digit
  year conversion, numeric datetime conversion, parameters, user variables,
  functions, arbitrary expressions, column-to-column assignments, or
  `DEFAULT(col_name)`;
- mutable `sql_mode`, `ALLOW_INVALID_DATES`, `NO_ZERO_DATE`, or
  `NO_ZERO_IN_DATE` behavior;
- direct storage of partial-zero datetimes such as `2024-00-01 00:00:00` or
  invalid times such as `2024-01-01 24:00:00`;
- datetime arithmetic, casts, time-zone conversion, collation-sensitive
  datetime comparison, grouping, `DISTINCT`, datetime aggregates, datetime
  primary keys, auto-increment, generated columns, generated defaults, `ON
  UPDATE CURRENT_TIMESTAMP`, or optimizer behavior;
- protocol-grade type metadata, field flags, binary protocol values, or origin
  metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and cleanup on failure.
- Statement context owns per-statement diagnostics, warnings, affected rows, and
  transaction completion. Supported `INSERT IGNORE` datetime adjustments record
  warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for bare `DATETIME` type names and
  string literal default/DML/predicate values. They preserve source spans but do
  not resolve descriptors or convert datetime values.
- Analyzer/planner code maps `DATETIME` AST nodes to durable descriptors,
  resolves schemas/tables/columns through MyLite catalog descriptors, converts
  admitted datetime values, rejects unsupported temporal operations, and
  produces descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, default kind, default text, column order,
  primary-key membership, index membership, and auto-increment attributes.
  SQLite schema text is not metadata authority.
- Result and introspection builders render logical descriptors and descriptor
  defaults to MySQL-shaped text.
- SQLite owns physical row storage, row mutation, secondary-index enforcement,
  and row ordering for generated prepared statements. Datetime values bind as
  canonical `TEXT`; MyLite never stores them through SQLite numeric affinity.
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
  | DATE
  | DATETIME

column_default_value:
    existing_default_value
  | string_literal

insert_value:
    existing_insert_value
  | string_literal

update_value:
    existing_update_value
  | string_literal

predicate_comparison_value:
    existing_predicate_integer_or_temporal_value
  | string_literal

predicate_range_value:
    existing_predicate_integer_or_temporal_value
  | string_literal

predicate_in_value:
    predicate_range_value
  | NULL
```

String literals are admitted syntactically only because existing string types,
`DATE`, and this `DATETIME` slice need them. `DATETIME` conversion accepts only
canonical `YYYY-MM-DD HH:MM:SS` and, for internal `INSERT IGNORE` adjustment,
`0000-00-00 00:00:00`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
column_type ::= datetime_type.
datetime_type ::= DATETIME.

identifier ::= DATETIME.

column_default_value ::= STRING.

predicate_range_value ::= predicate_integer_value.
predicate_range_value ::= STRING.

predicate_atom ::= qualified_identifier BETWEEN predicate_range_value AND
        predicate_range_value.
predicate_atom ::= qualified_identifier NOT BETWEEN predicate_range_value AND
        predicate_range_value.

predicate_in_value ::= predicate_range_value.
predicate_in_value ::= NULL.
```

## Descriptor and Storage Mapping

`DATETIME` descriptors use:

| Descriptor field | Value |
| --- | --- |
| `logical_type` | `DATETIME` |
| `physical_type` | `TEXT` |
| default kind | `NONE`, `TEXT`, or effective `NULL` |
| default text | canonical `YYYY-MM-DD HH:MM:SS` for explicit non-`NULL` defaults |
| visible | follows existing column visibility rules |
| primary key | rejected for this slice |
| secondary index | admitted through existing nonunique and unique secondary-index paths |

Generated SQLite physical tables remain rowid tables with stable physical names
such as `_mylite_user_table_<table_id>`. Datetime columns are emitted as quoted
SQLite identifiers with `TEXT` affinity. Generated SQL must quote every
identifier and bind values through prepared statements.

No catalog table shape changes are required. Existing text-default catalog
storage is sufficient for canonical datetime defaults.

## Conversion Semantics

`DATETIME` strict conversion accepts exactly 19 bytes:

```text
YYYY-MM-DD HH:MM:SS
```

The year must be `1000` through `9999`, the month and day must form a real
Gregorian calendar date, the hour must be `00` through `23`, and minute and
second must be `00` through `59`. Direct `0000-00-00 00:00:00`, partial-zero
dates, invalid dates, invalid times, fractional seconds, timezone suffixes,
extra whitespace, `T` separators, relaxed punctuation, and numeric literals are
outside this slice.

For strict `INSERT`, `REPLACE`, and `UPDATE`, invalid canonical-shaped input and
zero datetime input fail with MySQL error `1292`, SQLSTATE `22007`, and
`Incorrect datetime value: '<value>' for column '<column>' at row <n>`.

For `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET`, invalid
canonical-shaped input and zero datetime input append warning `1264` and store
`0000-00-00 00:00:00`. Explicit `NULL` into a `NOT NULL` datetime column appends
warning `1048` and stores `0000-00-00 00:00:00`. Omitted values or explicit
`DEFAULT` for a `NOT NULL` datetime column with no explicit default append
warning `1364` and store `0000-00-00 00:00:00`.

Noncanonical input that MySQL accepts only through wider temporal conversion is
rejected with deterministic MyLite-specific parse diagnostics for now. This
keeps accepted data within the fixed-width text ordering invariant.

## Predicate and Ordering Semantics

`WHERE` support mirrors the current descriptor-driven predicate subset:

- `column = 'YYYY-MM-DD HH:MM:SS'`
- `column <> 'YYYY-MM-DD HH:MM:SS'` and `column != ...`
- `column < '...'`, `<=`, `>`, `>=`
- `column <=> 'YYYY-MM-DD HH:MM:SS'`
- `column BETWEEN '...' AND '...'`
- `column NOT BETWEEN '...' AND '...'`
- `column IN ('...', NULL, ...)`
- `column NOT IN ('...', NULL, ...)`
- `column IS NULL`
- `column IS NOT NULL`
- existing boolean composition over supported atoms

Predicate literals must be canonical valid datetimes or the internal zero
datetime value already stored by supported `INSERT IGNORE` behavior. Invalid
predicate datetime strings fail with the same `1292` datetime diagnostic used by
DML conversion.

`ORDER BY datetime_column` is admitted for one unqualified descriptor column in
`SELECT`, single-table `DELETE`, and single-table `UPDATE`. Default direction is
ascending; explicit `ASC` and `DESC` are supported. `NULL` sorts before
non-`NULL` values ascending and after non-`NULL` values descending, matching the
observed MySQL surface. Duplicate datetime values have no deterministic
tie-breaker unless the statement includes a separate deterministic operation
outside this slice; tests may assert counts or membership, not which duplicate
tied row is chosen.

The fixed-width canonical text format preserves chronological order under
SQLite bytewise text ordering for the admitted second-precision values. This is
why fractional seconds and relaxed input are deferred.

## Metadata Semantics

Descriptor rendering must match the observed MySQL 8.4.9 surface:

- `SHOW COLUMNS` / `DESCRIBE` / `EXPLAIN table`: type `datetime`, nullable
  column defaults as `NULL`, explicit defaults as unquoted canonical text.
- `SHOW CREATE TABLE`: type `datetime`, explicit non-`NULL` defaults quoted as
  SQL strings, `DEFAULT NULL` for nullable no-explicit-default columns.
- `INFORMATION_SCHEMA.COLUMNS`: `DATA_TYPE = datetime`,
  `COLUMN_TYPE = datetime`, `DATETIME_PRECISION = 0`, character and numeric
  metadata `NULL`, `COLUMN_DEFAULT` unquoted canonical text for explicit
  defaults and `NULL` for explicit `DEFAULT NULL` or no default.
- Secondary-index metadata follows the existing descriptor-owned index paths.

## Diagnostics

The implementation must keep deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` target names;
- unsupported object kinds once non-base-table descriptors exist;
- unknown assignment, predicate, ordering, index, or DDL column names;
- unsupported `DATETIME(fsp)`, fractional seconds, temporal literal
  introducers, numeric datetime values, `T` separators, relaxed delimiters,
  generated defaults, functions, parameters, expression assignments,
  column-to-column assignments, datetime primary keys, datetime auto-increment,
  and general expression contexts;
- invalid strict datetime value, zero datetime, partial-zero date, invalid time,
  and invalid default;
- `NULL` into `NOT NULL` datetime columns;
- physical SQLite failures, allocation failures, and public API misuse through
  existing public result/diagnostic conventions.

Supported in-range statements produce `warning_count == 0` unless they are
documented `INSERT IGNORE` adjustments. Successful non-query statements return
through the existing non-row result conventions and preserve MySQL-compatible
affected-row semantics for changed rows.

## SQLite Integration and Performance

This feature is a MyLite wrapper/translation feature over public SQLite APIs.
It needs no SQLite fork patch.

MyLite performs descriptor resolution and datetime conversion before binding,
then lets SQLite execute physical `INSERT`, `REPLACE`, `UPDATE`, `DELETE`,
`SELECT`, and secondary-index checks. It must not materialize full tables in
memory for normal DML. Existing ordered/limited DML may build descriptor-driven
SQLite shapes that use physical row identifiers internally, but row filtering,
ordering, limiting, mutation, and index enforcement should remain inside SQLite
where the current execution layer already does so.

## Test Plan

Add a fast C runtime test, parser coverage, and a MySQL expectation script that
cover:

- `CREATE TABLE`, `ALTER ADD COLUMN`, `ALTER COLUMN SET/DROP DEFAULT`,
  `CREATE TABLE LIKE`, compatible `CREATE TABLE ... SELECT`, compatible
  `INSERT ... SELECT`, and compatible `REPLACE ... SELECT`;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `DESCRIBE` / `EXPLAIN table`, and
  `INFORMATION_SCHEMA.COLUMNS`, including `DATETIME_PRECISION = 0`;
- canonical boundary values `1000-01-01 00:00:00` and
  `9999-12-31 23:59:59`;
- `NULL`, default, omitted, `NOT NULL`, strict invalid, strict zero, strict
  partial-zero, strict invalid-time, and `INSERT IGNORE` zero-datetime
  adjustment;
- single-table `UPDATE`, changed-row affected counts, no-op updates, `DEFAULT`,
  `NULL`, invalid values, and `NOT NULL` diagnostics;
- comparison, null-safe comparison, `BETWEEN`, `IN`, `IS NULL`, `IS NOT NULL`,
  and boolean predicate composition already supported by the predicate layer;
- `ORDER BY` / `LIMIT` in `SELECT`, `DELETE`, and `UPDATE`, including default
  direction, `ASC`, `DESC`, `NULL` ordering, and duplicate datetime ties without
  overclaiming tied-row order;
- supported nonunique and unique secondary-index declaration, metadata, duplicate
  checks, duplicate `NULL` behavior, update duplicate checks, and descriptor
  cloning;
- reopen persistence, table rename/drop interactions, independent file-backed
  handles, and `.mylite` preamble preservation;
- unsupported syntax and conversion forms: `DATETIME(fsp)`, fractional seconds,
  relaxed delimiters, `T` separators, numeric datetime literals, `TIMESTAMP`
  literal introducers, `CURRENT_TIMESTAMP` defaults, functions, parameters,
  expression assignments, column-to-column assignments, primary keys, and
  auto-increment.

Verification before marking done:

1. `cmake --build --preset dev`
2. Focused parser/runtime CTest entries, including the new datetime entry and
   existing DATE/string/decimal/update/select/order/index lifecycle entries.
3. `packages/libmylite/tests/mysql_baseline_datetime_type_expectations.sh`
4. `cmake --workflow --preset check`

# Baseline TIMESTAMP Type

## Status

This feature specifies the first `TIMESTAMP` row-value slice for persistent
`.mylite` handles. It follows the existing descriptor-owned temporal type
architecture used by the `DATE` and `DATETIME` slices, but keeps the
time-zone-sensitive parts of MySQL `TIMESTAMP` deliberately out of scope.

The supported behavior is equivalent to MySQL 8.4.9 observations with the
session `time_zone` fixed to `+00:00`: bare second-precision `TIMESTAMP`
descriptors, canonical string values, descriptor-driven DML, metadata, indexes,
and ordering. This feature does not implement fractional precision,
`CURRENT_TIMESTAMP`, `ON UPDATE CURRENT_TIMESTAMP`, session time-zone
conversion, timestamp literals, relaxed temporal conversion, numeric temporal
conversion, temporal functions, mutable SQL modes, or protocol-grade temporal
metadata.

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
- MySQL 8.4 Reference Manual, `DATE`, `DATETIME`, and `TIMESTAMP` types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, automatic initialization and updating for
  `TIMESTAMP` and `DATETIME`:
  https://dev.mysql.com/doc/refman/8.4/en/timestamp-initialization.html
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
`packages/libmylite/tests/mysql_baseline_timestamp_type_expectations.sh`
records the runtime probes for this feature. The probes set
`time_zone = '+00:00'` so `TIMESTAMP` storage and retrieval are stable and match
the MyLite fixed-UTC slice.

Observed behavior that defines this slice:

- The local compatibility runtime reports MySQL `8.4.9`, default strict SQL
  mode including `STRICT_TRANS_TABLES`, `NO_ZERO_IN_DATE`, and `NO_ZERO_DATE`,
  and `explicit_defaults_for_timestamp = 1`.
- MySQL renders a bare `TIMESTAMP` descriptor as `timestamp` in
  `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = timestamp`,
  `COLUMN_TYPE = timestamp`, and `DATETIME_PRECISION = 0` for bare
  `TIMESTAMP`. Character and numeric metadata columns are `NULL`.
- `TIMESTAMP` values display as `YYYY-MM-DD hh:mm:ss`. The normal admitted
  range in the fixed-UTC slice is `1970-01-01 00:00:01` through
  `2038-01-19 03:14:07`.
- With default strict SQL mode, direct canonical strings outside the range,
  direct zero timestamps, partial-zero timestamps, invalid dates, and invalid
  times fail with error `1292` / SQLSTATE `22007` and an
  `Incorrect datetime value` message.
- `INSERT IGNORE` demotes invalid canonical-shaped timestamp inputs, zero
  timestamp inputs, out-of-range timestamp inputs, and explicit `NULL` into a
  `NOT NULL` timestamp column to warnings. The adjusted stored value is
  `0000-00-00 00:00:00`. Observed warning codes are `1264` for invalid, zero,
  and out-of-range timestamp inputs and `1048` for explicit `NULL`.
- Omitted or explicit `DEFAULT` for a `TIMESTAMP NOT NULL` column with no
  explicit default fails with error `1364`; `INSERT IGNORE` demotes that
  condition to warning `1364` and stores `0000-00-00 00:00:00`.
- `TIMESTAMP DEFAULT 'YYYY-MM-DD hh:mm:ss'` renders as a quoted default in
  `SHOW CREATE TABLE` and as unquoted canonical text in `SHOW COLUMNS` and
  `INFORMATION_SCHEMA.COLUMNS`. Invalid, zero, and out-of-range defaults fail
  with error `1067`.
- `TIMESTAMP` with no explicit nullability is nullable in the observed MySQL
  8.4.9 default configuration. `SHOW CREATE TABLE` renders create-time bare
  `TIMESTAMP` as `timestamp NULL DEFAULT NULL`.
- `ALTER TABLE ... ADD COLUMN ts TIMESTAMP` backfills existing rows with
  `NULL`. `ALTER TABLE ... ADD COLUMN ts TIMESTAMP NOT NULL DEFAULT
  'YYYY-MM-DD hh:mm:ss'` backfills with that default.
- `ALTER TABLE ... ADD COLUMN ts TIMESTAMP NOT NULL` succeeds on both empty and
  nonempty tables. For existing rows, MySQL backfills `0000-00-00 00:00:00`
  even though direct zero timestamp input is rejected under the default SQL
  mode.
- Dropping a nullable timestamp column default leaves a descriptor with no
  default. Later omitted or explicit `DEFAULT` values for that column fail with
  error `1364`, matching the existing dropped-default MyLite policy.
- Single-table `UPDATE` reports changed-row affected counts after canonical
  timestamp conversion. Assigning a timestamp column to its already-stored
  canonical text reports zero changed rows.
- `TIMESTAMP` comparisons against canonical string constants, `BETWEEN`, `IN`,
  `IS NULL`, and `IS NOT NULL` work as timestamp-context predicates.
  `ORDER BY` defaults to ascending order, orders `NULL` before non-`NULL`
  values in ascending order, and orders `NULL` after non-`NULL` values in
  descending order. Stored zero timestamp values sort before valid timestamp
  values.
- MySQL accepts wider behavior that MyLite defers here: `TIMESTAMP(fsp)`,
  fractional input, `T` separators, standard `TIMESTAMP 'str'` literals,
  `DEFAULT CURRENT_TIMESTAMP`, `ON UPDATE CURRENT_TIMESTAMP`, and session
  time-zone conversion.

## Scope

The implementation must add:

- parser and AST support for bare `TIMESTAMP` column types;
- descriptor-owned logical type text `TIMESTAMP`;
- physical SQLite type text `TEXT` for admitted timestamp descriptors;
- `CREATE TABLE` support for persistent base tables containing timestamp
  columns, including nullable and not-null columns plus explicit `DEFAULT NULL`
  and non-`NULL` canonical timestamp defaults;
- `ALTER TABLE ... ADD [COLUMN]` support for timestamp columns, including
  nullable existing-row backfill with `NULL`, explicit-default backfill with
  the canonical default, empty-table `TIMESTAMP NOT NULL` metadata, and
  nonempty-table `TIMESTAMP NOT NULL` zero-timestamp backfill;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` and `DROP DEFAULT` support
  for timestamp descriptors;
- `CREATE TABLE ... LIKE` descriptor cloning for timestamp columns, defaults,
  indexes, visibility, and no-default state;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source values are already compatible
  nonzero timestamp descriptor values;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for timestamp descriptors,
  including `DATETIME_PRECISION = 0`;
- canonical string literal, `NULL`, and `DEFAULT` values for
  `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignments into timestamp
  columns;
- MyLite-owned timestamp conversion before SQLite binding: exact canonical
  `YYYY-MM-DD HH:MM:SS` syntax, leap-year validation, time validation,
  fixed-UTC range checking, direct zero-timestamp rejection under the fixed
  default SQL mode, strict error `1292`, invalid-default error `1067`,
  supported `INSERT IGNORE` warning adjustment of invalid, zero, or
  out-of-range canonical-shaped timestamps to `0000-00-00 00:00:00`, and
  canonical text output;
- descriptor-backed `WHERE` predicates for timestamp columns using canonical
  string right operands for comparison operators, `BETWEEN` bounds, and `IN`
  list values, with `NULL` preserved in admitted `IN` lists;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  timestamp columns;
- descriptor-backed `ORDER BY` for one timestamp column in `SELECT`, `DELETE`,
  and single-table `UPDATE`, reusing the existing single-key `ASC`/`DESC` and
  `LIMIT` paths;
- supported nonunique and unique secondary-index declaration, duplicate
  enforcement, copying, metadata, and row-update checks for timestamp columns,
  using canonical fixed-width text order and equality;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted timestamp data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `TIMESTAMP(fsp)`, fractional-second storage, fractional-second metadata above
  zero, `TIME`, `YEAR`, intervals, temporal arithmetic, casts, or temporal
  functions such as `CURRENT_TIMESTAMP`, `NOW()`, `UTC_TIMESTAMP()`,
  `TIMESTAMP()`, `TIMESTAMPADD()`, or `TIMESTAMPDIFF()`;
- `DEFAULT CURRENT_TIMESTAMP`, `ON UPDATE CURRENT_TIMESTAMP`, automatic update
  columns, or the historical disabled-`explicit_defaults_for_timestamp`
  behavior where assigning `NULL` to a nonnullable timestamp can mean current
  timestamp;
- mutable `time_zone` state, conversion between session time zones and UTC,
  offsets in temporal input values, or `AT TIME ZONE`;
- standard `TIMESTAMP 'str'` literals, ODBC temporal literals, relaxed
  delimiter conversion, `T` separators, nondelimited timestamp strings,
  two-digit year conversion, numeric timestamp conversion, parameters, user
  variables, functions, arbitrary expressions, column-to-column assignments,
  or `DEFAULT(col_name)`;
- mutable `sql_mode`, `ALLOW_INVALID_DATES`, `NO_ZERO_DATE`, or
  `NO_ZERO_IN_DATE` behavior;
- direct user storage of partial-zero timestamps or zero timestamps outside the
  specific MySQL-observed `INSERT IGNORE` and `ALTER ADD TIMESTAMP NOT NULL`
  adjustment paths;
- timestamp primary keys, auto-increment, generated columns, generated
  defaults, grouping, `DISTINCT`, timestamp aggregates, optimizer behavior,
  protocol-grade type metadata, binary protocol values, or origin metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and cleanup on failure.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported `INSERT IGNORE` timestamp adjustments
  record warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for bare `TIMESTAMP` type names and
  string literal default/DML/predicate values. They preserve source spans but
  do not resolve descriptors or convert timestamp values.
- Analyzer/planner code maps `TIMESTAMP` AST nodes to durable descriptors,
  resolves schemas/tables/columns through MyLite catalog descriptors, converts
  admitted timestamp values, rejects unsupported temporal operations, and
  produces descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, default kind, default text, column order,
  primary-key membership, index membership, and auto-increment attributes.
  SQLite schema text is never metadata authority.
- Result and introspection builders render logical descriptors and descriptor
  defaults to MySQL-shaped text.
- SQLite owns physical row storage, row mutation, secondary-index enforcement,
  and row ordering for generated prepared statements. Timestamp values bind as
  canonical `TEXT`; MyLite never relies on SQLite date/time functions,
  numeric affinity, or SQLite schema introspection for timestamp semantics.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

This feature extends the current limited column definition and row-value
grammar:

```sql
column_type:
    existing_integer_type
  | existing_decimal_type
  | existing_string_type
  | DATE
  | DATETIME
  | TIMESTAMP

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

String literals are admitted syntactically because existing string types and
canonical temporal descriptors need them. Timestamp conversion accepts only
canonical `YYYY-MM-DD HH:MM:SS` and, for internal adjustment paths,
`0000-00-00 00:00:00`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
column_type ::= timestamp_type.
timestamp_type ::= TIMESTAMP.

identifier ::= TIMESTAMP.

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

The grammar must reject `TIMESTAMP(fsp)`, `TIMESTAMP 'value'`, time-zone
offsets, `CURRENT_TIMESTAMP` defaults, and `ON UPDATE` clauses until those
features have separate specifications and runtime expectations.

## Descriptor and Storage Mapping

`TIMESTAMP` descriptors use:

| Descriptor field | Value |
| --- | --- |
| Logical type | `TIMESTAMP` |
| Physical SQLite type | `TEXT` |
| Display type | `timestamp` |
| Fractional precision | `0` |
| Character metadata | `NULL` |
| Numeric metadata | `NULL` |
| Sort/equality representation | canonical fixed-width text |
| User-visible valid range | `1970-01-01 00:00:01` to `2038-01-19 03:14:07` |
| Internal zero adjustment value | `0000-00-00 00:00:00` |

Catalog descriptors must persist the logical type and default text. Existing
descriptor versions, catalog generation, and `sqlite_schema_generation` rules
continue to apply. DML updates of row values must not mutate descriptor rows or
catalog generation.

## Conversion Semantics

Supported direct timestamp input is an ordinary SQL string literal whose
decoded value is exactly 19 bytes in canonical `YYYY-MM-DD HH:MM:SS` shape.
The converter validates:

- separators at positions 5, 8, 14, and 17;
- decimal digits in all year, month, day, hour, minute, and second positions;
- Gregorian month/day validity including leap years;
- hour `00..23`, minute `00..59`, and second `00..59`;
- value range from `1970-01-01 00:00:01` through
  `2038-01-19 03:14:07`.

Direct zero timestamp and direct out-of-range values fail in strict paths with
`1292 / 22007` and an `Incorrect datetime value` diagnostic naming the target
column and row. Invalid timestamp defaults fail with `1067 / 42000`.

`INSERT IGNORE` may store `0000-00-00 00:00:00` only for the observed MySQL
warning-demoted cases: invalid canonical-shaped timestamp input, direct zero
timestamp input, out-of-range canonical timestamp input, explicit `NULL` into a
`NOT NULL` timestamp, and missing/defaulted no-explicit-default `NOT NULL`
timestamp values. `ALTER TABLE ... ADD COLUMN ts TIMESTAMP NOT NULL` may
backfill `0000-00-00 00:00:00` for preexisting rows. Other direct SQL paths
must not accept zero timestamps.

Stored zero timestamps remain readable and orderable, but strict copying into a
new timestamp descriptor through `INSERT ... SELECT`, `REPLACE ... SELECT`, or
`CREATE TABLE ... SELECT` rejects them with the observed MySQL `1292 / 22007`
datetime-value diagnostic.

`NULL` is stored only for nullable timestamp descriptors. `NULL` into a
`NOT NULL` descriptor fails with `1048 / 23000` unless the current statement is
an admitted `INSERT IGNORE` adjustment. `DEFAULT` resolves through the
descriptor before conversion. A descriptor with no explicit default produces
`1364 / HY000` for omitted or explicit `DEFAULT` values unless the current
statement is an admitted `INSERT IGNORE` adjustment.

## Predicate, Ordering, and Index Semantics

Predicate conversion is descriptor-driven:

- comparison operators accept non-`NULL` canonical timestamp string literals;
- `<=>` accepts non-`NULL` canonical timestamp string literals for this slice;
- `BETWEEN` and `NOT BETWEEN` accept two compatible canonical timestamp
  bounds;
- `IN` and `NOT IN` accept nonempty lists of compatible canonical timestamp
  strings plus `NULL` list elements;
- `IS NULL` and `IS NOT NULL` test stored values without conversion.

Direct predicate literals of `0000-00-00 00:00:00` are rejected with the
observed MySQL `1525 / HY000` incorrect timestamp-value diagnostic; applications
can observe stored zero timestamps through ordering and ordinary readback, not
by admitting a zero timestamp literal in this slice.

Ordering is supported for one descriptor timestamp column in existing
single-table `SELECT`, `DELETE`, and `UPDATE` paths. `ASC` is the default.
`NULL` sorts before non-`NULL` ascending and after non-`NULL` descending.
Canonical fixed-width timestamp text preserves MySQL order for admitted valid
timestamps, and the internal zero timestamp sorts before valid timestamps. For
ties, MyLite must not promise a deterministic row choice unless a future
feature adds and verifies an explicit tie-breaker.

Supported unique and nonunique secondary indexes may include one timestamp
descriptor column. Duplicate checks compare canonical text. Multiple `NULL`
values remain allowed for unique indexes, matching the current unique-index
policy.

## Generated SQLite Handling

No SQLite fork patch is needed. MyLite uses wrapper/planner code plus public
SQLite prepared-statement APIs:

- generated physical table names remain stable descriptor-owned names such as
  `_mylite_user_table_<table_id>`;
- every generated SQLite identifier is quoted;
- timestamp values bind with `sqlite3_bind_text()` or `sqlite3_bind_null()`;
- predicate, default, and DML values are converted before binding;
- supported ordered/limited updates and deletes reuse the existing descriptor
  rowid-subquery strategy rather than relying on optional SQLite
  `UPDATE ... ORDER BY ... LIMIT` syntax;
- SQLite's date/time functions, type affinity conversions, and schema text are
  not timestamp semantic authority.

## Result and Metadata Behavior

Successful DDL and DML use the existing public result conventions. Successful
timestamp DML statements do not return row result sets. Supported in-range
statements report `warning_count == 0` unless they deliberately use an
admitted warning-producing form such as `INSERT IGNORE`.

`UPDATE` reports changed-row affected counts after timestamp canonicalization.
Assigning an already-stored timestamp value reports zero changed rows.

Metadata rendering:

- `SHOW COLUMNS` type text is `timestamp`; nullable create-time columns render
  `Default = NULL`, while dropped-default descriptors render no default.
- `SHOW CREATE TABLE` renders `timestamp NULL DEFAULT NULL` for create-time
  nullable default-`NULL` descriptors, `timestamp NULL` for nullable dropped
  defaults, `timestamp NOT NULL` for not-null no-default descriptors, and
  quoted canonical defaults for explicit non-`NULL` defaults.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = timestamp`,
  `COLUMN_TYPE = timestamp`, `DATETIME_PRECISION = 0`, and `NULL` for
  character and numeric metadata columns.

## Diagnostics

The implementation must cover deterministic diagnostics for:

- syntax errors and unsupported grammar, including fractional precision,
  timestamp literals, generated current-timestamp defaults, `ON UPDATE`,
  expression values, functions, and parameters;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, and reserved `_mylite_*` schema/table names;
- unknown assignment, predicate, ordering, index, and default-target columns;
- unsupported assignment, predicate, order, default, and limit expressions;
- unsupported literal forms, relaxed temporal strings, numeric timestamp
  values, fractional timestamp values, `T` separators, time-zone offsets, and
  out-of-range canonical timestamps;
- direct zero timestamp values in strict paths;
- `NULL` into `NOT NULL` timestamp descriptors;
- missing/defaulted no-explicit-default timestamp descriptors;
- duplicate-key conflicts on supported unique timestamp indexes;
- physical SQLite failures, allocation failures, and public API misuse through
  existing public API conventions.

Unsupported behavior should fail before generated SQLite SQL is prepared
whenever the unsupported shape can be detected from the AST and descriptors.

## Compatibility Documentation

Update `COMPATIBILITY.md` and the detailed compatibility docs to mark only the
limited timestamp type behavior as supported. The docs must not claim full
`TIMESTAMP`, fractional precision, timezone conversion, automatic
initialization or update, timestamp functions, expression conversion, full
metadata/protocol parity, timestamp primary keys, or general temporal
expression support.

## Tests

Add a MySQL expectation script and focused C tests under
`packages/libmylite/tests/`. A new runtime test binary is preferred if it keeps
timestamp coverage separate from the already-large date/datetime tests.

Coverage must include:

- parser acceptance for bare `TIMESTAMP` descriptors and rejection of
  fractional precision and current-timestamp clauses;
- `CREATE TABLE`, `ALTER ADD`, `ALTER SET/DROP DEFAULT`,
  `CREATE TABLE LIKE`, `CREATE TABLE SELECT`, `INSERT SELECT`, and
  `REPLACE SELECT` behavior for timestamp descriptors;
- metadata in `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`;
- valid lower and upper range boundaries, leap-day values, `NULL`, explicit
  defaults, dropped defaults, and no-default `NOT NULL` behavior;
- strict diagnostics for invalid date/time strings, direct zero timestamp,
  partial-zero timestamp, lower/upper out-of-range values, invalid defaults,
  `NULL` into `NOT NULL`, and missing no-default values;
- `INSERT IGNORE` warning adjustment to zero timestamp for invalid, zero,
  out-of-range, nullability, and missing-default cases;
- `UPDATE` assignment of canonical strings, `NULL`, and `DEFAULT`, including
  changed-row affected-count semantics and rejection of invalid/out-of-range
  values;
- comparisons, `<=>`, `BETWEEN`, `IN`, `IS NULL`, `IS NOT NULL`, `ORDER BY`,
  `DELETE ... ORDER BY ... LIMIT`, and `UPDATE ... ORDER BY ... LIMIT` over
  timestamp descriptors;
- nonunique and unique secondary indexes on timestamp descriptors, metadata,
  duplicate enforcement, copying, and row-update checks;
- reopen persistence, table rename/drop behavior, independent file-backed
  handles, and `.mylite` preamble preservation;
- no catalog generation or descriptor mutation from ordinary timestamp row
  updates;
- existing lexer, parser, runtime, catalog, DDL, DML, result, storage, VFS,
  date, datetime, string, decimal, index, and metadata tests still pass.

Required verification before completion:

1. `cmake --build --preset dev`
2. Focused new parser/runtime CTest entries and relevant existing temporal,
   DML, index, metadata, and lifecycle entries
3. `packages/libmylite/tests/mysql_baseline_timestamp_type_expectations.sh`
4. `cmake --workflow --preset check`

# Baseline DATE Type

## Status

This feature specifies the first temporal row-value slice for persistent
`.mylite` handles. It adds descriptor-owned `DATE` columns on top of the
existing base-table, DML, default, result, and introspection paths.

The feature is intentionally not full MySQL temporal conversion. It stores and
returns canonical date text and supports canonical string literals in date
contexts. It does not implement `TIME`, `DATETIME`, `TIMESTAMP`, `YEAR`,
temporal functions, standard `DATE '...'` literals, relaxed delimiters,
two-digit year conversion, numeric date conversion, time-zone behavior,
mutable SQL modes, generated defaults, casts, or protocol-grade temporal
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
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values, defaults, DML, primary-key, auto-increment, select,
  update, `SHOW`, and `INFORMATION_SCHEMA` specs under `docs/specs/`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `DATE`, `DATETIME`, and `TIMESTAMP` types:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- MySQL 8.4 Reference Manual, date and time literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, problems using `DATE` columns:
  https://dev.mysql.com/doc/refman/8.4/en/using-date.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_date_type_expectations.sh` records
the runtime probes for this feature. Observed behavior that shapes this slice:

- MySQL renders a `DATE` descriptor as `date` in `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.
- `DATE` values are displayed as `YYYY-MM-DD`. The documented supported normal
  range is `1000-01-01` through `9999-12-31`.
- With MySQL 8.4.9 default SQL mode
  `ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`,
  canonical valid date strings store and read back unchanged, while direct
  invalid canonical dates and direct zero dates fail with error `1292` /
  SQLSTATE `22007`.
- `INSERT IGNORE` demotes invalid date inputs, zero-date inputs, and explicit
  `NULL` into a `NOT NULL` date column to warnings and stores `0000-00-00`.
  The observed invalid-date and zero-date warning is `1264`; the observed
  explicit-`NULL` warning is `1048`.
- Omitted or explicit `DEFAULT` for a `DATE NOT NULL` column with no explicit
  default fails with error `1364`; `INSERT IGNORE` demotes that condition to
  warning `1364` and stores `0000-00-00`.
- `DATE DEFAULT 'YYYY-MM-DD'` renders as a quoted default in `SHOW CREATE
  TABLE` and as unquoted canonical text in `SHOW COLUMNS` and
  `INFORMATION_SCHEMA.COLUMNS`. Invalid and zero-date defaults fail with
  error `1067`.
- `ALTER TABLE ... ADD COLUMN d DATE` backfills existing rows with `NULL`.
  `ALTER TABLE ... ADD COLUMN d DATE NOT NULL DEFAULT 'YYYY-MM-DD'` backfills
  existing rows with that default. `ALTER TABLE ... ADD COLUMN d DATE NOT NULL`
  succeeds on an empty table but fails on a nonempty table with error `1292`
  because MySQL would have to fill `0000-00-00` under the default SQL mode.
- Single-table `UPDATE` reports changed-row affected counts after canonical
  date conversion. Assigning a date column to its already-stored canonical text
  reports zero changed rows.
- `DATE` comparisons against canonical string constants, `BETWEEN`, `IN`,
  `IS NULL`, and `IS NOT NULL` work as date-context predicates. `ORDER BY`
  defaults to ascending order, orders `NULL` before non-`NULL` values in
  ascending order, and orders `NULL` after non-`NULL` values in descending
  order.
- MySQL accepts wider temporal input forms, including standard `DATE 'str'`
  literals, relaxed delimiters, nondelimited strings, two-digit years,
  numeric dates, date-time strings in date context, and mode-dependent invalid
  or partial-zero dates. MyLite defers those until the temporal conversion and
  SQL-mode layers are broadened.

## Scope

The implementation must add:

- parser and AST support for `DATE` column types;
- string literal defaults in column definitions, with DATE validation limited
  to canonical date text;
- descriptor-owned logical type text `DATE`;
- physical SQLite type text `TEXT` for admitted date descriptors;
- durable catalog support for quoted text defaults while preserving existing
  integer and decimal defaults, descriptor generations, and old catalog
  migration behavior;
- `CREATE TABLE` support for persistent base tables containing date columns,
  including nullable and not-null columns plus explicit `DEFAULT NULL` and
  non-`NULL` canonical date defaults;
- `ALTER TABLE ... ADD [COLUMN]` support for date columns, including nullable
  existing-row backfill with `NULL`, explicit-default backfill with the
  canonical default, empty-table `DATE NOT NULL` no-explicit-default metadata,
  and nonempty-table rejection for `DATE NOT NULL` without an explicit default;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` and `DROP DEFAULT` support
  for date descriptors;
- `CREATE TABLE ... LIKE` descriptor cloning for date columns and their
  defaults;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source values are already compatible date
  descriptors;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for date descriptors;
- canonical string literal, `NULL`, and `DEFAULT` values for
  `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignments into date columns;
- MyLite-owned date conversion before SQLite binding: exact canonical string
  syntax, leap-year validation, normal supported range checking, direct
  zero-date rejection under the fixed default SQL mode, strict error `1292`,
  invalid-default error `1067`, supported `INSERT IGNORE` warning adjustment to
  `0000-00-00`, and canonical text output;
- descriptor-backed `WHERE` predicates for date columns using canonical string
  right operands for comparison operators, `BETWEEN` bounds, and `IN` list
  values, with `NULL` preserved in admitted `IN` lists;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  date columns;
- descriptor-backed `ORDER BY` for one date column in `SELECT`, `DELETE`, and
  single-table `UPDATE`, reusing the existing single-key `ASC`/`DESC` and
  `LIMIT` paths;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted date data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `TIME`, `DATETIME`, `TIMESTAMP`, `YEAR`, temporal intervals, or temporal
  functions such as `CURRENT_DATE`;
- standard `DATE 'str'` literals, ODBC temporal literals, relaxed delimiter
  conversion, nondelimited date strings, two-digit year conversion, numeric
  date conversion, date-time string truncation to date, parameters, user
  variables, functions, arbitrary expressions, column-to-column assignments,
  or `DEFAULT(col_name)`;
- mutable `sql_mode`, `ALLOW_INVALID_DATES`, `NO_ZERO_DATE`, or
  `NO_ZERO_IN_DATE` behavior;
- direct storage of partial-zero dates such as `2024-00-01` or `2024-01-00`;
- date arithmetic, casts, collation-sensitive date comparison, grouping,
  `DISTINCT`, min/max date aggregates, date primary keys, date indexes,
  generated columns, or optimizer behavior;
- protocol-grade type metadata, field flags, binary protocol values, or origin
  metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and cleanup on failure.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported `INSERT IGNORE` date adjustments record
  warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for `DATE` type names and string
  literal default/DML/predicate values. They preserve source spans but do not
  resolve descriptors or convert dates.
- Analyzer/planner code maps `DATE` AST nodes to durable descriptors, resolves
  schemas/tables/columns through MyLite catalog descriptors, converts admitted
  date values, rejects unsupported temporal operations, and produces
  descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, default kind, default text, column order,
  primary-key membership, and auto-increment attributes. SQLite schema text is
  not metadata authority.
- Result and introspection builders render logical descriptors and descriptor
  defaults to MySQL-shaped text.
- SQLite owns physical row storage and row mutation for generated prepared
  statements. Date values bind as canonical `TEXT`; MyLite never stores them
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
  | DATE

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
    existing_predicate_integer_or_function_value
  | string_literal

predicate_range_value:
    existing_predicate_integer_value
  | string_literal

predicate_in_value:
    predicate_range_value
  | NULL
```

String literals are admitted syntactically only because existing string types
and this DATE slice need them. DATE conversion accepts only canonical
`YYYY-MM-DD` and, for internal `INSERT IGNORE` adjustment, `0000-00-00`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
column_type ::= date_type.
date_type ::= DATE.

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

`DATE` remains usable as an unquoted identifier in identifier positions.

## Semantics

### Descriptor and Storage

An admitted `DATE` column persists as:

- logical type: `DATE`;
- physical SQLite type: `TEXT`;
- default kind: existing integer default kinds for integer columns, existing
  decimal default kind for decimal columns, and a new text-default kind for
  DATE canonical defaults;
- default text: canonical `YYYY-MM-DD` for explicit date defaults.

The catalog schema version must advance because older MyLite readers do not
understand the new text-default kind. No SQLite table-layout change is needed
because `default_text` already exists.

### Date Conversion

Direct DATE input accepts only a string literal whose decoded content is
exactly ten bytes in `YYYY-MM-DD` form. Bytes at positions 4 and 7 must be
hyphens; all other bytes must be ASCII digits.

`0000-00-00` is the internal zero date. It is stored only through supported
`INSERT IGNORE` adjustment paths and is rejected for direct strict inputs and
explicit defaults.

Normal direct inputs must satisfy:

- year from `1000` through `9999`;
- month from `01` through `12`;
- day valid for the resolved month and leap year.

Leap years follow the proleptic Gregorian rule used by MySQL for supported
dates: years divisible by 4 are leap years except years divisible by 100 unless
also divisible by 400.

### Defaults and Nullability

Nullable DATE columns with no explicit default have an effective DML default of
`NULL` and render `DEFAULT NULL` where existing MyLite nullable descriptors
render implicit defaults.

`DATE NOT NULL` with no explicit default has no effective strict DML default.
Omitted values and explicit `DEFAULT` fail with error `1364`. In
`INSERT IGNORE`, they warn `1364` and store `0000-00-00`.

Explicit `DEFAULT 'YYYY-MM-DD'` stores canonical default text. Invalid
canonical dates, zero dates, noncanonical strings, numeric defaults,
expressions, and `DEFAULT CURRENT_DATE` are rejected for this slice.

`NULL` into `DATE NOT NULL` fails with error `1048`; `INSERT IGNORE` warns
`1048` and stores `0000-00-00`.

### Predicates and Ordering

DATE comparison predicates are admitted only with canonical string right
operands. The planner converts the string to canonical date text and binds it
as SQLite text. Because admitted nonzero dates are canonical `YYYY-MM-DD`, byte
lexicographic ordering matches chronological ordering for the supported range.
The zero date sorts before supported nonzero dates, matching the observed
MySQL 8.4.9 order.

`BETWEEN` and `IN` use the same canonical date conversion. `NULL` values in
`IN` lists remain SQL `NULL`. `IS NULL` and `IS NOT NULL` work directly over
the descriptor column.

One-column DATE `ORDER BY` is admitted for `SELECT`, `DELETE`, and
single-table `UPDATE`. Default direction and `ASC` are ascending; `DESC` is
descending. `NULL` sorts before non-`NULL` values in ascending order and after
non-`NULL` values in descending order. Duplicate sort values have no claimed
tie order beyond the existing single-key baseline.

### Generated SQLite SQL

Generated SQLite statements must continue to:

- use stable physical table names such as `_mylite_user_table_<table_id>`;
- quote every generated SQLite identifier;
- bind DATE assignment, predicate, default-copy, and limit values through
  prepared-statement parameters;
- avoid relying on SQLite optional `UPDATE ... ORDER BY ... LIMIT` syntax;
- use the existing rowid-subquery shape for ordered/limited `UPDATE` and
  `DELETE`;
- keep DATE conversion and MySQL diagnostics in MyLite before binding values.

### Results

Successful DATE DDL and DML follow existing public result conventions:

- DDL returns no row result set and reports the existing affected-row
  conventions for DDL;
- `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` report affected rows through the
  existing result object;
- supported in-range DATE operations record `warning_count == 0`;
- adjusted `INSERT IGNORE` records the MySQL-runtime-verified warnings above;
- `SELECT` readback returns DATE text exactly as stored.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- unsupported `DATE` declaration attributes such as precision, `UNSIGNED`,
  `ZEROFILL`, charset/collation attributes, `ON UPDATE`, generated defaults,
  and expression defaults;
- missing default schema, unknown schema, unknown table, reserved names, and
  unsupported object kind through existing table-resolution paths;
- unknown assignment, predicate, and ordering columns through existing
  descriptor-resolution diagnostics;
- invalid date values with MySQL error `1292` / SQLSTATE `22007` for direct
  strict row inputs;
- invalid date defaults with MySQL error `1067`;
- `NULL` into `DATE NOT NULL` with MySQL error or warning `1048`;
- no default for `DATE NOT NULL` with MySQL error or warning `1364`;
- `INSERT IGNORE` invalid or zero date adjustment with MySQL warning `1264`;
- nonempty `ALTER TABLE ... ADD COLUMN d DATE NOT NULL` with MySQL error
  `1292` for the implicit zero date at row 1;
- unsupported DATE conversion inputs with a MyLite-specific unsupported
  message until that wider MySQL conversion surface is specified;
- physical SQLite failures, allocation failures, and public API misuse through
  existing runtime paths.

## Test Plan

Add a fast C test binary, preferably `runtime_date_type`, covering:

- parser support for `DATE` descriptors and `DATE` as an identifier;
- successful `CREATE TABLE` with nullable, not-null, default-null, and
  canonical default date columns;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  metadata;
- valid boundary and leap-day inserts;
- invalid normal dates, zero dates, noncanonical dates, and unsupported
  conversion inputs;
- `NULL`, `DEFAULT`, omitted columns, and `INSERT IGNORE` adjustment behavior;
- `UPDATE` canonical assignment, no-op changed-row counts, `NULL`, `DEFAULT`,
  and invalid values;
- DATE comparison, null-safe comparison, `BETWEEN`, `IN`, `IS NULL`,
  `IS NOT NULL`, `ORDER BY`, `ORDER BY ... LIMIT` update/delete behavior, and
  `LIMIT 0`;
- `ALTER TABLE ... ADD COLUMN` nullable/default/not-null empty and nonempty
  cases plus `ALTER COLUMN SET/DROP DEFAULT`;
- `CREATE TABLE ... LIKE`, compatible `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, and `REPLACE ... SELECT`;
- table rename/drop interactions;
- reopen persistence and independent file-backed handles;
- `.mylite` preamble preservation;
- zero-initialized cleanup for new planner/result objects;
- unchanged public API misuse behavior.

Run:

1. `packages/libmylite/tests/mysql_baseline_date_type_expectations.sh`
2. `cmake --build --preset dev`
3. the new CTest entry and relevant parser/runtime lifecycle entries
4. `cmake --workflow --preset check`


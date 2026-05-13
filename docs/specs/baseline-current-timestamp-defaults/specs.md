# Baseline Current Timestamp Defaults

## Status

This feature adds the next common temporal table-behavior slice: statement-time
`CURRENT_TIMESTAMP` / `NOW()` values, explicit `DEFAULT CURRENT_TIMESTAMP`, and
explicit `ON UPDATE CURRENT_TIMESTAMP` for existing second-precision
`DATETIME` and fixed-UTC `TIMESTAMP` descriptors.

The slice is intentionally narrow. It does not implement fractional seconds,
mutable session time zones, `SYSDATE()`, `UTC_TIMESTAMP()`, general temporal
expression evaluation, generated columns, broad default expressions, triggers,
or SQLite default clauses. MyLite owns the compatibility semantics in its
parser, catalog descriptors, analyzer/runtime conversion, result metadata, and
generated SQLite statements.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline SQL mode session state and zero temporal SQL modes:
  `docs/specs/baseline-sql-mode-session-state/specs.md` and
  `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
- Baseline `DATETIME` and `TIMESTAMP` descriptors:
  `docs/specs/baseline-datetime-type/specs.md` and
  `docs/specs/baseline-timestamp-type/specs.md`
- Existing DML/default/update/introspection specs under `docs/specs/`
- MySQL 8.4 Reference Manual, automatic initialization and updating for
  `TIMESTAMP` and `DATETIME`:
  https://dev.mysql.com/doc/refman/8.4/en/timestamp-initialization.html
- MySQL 8.4 Reference Manual, date and time functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- MySQL 8.4 Reference Manual, `timestamp` system variable:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_current_timestamp_defaults_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The MySQL expectation script records the probes that define this phase. The
probes set `time_zone = '+00:00'` so the current MyLite fixed-UTC timestamp
slice can compare deterministic values.

- `NOW()`, `CURRENT_TIMESTAMP`, `CURRENT_TIMESTAMP()`, `LOCALTIME`,
  `LOCALTIME()`, `LOCALTIMESTAMP`, and `LOCALTIMESTAMP()` return the statement
  current timestamp in `YYYY-MM-DD HH:MM:SS` form for the zero-fractional slice.
- `SET timestamp = unix_epoch_seconds` changes the session value used by
  `NOW()` and its synonyms. `SET timestamp = DEFAULT` restores the nonconstant
  current-time behavior. MySQL accepts positive integer values through
  `2147483647`, rejects values above that maximum with `1231 / 42000`, and
  treats `0` or negative values as a reset to default current-time behavior.
- `DATETIME` and `TIMESTAMP` columns accept `DEFAULT CURRENT_TIMESTAMP` and
  `ON UPDATE CURRENT_TIMESTAMP`. The order of those column attributes is not
  significant. MySQL also accepts the documented current-timestamp synonyms in
  those clauses and normalizes `SHOW CREATE TABLE` output to
  `CURRENT_TIMESTAMP`.
- `SHOW COLUMNS` reports `CURRENT_TIMESTAMP` as the default for defaulted
  automatic columns. `Extra` includes `DEFAULT_GENERATED` for a current
  timestamp default and `on update CURRENT_TIMESTAMP` for an auto-update
  column.
- `INFORMATION_SCHEMA.COLUMNS` reports `COLUMN_DEFAULT = CURRENT_TIMESTAMP`,
  `DATETIME_PRECISION = 0`, and the same `EXTRA` text shape for this
  zero-fractional slice.
- Inserting a row that omits a current-timestamp default column stores the
  statement timestamp. All automatic columns in a row use the same statement
  value.
- `ALTER TABLE ... ADD COLUMN ... DEFAULT CURRENT_TIMESTAMP` backfills existing
  rows with the statement timestamp used by the alter statement.
- Updating any other column to a different value changes every `ON UPDATE`
  temporal column that is not explicitly assigned by that `UPDATE`. Updating a
  row to the same stored value changes no row and does not advance automatic
  columns.
- Explicitly assigning an auto-update column to itself suppresses the automatic
  change for that column while other auto-update columns still advance if some
  other column value changes. MyLite cannot express column-to-column
  assignments yet, so this behavior stays documented but deferred.
- Explicitly assigning `CURRENT_TIMESTAMP` / `NOW()` to a temporal column stores
  the statement timestamp and counts as a change only when the stored value is
  different.
- Successful supported statements report MySQL changed-row affected counts and
  `@@warning_count = 0`.

## Scope

Supported:

- persistent MyLite base tables only;
- zero-fractional statement current timestamp values in the fixed UTC session
  used by the current `TIMESTAMP` descriptor slice;
- no-source, `FROM DUAL`, `DO`, and row-scalar projection support for
  `NOW()`, `CURRENT_TIMESTAMP`, `CURRENT_TIMESTAMP()`, `LOCALTIME`,
  `LOCALTIME()`, `LOCALTIMESTAMP`, and `LOCALTIMESTAMP()`;
- limited `SET timestamp = integer_literal`, `SET timestamp = +integer_literal`,
  `SET timestamp = DEFAULT`, and the same existing unqualified/session-scoped
  target forms accepted by the system-variable `SET` parser;
- `@@timestamp`, `@@SESSION.timestamp`, and `SHOW VARIABLES LIKE 'timestamp'`
  readback for the limited numeric session value;
- `CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]` `DATETIME` / `TIMESTAMP`
  columns with `DEFAULT CURRENT_TIMESTAMP`, `DEFAULT CURRENT_TIMESTAMP()`,
  `DEFAULT NOW()`, `DEFAULT LOCALTIME`, `DEFAULT LOCALTIME()`,
  `DEFAULT LOCALTIMESTAMP`, or `DEFAULT LOCALTIMESTAMP()`;
- `CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]` `DATETIME` / `TIMESTAMP`
  columns with `ON UPDATE` followed by the same zero-fractional
  current-timestamp forms;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT CURRENT_TIMESTAMP` for
  existing `DATETIME` / `TIMESTAMP` descriptors;
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignment support for explicit
  current-timestamp values into `DATETIME` / `TIMESTAMP` targets;
- descriptor-driven omitted-column and explicit `DEFAULT` materialization of
  current-timestamp defaults;
- descriptor-driven automatic update of `ON UPDATE CURRENT_TIMESTAMP` columns
  when a supported one-assignment `UPDATE` changes a row;
- descriptor-backed `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS` rendering for
  current timestamp defaults and auto-update metadata;
- `CREATE TABLE ... LIKE`, table rename/drop, reopen persistence, independent
  handles, `.mylite` preamble preservation, and file-backed storage invariants;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider behavior.

Deferred:

- fractional seconds and `CURRENT_TIMESTAMP(fsp)` / `NOW(fsp)` forms except
  exact `(0)` in default and `ON UPDATE` clauses if implementation can admit it
  without adding fractional metadata;
- mutable session `time_zone` semantics, local time-zone conversion, and
  non-UTC timestamp storage;
- `UTC_TIMESTAMP()`, `SYSDATE()`, `UNIX_TIMESTAMP()`, `FROM_UNIXTIME()`,
  current date/time-only functions, and temporal arithmetic beyond existing
  slices;
- `explicit_defaults_for_timestamp = OFF` legacy behavior where assigning
  `NULL` to a nonnullable `TIMESTAMP` can mean current timestamp;
- column-to-column assignments such as `SET ts = ts`, arithmetic assignments,
  table-qualified assignment targets, multiple assignments, generated columns,
  triggers, cascades, foreign keys, privilege semantics, and protocol-grade
  changed-column metadata;
- automatic timestamp columns that participate in supported primary or unique
  indexes. MyLite must reject those definitions for this slice rather than
  reporting generic SQLite constraint failures from automatic updates.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns misuse diagnostics, result
  ownership, and cleanup.
- Statement context: owns statement boundaries, diagnostics, warning counts,
  affected rows, and the statement-time snapshot. This feature uses the
  existing statement-time field, optionally overridden by the session
  `timestamp` value, and keeps automatic temporal values stable across a single
  statement.
- Lexer/parser/AST: admits the exact current-timestamp forms in scalar
  projection, DML value, column default, and `ON UPDATE` column-attribute
  positions. The parser stores syntax; it does not bind object descriptors or
  format time values.
- Analyzer/planner: resolves target schemas/tables/columns through MyLite
  descriptors, checks that current timestamp defaults and auto-update
  attributes are used only on supported temporal descriptors, and plans the
  exact generated values.
- Catalog: remains the durable authority for logical type, nullability,
  default kind, auto-update current-timestamp flag, column order, and table
  identity. This feature requires a catalog schema bump to persist both a
  current-timestamp default kind and the `ON UPDATE CURRENT_TIMESTAMP`
  attribute. SQLite schema text is never metadata authority.
- Result and introspection builders: render descriptor metadata into MySQL-like
  `SHOW`, `DESCRIBE`, and `INFORMATION_SCHEMA.COLUMNS` rows.
- SQLite physical storage: stores automatic `DATETIME` and `TIMESTAMP` values
  as canonical text in MyLite-generated rowid tables. MyLite generates quoted
  physical `UPDATE`/`INSERT` statements and binds all current-timestamp values
  as parameters; it does not rely on SQLite's `CURRENT_TIMESTAMP` default or
  trigger machinery.
- Storage/VFS: unchanged. This feature writes only inside the shifted SQLite
  payload and must not touch the MyLite preamble.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippets:

```sql
current_timestamp_value:
    CURRENT_TIMESTAMP
  | CURRENT_TIMESTAMP LPAREN RPAREN
  | NOW LPAREN RPAREN
  | LOCALTIME
  | LOCALTIME LPAREN RPAREN
  | LOCALTIMESTAMP
  | LOCALTIMESTAMP LPAREN RPAREN

column_default:
    DEFAULT NULL
  | DEFAULT column_default_value
  | DEFAULT current_timestamp_value

column_attribute:
    nullability
  | column_default
  | ON UPDATE current_timestamp_value
  | PRIMARY KEY
  | UNIQUE
  | UNIQUE KEY
  | AUTO_INCREMENT

insert_value:
    existing_insert_value
  | current_timestamp_value

update_value:
    existing_update_value
  | current_timestamp_value

expression:
    existing_expression
  | current_timestamp_value

set_system_variable_value:
    existing_set_value
  | PLUS INTEGER
```

Parser admission must reject unsupported arities such as `NOW(1)` with the
same native-function argument-count class used by other scalar functions where
possible. Unsupported fractional precision in defaults or `ON UPDATE` must
produce deterministic MyLite diagnostics unless exact `(0)` is explicitly
implemented.

## Statement Current Time

MyLite already captures `time(NULL)` when a statement context begins. This
feature uses that value as the single statement timestamp for all automatic
values produced by the statement. If the session `timestamp` variable is set to
a constant in the supported range, the statement timestamp uses that constant
instead.

Formatting:

- current timestamp text is `YYYY-MM-DD HH:MM:SS`;
- formatting uses UTC so it matches the current fixed-UTC `TIMESTAMP` slice and
  avoids host time-zone dependence;
- all automatic columns and scalar current-timestamp projections in one
  statement use the same value;
- the maximum supported value is `2038-01-19 03:14:07`, matching MySQL's
  documented `timestamp` system-variable maximum and the current MyLite
  `TIMESTAMP` descriptor range;
- nonconstant default mode reads the host current time at statement begin.

## Defaults And Auto-Update Metadata

Catalog metadata must represent current-timestamp defaults separately from text
defaults. A current timestamp default has no stored `default_text`; it is
materialized at DML or backfill time. Rendering uses the canonical text
`CURRENT_TIMESTAMP`.

`ON UPDATE CURRENT_TIMESTAMP` is a separate boolean descriptor property. It
does not change `default_kind`. A column can have no default but have
auto-update metadata.

Allowed descriptor targets:

- `DATETIME` and `TIMESTAMP` only;
- second-precision descriptors only;
- non-primary and non-unique columns for this slice.

Rejected descriptor targets:

- all numeric, string, `DATE`, `TIME`, `TEXT`, and unsupported types;
- any supported primary-key or unique-key part;
- duplicate current-timestamp default attributes;
- duplicate `ON UPDATE CURRENT_TIMESTAMP` attributes;
- unsupported fractional forms beyond the selected zero-fractional subset.

## DML Semantics

Omitted or explicit `DEFAULT` values for current-timestamp default columns are
materialized before SQLite binding. `INSERT` and `REPLACE` rows in the same
statement share one timestamp value.

Explicit current-timestamp values are admitted only when the target descriptor
is `DATETIME` or `TIMESTAMP`. MyLite materializes them before binding, then
applies the existing temporal storage rules:

- `DATETIME` accepts the full current-time range reachable from supported
  session timestamps;
- `TIMESTAMP` accepts values through `2038-01-19 03:14:07`;
- `NULL` remains governed by existing nullability rules and never means current
  timestamp while `explicit_defaults_for_timestamp` stays effectively enabled.

For single-table `UPDATE`, MyLite keeps the existing changed-row policy. It
adds unassigned auto-update columns to the generated physical `SET` list only
when the one explicit assignment would change the row under the existing
descriptor-aware changed condition. The generated statement must therefore keep
no-op assignments from advancing automatic columns.

If the explicit assignment target is itself an auto-update column, that column
uses the explicit assignment value. Other unassigned auto-update columns still
advance when the row changes.

`UPDATE ... LIMIT 0` and no-match updates must not materialize scalar
subqueries or modify auto-update columns, preserving the existing update slice.

## Generated SQLite Shape

Generated SQL must use stable physical table and column names from descriptors,
quote every generated identifier, and bind all values. For updates with
auto-update columns, the physical statement shape is:

```sql
UPDATE "_mylite_user_table_N"
SET "assignment_col" = ?1,
    "auto_col_1" = ?auto1,
    ...
WHERE existing_predicate_or_rowid_limited_predicate
  AND changed_condition_for_assignment_col
```

For ordered/limited updates, MyLite continues to select rowids through the
existing descriptor-built rowid subquery rather than relying on optional SQLite
`UPDATE ... ORDER BY ... LIMIT` support.

No SQLite fork patch is required.

## Diagnostics

Supported successful statements return through the existing non-row result
conventions: no result rows, exact changed-row `affected_rows`, and
`warning_count == 0`.

Diagnostics to specify and test:

- syntax errors and unsupported current-timestamp arities;
- unsupported fractional precision;
- `CURRENT_TIMESTAMP` defaults or `ON UPDATE` on non-`DATETIME` /
  non-`TIMESTAMP` descriptors;
- duplicate default or auto-update attributes;
- current-timestamp assignment to non-temporal targets;
- unsupported `SET timestamp` values, including strings, floats, `NULL`,
  values above `2147483647`, and unsupported target scopes;
- unknown schemas/tables/columns through existing descriptor diagnostics;
- automatic timestamp columns in primary or unique keys;
- physical SQLite failures, allocation failures, and public API misuse through
  existing diagnostics.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-temporal.md`,
`docs/compatibility/sql-table-ddl.md`,
`docs/compatibility/sql-table-dml.md`,
`docs/compatibility/runtime-system-variables.md`, and
`docs/compatibility/type-system-literals-conversion.md` for only the supported
subset. Do not claim fractional seconds, mutable time zones, full temporal
functions, general expression defaults, triggers, generated columns, or full
protocol metadata.

## Tests

Add a focused C runtime test, preferably
`packages/libmylite/tests/runtime_current_timestamp_defaults_test.c`, and
register it with a dotted CTest name.

Coverage must include:

- MySQL expectation script version check and deterministic `SET timestamp`
  probes;
- no-source, `DUAL`, `DO`, and row-scalar current timestamp projection;
- `SET timestamp` supported forms, readback, default reset, independent
  handles, and unsupported values;
- `CREATE TABLE` current timestamp defaults and auto-update clauses for
  `DATETIME` and `TIMESTAMP`;
- attribute order normalization and descriptor rendering through
  `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`;
- omitted column, explicit `DEFAULT`, explicit current timestamp, and explicit
  `NULL` DML behavior;
- `ALTER TABLE ... ADD COLUMN` backfill with current timestamp defaults;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT CURRENT_TIMESTAMP`;
- auto-update behavior for changed updates, no-op updates, `WHERE`, `ORDER BY`
  / `LIMIT`, `LIMIT 0`, and explicit assignment to one auto-update column;
- affected rows, warning counts, no result rows, persistence after reopen,
  table rename/drop, and independent file-backed handles;
- deterministic rejection of unsupported syntax and unsupported descriptor
  combinations;
- `.mylite` preamble preservation.

Verification before completion:

1. `cmake --build --preset dev`
2. Focused CTest entries for current timestamp defaults, parser, sql-mode,
   datetime, timestamp, update, insert/replacement, alter-column defaults, and
   catalog/persistence lifecycle tests.
3. `packages/libmylite/tests/mysql_baseline_current_timestamp_defaults_expectations.sh`
4. `cmake --workflow --preset check`

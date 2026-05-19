# Baseline UTC Date And Time Functions

## Status

This feature adds a narrow current UTC temporal-function slice:
`UTC_DATE`, `UTC_DATE()`, `UTC_TIME`, `UTC_TIME()`, `UTC_TIMESTAMP`, and
`UTC_TIMESTAMP()` in scalar projection, `DO`, row-scalar projection, and direct
DML assignment to compatible temporal descriptors.

The slice deliberately stays zero-fractional. It does not add fractional
precision arguments, time-zone table support, temporal arithmetic, broader
temporal coercion, generated columns, or arbitrary expression evaluation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Current date/time functions:
  `docs/specs/baseline-current-date-time-functions/specs.md`
- Current timestamp defaults:
  `docs/specs/baseline-current-timestamp-defaults/specs.md`
- Time-zone system variable:
  `docs/specs/baseline-time-zone-system-variable/specs.md`
- Official MySQL 8.4 Reference Manual, date and time functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- Official MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_utc_date_time_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase. Probes set
`timestamp` to deterministic Unix epoch seconds and compare UTC functions
against session-local current functions under both UTC and non-UTC session time
zones.

- `UTC_DATE` and `UTC_DATE()` return the statement UTC date in `YYYY-MM-DD`
  form.
- `UTC_TIME` and `UTC_TIME()` return the statement UTC time in `HH:MM:SS` form
  for the zero-fractional slice.
- `UTC_TIMESTAMP` and `UTC_TIMESTAMP()` return the statement UTC timestamp in
  `YYYY-MM-DD HH:MM:SS` form for the zero-fractional slice.
- These functions are statement-stable and observe the same session
  `timestamp` override as `NOW()`.
- Unlike `NOW()`, `CURRENT_DATE`, and `CURRENT_TIME`, UTC functions ignore the
  session `time_zone` offset. With `time_zone = '+02:00'` and
  `timestamp = 1700000000`, MySQL returns `NOW() = '2023-11-15 00:13:20'` but
  `UTC_TIMESTAMP() = '2023-11-14 22:13:20'`.
- MySQL accepts the bare keyword forms `UTC_DATE`, `UTC_TIME`, and
  `UTC_TIMESTAMP`, and it accepts whitespace before empty parentheses.
- `DO UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP()` succeeds, returns no rows,
  reports `ROW_COUNT() == 0`, and emits no warnings.
- Direct `INSERT`, `REPLACE`, and single-table `UPDATE` assignments can store
  `UTC_DATE()` into `DATE`, `UTC_TIME()` into `TIME`, and `UTC_TIMESTAMP()`
  into `DATETIME` or `TIMESTAMP` columns.
- `UTC_DATE(1)` is a syntax error. `UTC_TIME(fsp)` and `UTC_TIMESTAMP(fsp)`
  are accepted by MySQL for precision values `0..6`, but fractional precision
  is outside this slice.

## Scope

Supported:

- no-source `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table row-scalar
  projection for:
  - `UTC_DATE`;
  - `UTC_DATE()`;
  - `UTC_TIME`;
  - `UTC_TIME()`;
  - `UTC_TIMESTAMP`;
  - `UTC_TIMESTAMP()`;
- scalar values use the statement timestamp already captured by statement
  context, or the current session `timestamp` override when set;
- formatting always uses UTC, independent of the current session `time_zone`;
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` direct assignment of:
  - UTC date values to `DATE` columns;
  - UTC time values to `TIME` columns;
  - UTC timestamp values to `DATETIME` and `TIMESTAMP` columns;
- descriptor-backed generated SQLite SQL remains built from stable physical
  table and column names with bound text values;
- successful supported statements produce `warning_count == 0` and follow the
  existing public result API conventions for scalar, `DO`, and DML statements;
- UTC functions do not mutate catalog descriptors, catalog generation, SQLite
  schema generation, or the `.mylite` preamble;
- reopen persistence and independent file-backed handles preserve updated row
  state through the existing storage layer.

Deferred:

- fractional forms such as `UTC_TIME(1)` and `UTC_TIMESTAMP(6)`;
- `UTC_DATE(...)` with arguments;
- using UTC functions in defaults, generated columns, predicates, ordering
  expressions, grouping, joins, scalar subqueries, prepared-statement
  parameters, or arbitrary expression trees beyond the existing admitted scalar
  projection and DML value positions;
- assigning UTC functions to integer, decimal, approximate, string, binary
  string, `BIT`, `YEAR`, `ENUM`, `SET`, `JSON`, or incompatible temporal
  descriptor families;
- named time-zone tables, daylight-saving rules, fractional seconds, temporal
  arithmetic, casts, triggers, cascades, privilege semantics, and protocol-grade
  temporal metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns misuse behavior, result
  ownership, diagnostics access, and cleanup.
- Statement context: owns the statement-time snapshot. This feature reads that
  snapshot without changing statement lifecycle or transaction behavior.
- Lexer/parser/AST: admits only the listed zero-fractional UTC date/time forms
  in existing expression and DML value positions. The parser records syntax and
  source spans only.
- Analyzer/planner/runtime: resolves DML targets through MyLite catalog
  descriptors and converts admitted UTC functions into canonical text values
  before binding generated SQLite statements.
- Catalog: unchanged. Descriptors remain the authority for logical type,
  nullability, defaults, column order, and table identity.
- Result builder: returns scalar UTC text through existing public result cells
  and keeps default labels from the original expression span unless an alias is
  present.
- SQLite physical storage: stores UTC assignment results as canonical text in
  MyLite-generated tables. It does not rely on SQLite date functions, defaults,
  triggers, or optional syntax.
- Storage/VFS: unchanged. This feature writes only inside the shifted SQLite
  payload and must not touch the MyLite preamble.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippets:

```lemon
utc_date_value:
    UTC_DATE
  | UTC_DATE LPAREN RPAREN

utc_time_value:
    UTC_TIME
  | UTC_TIME LPAREN RPAREN

utc_timestamp_value:
    UTC_TIMESTAMP
  | UTC_TIMESTAMP LPAREN RPAREN

expression:
    existing_expression
  | utc_date_value
  | utc_time_value
  | utc_timestamp_value

insert_value:
    existing_insert_value
  | utc_date_value
  | utc_time_value
  | utc_timestamp_value

update_value:
    existing_update_value
  | utc_date_value
  | utc_time_value
  | utc_timestamp_value
```

`UTC_DATE`, `UTC_TIME`, and `UTC_TIMESTAMP` are reserved keyword-like current
temporal forms in MyLite's lexer today. The supported grammar accepts both bare
and empty-parenthesized forms, including whitespace before the parentheses.

## Runtime Semantics

Evaluation:

1. Resolve the current statement epoch with the existing current timestamp
   helper: session `timestamp` override first, then statement context time,
   then host time fallback.
2. Convert the epoch to UTC broken-down time without applying the session
   `time_zone` offset.
3. Format `utc_date_value` as `YYYY-MM-DD`.
4. Format `utc_time_value` as `HH:MM:SS`.
5. Format `utc_timestamp_value` as `YYYY-MM-DD HH:MM:SS`.
6. Return text scalar values or bind text DML values. No warnings are emitted
   for supported in-range values.

DML conversion:

- UTC date values are accepted only for `DATE` descriptors.
- UTC time values are accepted only for `TIME` descriptors.
- UTC timestamp values are accepted only for `DATETIME` and `TIMESTAMP`
  descriptors.
- Assigning them to other descriptors fails with deterministic MyLite
  unsupported diagnostics instead of relying on SQLite coercion.
- `NOT NULL` does not need special handling because supported functions never
  produce `NULL`.
- Affected rows follow the existing MySQL-compatible changed-row policy for
  `INSERT`, `REPLACE`, and `UPDATE` slices.

## Diagnostics

Supported in-range expressions return successfully with `warning_count == 0`.
Unsupported or out-of-scope forms fail deterministically through existing
MyLite parser or unsupported-diagnostic paths unless noted otherwise.

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- `UTC_DATE(...)` with arguments: existing parser syntax diagnostics;
- `UTC_TIME(...)` and `UTC_TIMESTAMP(...)` with arguments: existing parser
  syntax diagnostics for this zero-fractional slice, even though MySQL accepts
  fractional precision arguments;
- assigning UTC date to a non-`DATE` descriptor:
  `UTC_DATE values are supported only for DATE columns`;
- assigning UTC time to a non-`TIME` descriptor:
  `UTC_TIME values are supported only for TIME columns`;
- assigning UTC timestamp to a non-`DATETIME`/`TIMESTAMP` descriptor:
  `UTC_TIMESTAMP values are supported only for DATETIME and TIMESTAMP columns`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics;
- public API misuse: no public API changes.

## Performance And Storage

The scalar path formats one stack buffer per evaluated scalar cell. DML
assignment converts each statement-level function to a bound canonical text
value through the same plan/bind machinery as existing row values. MyLite does
not scan or materialize extra rows for these functions, does not add indexes,
and does not invoke SQLite expression evaluation for current UTC values.

## Tests

Add MySQL-runtime expectation coverage and fast C runtime tests for:

- scalar no-source and `FROM DUAL` projection with bare and parenthesized forms;
- the contrast between session-local current functions and UTC functions under
  a nonzero `time_zone` offset;
- statement timestamp override and statement-stable results;
- `DO` execution, `ROW_COUNT()`, and `@@warning_count`;
- `INSERT`, `INSERT ... SET`, `REPLACE`, `REPLACE ... SET`, and `UPDATE`
  assignment to compatible `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`
  descriptors;
- row-scalar single-table projection;
- no-op and changed-row affected counts for updates;
- close/reopen persistence, independent handle timestamp/time-zone state, and
  `.mylite` preamble preservation;
- deterministic diagnostics for incompatible DML targets and unsupported
  argument/fractional forms;
- parser/lexer coverage for the new admitted forms.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-temporal.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-table-dml.md` only for the exact supported limited
subset. Do not claim fractional precision, general temporal expression support,
defaults, generated columns, predicates, ordering expressions, time-zone table
behavior, or protocol metadata.

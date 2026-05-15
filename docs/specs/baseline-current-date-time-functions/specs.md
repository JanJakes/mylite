# Baseline Current Date And Time Functions

## Status

This feature adds the next narrow statement-time temporal function slice:
`CURDATE()`, `CURRENT_DATE`, `CURRENT_DATE()`, `CURTIME()`, `CURRENT_TIME`,
and `CURRENT_TIME()` in the same fixed-UTC, zero-fractional envelope already
used by `NOW()` and `CURRENT_TIMESTAMP`.

The slice is intentionally small. It supports scalar projection, `DO`, and
direct DML assignment to compatible `DATE` and `TIME` descriptors. It does not
add fractional precision, mutable time zones, broader temporal coercion,
date/time expression defaults, temporal arithmetic, generated columns, or
arbitrary expression evaluation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Current timestamp defaults:
  `docs/specs/baseline-current-timestamp-defaults/specs.md`
- Baseline `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` descriptors under
  `docs/specs/`
- Official MySQL 8.4 Reference Manual, date and time functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- Official MySQL 8.4 Reference Manual, `timestamp` system variable:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_current_date_time_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase. Probes set
`time_zone = '+00:00'` and `timestamp` to deterministic Unix epoch seconds so
MyLite's current fixed-UTC statement-time model can compare values exactly.

- `CURDATE()`, `CURRENT_DATE`, and `CURRENT_DATE()` return the statement date
  in `YYYY-MM-DD` form.
- `CURTIME()`, `CURRENT_TIME`, and `CURRENT_TIME()` return the statement time
  in `HH:MM:SS` form for the zero-fractional slice.
- All listed current date/time functions are statement-stable. They observe
  the same session `timestamp` override as `NOW()`.
- `DO CURDATE(), CURRENT_DATE, CURTIME(), CURRENT_TIME` succeeds, returns no
  rows, reports affected rows `0`, and emits no warnings.
- `INSERT` and single-table `UPDATE` can directly assign these functions to
  compatible `DATE` and `TIME` columns. A changed update reports changed rows,
  not matched rows, and emits no warnings for the supported in-range forms.
- MySQL accepts wider behavior, including `CURTIME(fsp)`,
  `CURRENT_TIME(fsp)`, function-backed date/time expression defaults, temporal
  arithmetic such as `CURRENT_DATE + INTERVAL 30 DAY`, and coercions such as
  assigning `CURDATE()` to an integer or datetime target. Those behaviors are
  outside this slice.

## Scope

Supported:

- no-source `SELECT`, `SELECT ... FROM DUAL`, `DO`, and row-scalar session
  value projection for:
  - `CURDATE()`;
  - `CURRENT_DATE`;
  - `CURRENT_DATE()`;
  - `CURTIME()`;
  - `CURRENT_TIME`;
  - `CURRENT_TIME()`;
- scalar values use the statement timestamp already captured by statement
  context, or the current session `timestamp` override when set;
- formatting uses UTC, matching the existing MyLite `TIMESTAMP` baseline:
  `YYYY-MM-DD` for current-date values and `HH:MM:SS` for current-time values;
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` direct assignment of current
  date values to `DATE` columns and current time values to `TIME` columns;
- descriptor-backed generated SQLite SQL remains built from stable physical
  table and column names with bound text values;
- successful supported statements produce `warning_count == 0` and follow the
  existing public result API conventions for scalar, `DO`, and DML statements;
- current date/time functions do not mutate catalog descriptors, catalog
  generation, SQLite schema generation, or the `.mylite` preamble;
- reopen persistence and independent file-backed handles preserve updated row
  state through the existing storage layer.

Deferred:

- fractional forms such as `CURTIME(1)` and `CURRENT_TIME(6)`;
- `CURDATE` / `CURRENT_DATE` argument forms;
- `UTC_DATE()`, `UTC_TIME()`, `UTC_TIMESTAMP()`, `SYSDATE()`,
  `FROM_UNIXTIME()`, `UNIX_TIMESTAMP()`, `DATE()`, and general temporal
  extraction/conversion functions;
- `DEFAULT (CURDATE())`, `DEFAULT (CURTIME())`, nonparenthesized current date
  or time defaults, and other date/time expression defaults;
- assigning current date/time functions to `DATETIME`, `TIMESTAMP`, integer,
  string, `YEAR`, or other descriptor families;
- temporal arithmetic, intervals, casts, predicates, ordering expressions,
  generated columns, triggers, cascades, privilege semantics, and protocol-grade
  temporal metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns misuse behavior, result
  ownership, and cleanup.
- Statement context: owns the statement-time snapshot. This feature reads the
  same snapshot used by `NOW()` / `CURRENT_TIMESTAMP`.
- Lexer/parser/AST: admits only the listed zero-fractional current date/time
  forms in expression and DML value positions. The parser records syntax only;
  it does not resolve descriptors or format temporal values.
- Analyzer/planner/runtime: resolves DML targets through MyLite catalog
  descriptors and converts admitted functions into canonical text values before
  binding generated SQLite statements.
- Catalog: unchanged. Descriptors remain the authority for logical type,
  nullability, defaults, column order, and table identity.
- Result builder: returns scalar date/time text through existing public result
  cells and keeps default labels from the original expression span unless an
  alias is present.
- SQLite physical storage: stores current date/time assignment results as
  canonical text in MyLite-generated tables. It does not rely on SQLite date
  functions, defaults, triggers, or optional syntax.
- Storage/VFS: unchanged. This feature writes only inside the shifted SQLite
  payload and must not touch the MyLite preamble.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippets:

```lemon
current_date_value:
    CURRENT_DATE
  | CURRENT_DATE LPAREN RPAREN
  | CURDATE LPAREN RPAREN

current_time_value:
    CURRENT_TIME
  | CURRENT_TIME LPAREN RPAREN
  | CURTIME LPAREN RPAREN

expression:
    existing_expression
  | current_date_value
  | current_time_value

insert_value:
    existing_insert_value
  | current_date_value
  | current_time_value

update_value:
    existing_update_value
  | current_date_value
  | current_time_value
```

`CURRENT_DATE` and `CURRENT_TIME` are reserved keyword forms. `CURDATE` and
`CURTIME` are ordinary built-in function names in MySQL's function-name parsing
model. MyLite should keep them usable as identifiers where the existing
identifier grammar permits affected function names, while requiring no
intervening whitespace before `(` in default SQL mode. Existing `IGNORE_SPACE`
mode permits whitespace before `(`.

## Runtime Semantics

Evaluation:

1. Resolve the current statement epoch with the existing current timestamp
   helper: session `timestamp` override first, then statement context time,
   then host time fallback.
2. Convert the epoch to UTC broken-down time.
3. Format `current_date_value` as `YYYY-MM-DD`.
4. Format `current_time_value` as `HH:MM:SS`.
5. Return text scalar values or bind text DML values. No warnings are emitted
   for supported in-range values.

DML conversion:

- current date values are accepted only for `DATE` descriptors;
- current time values are accepted only for `TIME` descriptors;
- assigning them to other descriptors fails with deterministic MyLite
  unsupported diagnostics instead of relying on SQLite coercion;
- `NOT NULL` does not need special handling because supported functions never
  produce `NULL`;
- affected rows follow the existing MySQL-compatible changed-row policy for
  `INSERT`, `REPLACE`, and `UPDATE` slices.

## Diagnostics

Supported in-range expressions return successfully with `warning_count == 0`.
Unsupported or out-of-scope forms fail deterministically through existing
MyLite parser or unsupported-diagnostic paths unless noted otherwise.

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- unsupported fractional/argument forms: existing parser syntax diagnostics for
  forms not admitted by grammar;
- assigning current date to a non-`DATE` descriptor:
  `CURRENT_DATE values are supported only for DATE columns`;
- assigning current time to a non-`TIME` descriptor:
  `CURRENT_TIME values are supported only for TIME columns`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics;
- public API misuse: no public API changes.

## Performance And Storage

The scalar path formats a single stack buffer per evaluated scalar cell. DML
assignment converts each statement-level function to a bound canonical text
value through the same plan/bind machinery as existing row values. MyLite does
not scan or materialize extra rows for these functions, does not add indexes,
and does not invoke SQLite expression evaluation for current date/time.

## Tests

Coverage for this feature must include:

- MySQL 8.4.9 expectation script for scalar functions, `DO`, DML assignment,
  row counts, warning counts, and intentionally deferred forms;
- parser coverage for every admitted grammar form and representative rejected
  fractional/argument forms;
- runtime scalar `SELECT` / `FROM DUAL` / `DO` behavior under deterministic
  `SET timestamp` values;
- `INSERT`, `REPLACE`, and single-table `UPDATE` assignment to `DATE` and
  `TIME` columns;
- changed-row and no-op affected-row counts;
- deterministic unsupported diagnostics for incompatible descriptor targets;
- reopen persistence, independent handles, and `.mylite` preamble preservation;
- existing current timestamp, date, time, DML, parser, statement-context,
  storage, VFS, and compatibility tests.

## Compatibility Docs

Update `COMPATIBILITY.md`, `docs/compatibility/functions-temporal.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/sql-table-dml.md`, and
`docs/compatibility/type-system-literals-conversion.md` with limited wording.
Do not claim fractional precision, date/time expression defaults, temporal
arithmetic, coercions outside direct `DATE`/`TIME` targets, mutable time zones,
or general expression support.

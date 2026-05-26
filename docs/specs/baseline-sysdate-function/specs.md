# Baseline SYSDATE Function

## Status

This feature adds a narrow zero-fractional `SYSDATE()` slice. It complements
the existing statement-time `NOW()` / `CURRENT_TIMESTAMP` support with an
execution-time timestamp that ignores the session `timestamp` override.

The slice is intentionally small. It supports scalar projection, `DO`,
single-table row-scalar projection, and direct DML assignment to compatible
`DATETIME` and `TIMESTAMP` descriptors. It does not add fractional precision,
defaults, string/date/time coercion targets, temporal arithmetic, predicates,
or general expression evaluation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Current timestamp defaults:
  `docs/specs/baseline-current-timestamp-defaults/specs.md`
- Current date/time functions:
  `docs/specs/baseline-current-date-time-functions/specs.md`
- UTC date/time functions:
  `docs/specs/baseline-utc-date-time-functions/specs.md`
- Official MySQL 8.4 Reference Manual, date and time functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_sysdate_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase.

- `SYSDATE()` returns a `YYYY-MM-DD HH:MM:SS` timestamp for the time at which
  the function executes in the current session time zone.
- `SYSDATE()` is not statement-stable in the same way as `NOW()`, and it
  ignores `SET timestamp`. With `timestamp = 1700000000`, MySQL returns
  `NOW() = '2023-11-14 22:13:20'`, while `SYSDATE()` returns the runtime wall
  clock.
- `SYSDATE()` observes `time_zone`. Under `time_zone = '+02:00'`, the returned
  value is two hours ahead of the UTC wall clock.
- MySQL accepts `SYSDATE(fsp)` for precision values `0..6` and rejects larger
  precision with `1426 / 42000`.
- `SYSDATE()` is a function call, not a bare current timestamp keyword.
  `SELECT SYSDATE` resolves as an identifier and fails when no such column is
  in scope.
- In default SQL mode, whitespace between `SYSDATE` and `(` resolves through
  function-name parsing and fails as a missing stored function, including
  argument-bearing calls such as `SYSDATE (1)`. With `IGNORE_SPACE`,
  `SYSDATE ()` is accepted and `SYSDATE (1)` reaches the ordinary native
  function argument-count diagnostic.
- `DO SYSDATE()` succeeds, reports `ROW_COUNT() == 0`, and emits no warnings.
- MySQL accepts direct assignment into wider target families, including
  `DATE`, `TIME`, and nonbinary string columns. MyLite defers those conversions
  until the corresponding warning and coercion behavior is specified for this
  compatibility layer.

## Scope

Supported:

- no-source `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
  row-scalar projection of `SYSDATE()`;
- `SYSDATE ()` when `IGNORE_SPACE` is active;
- scalar values use host execution time, not statement context time and not the
  session `timestamp` override;
- formatting applies the current limited session `time_zone` offset and emits
  `YYYY-MM-DD HH:MM:SS`;
- direct `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignment to `DATETIME` and
  `TIMESTAMP` descriptors;
- descriptor-backed generated SQLite SQL remains built from stable physical
  table and column names with bound text values;
- supported successful statements produce `warning_count == 0` and follow the
  existing public result API conventions for scalar, `DO`, and DML statements;
- `SYSDATE()` does not mutate catalog descriptors, catalog generation, SQLite
  schema generation, statement context, or the `.mylite` preamble;
- reopen persistence and independent file-backed handles preserve row state
  through the existing storage layer.

Deferred:

- `SYSDATE(fsp)` and fractional seconds;
- bare `SYSDATE` keyword behavior beyond ordinary identifier resolution;
- `DEFAULT SYSDATE()` / `DEFAULT (SYSDATE())`, `ON UPDATE SYSDATE()`, generated
  columns, check constraints, predicates, ordering, grouping, joins, scalar
  subqueries, prepared-statement parameters, or arbitrary expression trees
  beyond the currently admitted scalar and DML value positions;
- direct assignment to `DATE`, `TIME`, string, binary string, numeric, `YEAR`,
  `BIT`, `ENUM`, `SET`, `JSON`, or spatial descriptor families;
- statement-level non-determinism observable through `SLEEP()` or other
  long-running expression evaluation, because those expression forms are
  outside the current MyLite slice;
- named time-zone tables, daylight-saving rules, fractional seconds, temporal
  arithmetic, casts, triggers, cascades, privilege semantics, and protocol-grade
  temporal metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns misuse behavior, result
  ownership, diagnostics access, and cleanup.
- Statement context: unchanged. `SYSDATE()` deliberately does not read the
  statement-time snapshot or session `timestamp` override.
- Lexer/parser/AST: admits only `SYSDATE()` as an ordinary no-argument function
  in existing expression and DML value positions. The parser records syntax and
  source spans only.
- Analyzer/planner/runtime: resolves DML targets through MyLite catalog
  descriptors and converts admitted `SYSDATE()` values into canonical text
  before binding generated SQLite statements.
- Catalog: unchanged. Descriptors remain the authority for logical type,
  nullability, defaults, column order, and table identity.
- Result builder: returns scalar timestamp text through existing public result
  cells and keeps default labels from the original expression span unless an
  alias is present.
- SQLite physical storage: stores `SYSDATE()` assignment results as canonical
  text in MyLite-generated tables. It does not rely on SQLite date functions,
  defaults, triggers, or optional syntax.
- Storage/VFS: unchanged. This feature writes only inside the shifted SQLite
  payload and must not touch the MyLite preamble.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippets:

```lemon
sysdate_value:
    SYSDATE LPAREN RPAREN

expression:
    existing_expression
  | sysdate_value

insert_value:
    existing_insert_value
  | sysdate_value

update_value:
    existing_update_value
  | sysdate_value
```

`SYSDATE` remains an ordinary nonreserved function name. MyLite requires no
intervening whitespace before `(` in default SQL mode. Existing `IGNORE_SPACE`
mode permits whitespace before `(`.

## Runtime Semantics

Evaluation:

1. Read the host execution-time epoch with the C runtime clock.
2. Ignore `database->session.has_timestamp_override` and
   `database->session.active_statement_time`.
3. Apply the current limited session `time_zone` offset.
4. Format the adjusted time as `YYYY-MM-DD HH:MM:SS`.
5. Return text scalar values or bind text DML values. No warnings are emitted
   for supported in-range values.

DML conversion:

- `SYSDATE()` values are accepted only for `DATETIME` and `TIMESTAMP`
  descriptors in this slice.
- Assignment to other descriptor families fails with deterministic MyLite
  unsupported diagnostics instead of relying on SQLite coercion.
- `NOT NULL` does not need special handling because supported `SYSDATE()`
  evaluations never produce `NULL`.
- Affected rows follow the existing MySQL-compatible changed-row policy for
  `INSERT`, `REPLACE`, and `UPDATE` slices.

## Diagnostics

Supported in-range expressions return successfully with `warning_count == 0`.
Unsupported or out-of-scope forms fail deterministically through existing
MyLite parser or unsupported-diagnostic paths unless noted otherwise.

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- `SYSDATE(...)` with arguments: existing native-function parameter-count
  diagnostic for `SYSDATE`;
- assigning `SYSDATE()` to a non-`DATETIME`/`TIMESTAMP` descriptor:
  `SYSDATE values are supported only for DATETIME and TIMESTAMP columns`;
- `SYSDATE (...)` in default SQL mode: existing parser syntax diagnostic
  through MyLite's function-name parsing policy;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics;
- public API misuse: no public API changes.

## Performance And Storage

The scalar path formats one stack buffer per evaluated scalar cell and performs
no table scans unless the surrounding query already scans a descriptor table.
DML assignment converts `SYSDATE()` to a bound canonical text value through the
same plan/bind machinery as existing row values. MyLite does not invoke SQLite
date functions and does not add SQLite fork patches for this slice.

## Tests

Add MySQL-runtime expectation coverage and fast C runtime tests for:

- scalar no-source and `FROM DUAL` projection;
- contrast between `NOW()` and `SYSDATE()` under a deterministic
  `SET timestamp`;
- session `time_zone` application;
- `IGNORE_SPACE` handling and default-mode whitespace rejection;
- `DO` execution, `ROW_COUNT()`, and `@@warning_count`;
- `INSERT`, `INSERT ... SET`, `REPLACE`, `REPLACE ... SET`, and `UPDATE`
  assignment to compatible `DATETIME` and `TIMESTAMP` descriptors;
- row-scalar single-table projection;
- changed-row affected counts for updates, avoiding time-sensitive no-op
  assertions for an execution-time function;
- close/reopen persistence, independent handle timestamp/time-zone state, and
  `.mylite` preamble preservation;
- deterministic diagnostics for incompatible DML targets and unsupported
  argument/fractional forms;
- parser/lexer coverage for the admitted forms.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-temporal.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-table-dml.md` only for the exact supported limited
subset. Do not claim fractional precision, defaults, general temporal
expression support, broader coercions, named time zones, daylight-saving
behavior, or protocol-grade temporal metadata.

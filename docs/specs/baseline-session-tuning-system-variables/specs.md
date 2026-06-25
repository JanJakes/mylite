# Baseline Session Tuning System Variables

## Summary

This slice exposes a MySQL 8.4.9-shaped baseline for four commonly inspected
or initialized system variables:

- `lock_wait_timeout`
- `low_priority_updates`
- `slow_launch_time`
- `sort_buffer_size`

MyLite supports SQL-visible readback, `SHOW VARIABLES`, scope diagnostics,
session-local assignment where MySQL has session scope, and fixed global no-op
assignments for default-compatible values. MyLite does not implement actual
metadata-lock wait timing, low-priority DML scheduling, thread launch
measurement, sort-buffer allocation, mutable shared global state, persisted
variables, startup options, privileges, or Performance Schema variable tables.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_session_tuning_system_variables_expectations.sh`.

## MySQL 8.4.9 Observations

With the comparison server reset to defaults, MySQL reports:

| Variable | Scalar value | `SHOW VARIABLES` value | Scope |
| --- | --- | --- | --- |
| `lock_wait_timeout` | `31536000` | `31536000` | Global, session |
| `low_priority_updates` | `0` | `OFF` | Global, session |
| `slow_launch_time` | `2` | `2` | Global only |
| `sort_buffer_size` | `262144` | `262144` | Global, session |

`SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` include
all four rows. `@@SESSION.slow_launch_time` and `@@LOCAL.slow_launch_time`
return `1238 / HY000` because the variable is global-only. Non-global
`SET slow_launch_time = ...` returns `1229 / HY000`.

`lock_wait_timeout` accepts integer, `TRUE`, and `FALSE` session assignments,
clamps below `1` to `1`, clamps above `31536000` to `31536000`, and emits
warning `1292` for clamped values. String and `ON` assignments return
`1232 / 42000`.

`low_priority_updates` accepts boolean tokens, `0`/`1`, string `ON`/`OFF`, and
integer user-variable values. Unsupported values such as `2`, `-1`, `NULL`, or
string `2` return `1231 / 42000`.

`sort_buffer_size` accepts integer session assignments, clamps values below
`32768` to `32768` with warning `1292`, accepts `TRUE` and `FALSE` as `1` and
`0` before clamping, and rejects strings, `NULL`, and `ON` with `1232 / 42000`.

MySQL supports mutable global assignments for these variables. MyLite
intentionally does not mutate process-global state; it accepts only
default-compatible global no-ops and returns deterministic unsupported
diagnostics for state-changing global assignments.

## MyLite Scope

MyLite supports:

- scalar reads for default, `GLOBAL`, `SESSION`, and `LOCAL` scopes where MySQL
  allows them;
- MySQL-shaped `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
  `SHOW SESSION VARIABLES` rows;
- global-only scalar and SET diagnostics for `slow_launch_time`;
- session/local/default assignment, clamping warnings, and integer user
  variables for `lock_wait_timeout`;
- session/local/default assignment and integer/string user variables for
  `low_priority_updates`;
- session/local/default assignment, clamping warnings, and integer user
  variables for `sort_buffer_size`;
- exact default-compatible `SET GLOBAL` no-ops for all four variables.

MyLite intentionally does not support:

- actual metadata-lock wait timeouts driven by `lock_wait_timeout`;
- changed DML scheduling from `low_priority_updates`;
- slow-launch thread accounting;
- changed sorting memory allocation from `sort_buffer_size`;
- mutable shared global state, persisted variables, startup options, privilege
  checks, `SET_VAR` effects, or Performance Schema variable tables.

## Syntax

No new grammar is required. Existing scalar system-variable, `SHOW VARIABLES`,
and `SET` productions admit the supported forms:

```sql
SELECT @@lock_wait_timeout, @@SESSION.sort_buffer_size;
SHOW VARIABLES LIKE 'low_priority_updates';
SET lock_wait_timeout = 10;
SET LOCAL sort_buffer_size = 65536;
SET GLOBAL slow_launch_time = DEFAULT;
```

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- `@@SESSION.slow_launch_time` and `@@LOCAL.slow_launch_time` use
  `1238 / HY000`.
- Non-global `SET slow_launch_time` uses `1229 / HY000`.
- Invalid value-domain assignments to `low_priority_updates` use
  `1231 / 42000`.
- Invalid argument types for the integer variables use `1232 / 42000`.
- Clamped integer assignments use warning `1292`.
- State-changing global assignments that MyLite cannot honor use MyLite's
  deterministic unsupported diagnostic.

## Runtime And Storage

Session values live in handle-owned session state and participate in atomic
multi-assignment rollback. This slice does not add public ABI, SQLite SQL,
SQLite extension API use, SQLite fork hooks, catalog rows, file-format state,
or physical execution side effects.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scopes,
  diagnostics, clamping, and user-variable assignment behavior;
- a runtime C regression test for scalar values, `SHOW` rows, diagnostics,
  session mutation, global no-op behavior, warnings, and rollback;
- full `SHOW VARIABLES` registry regression coverage.

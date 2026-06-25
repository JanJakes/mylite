# Baseline Network Timeout System Variables

## Summary

This slice exposes a MySQL 8.4.9-shaped baseline for network timeout system
variables commonly inspected by applications and compatibility test suites:

- `connect_timeout`
- `net_read_timeout`
- `net_retry_count`
- `net_write_timeout`

MyLite supports SQL-visible readback, `SHOW VARIABLES`, scope diagnostics,
handle-local session assignment for the session-scoped `net_*` variables, and
fixed global no-op assignments for default-compatible values. MyLite does not
implement actual client connection, read, write, retry, socket, protocol,
startup-option, persisted-variable, privilege, or Performance Schema effects.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_network_timeout_system_variables_expectations.sh`.

## MySQL 8.4.9 Observations

With the comparison server reset to defaults, MySQL reports:

| Variable | Scalar value | `SHOW VARIABLES` value | Scope | Range |
| --- | --- | --- | --- | --- |
| `connect_timeout` | `10` | `10` | Global only | `2..31536000` |
| `net_read_timeout` | `30` | `30` | Global, session | `1..31536000` |
| `net_retry_count` | `10` | `10` | Global, session | `1..UINT64_MAX` |
| `net_write_timeout` | `60` | `60` | Global, session | `1..31536000` |

`SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` include
all four rows. `@@SESSION.connect_timeout` and `@@LOCAL.connect_timeout`
return `1238 / HY000` because the variable is global-only. Non-global
`SET connect_timeout = ...` returns `1229 / HY000`.

`net_read_timeout` and `net_write_timeout` accept integer, `TRUE`, and `FALSE`
session assignments, clamp below `1` to `1`, clamp above `31536000` to
`31536000`, and emit warning `1292` for clamped values. String, decimal,
`NULL`, `ON`, and `OFF` assignments return `1232 / 42000`.

`net_retry_count` uses the same integer and boolean session assignment rules,
but its 64-bit maximum is `18446744073709551615`. Literals above that maximum
return `1232 / 42000` rather than clamping, because they do not parse as a valid
unsigned integer value for the variable.

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
- global-only scalar and SET diagnostics for `connect_timeout`;
- session/local/default assignment, clamping warnings, boolean conversion, and
  integer user-variable assignment for `net_read_timeout` and
  `net_write_timeout`;
- session/local/default assignment, boolean conversion, integer user-variable
  assignment, and full unsigned-64-bit maximum handling for `net_retry_count`;
- exact default-compatible `SET GLOBAL` no-ops for all four variables.

MyLite intentionally does not support:

- changed connection handshake timeout behavior from `connect_timeout`;
- changed socket read/write timeout behavior from `net_read_timeout` or
  `net_write_timeout`;
- changed interrupted-port retry behavior from `net_retry_count`;
- mutable shared global state, persisted variables, startup options, privilege
  checks, `SET_VAR` effects, or Performance Schema variable tables.

## Syntax

No new grammar is required. Existing scalar system-variable, `SHOW VARIABLES`,
and `SET` productions admit the supported forms:

```sql
SELECT @@connect_timeout, @@net_read_timeout;
SHOW VARIABLES LIKE 'net_write_timeout';
SET SESSION net_read_timeout = 5;
SET LOCAL net_write_timeout = 6;
SET @@SESSION.net_retry_count = 7;
SET GLOBAL connect_timeout = DEFAULT;
```

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- `@@SESSION.connect_timeout` and `@@LOCAL.connect_timeout` use
  `1238 / HY000`.
- Non-global `SET connect_timeout` uses `1229 / HY000`.
- Invalid argument types for the integer variables use `1232 / 42000`.
- Clamped integer assignments use warning `1292`.
- State-changing global assignments that MyLite cannot honor use MyLite's
  deterministic unsupported diagnostic.

## Runtime And Storage

Session values live in handle-owned session state and participate in atomic
multi-assignment rollback. This slice does not add public ABI, SQLite SQL,
SQLite extension API use, SQLite fork hooks, catalog rows, file-format state,
or physical network execution side effects.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scopes,
  diagnostics, clamping, maximum handling, global MySQL behavior, and
  user-variable assignment behavior;
- a runtime C regression test for scalar values, `SHOW` rows, diagnostics,
  session mutation, global no-op behavior, warnings, maximum handling, and
  rollback;
- full `SHOW VARIABLES` registry regression coverage.

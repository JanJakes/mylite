# Baseline Internal Session System Variables

## Summary

This slice exposes an embedded-compatible baseline for MySQL internal/session
system variables that applications can inspect or set during ordinary session
setup:

- `original_commit_timestamp`
- `original_server_version`
- `proxy_user`
- `pseudo_replica_mode`
- `pseudo_slave_mode`
- `rbr_exec_mode`
- `transaction_allow_batching`

MyLite supports scalar reads, `SHOW VARIABLES` / `SHOW SESSION VARIABLES` /
`SHOW GLOBAL VARIABLES` rows with MySQL 8.4.9 scope, session-local assignments
for mutable variables, MySQL-style global-scope diagnostics, read-only
`proxy_user` diagnostics, and the deprecation warning for `pseudo_slave_mode`.
MyLite does not implement replication applier side effects, row-based
replication execution semantics, proxy-user authentication state, transaction
batching behavior, binary-log commit timestamp metadata, startup option
handling, persisted variables, privileges, or Performance Schema variable
tables.

`pseudo_thread_id`, `statement_id`, `rand_seed1`, `rand_seed2`, `profiling`,
`profiling_history_size`, and `terminology_use_previous` are intentionally
excluded. They require live per-session counters, RAND state, or mutable global
state and should be implemented in separate slices rather than hidden behind
fixed placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_internal_session_system_variables_expectations.sh`.

Runtime probes establish the exact defaults, scope, warning, and SET
diagnostics for this target-runtime slice.

## MySQL 8.4.9 Observations

The target runtime reports these defaults:

| Variable | Scalar value | `SHOW VARIABLES` value | `SHOW GLOBAL VARIABLES` |
| --- | --- | --- | --- |
| `original_commit_timestamp` | `36028797018963968` | `36028797018963968` | absent |
| `original_server_version` | `999999` | `999999` | absent |
| `proxy_user` | `NULL` | empty string | absent |
| `pseudo_replica_mode` | `0` | `OFF` | absent |
| `pseudo_slave_mode` | `0` plus warning `1287` on scalar read | `OFF` | absent |
| `rbr_exec_mode` | `STRICT` | `STRICT` | `STRICT` |
| `transaction_allow_batching` | `0` | `OFF` | absent |

`@@GLOBAL.rbr_exec_mode` is readable, but `SET GLOBAL rbr_exec_mode = DEFAULT`
fails with `1228 / HY000` because the variable is treated as session-settable.
The other variables in this slice fail global scalar reads with
`1238 / HY000`. `SET GLOBAL proxy_user = DEFAULT` fails with the read-only
`1238 / HY000` diagnostic, while the remaining mutable session variables fail
`SET GLOBAL` with `1228 / HY000`.

`proxy_user` is read-only. The numeric variables accept `DEFAULT` and ordinary
integer session assignments. The boolean variables accept MySQL boolean SET
forms and show `ON` / `OFF`. `rbr_exec_mode` accepts `STRICT`, `IDEMPOTENT`,
and `DEFAULT`, storing the canonical uppercase value. `pseudo_slave_mode`
emits deprecation warning `1287` on scalar reads and successful assignments.

## MyLite Scope

MyLite supports:

- scalar default, session, and local reads for all variables in this slice;
- `@@GLOBAL.rbr_exec_mode` reads;
- MySQL-style global scalar and `SET GLOBAL` diagnostics for session-only
  variables;
- `SHOW VARIABLES` and `SHOW SESSION VARIABLES` rows for all variables;
- `SHOW GLOBAL VARIABLES` row for `rbr_exec_mode` only;
- session-local `SET` for `original_commit_timestamp`,
  `original_server_version`, `pseudo_replica_mode`, `pseudo_slave_mode`,
  `rbr_exec_mode`, and `transaction_allow_batching`;
- `proxy_user` read-only assignment diagnostics;
- `pseudo_slave_mode` deprecation warnings.

MyLite intentionally does not support:

- replication applier mode changes or row-based replication conflict handling;
- transaction batching behavior;
- proxy authentication state or active proxied-user identity;
- commit timestamp or original-server-version metadata propagation;
- mutable global values, privileges, startup options, persisted variables, or
  Performance Schema variable tables;
- dynamic session counters or RAND state excluded from this slice.

## Syntax

No new grammar is required. Existing MyLite productions already admit the
required forms:

```lemon
expr ::= SYSTEM_VARIABLE.
set_statement ::= SET set_assignment_list.
set_assignment ::= set_system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

## Runtime Design

The variables are descriptor-backed system variables. `proxy_user` uses the
same scalar-NULL / SHOW-empty display path as existing read-only identity
placeholders. Mutable variables use session-local overrides; global reads stay
fixed where MySQL exposes them. `rbr_exec_mode` has a dedicated validator
because generic session placeholders would otherwise accept arbitrary text.
The boolean variables reuse the existing MySQL-compatible boolean SET parser.

SQLite changes are not required. The implementation is entirely in MyLite's
runtime wrapper/translation layer and does not need a targeted SQLite fork hook.

## Tests

The MySQL expectation script verifies against MySQL 8.4.9:

- scalar defaults and `proxy_user` NULL behavior;
- session and global `SHOW VARIABLES` rows;
- global scalar and `SET GLOBAL` diagnostics;
- session-local mutation and reset;
- `proxy_user` read-only diagnostics for session and global assignment forms;
- `pseudo_slave_mode` deprecation warnings;
- invalid boolean, enum, and numeric assignment diagnostics.

The runtime test mirrors those expectations in MyLite and covers user-variable
SET assignment forms for the enum and boolean paths.

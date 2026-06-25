# Baseline Session Tracking System Variables

## Summary

This slice exposes a fixed embedded-compatible baseline for a group of MySQL
system variables that applications commonly inspect around result metadata,
session-state tracking, secondary-engine behavior, and DDL output options:

- `default_collation_for_utf8mb4`
- `end_markers_in_json`
- `keep_files_on_create`
- `old_alter_table`
- `print_identified_with_as_hex`
- `require_row_format`
- `resultset_metadata`
- `select_into_disk_sync`
- `session_track_gtids`
- `session_track_schema`
- `session_track_state_change`
- `session_track_transaction_info`
- `show_create_table_skip_secondary_engine`
- `show_create_table_verbosity`
- `use_secondary_engine`

MyLite supports scalar reads, `SHOW VARIABLES` / `SHOW GLOBAL VARIABLES`
visibility, session-local placeholder assignment/readback, and fixed global
no-op validation where the variable has a global scope. It does not implement
protocol session-state packets, HeatWave/secondary-engine routing, JSON output
format changes, server-global mutable state, persisted variables, or option-file
state.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_session_tracking_system_variables_expectations.sh`.

The official `SHOW VARIABLES` documentation defines default `SESSION`
visibility and explains that `GLOBAL` omits variables without a global value.
The runtime probes establish the target build's actual defaults and scope
diagnostics for this specific group.

## MySQL 8.4.9 Observations

The target runtime reports these default scalar values:

| Variable | Default/session scalar | Global scalar |
| --- | --- | --- |
| `default_collation_for_utf8mb4` | `utf8mb4_0900_ai_ci` | `utf8mb4_0900_ai_ci` |
| `end_markers_in_json` | `0` | `0` |
| `keep_files_on_create` | `0` | `0` |
| `old_alter_table` | `0` | `0` |
| `print_identified_with_as_hex` | `0` | `0` |
| `require_row_format` | `0` | session-only error |
| `resultset_metadata` | `FULL` | session-only error |
| `select_into_disk_sync` | `0` | `0` |
| `session_track_gtids` | `OFF` | `OFF` |
| `session_track_schema` | `1` | `1` |
| `session_track_state_change` | `0` | `0` |
| `session_track_transaction_info` | `OFF` | `OFF` |
| `show_create_table_skip_secondary_engine` | `0` | session-only error |
| `show_create_table_verbosity` | `0` | `0` |
| `use_secondary_engine` | `ON` | session-only error |

`SHOW VARIABLES` and `SHOW SESSION VARIABLES` expose all 15 rows. `SHOW GLOBAL
VARIABLES` exposes the 11 variables with a global value and omits
`require_row_format`, `resultset_metadata`,
`show_create_table_skip_secondary_engine`, and `use_secondary_engine`.

Boolean values display as `ON` / `OFF` in `SHOW VARIABLES`, while scalar reads
return `1` / `0`. Text enum values display as their scalar text.

`@@GLOBAL.<session-only-name>` raises `1238 / HY000` with a session-variable
message. `SET GLOBAL <session-only-name> = DEFAULT` raises `1228 / HY000` with
the message that the variable cannot be used with `SET GLOBAL`.

## MyLite Scope

MyLite supports:

- scalar reads for default, `SESSION`, and `LOCAL` scope for all variables;
- scalar `GLOBAL` reads for the 11 global-capable variables;
- MySQL-style scalar and `SET GLOBAL` diagnostics for session-only variables;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows with the target visibility and display values;
- session-local placeholder assignment/readback for these variables through
  the existing system-variable override store;
- global no-op assignment for global-capable placeholders when the value is
  `DEFAULT` or the fixed default value;
- deterministic rejection for global assignments that would mutate shared
  server-global state.

MyLite intentionally does not support:

- actual session-state tracking packets or wire-protocol status changes;
- HeatWave or secondary-engine routing;
- altered `SHOW CREATE TABLE` formatting from these variables;
- JSON output end markers;
- server-global mutable state shared across handles;
- startup options, option files, `SET PERSIST`, `SET_VAR` hints, privileges, or
  Performance Schema variable tables.

## Syntax

No new grammar is required. Existing MyLite productions already admit the
required forms:

```lemon
expr ::= SYSTEM_VARIABLE.
set_statement ::= SET set_assignment_list.
set_assignment ::= set_system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

Examples:

```sql
SELECT @@session_track_schema, @@GLOBAL.session_track_schema;
SELECT @@SESSION.resultset_metadata, @@LOCAL.use_secondary_engine;
SHOW VARIABLES WHERE Variable_name IN ('resultset_metadata','use_secondary_engine');
SHOW GLOBAL VARIABLES LIKE 'session_track_%';
SET resultset_metadata = NONE;
SET SESSION session_track_gtids = OWN_GTID;
SET GLOBAL end_markers_in_json = DEFAULT;
```

## Diagnostics

- Unknown variables continue to use the existing `1193 / HY000` diagnostic.
- Scalar `@@GLOBAL` reads for session-only variables use `1238 / HY000`.
- `SET GLOBAL` for session-only variables uses `1228 / HY000`.
- Fixed global no-op validation admits `DEFAULT` or the fixed global default.
  Other global values are rejected deterministically by the current SET grammar
  or by MyLite's embedded unsupported diagnostic.
- Session assignment conversion reuses the current placeholder validation for
  boolean and text values.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for default values, SHOW visibility, scope
  diagnostics, and assignment behavior;
- a runtime C test for scalar reads, SHOW rows, session overrides, global no-op
  assignments, session-only global diagnostics, and unsupported global
  mutations;
- focused `SHOW VARIABLES` regression coverage through the existing registry
  path.

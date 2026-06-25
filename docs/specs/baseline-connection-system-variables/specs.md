# Baseline Connection System Variables

## Scope

This slice covers MySQL 8.4.9-shaped system-variable metadata and focused
session behavior for:

- `back_log`
- `bind_address`
- `host_cache_size`
- `connection_memory_chunk_size`
- `connection_memory_limit`
- `global_connection_memory_limit`
- `global_connection_memory_tracking`
- `connection_control_failed_connections_threshold`
- `connection_control_max_connection_delay`
- `connection_control_min_connection_delay`

The target behavior is based on the MySQL 8.4 Reference Manual sections for
server system variables and connection-control plugin variables, plus runtime
probes against MySQL 8.4.9. The MySQL runtime expectations are recorded in
`packages/libmylite/tests/mysql_baseline_connection_system_variables_expectations.sh`.

## MyLite behavior

MyLite exposes MySQL-shaped scalar reads and `SHOW VARIABLES` rows for the
listed variables. Global-only variables reject explicit `@@SESSION` and
`@@LOCAL` scalar reads with MySQL-style global-variable diagnostics while still
appearing in `SHOW SESSION VARIABLES`, matching MySQL behavior.

`back_log` and `bind_address` are read-only embedded listener placeholders:

- `back_log` reads as `151`.
- `bind_address` reads as `*`.
- assignments return read-only-variable diagnostics.

`host_cache_size`, `global_connection_memory_limit`, and the
`connection_control_*` variables are fixed embedded global placeholders. Exact
default/current global assignments are accepted as no-ops; non-default global
mutation is rejected as unsupported. MyLite does not maintain a process-global
host cache, connection-control delay state, or global connection-memory limiter.

`connection_memory_chunk_size` and `connection_memory_limit` are handle-local
session variables:

- global reads remain fixed at MySQL defaults: `8192` and
  `18446744073709551615`;
- session/local/unscoped reads return the current session value;
- session/local/unscoped `SET` accepts integer literals, `TRUE`, `FALSE`,
  `DEFAULT`, and integer user variables;
- string, decimal, `ON`, `OFF`, and overflow values use MySQL-style argument
  type diagnostics;
- out-of-range values clamp with warning `1292`.

`global_connection_memory_tracking` is a handle-local boolean session
placeholder with fixed global `OFF`. Scalar reads return `0` or `1`; `SHOW`
returns `OFF` or `ON`.

## Known incompatibilities

MyLite intentionally does not implement:

- real TCP listener backlog or bind-address behavior;
- DNS/host-cache storage, flushing, or Performance Schema host-cache side
  effects;
- mutable process-global connection memory limits or tracking;
- connection-control failed-login delay enforcement or status counter resets;
- privilege checks for setting restricted MySQL global/session variables;
- startup-option, persisted-system-variable, or `SET_VAR` hint behavior.

These are documented embedded-design gaps. The accepted no-op global assignment
surface is meant to keep application initialization and introspection code from
failing while avoiding fake mutable server state.

## Verification

Focused verification:

```sh
sh -n packages/libmylite/tests/mysql_baseline_connection_system_variables_expectations.sh
sh packages/libmylite/tests/mysql_baseline_connection_system_variables_expectations.sh
cmake --build --preset dev --target mylite_runtime_connection_system_variables_test
ctest --preset dev -R '^libmylite\.runtime\.connection_system_variables$' --output-on-failure
```

Release verification also runs the normal project check workflow.

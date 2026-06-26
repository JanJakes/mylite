# Baseline Replication Global System Variables

## Scope

This slice exposes MySQL 8.4.9-shaped baseline metadata for global
replication infrastructure variables that applications commonly inspect even
when no replication channel exists:

- `relay_log`
- `relay_log_basename`
- `relay_log_index`
- `relay_log_purge`
- `relay_log_recovery`
- `relay_log_space_limit`
- `replication_optimize_for_static_plugin_config`
- `replication_sender_observe_commit_only`
- `report_host`
- `report_password`
- `report_port`
- `report_user`
- `rpl_read_size`
- `rpl_stop_replica_timeout`
- `rpl_stop_slave_timeout`
- `skip_replica_start`
- `skip_slave_start`
- `source_verify_checksum`
- `sync_master_info`
- `sync_relay_log`
- `sync_relay_log_info`
- `sync_source_info`

The related `replica_*` and `slave_*` applier tuning variables remain a
separate batch because they have a larger alias surface. Session pseudo
replication variables such as `pseudo_thread_id` also remain out of scope.

References:

- <https://dev.mysql.com/doc/refman/8.4/en/replication-options-replica.html>
- <https://dev.mysql.com/doc/refman/8.4/en/replication-options-source.html>
- <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed:

| Variable | Scalar value | SHOW value | SET behavior |
| --- | --- | --- | --- |
| `relay_log` | relay-log basename | same | read-only |
| `relay_log_basename` | relay-log path | same | read-only |
| `relay_log_index` | relay-log index path | same | read-only |
| `relay_log_purge` | `1` | `ON` | `SET GLOBAL ... = DEFAULT` succeeds |
| `relay_log_recovery` | `0` | `OFF` | read-only |
| `relay_log_space_limit` | `0` | `0` | read-only |
| `replication_optimize_for_static_plugin_config` | `0` | `OFF` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replication_sender_observe_commit_only` | `0` | `OFF` | `SET GLOBAL ... = DEFAULT` succeeds |
| `report_host` | `NULL` | empty string | read-only |
| `report_password` | `NULL` | empty string | read-only |
| `report_port` | `3306` | `3306` | read-only |
| `report_user` | `NULL` | empty string | read-only |
| `rpl_read_size` | `8192` | `8192` | `SET GLOBAL ... = DEFAULT` succeeds |
| `rpl_stop_replica_timeout` | `31536000` | `31536000` | `SET GLOBAL ... = DEFAULT` succeeds |
| `rpl_stop_slave_timeout` | `31536000` | `31536000` | default SET succeeds with deprecation warning `1287` |
| `skip_replica_start` | `0` | `OFF` | read-only |
| `skip_slave_start` | `0` | `OFF` | read-only; scalar read warns `1287` |
| `source_verify_checksum` | `0` | `OFF` | `SET GLOBAL ... = DEFAULT` succeeds |
| `sync_master_info` | `10000` | `10000` | default SET succeeds with deprecation warning `1287` |
| `sync_relay_log` | `10000` | `10000` | `SET GLOBAL ... = DEFAULT` succeeds |
| `sync_relay_log_info` | `10000` | `10000` | default SET succeeds with deprecation warning `1287` |
| `sync_source_info` | `10000` | `10000` | `SET GLOBAL ... = DEFAULT` succeeds |

All variables in this slice are global-only for scalar scope purposes.
Unscoped and `@@GLOBAL` reads succeed. `@@SESSION` and `@@LOCAL` reads fail
with `1238 / HY000` and a global-variable diagnostic. `SHOW VARIABLES`,
`SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` include rows for the
variables.

Successful scalar reads of `rpl_stop_slave_timeout`, `skip_slave_start`,
`sync_master_info`, and `sync_relay_log_info` emit MySQL deprecation warning
`1287`. Successful default assignments to `rpl_stop_slave_timeout`,
`sync_master_info`, and `sync_relay_log_info` also emit those warnings.

## MyLite Semantics

MyLite supports:

- scalar reads for default and `GLOBAL` scopes;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  rows;
- MySQL-shaped global-only diagnostics for session/local scalar reads;
- MySQL-shaped read-only diagnostics for read-only variables;
- fixed global no-op assignments for dynamic variables when the assigned value
  is `DEFAULT` or the fixed MyLite placeholder value;
- MySQL-shaped deprecation warnings for the deprecated alias variables covered
  by this slice.

MyLite intentionally does not support:

- relay-log files, relay-log indexes, replication channels, source/replica
  topology, applier/receiver state, checksums, or file syncing;
- mutable shared server-global state or persisted startup option state;
- credential reporting or report-host identity side effects;
- Performance Schema variable tables or privilege checks.

MyLite uses stable embedded placeholders for path-like values:

| Variable | Scalar/SHOW placeholder |
| --- | --- |
| `relay_log` | `mylite-relay-bin` |
| `relay_log_basename` | `/var/lib/mysql/mylite-relay-bin` |
| `relay_log_index` | `/var/lib/mysql/mylite-relay-bin.index` |

`report_host`, `report_password`, and `report_user` return scalar `NULL` and
blank `SHOW VARIABLES` values, matching MySQL's default shape.

## Parser And Runtime Design

No new grammar is required. Existing system-variable expression, `SHOW
VARIABLES`, and `SET` syntax admits the supported forms:

```lemon
scalar_expression ::= system_variable_reference.
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

The implementation adds descriptors, default/readback values, global-only
scope rules, read-only classification, a compact replication-global SET
handler for fixed no-op assignments, deprecation warning plumbing, SHOW value
formatting, and focused tests.

This is pure MyLite runtime logic. It does not require SQLite extension APIs,
SQLite fork hooks, file-format changes, catalog storage, or mutable process
global state.

## Tests

- `packages/libmylite/tests/mysql_baseline_replication_global_system_variables_expectations.sh`
  verifies MySQL 8.4.9 defaults, `SHOW` rows, scalar scope diagnostics,
  read-only diagnostics, fixed default assignment behavior, and deprecation
  warnings.
- `packages/libmylite/tests/runtime_replication_global_system_variables_test.c`
  verifies MyLite scalar values, `SHOW` rows, null-vs-blank handling,
  fixed-global no-op assignment behavior, read-only diagnostics, global-only
  scope diagnostics, and warning behavior.

# Baseline Replica Applier System Variables

## Scope

This slice exposes MySQL 8.4.9-shaped metadata for replica applier tuning
system variables and their deprecated `slave_*` aliases:

- `replica_allow_batching`
- `replica_checkpoint_group`
- `replica_checkpoint_period`
- `replica_compressed_protocol`
- `replica_exec_mode`
- `replica_load_tmpdir`
- `replica_max_allowed_packet`
- `replica_net_timeout`
- `replica_parallel_type`
- `replica_parallel_workers`
- `replica_pending_jobs_size_max`
- `replica_preserve_commit_order`
- `replica_skip_errors`
- `replica_sql_verify_checksum`
- `replica_transaction_retries`
- `replica_type_conversions`
- `slave_allow_batching`
- `slave_checkpoint_group`
- `slave_checkpoint_period`
- `slave_compressed_protocol`
- `slave_exec_mode`
- `slave_load_tmpdir`
- `slave_max_allowed_packet`
- `slave_net_timeout`
- `slave_parallel_type`
- `slave_parallel_workers`
- `slave_pending_jobs_size_max`
- `slave_preserve_commit_order`
- `slave_skip_errors`
- `slave_sql_verify_checksum`
- `slave_transaction_retries`
- `slave_type_conversions`

Out of scope: replication channels, relay-log event application, NDB batching
effects, applier worker lifecycle, `START REPLICA`, replication filtering,
GTID interactions, Performance Schema applier tables, persisted startup state,
and mutable process-global tuning state.

References:

- <https://dev.mysql.com/doc/refman/8.4/en/replication-options-replica.html>
- <https://dev.mysql.com/doc/refman/8.4/en/mysql-cluster-options-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/replication-options-reference.html>
- <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>

## MySQL 8.4.9 Observations

Runtime probes against `mysql:8.4.9` showed these default scalar and `SHOW
VARIABLES` values:

| Variable | Scalar value | SHOW value | SET behavior |
| --- | --- | --- | --- |
| `replica_allow_batching` | `1` | `ON` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_checkpoint_group` | `512` | `512` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_checkpoint_period` | `300` | `300` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_compressed_protocol` | `0` | `OFF` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_exec_mode` | `STRICT` | `STRICT` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_load_tmpdir` | `/tmp` | `/tmp` | read-only |
| `replica_max_allowed_packet` | `1073741824` | `1073741824` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_net_timeout` | `60` | `60` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_parallel_type` | `LOGICAL_CLOCK` | `LOGICAL_CLOCK` | default SET succeeds with deprecation warning `1287` |
| `replica_parallel_workers` | `4` | `4` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_pending_jobs_size_max` | `134217728` | `134217728` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_preserve_commit_order` | `1` | `ON` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_skip_errors` | `OFF` | `OFF` | read-only |
| `replica_sql_verify_checksum` | `1` | `ON` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_transaction_retries` | `10` | `10` | `SET GLOBAL ... = DEFAULT` succeeds |
| `replica_type_conversions` | empty string | empty string | `SET GLOBAL ... = DEFAULT` succeeds |
| `slave_allow_batching` | `1` | `ON` | default SET succeeds with deprecation warning `1287` |
| `slave_checkpoint_group` | `512` | `512` | default SET succeeds with deprecation warning `1287` |
| `slave_checkpoint_period` | `300` | `300` | default SET succeeds with deprecation warning `1287` |
| `slave_compressed_protocol` | `0` | `OFF` | default SET succeeds with deprecation warning `1287` |
| `slave_exec_mode` | `STRICT` | `STRICT` | default SET succeeds with deprecation warning `1287` |
| `slave_load_tmpdir` | `/tmp` | `/tmp` | read-only |
| `slave_max_allowed_packet` | `1073741824` | `1073741824` | default SET succeeds with deprecation warning `1287` |
| `slave_net_timeout` | `60` | `60` | default SET succeeds with deprecation warning `1287` |
| `slave_parallel_type` | `LOGICAL_CLOCK` | `LOGICAL_CLOCK` | default SET succeeds with deprecation warning `1287` |
| `slave_parallel_workers` | `4` | `4` | default SET succeeds with deprecation warning `1287` |
| `slave_pending_jobs_size_max` | `134217728` | `134217728` | default SET succeeds with deprecation warning `1287` |
| `slave_preserve_commit_order` | `1` | `ON` | default SET succeeds with deprecation warning `1287` |
| `slave_skip_errors` | `OFF` | `OFF` | read-only |
| `slave_sql_verify_checksum` | `1` | `ON` | default SET succeeds with deprecation warning `1287` |
| `slave_transaction_retries` | `10` | `10` | default SET succeeds with deprecation warning `1287` |
| `slave_type_conversions` | empty string | empty string | default SET succeeds with deprecation warning `1287` |

All variables are global-only for scalar reads. `@@SESSION.name` and
`@@LOCAL.name` fail with `1238 / HY000` and a global-variable diagnostic.
`SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
include the rows.

Successful scalar reads of every `slave_*` variable emit warning `1287` with a
replacement pointing to the matching `replica_*` variable. Successful scalar
reads of `replica_parallel_type` also emit warning `1287` without a
replacement. Successful default assignments to mutable `slave_*` variables and
`replica_parallel_type` emit the same warnings. Read-only `slave_load_tmpdir`
and `slave_skip_errors` fail with a read-only diagnostic rather than producing
a deprecation warning for `SET GLOBAL ... = DEFAULT`.

## MyLite Semantics

MyLite supports:

- unscoped and `GLOBAL` scalar reads for all listed variables;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  rows with MySQL-shaped display values;
- global-only scalar diagnostics for `SESSION` and `LOCAL` reads;
- read-only diagnostics for `replica_load_tmpdir`, `slave_load_tmpdir`,
  `replica_skip_errors`, and `slave_skip_errors`;
- fixed global no-op assignments for mutable variables when the assigned value
  is `DEFAULT` or the fixed MyLite placeholder value;
- MySQL-shaped `1287` warnings for `replica_parallel_type` and deprecated
  `slave_*` aliases on successful reads and supported no-op assignments.

MyLite intentionally does not support:

- actual replication applier workers, worker queues, channels, relay-log
  checkpointing, packet handling, checksum verification, or error skipping;
- mutable process-global variable state or persisted startup option state;
- NDB batching side effects;
- privilege checks or Performance Schema variable tables.

## Parser And Runtime Design

No new grammar is required. Existing system-variable, `SHOW VARIABLES`, and
`SET` forms cover this surface:

```lemon
scalar_expression ::= system_variable_reference.
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

The implementation extends the system-variable descriptor registry, fixed
default-value table, `SHOW VARIABLES` display conversion, global-only scope
classification, read-only classification, fixed no-op `SET GLOBAL` validation,
and deprecated-warning dispatch. This remains MyLite runtime logic and does not
require SQLite extension APIs, SQLite fork hooks, file-format changes, catalog
storage, or global mutable state.

## Tests

- `packages/libmylite/tests/mysql_baseline_replica_applier_system_variables_expectations.sh`
  verifies MySQL 8.4.9 defaults, `SHOW` rows, scalar scope diagnostics,
  read-only diagnostics, fixed default assignment behavior, and deprecation
  warnings.
- `packages/libmylite/tests/runtime_replica_applier_system_variables_test.c`
  verifies MyLite scalar values, `SHOW` rows, fixed no-op `SET GLOBAL`
  behavior, read-only diagnostics, global-only scope diagnostics, and warning
  behavior.

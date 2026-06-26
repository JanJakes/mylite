# Baseline Resource Tuning System Variables

## Summary

This slice exposes a MySQL 8.4.9-shaped embedded baseline for deterministic
resource, buffer, profiling, tracking, and compatibility-tuning system
variables:

- `lc_time_names`
- `net_buffer_length`
- `preload_buffer_size`
- `profiling`
- `profiling_history_size`
- `query_alloc_block_size`
- `query_prealloc_size`
- `range_alloc_block_size`
- `range_optimizer_max_mem_size`
- `read_buffer_size`
- `read_rnd_buffer_size`
- `regexp_stack_limit`
- `regexp_time_limit`
- `restrict_fk_on_non_standard_key`
- `secondary_engine_cost_threshold`
- `select_into_buffer_size`
- `select_into_disk_sync_delay`
- `session_track_system_variables`
- `set_operations_buffer_size`
- `show_gipk_in_create_table_and_information_schema`
- `tmp_table_size`
- `transaction_alloc_block_size`
- `transaction_prealloc_size`
- `windowing_use_high_precision`
- `xa_detach_on_prepare`
- `terminology_use_previous`

MyLite supports scalar reads, `SHOW VARIABLES` / `SHOW SESSION VARIABLES` /
`SHOW GLOBAL VARIABLES` rows, session-local assignment where MySQL exposes a
session value, fixed global no-op assignment for default-compatible values,
global-only diagnostics for the regexp limit variables, the session read-only
diagnostic for `net_buffer_length`, and MySQL-shaped deprecation warnings for
the deprecated variables in this slice.

MyLite does not implement actual buffer allocation, range-optimizer memory
budgeting, regexp stack/time enforcement, session-state protocol tracking,
profiling collection, GIPK behavior changes, non-standard-FK policy changes,
window-function precision changes, XA detach behavior, global mutable shared
state, persisted variables, startup options, privileges, or Performance Schema
variable tables.

`open_files_limit` and `temptable_max_ram` are intentionally excluded because
their MySQL defaults are host/resource dependent in the comparison container.
`insert_id`, `pseudo_thread_id`, `statement_id`, `rand_seed1`, and `rand_seed2`
are also excluded because they need live per-session counters or random state.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_resource_tuning_system_variables_expectations.sh`.

## MySQL 8.4.9 Observations

The target runtime reports these defaults:

| Variable | Scalar value | `SHOW VARIABLES` value | Scope |
| --- | --- | --- | --- |
| `lc_time_names` | `en_US` | `en_US` | Global, session |
| `net_buffer_length` | `16384` | `16384` | Global, session read-only |
| `preload_buffer_size` | `32768` | `32768` | Global, session |
| `profiling` | `0` plus warning `1287` | `OFF` | Global, session |
| `profiling_history_size` | `15` plus warning `1287` | `15` | Global, session |
| `query_alloc_block_size` | `8192` | `8192` | Global, session |
| `query_prealloc_size` | `8192` plus warning `1287` | `8192` | Global, session |
| `range_alloc_block_size` | `4096` | `4096` | Global, session |
| `range_optimizer_max_mem_size` | `8388608` | `8388608` | Global, session |
| `read_buffer_size` | `131072` | `131072` | Global, session |
| `read_rnd_buffer_size` | `262144` | `262144` | Global, session |
| `regexp_stack_limit` | `8000000` global only | `8000000` | Global only |
| `regexp_time_limit` | `32` global only | `32` | Global only |
| `restrict_fk_on_non_standard_key` | `1` | `ON` | Global, session |
| `secondary_engine_cost_threshold` | `100000.000000` | `100000.000000` | Global, session |
| `select_into_buffer_size` | `131072` | `131072` | Global, session |
| `select_into_disk_sync_delay` | `0` | `0` | Global, session |
| `session_track_system_variables` | default comma list | same | Global, session |
| `set_operations_buffer_size` | `262144` | `262144` | Global, session |
| `show_gipk_in_create_table_and_information_schema` | `1` | `ON` | Global, session |
| `tmp_table_size` | `16777216` | `16777216` | Global, session |
| `transaction_alloc_block_size` | `8192` | `8192` | Global, session |
| `transaction_prealloc_size` | `4096` plus warning `1287` | `4096` | Global, session |
| `windowing_use_high_precision` | `1` | `ON` | Global, session |
| `xa_detach_on_prepare` | `1` | `ON` | Global, session |
| `terminology_use_previous` | `NONE` plus warning `1287` | `NONE` | Global, session |

`SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` include
all variables in this slice. `@@SESSION.regexp_stack_limit` and
`@@SESSION.regexp_time_limit` return `1238 / HY000`; non-global assignment for
those variables returns `1229 / HY000`. `net_buffer_length` permits session
scalar reads but rejects non-global assignment with `1621 / HY000`.
Successful `restrict_fk_on_non_standard_key` assignment emits warning `4166`.

## MyLite Scope

MyLite supports:

- scalar default/global/session/local reads where MySQL allows them;
- MySQL-shaped scope diagnostics for global-only and session-read-only
  variables;
- MySQL-shaped `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
  `SHOW SESSION VARIABLES` rows;
- session-local readback for deterministic non-global assignment forms;
- fixed global no-op assignment when the assigned value is `DEFAULT` or the
  fixed MyLite default;
- boolean assignment and display for boolean variables;
- integer assignment and readback for numeric variables in ordinary in-range
  values;
- canonical six-decimal readback for `secondary_engine_cost_threshold`;
- deprecation warnings on scalar reads and successful assignment for deprecated
  variables.

MyLite intentionally does not support:

- live resource allocation or execution-planner behavior changes;
- server-global mutation shared by other handles;
- complete locale validation or localized date/time name formatting from
  `lc_time_names`;
- profiling row collection;
- protocol-level session-state tracking;
- regexp execution limits driven by the deprecated regexp variables;
- generated-invisible-primary-key metadata behavior changes;
- persisted variables, startup option files, privileges, `SET PERSIST`, or
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

## Runtime Design

The variables are descriptor-backed system variables. Session-scoped variables
use handle-local system-variable overrides. Fixed global assignments validate
the assigned value and do not mutate shared state. Boolean variables reuse the
existing MySQL-compatible boolean SET parser. Numeric variables parse ordinary
integer literals and integer user variables; decimal cost-threshold assignment
uses a small canonical formatter. The implementation stays in the MyLite
runtime wrapper and does not require SQLite fork changes.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, session mutation, fixed global no-op behavior, and warnings;
- a runtime C regression test mirroring the MySQL observations;
- aggregate `SHOW VARIABLES` registry coverage and compatibility docs updates.

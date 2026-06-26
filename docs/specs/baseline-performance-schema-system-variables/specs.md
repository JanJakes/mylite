# Baseline Performance Schema System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata and embedded placeholder handling
for Performance Schema system variables that are still red in the baseline
matrix:

- `performance_schema`
- `performance_schema_accounts_size`
- `performance_schema_digests_size`
- `performance_schema_error_size`
- `performance_schema_events_stages_history_long_size`
- `performance_schema_events_stages_history_size`
- `performance_schema_events_statements_history_long_size`
- `performance_schema_events_statements_history_size`
- `performance_schema_events_transactions_history_long_size`
- `performance_schema_events_transactions_history_size`
- `performance_schema_events_waits_history_long_size`
- `performance_schema_events_waits_history_size`
- `performance_schema_hosts_size`
- `performance_schema_max_cond_classes`
- `performance_schema_max_cond_instances`
- `performance_schema_max_digest_length`
- `performance_schema_max_digest_sample_age`
- `performance_schema_max_file_classes`
- `performance_schema_max_file_handles`
- `performance_schema_max_file_instances`
- `performance_schema_max_index_stat`
- `performance_schema_max_memory_classes`
- `performance_schema_max_metadata_locks`
- `performance_schema_max_meter_classes`
- `performance_schema_max_metric_classes`
- `performance_schema_max_mutex_classes`
- `performance_schema_max_mutex_instances`
- `performance_schema_max_prepared_statements_instances`
- `performance_schema_max_program_instances`
- `performance_schema_max_rwlock_classes`
- `performance_schema_max_rwlock_instances`
- `performance_schema_max_socket_classes`
- `performance_schema_max_socket_instances`
- `performance_schema_max_sql_text_length`
- `performance_schema_max_stage_classes`
- `performance_schema_max_statement_classes`
- `performance_schema_max_statement_stack`
- `performance_schema_max_table_handles`
- `performance_schema_max_table_instances`
- `performance_schema_max_table_lock_stat`
- `performance_schema_max_thread_classes`
- `performance_schema_max_thread_instances`
- `performance_schema_session_connect_attrs_size`
- `performance_schema_setup_actors_size`
- `performance_schema_setup_objects_size`
- `performance_schema_show_processlist`
- `performance_schema_users_size`

The official MySQL 8.4 Reference Manual describes these as Performance Schema
startup and runtime system variables. Most are startup sizing controls. The
embedded MyLite baseline exposes MySQL-shaped values and diagnostics without
allocating Performance Schema instrumentation.

Reference:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-system-variables.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed these defaults:

| Variable | Scalar value | SHOW value | Mutation |
| --- | --- | --- | --- |
| `performance_schema` | `1` | `ON` | read-only global |
| `performance_schema_accounts_size` | `-1` | `-1` | read-only global |
| `performance_schema_digests_size` | `10000` | `10000` | read-only global |
| `performance_schema_error_size` | `5556` | `5556` | read-only global |
| `performance_schema_events_stages_history_long_size` | `10000` | `10000` | read-only global |
| `performance_schema_events_stages_history_size` | `10` | `10` | read-only global |
| `performance_schema_events_statements_history_long_size` | `10000` | `10000` | read-only global |
| `performance_schema_events_statements_history_size` | `10` | `10` | read-only global |
| `performance_schema_events_transactions_history_long_size` | `10000` | `10000` | read-only global |
| `performance_schema_events_transactions_history_size` | `10` | `10` | read-only global |
| `performance_schema_events_waits_history_long_size` | `10000` | `10000` | read-only global |
| `performance_schema_events_waits_history_size` | `10` | `10` | read-only global |
| `performance_schema_hosts_size` | `-1` | `-1` | read-only global |
| `performance_schema_max_cond_classes` | `150` | `150` | read-only global |
| `performance_schema_max_cond_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_digest_length` | `1024` | `1024` | read-only global |
| `performance_schema_max_digest_sample_age` | `60` | `60` | dynamic global |
| `performance_schema_max_file_classes` | `80` | `80` | read-only global |
| `performance_schema_max_file_handles` | `32768` | `32768` | read-only global |
| `performance_schema_max_file_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_index_stat` | `-1` | `-1` | read-only global |
| `performance_schema_max_memory_classes` | `470` | `470` | read-only global |
| `performance_schema_max_metadata_locks` | `-1` | `-1` | read-only global |
| `performance_schema_max_meter_classes` | `30` | `30` | read-only global |
| `performance_schema_max_metric_classes` | `600` | `600` | read-only global |
| `performance_schema_max_mutex_classes` | `350` | `350` | read-only global |
| `performance_schema_max_mutex_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_prepared_statements_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_program_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_rwlock_classes` | `100` | `100` | read-only global |
| `performance_schema_max_rwlock_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_socket_classes` | `10` | `10` | read-only global |
| `performance_schema_max_socket_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_sql_text_length` | `1024` | `1024` | read-only global |
| `performance_schema_max_stage_classes` | `175` | `175` | read-only global |
| `performance_schema_max_statement_classes` | `220` | `220` | read-only global |
| `performance_schema_max_statement_stack` | `10` | `10` | read-only global |
| `performance_schema_max_table_handles` | `-1` | `-1` | read-only global |
| `performance_schema_max_table_instances` | `-1` | `-1` | read-only global |
| `performance_schema_max_table_lock_stat` | `-1` | `-1` | read-only global |
| `performance_schema_max_thread_classes` | `100` | `100` | read-only global |
| `performance_schema_max_thread_instances` | `-1` | `-1` | read-only global |
| `performance_schema_session_connect_attrs_size` | `512` | `512` | read-only global |
| `performance_schema_setup_actors_size` | `-1` | `-1` | read-only global |
| `performance_schema_setup_objects_size` | `-1` | `-1` | read-only global |
| `performance_schema_show_processlist` | `0` | `OFF` | dynamic global |
| `performance_schema_users_size` | `-1` | `-1` | read-only global |

All variables appear in default, global, and session `SHOW VARIABLES` output.
Scalar `@@SESSION.variable` and `@@LOCAL.variable` reads fail with
`1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.

Read-only variables reject every assignment form with `1238/HY000` and text
containing `read only variable`.

`performance_schema_max_digest_sample_age` and
`performance_schema_show_processlist` reject unqualified, `SESSION`, and
`LOCAL` assignments with `1229/HY000`. `SET GLOBAL ... = DEFAULT` and exact or
alternate typed values succeed in MySQL. Invalid `NULL` and string values fail
with `1232/42000` for `performance_schema_max_digest_sample_age` and
`1231/42000` for `performance_schema_show_processlist`.

## MyLite Semantics

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

Session and local scalar scopes return the MySQL-shaped global-variable
diagnostic for every variable in this slice.

For read-only variables, every direct and user-variable assignment path returns
the MySQL-shaped read-only diagnostic before value validation.

For dynamic globals, MyLite accepts `SET GLOBAL ... = DEFAULT` and exact fixed
default assignments as no-ops. Alternate global values are rejected with a
fixed-placeholder unsupported diagnostic instead of mutating shared global
state.

The baseline deliberately does not implement Performance Schema collection,
instrument setup, live memory sizing, process-list routing through Performance
Schema, startup options, persisted variables, privileges, or Performance Schema
variable tables.

## Parser And Runtime Design

No new grammar is required. Existing system-variable scalar and `SET` syntax
covers the baseline:

```lemon
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
system_variable_target ::= GLOBAL ident.
system_variable_target ::= SESSION ident.
system_variable_target ::= LOCAL ident.
scalar_expression ::= system_variable_reference.
system_variable_reference ::= AT_AT ident.
system_variable_reference ::= AT_AT GLOBAL DOT ident.
system_variable_reference ::= AT_AT SESSION DOT ident.
```

Runtime implementation adds descriptors, fixed scalar/SHOW values,
global-only scope wiring, read-only classification, and fixed no-op global
assignment validation for the two dynamic global placeholders.

This is pure MyLite runtime logic. It does not require a SQLite public
extension API, wrapper translation, file-format change, or targeted SQLite fork
hook.

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_system_variables_expectations.sh`
  verifies the observed MySQL 8.4.9 expectations.
- `packages/libmylite/tests/runtime_performance_schema_system_variables_test.c`
  verifies MyLite scalar reads, `SHOW VARIABLES`, scope diagnostics,
  read-only diagnostics, fixed global no-op assignment for the dynamic globals,
  rejected alternate global values, user-variable assignment paths, and
  aggregate SHOW row counts.

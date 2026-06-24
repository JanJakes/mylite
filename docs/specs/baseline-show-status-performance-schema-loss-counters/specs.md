# Baseline SHOW STATUS Performance Schema Loss Counters

## Scope

This slice adds MySQL 8.4.9-shaped `SHOW STATUS` metadata for the
`Performance_schema_%` loss-counter family. MyLite does not implement live
Performance Schema instrumentation for this slice; the rows are deterministic
embedded placeholders that let applications discover the same status names and
scope visibility as MySQL.

The covered variables are:

| Variable | MyLite value |
| --- | --- |
| `Performance_schema_accounts_lost` | `0` |
| `Performance_schema_cond_classes_lost` | `0` |
| `Performance_schema_cond_instances_lost` | `0` |
| `Performance_schema_digest_lost` | `0` |
| `Performance_schema_file_classes_lost` | `0` |
| `Performance_schema_file_handles_lost` | `0` |
| `Performance_schema_file_instances_lost` | `0` |
| `Performance_schema_hosts_lost` | `0` |
| `Performance_schema_index_stat_lost` | `0` |
| `Performance_schema_locker_lost` | `0` |
| `Performance_schema_logger_lost` | `0` |
| `Performance_schema_memory_classes_lost` | `0` |
| `Performance_schema_metadata_lock_lost` | `0` |
| `Performance_schema_meter_lost` | `0` |
| `Performance_schema_metric_lost` | `0` |
| `Performance_schema_mutex_classes_lost` | `0` |
| `Performance_schema_mutex_instances_lost` | `0` |
| `Performance_schema_nested_statement_lost` | `0` |
| `Performance_schema_prepared_statements_lost` | `0` |
| `Performance_schema_program_lost` | `0` |
| `Performance_schema_rwlock_classes_lost` | `0` |
| `Performance_schema_rwlock_instances_lost` | `0` |
| `Performance_schema_session_connect_attrs_longest_seen` | `0` |
| `Performance_schema_session_connect_attrs_lost` | `0` |
| `Performance_schema_socket_classes_lost` | `0` |
| `Performance_schema_socket_instances_lost` | `0` |
| `Performance_schema_stage_classes_lost` | `0` |
| `Performance_schema_statement_classes_lost` | `0` |
| `Performance_schema_table_handles_lost` | `0` |
| `Performance_schema_table_instances_lost` | `0` |
| `Performance_schema_table_lock_stat_lost` | `0` |
| `Performance_schema_thread_classes_lost` | `0` |
| `Performance_schema_thread_instances_lost` | `0` |
| `Performance_schema_users_lost` | `0` |

## Sources

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html>
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

## Semantics

`SHOW STATUS LIKE 'Performance_schema\_%'`, `SHOW SESSION STATUS`,
`SHOW LOCAL STATUS`, and `SHOW GLOBAL STATUS` return the same row names and
order observed from MySQL 8.4.9. Every value is a fixed decimal text `0`.

`sys.metrics` exposes the same rows as lowercase `Global Status` metrics because
they are global-visible `SHOW STATUS` descriptors.

## Unsupported Behavior

This slice does not implement:

- Performance Schema setup, consumers, instruments, events, or live loss
  accounting;
- connection-attribute length tracking for
  `Performance_schema_session_connect_attrs_longest_seen`;
- warning/error changes based on dropped Performance Schema data.

The rows are placeholders that preserve metadata shape for embedded
applications.

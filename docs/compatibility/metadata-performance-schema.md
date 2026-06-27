# Performance Schema tables

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

The `performance_schema` schema name is exposed in the limited built-in schema
catalog and can be selected with `USE performance_schema`. The MySQL 8.4.9
target runtime's Performance Schema table names are exposed as metadata rows
through `INFORMATION_SCHEMA.TABLES`, `SHOW TABLES`, `SHOW FULL TABLES`, and
`SHOW TABLE STATUS`. The variable/status tables listed below are queryable for
MyLite's supported registries, `performance_timers` exposes deterministic timer
placeholder rows, and setup actor/consumer/object/thread tables expose read-only
default configuration rows. `setup_loggers` and `setup_meters` expose read-only
telemetry setup rows, and `setup_metrics` exposes the read-only default metric
catalog. `table_handles` exposes MySQL-shaped read-only empty lock-handle
metadata. Thread/account/host/user status and variable tables expose limited
current-handle registry snapshots. `variables_info` exposes MyLite's supported
system-variable registry with embedded source/range placeholders, and
`persisted_variables` is an empty persisted-global placeholder. Connection
summary, processlist, and thread tables expose limited rows from MyLite's
open-connection registry. Connection attribute tables expose deterministic
embedded MyLite attributes for open handles. Instance tables for synchronization
objects, files, and sockets expose empty read-only metadata placeholders.
Binary-log compression, data-lock, data-lock-wait, and prepared-statement
instrumentation tables expose empty read-only metadata placeholders.
Host-cache and keyring tables expose empty read-only metadata placeholders.
The supported no-replication Performance Schema replication tables expose empty
read-only metadata placeholders. Optional Clone, Enterprise Firewall, NDB, Group
Replication, component scheduler, and Enterprise Thread Pool Performance Schema
tables that are absent from the MySQL 8.4.9 target runtime are also absent from
MyLite metadata and return MySQL-shaped missing-table diagnostics.
Event current/history placeholders for stage and wait tables, statement
history-long, and stored-program statement summaries expose MySQL-shaped
metadata with empty read-only rows; live event instrumentation remains
unsupported. Error, file, memory, socket, stage, table I/O, table lock, and wait
summary placeholders expose MySQL-shaped metadata with empty read-only rows;
live aggregation for those summary families remains unsupported.
`user_defined_functions` exposes MySQL 8.4.9-shaped loadable-function registry
rows, and `user_variables_by_thread` exposes MyLite session user variables for
the current connection. The remaining tables are metadata-only and unsupported
unless listed otherwise. MyLite rejects schema, table, index, rename, truncate,
and single-table DML writes targeting `performance_schema` with
`1044 / 42000` access-denied diagnostics.

| Table | Status | Notes |
| --- | --- | --- |
| `performance_schema.accounts` | 🟡 | Limited embedded `root@%` current-connection count row with MySQL-shaped fixed-table metadata; no disconnected account history, memory accounting, or instrumentation limits |
| `performance_schema.binary_log_transaction_compression_stats` | 🟡 | Empty read-only binary-log compression stats placeholder with MySQL-shaped metadata; no binary/relay log instrumentation |
| `performance_schema.clone_progress` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.clone_status` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.component_scheduler_tasks` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.cond_instances` | 🟡 | Empty read-only condition-instance placeholder with MySQL-shaped columns, HASH primary/secondary index metadata, table metadata, selected-schema reads, and write protection; no live synchronization instrumentation |
| `performance_schema.data_lock_waits` | 🟡 | Empty read-only data-lock-wait placeholder with MySQL-shaped metadata and HASH index introspection; no live wait graph |
| `performance_schema.data_locks` | 🟡 | Empty read-only data-lock placeholder with MySQL-shaped metadata and HASH index introspection; no live data-lock instrumentation |
| `performance_schema.error_log` | ❌ | Server error log recent entries |
| `performance_schema.events_errors_summary_by_account_by_error` | 🟡 | Empty read-only error-summary placeholder with MySQL-shaped metadata; no live error aggregation |
| `performance_schema.events_errors_summary_by_host_by_error` | 🟡 | Empty read-only error-summary placeholder with MySQL-shaped metadata; no live error aggregation |
| `performance_schema.events_errors_summary_by_thread_by_error` | 🟡 | Empty read-only error-summary placeholder with MySQL-shaped metadata; no live error aggregation |
| `performance_schema.events_errors_summary_by_user_by_error` | 🟡 | Empty read-only error-summary placeholder with MySQL-shaped metadata; no live error aggregation |
| `performance_schema.events_errors_summary_global_by_error` | 🟡 | Empty read-only error-summary placeholder with MySQL-shaped metadata; no live error aggregation |
| `performance_schema.events_stages_current` | 🟡 | Empty read-only current-stage placeholder with MySQL-shaped metadata; no live stage instrumentation |
| `performance_schema.events_stages_history` | 🟡 | Empty read-only per-thread stage-history placeholder with MySQL-shaped metadata; no live stage instrumentation |
| `performance_schema.events_stages_history_long` | 🟡 | Empty read-only global stage-history placeholder with MySQL-shaped metadata; no live stage instrumentation |
| `performance_schema.events_stages_summary_by_account_by_event_name` | 🟡 | Empty read-only stage-summary placeholder with MySQL-shaped metadata; no live stage aggregation |
| `performance_schema.events_stages_summary_by_host_by_event_name` | 🟡 | Empty read-only stage-summary placeholder with MySQL-shaped metadata; no live stage aggregation |
| `performance_schema.events_stages_summary_by_thread_by_event_name` | 🟡 | Empty read-only stage-summary placeholder with MySQL-shaped metadata; no live stage aggregation |
| `performance_schema.events_stages_summary_by_user_by_event_name` | 🟡 | Empty read-only stage-summary placeholder with MySQL-shaped metadata; no live stage aggregation |
| `performance_schema.events_stages_summary_global_by_event_name` | 🟡 | Empty read-only stage-summary placeholder with MySQL-shaped metadata; no live stage aggregation |
| `performance_schema.events_statements_current` | ❌ | Current statement events |
| `performance_schema.events_statements_histogram_by_digest` | ❌ | Statement histograms per schema and digest value |
| `performance_schema.events_statements_histogram_global` | ❌ | Statement histogram summarized globally |
| `performance_schema.events_statements_history` | ❌ | Most recent statement events per thread |
| `performance_schema.events_statements_history_long` | 🟡 | Empty read-only global statement-history placeholder with MySQL-shaped metadata; no live statement instrumentation |
| `performance_schema.events_statements_summary_by_account_by_event_name` | ❌ | Statement events per account and event name |
| `performance_schema.events_statements_summary_by_digest` | ❌ | Statement events per schema and digest value |
| `performance_schema.events_statements_summary_by_host_by_event_name` | ❌ | Statement events per host name and event name |
| `performance_schema.events_statements_summary_by_program` | 🟡 | Empty read-only stored-program statement-summary placeholder with MySQL-shaped metadata; no stored-program instrumentation |
| `performance_schema.events_statements_summary_by_thread_by_event_name` | ❌ | Statement events per thread and event name |
| `performance_schema.events_statements_summary_by_user_by_event_name` | ❌ | Statement events per user name and event name |
| `performance_schema.events_statements_summary_global_by_event_name` | ❌ | Statement events per event name |
| `performance_schema.events_transactions_current` | ❌ | Current transaction events |
| `performance_schema.events_transactions_history` | ❌ | Most recent transaction events per thread |
| `performance_schema.events_transactions_history_long` | ❌ | Most recent transaction events overall |
| `performance_schema.events_transactions_summary_by_account_by_event_name` | ❌ | Transaction events per account and event name |
| `performance_schema.events_transactions_summary_by_host_by_event_name` | ❌ | Transaction events per host name and event name |
| `performance_schema.events_transactions_summary_by_thread_by_event_name` | ❌ | Transaction events per thread and event name |
| `performance_schema.events_transactions_summary_by_user_by_event_name` | ❌ | Transaction events per user name and event name |
| `performance_schema.events_transactions_summary_global_by_event_name` | ❌ | Transaction events per event name |
| `performance_schema.events_waits_current` | 🟡 | Empty read-only current-wait placeholder with MySQL-shaped metadata; no live wait instrumentation |
| `performance_schema.events_waits_history` | 🟡 | Empty read-only per-thread wait-history placeholder with MySQL-shaped metadata; no live wait instrumentation |
| `performance_schema.events_waits_history_long` | 🟡 | Empty read-only global wait-history placeholder with MySQL-shaped metadata; no live wait instrumentation |
| `performance_schema.events_waits_summary_by_account_by_event_name` | 🟡 | Empty read-only wait-summary placeholder with MySQL-shaped metadata; no live wait aggregation |
| `performance_schema.events_waits_summary_by_host_by_event_name` | 🟡 | Empty read-only wait-summary placeholder with MySQL-shaped metadata; no live wait aggregation |
| `performance_schema.events_waits_summary_by_instance` | 🟡 | Empty read-only wait-summary placeholder with MySQL-shaped metadata; no live wait aggregation |
| `performance_schema.events_waits_summary_by_thread_by_event_name` | 🟡 | Empty read-only wait-summary placeholder with MySQL-shaped metadata; no live wait aggregation |
| `performance_schema.events_waits_summary_by_user_by_event_name` | 🟡 | Empty read-only wait-summary placeholder with MySQL-shaped metadata; no live wait aggregation |
| `performance_schema.events_waits_summary_global_by_event_name` | 🟡 | Empty read-only wait-summary placeholder with MySQL-shaped metadata; no live wait aggregation |
| `performance_schema.file_instances` | 🟡 | Empty read-only file-instance placeholder with MySQL-shaped columns, HASH primary/secondary index metadata, table metadata, selected-schema reads, and write protection; no live file I/O instrumentation |
| `performance_schema.file_summary_by_event_name` | 🟡 | Empty read-only file-summary placeholder with MySQL-shaped metadata; no live file I/O aggregation |
| `performance_schema.file_summary_by_instance` | 🟡 | Empty read-only file-summary placeholder with MySQL-shaped metadata; no live file I/O aggregation |
| `performance_schema.firewall_group_allowlist` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.firewall_groups` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.firewall_membership` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.global_status` | 🟡 | Queryable global rows from MyLite's supported status registry, with MySQL-shaped columns and `HASH` primary-key metadata; live instrumentation and full optional status universe remain unsupported |
| `performance_schema.global_variables` | 🟡 | Queryable global rows from MyLite's supported system-variable registry, with MySQL-shaped columns and `HASH` primary-key metadata; persisted/global mutation and full optional variable universe remain unsupported |
| `performance_schema.host_cache` | 🟡 | Empty read-only host-cache placeholder with MySQL-shaped metadata and HASH index introspection; no live DNS/cache error state |
| `performance_schema.hosts` | 🟡 | Limited embedded `%` current-host count row with MySQL-shaped fixed-table metadata; no disconnected host history, memory accounting, or instrumentation limits |
| `performance_schema.keyring_component_status` | 🟡 | Empty read-only keyring component status placeholder with MySQL-shaped metadata; no keyring service state |
| `performance_schema.keyring_keys` | 🟡 | Empty read-only keyring key placeholder with MySQL-shaped metadata; no key inventory |
| `performance_schema.log_status` | ❌ | Information about server logs for backup purposes |
| `performance_schema.memory_summary_by_account_by_event_name` | 🟡 | Empty read-only memory-summary placeholder with MySQL-shaped metadata; no live memory aggregation |
| `performance_schema.memory_summary_by_host_by_event_name` | 🟡 | Empty read-only memory-summary placeholder with MySQL-shaped metadata; no live memory aggregation |
| `performance_schema.memory_summary_by_thread_by_event_name` | 🟡 | Empty read-only memory-summary placeholder with MySQL-shaped metadata; no live memory aggregation |
| `performance_schema.memory_summary_by_user_by_event_name` | 🟡 | Empty read-only memory-summary placeholder with MySQL-shaped metadata; no live memory aggregation |
| `performance_schema.memory_summary_global_by_event_name` | 🟡 | Empty read-only memory-summary placeholder with MySQL-shaped metadata; no live memory aggregation |
| `performance_schema.metadata_locks` | ❌ | Metadata locks and lock requests |
| `performance_schema.mutex_instances` | 🟡 | Empty read-only mutex-instance placeholder with MySQL-shaped columns, HASH primary/secondary index metadata, table metadata, selected-schema reads, and write protection; no live mutex instrumentation |
| `performance_schema.ndb_sync_excluded_objects` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.ndb_sync_pending_objects` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.objects_summary_global_by_type` | ❌ | Object summaries |
| `performance_schema.performance_timers` | 🟡 | Queryable MySQL-shaped timer-name rows with deterministic non-NULL placeholder timer values; live timer calibration and event timing remain unsupported |
| `performance_schema.persisted_variables` | 🟡 | Empty read-only persisted-global variable placeholder with MySQL-shaped columns, HASH primary-key metadata, table metadata, selected-schema reads, and write protection; no `mysqld-auto.cnf`, `SET PERSIST`, or persisted-global load path |
| `performance_schema.prepared_statements_instances` | 🟡 | Empty read-only prepared-statement instrumentation placeholder with MySQL-shaped metadata and HASH index introspection; no live prepared-statement statistics |
| `performance_schema.processlist` | 🟡 | Limited open-connection rows from MyLite's processlist registry with current SQL text for the active handle; no privilege filtering or full Performance Schema statement/wait instrumentation |
| `performance_schema.replication_applier_configuration` | 🟡 | Empty read-only no-replication placeholder with MySQL-shaped metadata and HASH primary-key introspection; no channel/applier configuration state |
| `performance_schema.replication_applier_filters` | 🟡 | Empty read-only no-replication filter placeholder with MySQL-shaped metadata; no channel filter state |
| `performance_schema.replication_applier_global_filters` | 🟡 | Empty read-only global-filter placeholder with MySQL-shaped metadata; no global replication filter state |
| `performance_schema.replication_applier_status` | 🟡 | Empty read-only no-replication applier-status placeholder with MySQL-shaped metadata and HASH primary-key introspection; no applier state |
| `performance_schema.replication_applier_status_by_coordinator` | 🟡 | Empty read-only coordinator-status placeholder with MySQL-shaped metadata and HASH index introspection; no coordinator thread state |
| `performance_schema.replication_applier_status_by_worker` | 🟡 | Empty read-only worker-status placeholder with MySQL-shaped metadata and HASH index introspection; no worker thread state |
| `performance_schema.replication_asynchronous_connection_failover` | 🟡 | Empty read-only failover-source placeholder with MySQL-shaped metadata; no asynchronous failover source list |
| `performance_schema.replication_asynchronous_connection_failover_managed` | 🟡 | Empty read-only managed-failover placeholder with MySQL-shaped metadata; no managed failover configuration |
| `performance_schema.replication_connection_configuration` | 🟡 | Empty read-only source-connection configuration placeholder with MySQL-shaped metadata and HASH primary-key introspection; no source/channel configuration |
| `performance_schema.replication_connection_status` | 🟡 | Empty read-only source-connection status placeholder with MySQL-shaped metadata and HASH index introspection; no source connection state |
| `performance_schema.replication_group_communication_information` | ✅ | Optional Group Replication table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.replication_group_configuration_version` | ✅ | Optional Group Replication table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.replication_group_member_actions` | ✅ | Optional Group Replication table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.replication_group_member_stats` | 🟡 | Empty read-only group-member stats placeholder with MySQL-shaped metadata; no Group Replication statistics |
| `performance_schema.replication_group_members` | 🟡 | Empty read-only group-member placeholder with MySQL-shaped metadata; no Group Replication membership |
| `performance_schema.rwlock_instances` | 🟡 | Empty read-only rwlock-instance placeholder with MySQL-shaped columns, HASH primary/secondary index metadata, table metadata, selected-schema reads, and write protection; no live rwlock instrumentation |
| `performance_schema.session_account_connect_attrs` | 🟡 | Limited current-account rows from MyLite's open-connection registry with deterministic `_client_name`, `_client_version`, and `program_name` attributes; no arbitrary connector attributes, truncation accounting, or account filtering beyond embedded `root@%` |
| `performance_schema.session_connect_attrs` | 🟡 | Limited open-connection rows with deterministic embedded MyLite attributes and MySQL-shaped primary-key metadata; no arbitrary connector attributes, wire-protocol attribute ingestion, or truncation accounting |
| `performance_schema.session_status` | 🟡 | Queryable current-session rows from MyLite's supported status registry, including observed session-only status rows and Performance Schema command-counter filtering; live per-thread accounting remains unsupported |
| `performance_schema.session_variables` | 🟡 | Queryable current-session rows from MyLite's supported system-variable registry, including session overrides; thread-specific variable tables remain unsupported |
| `performance_schema.setup_actors` | 🟡 | Read-only MySQL-shaped default actor row and primary-key metadata; no mutable foreground-thread instrumentation setup |
| `performance_schema.setup_consumers` | 🟡 | Read-only MySQL 8.4.9 default consumer rows and primary-key metadata; no mutable consumer state |
| `performance_schema.setup_instruments` | ❌ | Table shape and diagnostics |
| `performance_schema.setup_loggers` | 🟡 | Read-only MySQL 8.4.9 default logger row with MySQL-shaped metadata; no mutable telemetry logger configuration |
| `performance_schema.setup_meters` | 🟡 | Read-only MySQL 8.4.9 default meter rows with MySQL-shaped metadata and HASH primary-key introspection; no mutable telemetry meter configuration |
| `performance_schema.setup_metrics` | 🟡 | Read-only MySQL 8.4.9 default metric rows with MySQL-shaped metadata and HASH primary-key introspection; no metric collection or telemetry export |
| `performance_schema.setup_objects` | 🟡 | Read-only MySQL 8.4.9 default object-filter rows and multi-column `OBJECT` unique-index metadata; no mutable instrumentation filters |
| `performance_schema.setup_threads` | 🟡 | Read-only MySQL 8.4.9 default setup-thread class rows and HASH primary-key metadata; no mutable thread instrumentation setup |
| `performance_schema.socket_instances` | 🟡 | Empty read-only socket-instance placeholder with MySQL-shaped columns, HASH primary/secondary index metadata, table metadata, selected-schema reads, and write protection; no live socket instrumentation |
| `performance_schema.socket_summary_by_event_name` | 🟡 | Empty read-only socket-summary placeholder with MySQL-shaped metadata; no live socket I/O aggregation |
| `performance_schema.socket_summary_by_instance` | 🟡 | Empty read-only socket-summary placeholder with MySQL-shaped metadata; no live socket I/O aggregation |
| `performance_schema.status_by_account` | 🟡 | Limited current-handle account status rows for embedded `root@%`, backed by MyLite's supported status registry; no disconnected aggregate storage or cross-handle Performance Schema accounting |
| `performance_schema.status_by_host` | 🟡 | Limited current-handle host status rows for embedded `%`, backed by MyLite's supported status registry; no disconnected aggregate storage or cross-handle Performance Schema accounting |
| `performance_schema.status_by_thread` | 🟡 | Limited current-handle thread status rows keyed by the MyLite connection id and backed by MyLite's supported status registry; no cross-handle foreground-thread visibility or instrumentation filtering |
| `performance_schema.status_by_user` | 🟡 | Limited current-handle user status rows for embedded `root`, backed by MyLite's supported status registry; no disconnected aggregate storage or cross-handle Performance Schema accounting |
| `performance_schema.table_handles` | 🟡 | Read-only empty MySQL 8.4.9-shaped table-handle metadata with HASH primary and secondary index introspection; no live table-lock instrumentation |
| `performance_schema.table_io_waits_summary_by_index_usage` | 🟡 | Empty read-only table I/O index-summary placeholder with MySQL-shaped metadata; no live table I/O or index-usage aggregation |
| `performance_schema.table_io_waits_summary_by_table` | 🟡 | Empty read-only table I/O summary placeholder with MySQL-shaped metadata; no live table I/O aggregation |
| `performance_schema.table_lock_waits_summary_by_table` | 🟡 | Empty read-only table-lock summary placeholder with MySQL-shaped metadata; no live table-lock aggregation |
| `performance_schema.threads` | 🟡 | Limited foreground thread rows from MyLite's processlist registry with MySQL-shaped metadata and deterministic instrumentation placeholders; no OS thread ids, resource groups, memory accounting, or event instrumentation |
| `performance_schema.tls_channel_status` | ❌ | TLS status for each connection interface |
| `performance_schema.tp_thread_group_state` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.tp_thread_group_stats` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.tp_thread_state` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |
| `performance_schema.user_defined_functions` | 🟡 | Read-only MySQL 8.4.9 default component/plugin rows with MySQL-shaped metadata; no `CREATE FUNCTION` registry lifecycle |
| `performance_schema.user_variables_by_thread` | 🟡 | Current-connection user variables with MySQL-shaped metadata and HASH primary-key introspection; no cross-connection visibility or binary-longblob fidelity in this row path |
| `performance_schema.users` | 🟡 | Limited embedded `root` current-user count row with MySQL-shaped fixed-table metadata; no disconnected user history, memory accounting, or instrumentation limits |
| `performance_schema.variables_by_thread` | 🟡 | Limited current-handle thread variable rows keyed by the MyLite connection id and backed by MyLite's supported session-variable registry; no cross-handle foreground-thread visibility, sensitive-variable masking, or full optional variable universe |
| `performance_schema.variables_info` | 🟡 | Queryable rows for MyLite's supported system-variable registry with MySQL-shaped source/range columns and no indexes; source/range fields are embedded placeholders (`COMPILED`, empty path/user/host, `NULL` set time, `0..0` range) with no true last-set tracking or exact numeric ranges |
| `performance_schema.tp_connections` | ✅ | Optional target-runtime table absence with no metadata rows and MySQL-shaped missing-table diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)

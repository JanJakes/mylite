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
telemetry setup rows. `user_defined_functions` exposes MySQL 8.4.9-shaped
loadable-function registry rows, and `user_variables_by_thread` exposes MyLite
session user variables for the current connection. The remaining tables are
metadata-only and unsupported unless listed otherwise. MyLite rejects schema,
table, index, rename, truncate, and single-table DML writes targeting
`performance_schema` with `1044 / 42000` access-denied diagnostics.

| Table | Status | Notes |
| --- | --- | --- |
| `performance_schema.accounts` | ❌ | Connection stats per account |
| `performance_schema.binary_log_transaction_compression_stats` | ❌ | Binary log transaction compression |
| `performance_schema.clone_progress` | ❌ | Clone operation progress |
| `performance_schema.clone_status` | ❌ | Clone operation status |
| `performance_schema.component_scheduler_tasks` | ❌ | Status of scheduled tasks |
| `performance_schema.cond_instances` | ❌ | Synchronization object instances |
| `performance_schema.data_lock_waits` | ❌ | Data lock wait relationships |
| `performance_schema.data_locks` | ❌ | Data locks held and requested |
| `performance_schema.error_log` | ❌ | Server error log recent entries |
| `performance_schema.events_errors_summary_by_account_by_error` | ❌ | Errors per account and error code |
| `performance_schema.events_errors_summary_by_host_by_error` | ❌ | Errors per host and error code |
| `performance_schema.events_errors_summary_by_thread_by_error` | ❌ | Errors per thread and error code |
| `performance_schema.events_errors_summary_by_user_by_error` | ❌ | Errors per user and error code |
| `performance_schema.events_errors_summary_global_by_error` | ❌ | Errors per error code |
| `performance_schema.events_stages_current` | ❌ | Current stage events |
| `performance_schema.events_stages_history` | ❌ | Most recent stage events per thread |
| `performance_schema.events_stages_history_long` | ❌ | Most recent stage events overall |
| `performance_schema.events_stages_summary_by_account_by_event_name` | ❌ | Stage events per account and event name |
| `performance_schema.events_stages_summary_by_host_by_event_name` | ❌ | Stage events per host name and event name |
| `performance_schema.events_stages_summary_by_thread_by_event_name` | ❌ | Stage waits per thread and event name |
| `performance_schema.events_stages_summary_by_user_by_event_name` | ❌ | Stage events per user name and event name |
| `performance_schema.events_stages_summary_global_by_event_name` | ❌ | Stage waits per event name |
| `performance_schema.events_statements_current` | ❌ | Current statement events |
| `performance_schema.events_statements_histogram_by_digest` | ❌ | Statement histograms per schema and digest value |
| `performance_schema.events_statements_histogram_global` | ❌ | Statement histogram summarized globally |
| `performance_schema.events_statements_history` | ❌ | Most recent statement events per thread |
| `performance_schema.events_statements_history_long` | ❌ | Most recent statement events overall |
| `performance_schema.events_statements_summary_by_account_by_event_name` | ❌ | Statement events per account and event name |
| `performance_schema.events_statements_summary_by_digest` | ❌ | Statement events per schema and digest value |
| `performance_schema.events_statements_summary_by_host_by_event_name` | ❌ | Statement events per host name and event name |
| `performance_schema.events_statements_summary_by_program` | ❌ | Statement events per stored program |
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
| `performance_schema.events_waits_current` | ❌ | Current wait events |
| `performance_schema.events_waits_history` | ❌ | Most recent wait events per thread |
| `performance_schema.events_waits_history_long` | ❌ | Most recent wait events overall |
| `performance_schema.events_waits_summary_by_account_by_event_name` | ❌ | Wait events per account and event name |
| `performance_schema.events_waits_summary_by_host_by_event_name` | ❌ | Wait events per host name and event name |
| `performance_schema.events_waits_summary_by_instance` | ❌ | Wait events per instance |
| `performance_schema.events_waits_summary_by_thread_by_event_name` | ❌ | Wait events per thread and event name |
| `performance_schema.events_waits_summary_by_user_by_event_name` | ❌ | Wait events per user name and event name |
| `performance_schema.events_waits_summary_global_by_event_name` | ❌ | Wait events per event name |
| `performance_schema.file_instances` | ❌ | File instances |
| `performance_schema.file_summary_by_event_name` | ❌ | File events per event name |
| `performance_schema.file_summary_by_instance` | ❌ | File events per file instance |
| `performance_schema.firewall_group_allowlist` | ❌ | Firewall in-memory data for group profile allowlists |
| `performance_schema.firewall_groups` | ❌ | Firewall in-memory data for group profiles |
| `performance_schema.firewall_membership` | ❌ | Firewall in-memory data for group profile members |
| `performance_schema.global_status` | 🟡 | Queryable global rows from MyLite's supported status registry, with MySQL-shaped columns and `HASH` primary-key metadata; live instrumentation and full optional status universe remain unsupported |
| `performance_schema.global_variables` | 🟡 | Queryable global rows from MyLite's supported system-variable registry, with MySQL-shaped columns and `HASH` primary-key metadata; persisted/global mutation and full optional variable universe remain unsupported |
| `performance_schema.host_cache` | ❌ | Information from internal host cache |
| `performance_schema.hosts` | ❌ | Connection stats per host name |
| `performance_schema.keyring_component_status` | ❌ | Status information for installed keyring component |
| `performance_schema.keyring_keys` | ❌ | Metadata for keyring keys |
| `performance_schema.log_status` | ❌ | Information about server logs for backup purposes |
| `performance_schema.memory_summary_by_account_by_event_name` | ❌ | Memory operations per account and event name |
| `performance_schema.memory_summary_by_host_by_event_name` | ❌ | Memory operations per host and event name |
| `performance_schema.memory_summary_by_thread_by_event_name` | ❌ | Memory operations per thread and event name |
| `performance_schema.memory_summary_by_user_by_event_name` | ❌ | Memory operations per user and event name |
| `performance_schema.memory_summary_global_by_event_name` | ❌ | Memory operations globally per event name |
| `performance_schema.metadata_locks` | ❌ | Metadata locks and lock requests |
| `performance_schema.mutex_instances` | ❌ | Mutex synchronization object instances |
| `performance_schema.ndb_sync_excluded_objects` | ❌ | NDB objects which cannot be synchronized |
| `performance_schema.ndb_sync_pending_objects` | ❌ | NDB objects waiting for synchronization |
| `performance_schema.objects_summary_global_by_type` | ❌ | Object summaries |
| `performance_schema.performance_timers` | 🟡 | Queryable MySQL-shaped timer-name rows with deterministic non-NULL placeholder timer values; live timer calibration and event timing remain unsupported |
| `performance_schema.persisted_variables` | ❌ | Contents of mysqld-auto.cnf file |
| `performance_schema.prepared_statements_instances` | ❌ | Prepared statement instances and statistics |
| `performance_schema.processlist` | ❌ | Process list information |
| `performance_schema.replication_applier_configuration` | ❌ | Configuration parameters for replication applier on replica |
| `performance_schema.replication_applier_filters` | ❌ | Channel-specific replication filters on current replica |
| `performance_schema.replication_applier_global_filters` | ❌ | Global replication filters on current replica |
| `performance_schema.replication_applier_status` | ❌ | Current status of replication applier on replica |
| `performance_schema.replication_applier_status_by_coordinator` | ❌ | SQL or coordinator thread applier status |
| `performance_schema.replication_applier_status_by_worker` | ❌ | Worker thread applier status |
| `performance_schema.replication_asynchronous_connection_failover` | ❌ | Source lists for asynchronous connection failover mechanism |
| `performance_schema.replication_asynchronous_connection_failover_managed` | ❌ | Table shape and diagnostics |
| `performance_schema.replication_connection_configuration` | ❌ | Configuration parameters for connecting to source |
| `performance_schema.replication_connection_status` | ❌ | Current status of connection to source |
| `performance_schema.replication_group_communication_information` | ❌ | Replication group configuration options |
| `performance_schema.replication_group_configuration_version` | ❌ | Table shape and diagnostics |
| `performance_schema.replication_group_member_actions` | ❌ | Table shape and diagnostics |
| `performance_schema.replication_group_member_stats` | ❌ | Replication group member statistics |
| `performance_schema.replication_group_members` | ❌ | Replication group member network and status |
| `performance_schema.rwlock_instances` | ❌ | Lock synchronization object instances |
| `performance_schema.session_account_connect_attrs` | ❌ | Connection attributes per for current session |
| `performance_schema.session_connect_attrs` | ❌ | Connection attributes for all sessions |
| `performance_schema.session_status` | 🟡 | Queryable current-session rows from MyLite's supported status registry, including observed session-only status rows and Performance Schema command-counter filtering; live per-thread accounting remains unsupported |
| `performance_schema.session_variables` | 🟡 | Queryable current-session rows from MyLite's supported system-variable registry, including session overrides; thread-specific variable tables remain unsupported |
| `performance_schema.setup_actors` | 🟡 | Read-only MySQL-shaped default actor row and primary-key metadata; no mutable foreground-thread instrumentation setup |
| `performance_schema.setup_consumers` | 🟡 | Read-only MySQL 8.4.9 default consumer rows and primary-key metadata; no mutable consumer state |
| `performance_schema.setup_instruments` | ❌ | Table shape and diagnostics |
| `performance_schema.setup_loggers` | 🟡 | Read-only MySQL 8.4.9 default logger row with MySQL-shaped metadata; no mutable telemetry logger configuration |
| `performance_schema.setup_meters` | 🟡 | Read-only MySQL 8.4.9 default meter rows with MySQL-shaped metadata and HASH primary-key introspection; no mutable telemetry meter configuration |
| `performance_schema.setup_objects` | 🟡 | Read-only MySQL 8.4.9 default object-filter rows and multi-column `OBJECT` unique-index metadata; no mutable instrumentation filters |
| `performance_schema.setup_threads` | 🟡 | Read-only MySQL 8.4.9 default setup-thread class rows and HASH primary-key metadata; no mutable thread instrumentation setup |
| `performance_schema.socket_instances` | ❌ | Active connection instances |
| `performance_schema.socket_summary_by_event_name` | ❌ | Socket waits and I/O per event name |
| `performance_schema.socket_summary_by_instance` | ❌ | Socket waits and I/O per instance |
| `performance_schema.status_by_account` | ❌ | Session status variables per account |
| `performance_schema.status_by_host` | ❌ | Session status variables per host name |
| `performance_schema.status_by_thread` | ❌ | Session status variables per session |
| `performance_schema.status_by_user` | ❌ | Session status variables per user name |
| `performance_schema.table_handles` | ❌ | Table locks and lock requests |
| `performance_schema.table_io_waits_summary_by_index_usage` | ❌ | Table I/O waits per index |
| `performance_schema.table_io_waits_summary_by_table` | ❌ | Table I/O waits per table |
| `performance_schema.table_lock_waits_summary_by_table` | ❌ | Table lock waits per table |
| `performance_schema.threads` | ❌ | Information about server threads |
| `performance_schema.tls_channel_status` | ❌ | TLS status for each connection interface |
| `performance_schema.tp_thread_group_state` | ❌ | Thread pool thread group states |
| `performance_schema.tp_thread_group_stats` | ❌ | Thread pool thread group statistics |
| `performance_schema.tp_thread_state` | ❌ | Thread pool thread information |
| `performance_schema.user_defined_functions` | 🟡 | Read-only MySQL 8.4.9 default component/plugin rows with MySQL-shaped metadata; no `CREATE FUNCTION` registry lifecycle |
| `performance_schema.user_variables_by_thread` | 🟡 | Current-connection user variables with MySQL-shaped metadata and HASH primary-key introspection; no cross-connection visibility or binary-longblob fidelity in this row path |
| `performance_schema.users` | ❌ | Connection stats per user name |
| `performance_schema.variables_by_thread` | ❌ | Session system variables per session |
| `performance_schema.variables_info` | ❌ | How system variables were most recently set |
| `performance_schema.tp_connections` | ❌ | Thread pool connection state and queue information |

[Back to compatibility overview](../../COMPATIBILITY.md)

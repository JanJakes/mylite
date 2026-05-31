# sys schema objects

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

The `sys` schema name is exposed in the limited built-in schema catalog and can
be selected with `USE sys`. The MySQL 8.4.9 target runtime's `sys` table and
view names are exposed as metadata-only rows through
`INFORMATION_SCHEMA.TABLES`, `SHOW TABLES`, `SHOW FULL TABLES`, and
`SHOW TABLE STATUS`, but the tables and views remain non-queryable and
unsupported unless listed otherwise below. The current exceptions are a
limited read-only `sys.sys_config` synthetic table with MySQL-shaped default
rows and metadata and a limited read-only `sys.version` synthetic view with the
observed MySQL 8.4.9 version row and metadata, plus a limited read-only
`sys.schema_auto_increment_columns` synthetic view over MyLite user-table
auto-increment descriptors, and a limited read-only
`sys.schema_index_statistics` / `sys.x$schema_index_statistics` synthetic
views over descriptor index inventory with zero wait counters, a limited
read-only `sys.schema_object_overview` synthetic view over current MyLite
object metadata descriptors, and limited read-only
`sys.schema_redundant_indexes` / `sys.x$schema_flattened_keys` synthetic views
over persistent user-table BTREE index descriptors, plus limited read-only
empty `sys.schema_table_lock_waits` /
`sys.x$schema_table_lock_waits` synthetic metadata-lock wait placeholders, and
limited read-only `sys.schema_table_statistics` /
`sys.x$schema_table_statistics` synthetic table-statistics views over
descriptor table inventory with zero wait counters, plus limited read-only
`sys.schema_table_statistics_with_buffer` /
`sys.x$schema_table_statistics_with_buffer` synthetic table-statistics views
with zero wait and buffer counters, plus limited read-only empty
`sys.schema_tables_with_full_table_scans` /
`sys.x$schema_tables_with_full_table_scans` synthetic full-table-scan
placeholders, plus a limited read-only empty
`sys.ps_check_lost_instrumentation` synthetic view over
lost-instrumentation Performance Schema status metadata, and a limited
read-only `sys.schema_unused_indexes` synthetic view over persistent
user-table non-unique index descriptors.
MyLite rejects schema, table, index, rename,
truncate, and single-table DML writes targeting `sys` with
`3552 / HY000` system-schema diagnostics as a stricter embedded-design
decision; MySQL 8.4.9 permits some `root` temporary-table writes in `sys`.

| Table | Status | Notes |
| --- | --- | --- |
| `sys.sys_config` | 🟡 | Limited read-only synthetic sys configuration table with the six MySQL 8.4.9 default rows, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS`, and `SHOW TABLE STATUS` metadata; no writable sys configuration, sys trigger execution, sys functions, sys procedures, sys views, Performance Schema-backed values, privilege filtering, or persisted sys table storage |
| `sys.sys_config_insert_set_user` | 🟡 | Metadata-only built-in trigger row exposed through `INFORMATION_SCHEMA.TRIGGERS` and `SHOW TRIGGERS`; no trigger execution, trigger DDL, writable `sys.sys_config` side effects, or persisted trigger descriptor |
| `sys.sys_config_update_set_user` | 🟡 | Metadata-only built-in trigger row exposed through `INFORMATION_SCHEMA.TRIGGERS` and `SHOW TRIGGERS`; no trigger execution, trigger DDL, writable `sys.sys_config` side effects, or persisted trigger descriptor |
| `sys.host_summary` | ❌ | View shape and diagnostics |
| `sys.x$host_summary` | ❌ | View shape and diagnostics |
| `sys.host_summary_by_file_io` | ❌ | View shape and diagnostics |
| `sys.x$host_summary_by_file_io` | ❌ | View shape and diagnostics |
| `sys.host_summary_by_file_io_type` | ❌ | View shape and diagnostics |
| `sys.x$host_summary_by_file_io_type` | ❌ | View shape and diagnostics |
| `sys.host_summary_by_stages` | ❌ | View shape and diagnostics |
| `sys.x$host_summary_by_stages` | ❌ | View shape and diagnostics |
| `sys.host_summary_by_statement_latency` | ❌ | View shape and diagnostics |
| `sys.x$host_summary_by_statement_latency` | ❌ | View shape and diagnostics |
| `sys.host_summary_by_statement_type` | ❌ | View shape and diagnostics |
| `sys.x$host_summary_by_statement_type` | ❌ | View shape and diagnostics |
| `sys.innodb_buffer_stats_by_schema` | ❌ | View shape and diagnostics |
| `sys.x$innodb_buffer_stats_by_schema` | ❌ | View shape and diagnostics |
| `sys.innodb_buffer_stats_by_table` | ❌ | View shape and diagnostics |
| `sys.x$innodb_buffer_stats_by_table` | ❌ | View shape and diagnostics |
| `sys.innodb_lock_waits` | ❌ | View shape and diagnostics |
| `sys.x$innodb_lock_waits` | ❌ | View shape and diagnostics |
| `sys.io_by_thread_by_latency` | ❌ | View shape and diagnostics |
| `sys.x$io_by_thread_by_latency` | ❌ | View shape and diagnostics |
| `sys.io_global_by_file_by_bytes` | ❌ | View shape and diagnostics |
| `sys.x$io_global_by_file_by_bytes` | ❌ | View shape and diagnostics |
| `sys.io_global_by_file_by_latency` | ❌ | View shape and diagnostics |
| `sys.x$io_global_by_file_by_latency` | ❌ | View shape and diagnostics |
| `sys.io_global_by_wait_by_bytes` | ❌ | View shape and diagnostics |
| `sys.x$io_global_by_wait_by_bytes` | ❌ | View shape and diagnostics |
| `sys.io_global_by_wait_by_latency` | ❌ | View shape and diagnostics |
| `sys.x$io_global_by_wait_by_latency` | ❌ | View shape and diagnostics |
| `sys.latest_file_io` | ❌ | View shape and diagnostics |
| `sys.x$latest_file_io` | ❌ | View shape and diagnostics |
| `sys.memory_by_host_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.x$memory_by_host_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.memory_by_thread_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.x$memory_by_thread_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.memory_by_user_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.x$memory_by_user_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.memory_global_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.x$memory_global_by_current_bytes` | ❌ | View shape and diagnostics |
| `sys.memory_global_total` | ❌ | View shape and diagnostics |
| `sys.x$memory_global_total` | ❌ | View shape and diagnostics |
| `sys.metrics` | ❌ | View shape and diagnostics |
| `sys.processlist` | ❌ | View shape and diagnostics |
| `sys.x$processlist` | ❌ | View shape and diagnostics |
| `sys.x$ps_digest_95th_percentile_by_avg_us` | ❌ | Helper view shape and diagnostics |
| `sys.x$ps_digest_avg_latency_distribution` | ❌ | Helper view shape and diagnostics |
| `sys.x$ps_schema_table_statistics_io` | ❌ | Helper view shape and diagnostics |
| `sys.ps_check_lost_instrumentation` | 🟡 | Limited read-only empty lost-instrumentation check view with MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependency on `performance_schema.global_status`, empty index/constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no Performance Schema lost-instrumentation counters, positive rows, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.schema_auto_increment_columns` | 🟡 | Limited read-only synthetic auto-increment inventory view returning one row per supported persistent user base-table `AUTO_INCREMENT` column, with MySQL-shaped signedness, maximum-value, next-value, and ratio fields, default MySQL view ordering, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependencies on `COLUMNS` and `TABLES`, empty index/constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no Performance Schema-backed sys view execution, temporary-table rows, exact InnoDB stats-cache behavior for every empty-table edge case, privilege/definer enforcement, broader sys views, or sys helper functions |
| `sys.schema_index_statistics` | 🟡 | Limited read-only synthetic formatted index-statistics view returning one zero-counter row per supported mysql/sys system-table index descriptor and persistent user base-table index descriptor, with formatted zero latency strings, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependency on `performance_schema.table_io_waits_summary_by_index_usage`, empty constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no Performance Schema wait collection, real latency accumulation, temporary-table rows, unsupported Performance Schema/system-table indexes, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.x$schema_index_statistics` | 🟡 | Limited read-only synthetic raw index-statistics view returning the same descriptor-backed rows as `sys.schema_index_statistics`, but with raw unsigned integer zero latency counters instead of formatted latency strings; same metadata and unsupported behavior as the formatted view |
| `sys.schema_object_overview` | 🟡 | Limited read-only synthetic object summary view returning grouped counts for built-in table-directory entries, supported mysql/sys system-table BTREE index metadata, persistent user base tables, persistent user views, persistent user index key parts, and the metadata-only `sys.sys_config` triggers, with MySQL-shaped default ordering, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependencies on `EVENTS`, `ROUTINES`, `STATISTICS`, `TABLES`, and `TRIGGERS`, empty constraint/routine-dependency metadata for the view itself, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no sys routine/function inventory rows, event rows, Performance Schema HASH index inventory, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.schema_redundant_indexes` | 🟡 | Limited read-only synthetic redundant-index view derived from persistent user base-table BTREE index descriptors, with MySQL-shaped same-column unique dominance, same-column name-order dominance, left-prefix dominance, prefix-key `subpart_exists`, generated `ALTER TABLE ... DROP INDEX` text, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependency on `sys.x$schema_flattened_keys`, empty constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no physical sys view execution, temporary-table rows, built-in-schema rows, FULLTEXT/SPATIAL/functional indexes, privilege/definer enforcement, or broader sys view execution |
| `sys.x$schema_flattened_keys` | 🟡 | Limited read-only synthetic helper view returning one row per persistent user base-table BTREE index descriptor with MySQL-shaped nonunique, prefix-key, and comma-separated key-part column metadata; same metadata surface and unsupported behavior as `sys.schema_redundant_indexes`, except its `VIEW_TABLE_USAGE` dependency points to `INFORMATION_SCHEMA.STATISTICS` |
| `sys.schema_table_lock_waits` | 🟡 | Limited read-only empty synthetic metadata-lock wait view with MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependencies on `performance_schema.events_statements_current`, `performance_schema.metadata_locks`, `performance_schema.threads`, and `sys.sys_config`, `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` dependencies on `sys.format_statement` and `sys.ps_thread_account`, empty constraint metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no live Performance Schema metadata-lock waits, blocking session discovery, `KILL` behavior, sys helper-function execution, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.x$schema_table_lock_waits` | 🟡 | Limited read-only empty raw metadata-lock wait view with the same column metadata and unsupported behavior as `sys.schema_table_lock_waits`; its `VIEW_TABLE_USAGE` dependencies point to `performance_schema.events_statements_current`, `performance_schema.metadata_locks`, and `performance_schema.threads`, and its `VIEW_ROUTINE_USAGE` dependency points to `sys.ps_thread_account` |
| `sys.schema_table_statistics` | 🟡 | Limited read-only synthetic formatted table-statistics view returning one zero-counter row per supported mysql system-table descriptor, `sys.sys_config`, and persistent user base-table descriptor, with formatted zero latency and byte strings, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependencies on `performance_schema.table_io_waits_summary_by_table` and `sys.x$ps_schema_table_statistics_io`, empty constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no Performance Schema table wait collection, real row/latency/byte accumulation, temporary-table rows, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.x$schema_table_statistics` | 🟡 | Limited read-only synthetic raw table-statistics view returning the same descriptor-backed rows as `sys.schema_table_statistics`, but with raw numeric zero latency and byte counters instead of formatted strings; same metadata and unsupported behavior as the formatted view |
| `sys.schema_table_statistics_with_buffer` | 🟡 | Limited read-only synthetic formatted table-statistics-with-buffer view returning one zero-counter row per supported mysql system-table descriptor, `sys.sys_config`, and persistent user base-table descriptor, with formatted zero latency and byte strings, zero InnoDB buffer placeholders, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependencies on `performance_schema.table_io_waits_summary_by_table`, `sys.x$ps_schema_table_statistics_io`, and `sys.x$innodb_buffer_stats_by_table`, empty constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no Performance Schema table wait collection, InnoDB buffer-pool table accounting, real row/latency/byte/page accumulation, temporary-table rows, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.x$schema_table_statistics_with_buffer` | 🟡 | Limited read-only synthetic raw table-statistics-with-buffer view returning the same descriptor-backed rows as `sys.schema_table_statistics_with_buffer`, but with raw numeric zero latency, byte, and buffer counters instead of formatted strings; same metadata and unsupported behavior as the formatted view |
| `sys.schema_tables_with_full_table_scans` | 🟡 | Limited read-only empty formatted full-table-scan placeholder view with MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependency on `performance_schema.table_io_waits_summary_by_index_usage`, empty constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no Performance Schema table I/O wait collection, live full-table-scan detection, real row/latency accumulation, temporary-table rows, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.x$schema_tables_with_full_table_scans` | 🟡 | Limited read-only empty raw full-table-scan placeholder view with the same metadata and unsupported behavior as `sys.schema_tables_with_full_table_scans`, except its `latency` column exposes raw unsigned integer metadata instead of formatted latency text |
| `sys.schema_unused_indexes` | 🟡 | Limited read-only synthetic unused-index view returning descriptor-backed rows for supported persistent user base-table non-unique indexes, with MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` dependencies on `INFORMATION_SCHEMA.STATISTICS` and `performance_schema.table_io_waits_summary_by_index_usage`, empty constraint/routine-dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no Performance Schema index-usage collection, removal of rows after index reads, temporary-table rows, privilege/definer enforcement, physical sys views, or broader sys view execution |
| `sys.session` | ❌ | View shape and diagnostics |
| `sys.x$session` | ❌ | View shape and diagnostics |
| `sys.session_ssl_status` | ❌ | View shape and diagnostics |
| `sys.statement_analysis` | ❌ | View shape and diagnostics |
| `sys.x$statement_analysis` | ❌ | View shape and diagnostics |
| `sys.statements_with_errors_or_warnings` | ❌ | View shape and diagnostics |
| `sys.x$statements_with_errors_or_warnings` | ❌ | View shape and diagnostics |
| `sys.statements_with_full_table_scans` | ❌ | View shape and diagnostics |
| `sys.x$statements_with_full_table_scans` | ❌ | View shape and diagnostics |
| `sys.statements_with_runtimes_in_95th_percentile` | ❌ | View shape and diagnostics |
| `sys.x$statements_with_runtimes_in_95th_percentile` | ❌ | View shape and diagnostics |
| `sys.statements_with_sorting` | ❌ | View shape and diagnostics |
| `sys.x$statements_with_sorting` | ❌ | View shape and diagnostics |
| `sys.statements_with_temp_tables` | ❌ | View shape and diagnostics |
| `sys.x$statements_with_temp_tables` | ❌ | View shape and diagnostics |
| `sys.user_summary` | ❌ | View shape and diagnostics |
| `sys.x$user_summary` | ❌ | View shape and diagnostics |
| `sys.user_summary_by_file_io` | ❌ | View shape and diagnostics |
| `sys.x$user_summary_by_file_io` | ❌ | View shape and diagnostics |
| `sys.user_summary_by_file_io_type` | ❌ | View shape and diagnostics |
| `sys.x$user_summary_by_file_io_type` | ❌ | View shape and diagnostics |
| `sys.user_summary_by_stages` | ❌ | View shape and diagnostics |
| `sys.x$user_summary_by_stages` | ❌ | View shape and diagnostics |
| `sys.user_summary_by_statement_latency` | ❌ | View shape and diagnostics |
| `sys.x$user_summary_by_statement_latency` | ❌ | View shape and diagnostics |
| `sys.user_summary_by_statement_type` | ❌ | View shape and diagnostics |
| `sys.x$user_summary_by_statement_type` | ❌ | View shape and diagnostics |
| `sys.version` | 🟡 | Limited read-only synthetic version view returning `sys_version = '2.1.3'` and MyLite's MySQL-compatible server version, with MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX`, `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.VIEWS`, empty index/constraint/dependency metadata, `INFORMATION_SCHEMA.TABLES`, `SHOW CREATE VIEW` / `SHOW CREATE TABLE`, and `SHOW TABLE STATUS`; no persisted view descriptor, privilege/definer enforcement, broader sys views, or sys helper functions |
| `sys.wait_classes_global_by_avg_latency` | ❌ | View shape and diagnostics |
| `sys.x$wait_classes_global_by_avg_latency` | ❌ | View shape and diagnostics |
| `sys.wait_classes_global_by_latency` | ❌ | View shape and diagnostics |
| `sys.x$wait_classes_global_by_latency` | ❌ | View shape and diagnostics |
| `sys.waits_by_host_by_latency` | ❌ | View shape and diagnostics |
| `sys.x$waits_by_host_by_latency` | ❌ | View shape and diagnostics |
| `sys.waits_by_user_by_latency` | ❌ | View shape and diagnostics |
| `sys.x$waits_by_user_by_latency` | ❌ | View shape and diagnostics |
| `sys.waits_global_by_latency` | ❌ | View shape and diagnostics |
| `sys.x$waits_global_by_latency` | ❌ | View shape and diagnostics |
| `sys.create_synonym_db()` | ❌ | Procedure behavior and diagnostics |
| `sys.diagnostics()` | ❌ | Procedure behavior and diagnostics |
| `sys.execute_prepared_stmt()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_disable_background_threads()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_disable_consumer()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_disable_instrument()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_disable_thread()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_enable_background_threads()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_enable_consumer()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_enable_instrument()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_enable_thread()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_reload_saved()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_reset_to_default()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_save()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_show_disabled()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_show_disabled_consumers()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_show_disabled_instruments()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_show_enabled()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_show_enabled_consumers()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_setup_show_enabled_instruments()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_statement_avg_latency_histogram()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_trace_statement_digest()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_trace_thread()` | ❌ | Procedure behavior and diagnostics |
| `sys.ps_truncate_all_tables()` | ❌ | Procedure behavior and diagnostics |
| `sys.statement_performance_analyzer()` | ❌ | Procedure behavior and diagnostics |
| `sys.table_exists()` | ❌ | Procedure behavior and diagnostics |
| `sys.extract_schema_from_file_name()` | ❌ | Function behavior and diagnostics |
| `sys.extract_table_from_file_name()` | ❌ | Function behavior and diagnostics |
| `sys.format_bytes()` | ❌ | Function behavior and diagnostics |
| `sys.format_path()` | ❌ | Function behavior and diagnostics |
| `sys.format_statement()` | ❌ | Function behavior and diagnostics |
| `sys.format_time()` | ❌ | Function behavior and diagnostics |
| `sys.list_add()` | ❌ | Function behavior and diagnostics |
| `sys.list_drop()` | ❌ | Function behavior and diagnostics |
| `sys.ps_is_account_enabled()` | ❌ | Function behavior and diagnostics |
| `sys.ps_is_consumer_enabled()` | ❌ | Function behavior and diagnostics |
| `sys.ps_is_instrument_default_enabled()` | ❌ | Function behavior and diagnostics |
| `sys.ps_is_instrument_default_timed()` | ❌ | Function behavior and diagnostics |
| `sys.ps_is_thread_instrumented()` | ❌ | Function behavior and diagnostics |
| `sys.ps_thread_account()` | ❌ | Function behavior and diagnostics |
| `sys.ps_thread_id()` | ❌ | Function behavior and diagnostics |
| `sys.ps_thread_stack()` | ❌ | Function behavior and diagnostics |
| `sys.ps_thread_trx_info()` | ❌ | Function behavior and diagnostics |
| `sys.quote_identifier()` | ❌ | Function behavior and diagnostics |
| `sys.sys_get_config()` | ❌ | Function behavior and diagnostics |
| `sys.version_major()` | ❌ | Function behavior and diagnostics |
| `sys.version_minor()` | ❌ | Function behavior and diagnostics |
| `sys.version_patch()` | ❌ | Function behavior and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)

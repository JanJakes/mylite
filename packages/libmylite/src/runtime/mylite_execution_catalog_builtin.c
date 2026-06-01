#include "mylite_execution_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool catalog_text_equals_ascii_case_insensitive(const char *left, const char *right);
static char catalog_ascii_lower(char byte);

static const struct mylite_execution_catalog_builtin_schema builtin_schema_descriptors[] = {
    {"information_schema", "utf8mb3", "utf8mb3_general_ci"},
    {"mysql", "utf8mb4", "utf8mb4_0900_ai_ci"},
    {"performance_schema", "utf8mb4", "utf8mb4_0900_ai_ci"},
    {"sys", "utf8mb4", "utf8mb4_0900_ai_ci"},
};

static const char *const builtin_tablespace_extension_names[] = {
    "innodb_system",
    "innodb_temporary",
    "innodb_undo_001",
    "innodb_undo_002",
    "mysql",
    "sys/sys_config",
};

static const struct mylite_execution_catalog_files_row builtin_information_schema_files_rows[] = {
    {"0", "./ibdata1", "TABLESPACE", "innodb_system", "2", "12", "12582912", "67108864", "6291456"},
    {"4294967293",
     "./ibtmp1",
     "TEMPORARY",
     "innodb_temporary",
     "2",
     "12",
     "12582912",
     "67108864",
     "6291456"},
    {"4294967294", "./mysql.ibd", "TABLESPACE", "mysql", "1", "31", "0", "1048576", "4194304"},
    {"1", "./sys/sys_config.ibd", "TABLESPACE", "sys/sys_config", "0", "0", "0", "1048576", "0"},
    {"4294967279",
     "./undo_001",
     "UNDO LOG",
     "innodb_undo_001",
     "2",
     "16",
     "16777216",
     "16777216",
     "6291456"},
    {"4294967278",
     "./undo_002",
     "UNDO LOG",
     "innodb_undo_002",
     "2",
     "16",
     "16777216",
     "16777216",
     "6291456"},
};

static const struct mylite_execution_catalog_innodb_tablespace_row
    builtin_innodb_tablespace_rows[] = {
        {"0", "innodb_system", "ibdata1", "18432", "System"},
        {"4294967279", "innodb_undo_001", "./undo_001", "0", "Single"},
        {"4294967278", "innodb_undo_002", "./undo_002", "0", "Single"},
        {"1", "sys/sys_config", "./sys/sys_config.ibd", "16417", "Single"},
};

static const struct mylite_execution_catalog_innodb_tablespace_full_row
    builtin_innodb_tablespace_full_rows[] = {
        {"1",
         "sys/sys_config",
         "16417",
         "Dynamic",
         "16384",
         "0",
         "Single",
         "4096",
         "114688",
         "114688",
         "0",
         "8.4.9",
         "1",
         "N",
         "normal"},
        {"4294967278",
         "innodb_undo_002",
         "0",
         "Undo",
         "16384",
         "0",
         "Undo",
         "4096",
         "16777216",
         "16777216",
         "0",
         "8.4.9",
         "1",
         "N",
         "active"},
        {"4294967279",
         "innodb_undo_001",
         "0",
         "Undo",
         "16384",
         "0",
         "Undo",
         "4096",
         "16777216",
         "16777216",
         "0",
         "8.4.9",
         "1",
         "N",
         "active"},
        {"4294967293",
         "innodb_temporary",
         "4096",
         "Compact or Redundant",
         "16384",
         "0",
         "System",
         "4096",
         "12582912",
         "12582912",
         "0",
         "8.4.9",
         "1",
         "N",
         "normal"},
        {"4294967294",
         "mysql",
         "18432",
         "Any",
         "16384",
         "0",
         "General",
         "4096",
         "32505856",
         "32509952",
         "0",
         "8.4.9",
         "1",
         "N",
         "normal"},
};

static const struct mylite_execution_catalog_innodb_session_temp_tablespace_row
    builtin_innodb_session_temp_tablespace_rows[] = {
        {"4243767281", "./#innodb_temp/temp_1.ibt", "INACTIVE", "NONE"},
        {"4243767282", "./#innodb_temp/temp_2.ibt", "INACTIVE", "NONE"},
        {"4243767283", "./#innodb_temp/temp_3.ibt", "INACTIVE", "NONE"},
        {"4243767284", "./#innodb_temp/temp_4.ibt", "INACTIVE", "NONE"},
        {"4243767285", "./#innodb_temp/temp_5.ibt", "INACTIVE", "NONE"},
        {"4243767286", "./#innodb_temp/temp_6.ibt", "INACTIVE", "NONE"},
        {"4243767287", "./#innodb_temp/temp_7.ibt", "INACTIVE", "NONE"},
        {"4243767288", "./#innodb_temp/temp_8.ibt", "INACTIVE", "NONE"},
        {"4243767289", "./#innodb_temp/temp_9.ibt", "INACTIVE", "NONE"},
        {"4243767290", "./#innodb_temp/temp_10.ibt", "ACTIVE", "INTRINSIC"},
};

static const char *const innodb_ft_default_stopwords[] = {
    "a",    "about", "an",  "are",  "as",   "at",    "be",  "by",   "com",  "de",  "en",   "for",
    "from", "how",   "i",   "in",   "is",   "it",    "la",  "of",   "on",   "or",  "that", "the",
    "this", "to",    "was", "what", "when", "where", "who", "will", "with", "und", "the",  "www",
};

static const char *const innodb_compressed_page_sizes[] = {
    "1024",
    "2048",
    "4096",
    "8192",
    "16384",
};

static const struct mylite_execution_catalog_st_unit_of_measure_row st_units_of_measure_rows[] = {
    {"British chain (Benoit 1895 A)", "20.1167824"},
    {"British chain (Benoit 1895 B)", "20.116782494375872"},
    {"British chain (Sears 1922 truncated)", "20.116756"},
    {"British chain (Sears 1922)", "20.116765121552632"},
    {"British foot (1865)", "0.30480083333333335"},
    {"British foot (1936)", "0.3048007491"},
    {"British foot (Benoit 1895 A)", "0.3047997333333333"},
    {"British foot (Benoit 1895 B)", "0.30479973476327077"},
    {"British foot (Sears 1922 truncated)", "0.30479933333333337"},
    {"British foot (Sears 1922)", "0.3047994715386762"},
    {"British link (Benoit 1895 A)", "0.201167824"},
    {"British link (Benoit 1895 B)", "0.2011678249437587"},
    {"British link (Sears 1922 truncated)", "0.20116756"},
    {"British link (Sears 1922)", "0.2011676512155263"},
    {"British yard (Benoit 1895 A)", "0.9143992"},
    {"British yard (Benoit 1895 B)", "0.9143992042898124"},
    {"British yard (Sears 1922 truncated)", "0.914398"},
    {"British yard (Sears 1922)", "0.9143984146160288"},
    {"centimetre", "0.01"},
    {"chain", "20.1168"},
    {"Clarke's chain", "20.1166195164"},
    {"Clarke's foot", "0.3047972654"},
    {"Clarke's link", "0.201166195164"},
    {"Clarke's yard", "0.9143917962"},
    {"fathom", "1.8288"},
    {"foot", "0.3048"},
    {"German legal metre", "1.0000135965"},
    {"Gold Coast foot", "0.3047997101815088"},
    {"Indian foot", "0.30479951024814694"},
    {"Indian foot (1937)", "0.30479841"},
    {"Indian foot (1962)", "0.3047996"},
    {"Indian foot (1975)", "0.3047995"},
    {"Indian yard", "0.9143985307444408"},
    {"Indian yard (1937)", "0.91439523"},
    {"Indian yard (1962)", "0.9143988"},
    {"Indian yard (1975)", "0.9143985"},
    {"kilometre", "1000"},
    {"link", "0.201168"},
    {"metre", "1"},
    {"millimetre", "0.001"},
    {"nautical mile", "1852"},
    {"Statute mile", "1609.344"},
    {"US survey chain", "20.11684023368047"},
    {"US survey foot", "0.30480060960121924"},
    {"US survey link", "0.2011684023368047"},
    {"US survey mile", "1609.3472186944375"},
    {"yard", "0.9144"},
};

static const char *const builtin_information_schema_table_names[] = {
    "ADMINISTRABLE_ROLE_AUTHORIZATIONS",
    "APPLICABLE_ROLES",
    "CHARACTER_SETS",
    "CHECK_CONSTRAINTS",
    "COLLATIONS",
    "COLLATION_CHARACTER_SET_APPLICABILITY",
    "COLUMNS",
    "COLUMNS_EXTENSIONS",
    "COLUMN_PRIVILEGES",
    "COLUMN_STATISTICS",
    "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS",
    "ENABLED_ROLES",
    "ENGINES",
    "EVENTS",
    "FILES",
    "INNODB_BUFFER_PAGE",
    "INNODB_BUFFER_PAGE_LRU",
    "INNODB_BUFFER_POOL_STATS",
    "INNODB_CACHED_INDEXES",
    "INNODB_CMP",
    "INNODB_CMPMEM",
    "INNODB_CMPMEM_RESET",
    "INNODB_CMP_PER_INDEX",
    "INNODB_CMP_PER_INDEX_RESET",
    "INNODB_CMP_RESET",
    "INNODB_COLUMNS",
    "INNODB_DATAFILES",
    "INNODB_FIELDS",
    "INNODB_FOREIGN",
    "INNODB_FOREIGN_COLS",
    "INNODB_FT_BEING_DELETED",
    "INNODB_FT_CONFIG",
    "INNODB_FT_DEFAULT_STOPWORD",
    "INNODB_FT_DELETED",
    "INNODB_FT_INDEX_CACHE",
    "INNODB_FT_INDEX_TABLE",
    "INNODB_INDEXES",
    "INNODB_METRICS",
    "INNODB_SESSION_TEMP_TABLESPACES",
    "INNODB_TABLES",
    "INNODB_TABLESPACES",
    "INNODB_TABLESPACES_BRIEF",
    "INNODB_TABLESTATS",
    "INNODB_TEMP_TABLE_INFO",
    "INNODB_TRX",
    "INNODB_VIRTUAL",
    "KEYWORDS",
    "KEY_COLUMN_USAGE",
    "OPTIMIZER_TRACE",
    "PARAMETERS",
    "PARTITIONS",
    "PLUGINS",
    "PROCESSLIST",
    "PROFILING",
    "REFERENTIAL_CONSTRAINTS",
    "RESOURCE_GROUPS",
    "ROLE_COLUMN_GRANTS",
    "ROLE_ROUTINE_GRANTS",
    "ROLE_TABLE_GRANTS",
    "ROUTINES",
    "SCHEMATA",
    "SCHEMATA_EXTENSIONS",
    "SCHEMA_PRIVILEGES",
    "STATISTICS",
    "ST_GEOMETRY_COLUMNS",
    "ST_SPATIAL_REFERENCE_SYSTEMS",
    "ST_UNITS_OF_MEASURE",
    "TABLES",
    "TABLESPACES_EXTENSIONS",
    "TABLES_EXTENSIONS",
    "TABLE_CONSTRAINTS",
    "TABLE_CONSTRAINTS_EXTENSIONS",
    "TABLE_PRIVILEGES",
    "TRIGGERS",
    "USER_ATTRIBUTES",
    "USER_PRIVILEGES",
    "VIEWS",
    "VIEW_ROUTINE_USAGE",
    "VIEW_TABLE_USAGE",
};

static const char *const builtin_mysql_table_names[] = {
    "columns_priv",
    "component",
    "db",
    "default_roles",
    "engine_cost",
    "func",
    "general_log",
    "global_grants",
    "gtid_executed",
    "help_category",
    "help_keyword",
    "help_relation",
    "help_topic",
    "innodb_index_stats",
    "innodb_table_stats",
    "ndb_binlog_index",
    "password_history",
    "plugin",
    "procs_priv",
    "proxies_priv",
    "replication_asynchronous_connection_failover",
    "replication_asynchronous_connection_failover_managed",
    "replication_group_configuration_version",
    "replication_group_member_actions",
    "role_edges",
    "server_cost",
    "servers",
    "slave_master_info",
    "slave_relay_log_info",
    "slave_worker_info",
    "slow_log",
    "tables_priv",
    "time_zone",
    "time_zone_leap_second",
    "time_zone_name",
    "time_zone_transition",
    "time_zone_transition_type",
    "user",
};

static const char *const builtin_performance_schema_table_names[] = {
    "accounts",
    "binary_log_transaction_compression_stats",
    "cond_instances",
    "data_lock_waits",
    "data_locks",
    "error_log",
    "events_errors_summary_by_account_by_error",
    "events_errors_summary_by_host_by_error",
    "events_errors_summary_by_thread_by_error",
    "events_errors_summary_by_user_by_error",
    "events_errors_summary_global_by_error",
    "events_stages_current",
    "events_stages_history",
    "events_stages_history_long",
    "events_stages_summary_by_account_by_event_name",
    "events_stages_summary_by_host_by_event_name",
    "events_stages_summary_by_thread_by_event_name",
    "events_stages_summary_by_user_by_event_name",
    "events_stages_summary_global_by_event_name",
    "events_statements_current",
    "events_statements_histogram_by_digest",
    "events_statements_histogram_global",
    "events_statements_history",
    "events_statements_history_long",
    "events_statements_summary_by_account_by_event_name",
    "events_statements_summary_by_digest",
    "events_statements_summary_by_host_by_event_name",
    "events_statements_summary_by_program",
    "events_statements_summary_by_thread_by_event_name",
    "events_statements_summary_by_user_by_event_name",
    "events_statements_summary_global_by_event_name",
    "events_transactions_current",
    "events_transactions_history",
    "events_transactions_history_long",
    "events_transactions_summary_by_account_by_event_name",
    "events_transactions_summary_by_host_by_event_name",
    "events_transactions_summary_by_thread_by_event_name",
    "events_transactions_summary_by_user_by_event_name",
    "events_transactions_summary_global_by_event_name",
    "events_waits_current",
    "events_waits_history",
    "events_waits_history_long",
    "events_waits_summary_by_account_by_event_name",
    "events_waits_summary_by_host_by_event_name",
    "events_waits_summary_by_instance",
    "events_waits_summary_by_thread_by_event_name",
    "events_waits_summary_by_user_by_event_name",
    "events_waits_summary_global_by_event_name",
    "file_instances",
    "file_summary_by_event_name",
    "file_summary_by_instance",
    "global_status",
    "global_variables",
    "host_cache",
    "hosts",
    "innodb_redo_log_files",
    "keyring_component_status",
    "keyring_keys",
    "log_status",
    "memory_summary_by_account_by_event_name",
    "memory_summary_by_host_by_event_name",
    "memory_summary_by_thread_by_event_name",
    "memory_summary_by_user_by_event_name",
    "memory_summary_global_by_event_name",
    "metadata_locks",
    "mutex_instances",
    "objects_summary_global_by_type",
    "performance_timers",
    "persisted_variables",
    "prepared_statements_instances",
    "processlist",
    "replication_applier_configuration",
    "replication_applier_filters",
    "replication_applier_global_filters",
    "replication_applier_status",
    "replication_applier_status_by_coordinator",
    "replication_applier_status_by_worker",
    "replication_asynchronous_connection_failover",
    "replication_asynchronous_connection_failover_managed",
    "replication_connection_configuration",
    "replication_connection_status",
    "replication_group_member_stats",
    "replication_group_members",
    "rwlock_instances",
    "session_account_connect_attrs",
    "session_connect_attrs",
    "session_status",
    "session_variables",
    "setup_actors",
    "setup_consumers",
    "setup_instruments",
    "setup_loggers",
    "setup_meters",
    "setup_metrics",
    "setup_objects",
    "setup_threads",
    "socket_instances",
    "socket_summary_by_event_name",
    "socket_summary_by_instance",
    "status_by_account",
    "status_by_host",
    "status_by_thread",
    "status_by_user",
    "table_handles",
    "table_io_waits_summary_by_index_usage",
    "table_io_waits_summary_by_table",
    "table_lock_waits_summary_by_table",
    "threads",
    "tls_channel_status",
    "user_defined_functions",
    "user_variables_by_thread",
    "users",
    "variables_by_thread",
    "variables_info",
};

static const char *const builtin_sys_table_names[] = {
    "host_summary",
    "host_summary_by_file_io",
    "host_summary_by_file_io_type",
    "host_summary_by_stages",
    "host_summary_by_statement_latency",
    "host_summary_by_statement_type",
    "innodb_buffer_stats_by_schema",
    "innodb_buffer_stats_by_table",
    "innodb_lock_waits",
    "io_by_thread_by_latency",
    "io_global_by_file_by_bytes",
    "io_global_by_file_by_latency",
    "io_global_by_wait_by_bytes",
    "io_global_by_wait_by_latency",
    "latest_file_io",
    "memory_by_host_by_current_bytes",
    "memory_by_thread_by_current_bytes",
    "memory_by_user_by_current_bytes",
    "memory_global_by_current_bytes",
    "memory_global_total",
    "metrics",
    "processlist",
    "ps_check_lost_instrumentation",
    "schema_auto_increment_columns",
    "schema_index_statistics",
    "schema_object_overview",
    "schema_redundant_indexes",
    "schema_table_lock_waits",
    "schema_table_statistics",
    "schema_table_statistics_with_buffer",
    "schema_tables_with_full_table_scans",
    "schema_unused_indexes",
    "session",
    "session_ssl_status",
    "statement_analysis",
    "statements_with_errors_or_warnings",
    "statements_with_full_table_scans",
    "statements_with_runtimes_in_95th_percentile",
    "statements_with_sorting",
    "statements_with_temp_tables",
    "sys_config",
    "user_summary",
    "user_summary_by_file_io",
    "user_summary_by_file_io_type",
    "user_summary_by_stages",
    "user_summary_by_statement_latency",
    "user_summary_by_statement_type",
    "version",
    "wait_classes_global_by_avg_latency",
    "wait_classes_global_by_latency",
    "waits_by_host_by_latency",
    "waits_by_user_by_latency",
    "waits_global_by_latency",
    "x$host_summary",
    "x$host_summary_by_file_io",
    "x$host_summary_by_file_io_type",
    "x$host_summary_by_stages",
    "x$host_summary_by_statement_latency",
    "x$host_summary_by_statement_type",
    "x$innodb_buffer_stats_by_schema",
    "x$innodb_buffer_stats_by_table",
    "x$innodb_lock_waits",
    "x$io_by_thread_by_latency",
    "x$io_global_by_file_by_bytes",
    "x$io_global_by_file_by_latency",
    "x$io_global_by_wait_by_bytes",
    "x$io_global_by_wait_by_latency",
    "x$latest_file_io",
    "x$memory_by_host_by_current_bytes",
    "x$memory_by_thread_by_current_bytes",
    "x$memory_by_user_by_current_bytes",
    "x$memory_global_by_current_bytes",
    "x$memory_global_total",
    "x$processlist",
    "x$ps_digest_95th_percentile_by_avg_us",
    "x$ps_digest_avg_latency_distribution",
    "x$ps_schema_table_statistics_io",
    "x$schema_flattened_keys",
    "x$schema_index_statistics",
    "x$schema_table_lock_waits",
    "x$schema_table_statistics",
    "x$schema_table_statistics_with_buffer",
    "x$schema_tables_with_full_table_scans",
    "x$session",
    "x$statement_analysis",
    "x$statements_with_errors_or_warnings",
    "x$statements_with_full_table_scans",
    "x$statements_with_runtimes_in_95th_percentile",
    "x$statements_with_sorting",
    "x$statements_with_temp_tables",
    "x$user_summary",
    "x$user_summary_by_file_io",
    "x$user_summary_by_file_io_type",
    "x$user_summary_by_stages",
    "x$user_summary_by_statement_latency",
    "x$user_summary_by_statement_type",
    "x$wait_classes_global_by_avg_latency",
    "x$wait_classes_global_by_latency",
    "x$waits_by_host_by_latency",
    "x$waits_by_user_by_latency",
    "x$waits_global_by_latency",
};

static const struct mylite_execution_catalog_builtin_schema_table_directory
    builtin_schema_table_directories[] = {
        {"information_schema",
         builtin_information_schema_table_names,
         sizeof(builtin_information_schema_table_names) /
             sizeof(builtin_information_schema_table_names[0])},
        {"mysql",
         builtin_mysql_table_names,
         sizeof(builtin_mysql_table_names) / sizeof(builtin_mysql_table_names[0])},
        {"performance_schema",
         builtin_performance_schema_table_names,
         sizeof(builtin_performance_schema_table_names) /
             sizeof(builtin_performance_schema_table_names[0])},
        {"sys",
         builtin_sys_table_names,
         sizeof(builtin_sys_table_names) / sizeof(builtin_sys_table_names[0])},
};

static bool catalog_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }
    while (*left != '\0' && *right != '\0') {
        if (catalog_ascii_lower(*left) != catalog_ascii_lower(*right)) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static char catalog_ascii_lower(char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return byte;
}

size_t mylite_execution_catalog_builtin_schema_count(void) {
    return sizeof(builtin_schema_descriptors) / sizeof(builtin_schema_descriptors[0]);
}

const struct mylite_execution_catalog_builtin_schema *mylite_execution_catalog_builtin_schema_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_builtin_schema_count()) {
        return NULL;
    }
    return &builtin_schema_descriptors[index];
}

const struct mylite_execution_catalog_builtin_schema *mylite_execution_catalog_builtin_schema_by_name(
    const char *schema_name
) {
    if (catalog_text_equals_ascii_case_insensitive(schema_name, "information_schema")) {
        return &builtin_schema_descriptors[0];
    }
    if (schema_name == NULL) {
        return NULL;
    }
    for (size_t index = 1U; index < mylite_execution_catalog_builtin_schema_count(); ++index) {
        if (strcmp(schema_name, builtin_schema_descriptors[index].name) == 0) {
            return &builtin_schema_descriptors[index];
        }
    }
    return NULL;
}

size_t mylite_execution_catalog_builtin_schema_table_directory_count(void) {
    return sizeof(builtin_schema_table_directories) / sizeof(builtin_schema_table_directories[0]);
}

const struct mylite_execution_catalog_builtin_schema_table_directory *mylite_execution_catalog_builtin_schema_table_directory_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_builtin_schema_table_directory_count()) {
        return NULL;
    }
    return &builtin_schema_table_directories[index];
}

const struct mylite_execution_catalog_builtin_schema_table_directory *mylite_execution_catalog_builtin_schema_table_directory_by_name(
    const char *schema_name
) {
    if (catalog_text_equals_ascii_case_insensitive(schema_name, "information_schema")) {
        return &builtin_schema_table_directories[0];
    }
    if (schema_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++index) {
        if (strcmp(schema_name, builtin_schema_table_directories[index].schema_name) == 0) {
            return &builtin_schema_table_directories[index];
        }
    }
    return NULL;
}

size_t mylite_execution_catalog_information_schema_files_row_count(void) {
    return sizeof(builtin_information_schema_files_rows) /
           sizeof(builtin_information_schema_files_rows[0]);
}

const struct mylite_execution_catalog_files_row *mylite_execution_catalog_information_schema_files_row_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_information_schema_files_row_count()) {
        return NULL;
    }
    return &builtin_information_schema_files_rows[index];
}

size_t mylite_execution_catalog_innodb_tablespace_row_count(void) {
    return sizeof(builtin_innodb_tablespace_rows) / sizeof(builtin_innodb_tablespace_rows[0]);
}

const struct mylite_execution_catalog_innodb_tablespace_row *mylite_execution_catalog_innodb_tablespace_row_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_innodb_tablespace_row_count()) {
        return NULL;
    }
    return &builtin_innodb_tablespace_rows[index];
}

size_t mylite_execution_catalog_innodb_tablespace_full_row_count(void) {
    return sizeof(builtin_innodb_tablespace_full_rows) /
           sizeof(builtin_innodb_tablespace_full_rows[0]);
}

const struct mylite_execution_catalog_innodb_tablespace_full_row *mylite_execution_catalog_innodb_tablespace_full_row_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_innodb_tablespace_full_row_count()) {
        return NULL;
    }
    return &builtin_innodb_tablespace_full_rows[index];
}

size_t mylite_execution_catalog_innodb_session_temp_tablespace_row_count(void) {
    return sizeof(builtin_innodb_session_temp_tablespace_rows) /
           sizeof(builtin_innodb_session_temp_tablespace_rows[0]);
}

const struct mylite_execution_catalog_innodb_session_temp_tablespace_row *mylite_execution_catalog_innodb_session_temp_tablespace_row_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_innodb_session_temp_tablespace_row_count()) {
        return NULL;
    }
    return &builtin_innodb_session_temp_tablespace_rows[index];
}

size_t mylite_execution_catalog_st_unit_of_measure_row_count(void) {
    return sizeof(st_units_of_measure_rows) / sizeof(st_units_of_measure_rows[0]);
}

const struct mylite_execution_catalog_st_unit_of_measure_row *mylite_execution_catalog_st_unit_of_measure_row_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_st_unit_of_measure_row_count()) {
        return NULL;
    }
    return &st_units_of_measure_rows[index];
}

size_t mylite_execution_catalog_builtin_tablespace_extension_name_count(void) {
    return sizeof(builtin_tablespace_extension_names) /
           sizeof(builtin_tablespace_extension_names[0]);
}

const char *mylite_execution_catalog_builtin_tablespace_extension_name_at(size_t index) {
    if (index >= mylite_execution_catalog_builtin_tablespace_extension_name_count()) {
        return NULL;
    }
    return builtin_tablespace_extension_names[index];
}

size_t mylite_execution_catalog_innodb_compressed_page_size_count(void) {
    return sizeof(innodb_compressed_page_sizes) / sizeof(innodb_compressed_page_sizes[0]);
}

const char *mylite_execution_catalog_innodb_compressed_page_size_at(size_t index) {
    if (index >= mylite_execution_catalog_innodb_compressed_page_size_count()) {
        return NULL;
    }
    return innodb_compressed_page_sizes[index];
}

size_t mylite_execution_catalog_innodb_ft_default_stopword_count(void) {
    return sizeof(innodb_ft_default_stopwords) / sizeof(innodb_ft_default_stopwords[0]);
}

const char *mylite_execution_catalog_innodb_ft_default_stopword_at(size_t index) {
    if (index >= mylite_execution_catalog_innodb_ft_default_stopword_count()) {
        return NULL;
    }
    return innodb_ft_default_stopwords[index];
}

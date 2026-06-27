static int append_information_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct information_schema_catalog_filter *filter
) {
    if (!information_schema_catalog_filter_allows_system_rows(rows->definition, filter)) {
        return MYLITE_OK;
    }

    switch (rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA:
        return append_information_schema_schemata_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA_EXTENSIONS:
        return append_information_schema_schemata_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
        return append_information_schema_tables_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLESPACES_EXTENSIONS:
        return append_information_schema_tablespaces_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_FILES:
        return append_information_schema_files_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_DATAFILES:
        return append_information_schema_innodb_datafiles_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES:
        return append_information_schema_innodb_tablespaces_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_SESSION_TEMP_TABLESPACES:
        return append_information_schema_innodb_session_temp_tablespaces_system_rows(
            database,
            rows
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_RESET:
        return append_information_schema_innodb_cmp_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_POOL_STATS:
        return append_information_schema_innodb_buffer_pool_stats_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM_RESET:
        return append_information_schema_innodb_cmpmem_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DEFAULT_STOPWORD:
        return append_information_schema_innodb_ft_default_stopword_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES_BRIEF:
        return append_information_schema_innodb_tablespaces_brief_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
        return append_information_schema_tables_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
        return append_information_schema_columns_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
        return append_information_schema_columns_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_CHARACTER_SETS:
        return append_information_schema_character_sets_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATIONS:
        return append_information_schema_collations_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATION_CHARACTER_SET_APPLICABILITY:
        return append_information_schema_collation_applicability_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_ENGINES:
        return append_information_schema_engines_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_PLUGINS:
        return append_information_schema_plugins_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_KEYWORDS:
        return append_information_schema_keywords_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_PRIVILEGES:
        return append_information_schema_user_privileges_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_ATTRIBUTES:
        return append_information_schema_user_attributes_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_RESOURCE_GROUPS:
        return append_information_schema_resource_groups_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_UNITS_OF_MEASURE:
        return append_information_schema_st_units_of_measure_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_PARTITIONS:
        return append_information_schema_partitions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS:
        return append_information_schema_table_constraints_mysql_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS_EXTENSIONS:
        return append_information_schema_table_constraints_extensions_mysql_system_rows(
            database,
            rows
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_KEY_COLUMN_USAGE:
        return append_information_schema_key_column_usage_mysql_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS:
        return append_information_schema_statistics_mysql_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEWS:
        return append_information_schema_views_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE:
        return append_information_schema_view_table_usage_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_ROUTINE_USAGE:
        return append_information_schema_view_routine_usage_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_EVENTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ADMINISTRABLE_ROLE_AUTHORIZATIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_APPLICABLE_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_ENABLED_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PARAMETERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROFILING:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROUTINES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FIELDS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN_COLS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_BEING_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_CACHE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_METRICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_VIRTUAL:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TRX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE_LRU:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CACHED_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TEMP_TABLE_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_GEOMETRY_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_SPATIAL_REFERENCE_SYSTEMS:
    case MYLITE_EXECUTION_CATALOG_TABLE_CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_COLUMN_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_ROUTINE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_TABLE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_REFERENTIAL_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_CHECK_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMA_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COMPONENT:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DEFAULT_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DB:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ENGINE_COST:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_FUNC:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GENERAL_LOG:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GLOBAL_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GTID_EXECUTED:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_CATEGORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_KEYWORD:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_RELATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_TOPIC:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COLUMNS_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_NDB_BINLOG_INDEX:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PLUGIN:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROCS_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROXIES_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ROLE_EDGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVER_COST:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_MASTER_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_WORKER_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLOW_LOG:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TABLES_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_LEAP_SECOND:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_USER:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_BINARY_LOG_TRANSACTION_COMPRESSION_STATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_GLOBAL_STATUS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_GLOBAL_VARIABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_COND_INSTANCES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_DATA_LOCKS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_DATA_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_CURRENT:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_CURRENT:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_SUMMARY_BY_INSTANCE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_FILE_INSTANCES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_FILE_SUMMARY_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_FILE_SUMMARY_BY_INSTANCE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_HOST_CACHE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_KEYRING_COMPONENT_STATUS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_KEYRING_KEYS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_MUTEX_INSTANCES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_PERFORMANCE_TIMERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_PREPARED_STATEMENTS_INSTANCES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_APPLIER_CONFIGURATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_APPLIER_FILTERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_APPLIER_GLOBAL_FILTERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_APPLIER_STATUS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_APPLIER_STATUS_BY_COORDINATOR:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_APPLIER_STATUS_BY_WORKER:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_ASYNCHRONOUS_CONNECTION_FAILOVER:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_ASYNCHRONOUS_CONNECTION_FAILOVER_MANAGED:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_CONNECTION_CONFIGURATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_CONNECTION_STATUS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_GROUP_MEMBER_STATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_REPLICATION_GROUP_MEMBERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_RWLOCK_INSTANCES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_CONSUMERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_LOGGERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_METERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_METRICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_THREADS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_TABLE_HANDLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_TABLE_IO_WAITS_SUMMARY_BY_INDEX_USAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_TABLE_IO_WAITS_SUMMARY_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_TABLE_LOCK_WAITS_SUMMARY_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_ACCOUNTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_HOSTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_PROCESSLIST:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_PERSISTED_VARIABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_STATUS_BY_ACCOUNT:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_STATUS_BY_HOST:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_STATUS_BY_THREAD:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_STATUS_BY_USER:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_THREADS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SOCKET_INSTANCES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SOCKET_SUMMARY_BY_EVENT_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SOCKET_SUMMARY_BY_INSTANCE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SESSION_ACCOUNT_CONNECT_ATTRS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_USER_DEFINED_FUNCTIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_USER_VARIABLES_BY_THREAD:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_USERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_VARIABLES_BY_THREAD:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_VARIABLES_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SESSION_STATUS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SESSION_VARIABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAIT_CLASSES_GLOBAL_BY_AVG_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAIT_CLASSES_GLOBAL_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_BY_HOST_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_BY_USER_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_GLOBAL_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_SCHEMA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_BY_THREAD_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_LATEST_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_USER_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_GLOBAL_TOTAL:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_METRICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_PROCESSLIST:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SESSION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SESSION_SSL_STATUS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENT_ANALYSIS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_ERRORS_OR_WARNINGS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_FULL_TABLE_SCANS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_RUNTIMES_IN_95TH_PERCENTILE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_SORTING:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_TEMP_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_PS_CHECK_LOST_INSTRUMENTATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_INDEX_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_OBJECT_OVERVIEW:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_REDUNDANT_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_FLATTENED_KEYS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_UNUSED_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAIT_CLASSES_GLOBAL_BY_AVG_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAIT_CLASSES_GLOBAL_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAITS_BY_HOST_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAITS_BY_USER_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAITS_GLOBAL_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_BY_THREAD_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_LATEST_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_GLOBAL_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_GLOBAL_TOTAL:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PROCESSLIST:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SESSION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENT_ANALYSIS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_ERRORS_OR_WARNINGS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_FULL_TABLE_SCANS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_RUNTIMES_IN_95TH_PERCENTILE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_SORTING:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_TEMP_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_DIGEST_AVG_LATENCY_DISTRIBUTION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_DIGEST_95TH_PERCENTILE_BY_AVG_US:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_INDEX_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS:
        return MYLITE_OK;
    case MYLITE_EXECUTION_CATALOG_TABLE_TRIGGERS:
        return append_information_schema_triggers_system_rows(database, rows);
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA table");
    return MYLITE_ERROR;
}

static int append_information_schema_triggers_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char created[timestamp_2_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_triggers_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TRIGGERS columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp2(database, created, sizeof(created));
    for (size_t trigger_index = 0U;
         rc == MYLITE_OK && trigger_index < mylite_execution_catalog_sys_sys_config_trigger_count();
         ++trigger_index) {
        const struct mylite_execution_catalog_sys_config_trigger *trigger =
            mylite_execution_catalog_sys_sys_config_trigger_at(trigger_index);
        const char *values[information_schema_triggers_column_count] = {
            "def",
            "sys",
            trigger->name,
            trigger->event,
            "def",
            "sys",
            "sys_config",
            "1",
            NULL,
            mylite_execution_catalog_sys_sys_config_trigger_action_statement(),
            "ROW",
            "BEFORE",
            NULL,
            NULL,
            "OLD",
            "NEW",
            created,
            mylite_execution_catalog_sys_sys_config_trigger_sql_mode(),
            "mysql.sys@localhost",
            "utf8mb4",
            "utf8mb4_0900_ai_ci",
            "utf8mb4_0900_ai_ci",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_views_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_views_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.VIEWS columns");
        return MYLITE_ERROR;
    }

    for (size_t view_index = 0U;
         rc == MYLITE_OK &&
         view_index < mylite_execution_catalog_builtin_sys_view_definition_count();
         ++view_index) {
        const struct mylite_execution_catalog_builtin_sys_view *view =
            mylite_execution_catalog_builtin_sys_view_definition_at(view_index);
        const char *is_updatable =
            strcmp(view->name, "ps_check_lost_instrumentation") == 0 ||
                    strcmp(view->name, "io_global_by_file_by_bytes") == 0 ||
                    strcmp(view->name, "io_global_by_file_by_latency") == 0 ||
                    strcmp(view->name, "io_global_by_wait_by_bytes") == 0 ||
                    strcmp(view->name, "io_global_by_wait_by_latency") == 0 ||
                    strcmp(view->name, "host_summary_by_file_io_type") == 0 ||
                    strcmp(view->name, "x$host_summary_by_file_io_type") == 0 ||
                    strcmp(view->name, "host_summary_by_stages") == 0 ||
                    strcmp(view->name, "x$host_summary_by_stages") == 0 ||
                    strcmp(view->name, "host_summary_by_statement_type") == 0 ||
                    strcmp(view->name, "x$host_summary_by_statement_type") == 0 ||
                    strcmp(view->name, "user_summary_by_file_io_type") == 0 ||
                    strcmp(view->name, "x$user_summary_by_file_io_type") == 0 ||
                    strcmp(view->name, "user_summary_by_stages") == 0 ||
                    strcmp(view->name, "x$user_summary_by_stages") == 0 ||
                    strcmp(view->name, "user_summary_by_statement_type") == 0 ||
                    strcmp(view->name, "x$user_summary_by_statement_type") == 0 ||
                    strcmp(view->name, "waits_by_host_by_latency") == 0 ||
                    strcmp(view->name, "x$waits_by_host_by_latency") == 0 ||
                    strcmp(view->name, "waits_by_user_by_latency") == 0 ||
                    strcmp(view->name, "x$waits_by_user_by_latency") == 0 ||
                    strcmp(view->name, "waits_global_by_latency") == 0 ||
                    strcmp(view->name, "x$waits_global_by_latency") == 0 ||
                    strcmp(view->name, "x$io_global_by_file_by_bytes") == 0 ||
                    strcmp(view->name, "x$io_global_by_file_by_latency") == 0 ||
                    strcmp(view->name, "x$io_global_by_wait_by_bytes") == 0 ||
                    strcmp(view->name, "x$io_global_by_wait_by_latency") == 0 ||
                    strcmp(view->name, "memory_global_by_current_bytes") == 0 ||
                    strcmp(view->name, "x$memory_global_by_current_bytes") == 0 ||
                    strcmp(view->name, "schema_index_statistics") == 0 ||
                    strcmp(view->name, "x$schema_index_statistics") == 0 ||
                    strcmp(view->name, "schema_tables_with_full_table_scans") == 0 ||
                    strcmp(view->name, "x$schema_tables_with_full_table_scans") == 0 ||
                    strcmp(view->name, "statement_analysis") == 0 ||
                    strcmp(view->name, "x$statement_analysis") == 0 ||
                    strcmp(view->name, "statements_with_errors_or_warnings") == 0 ||
                    strcmp(view->name, "x$statements_with_errors_or_warnings") == 0 ||
                    strcmp(view->name, "statements_with_full_table_scans") == 0 ||
                    strcmp(view->name, "x$statements_with_full_table_scans") == 0 ||
                    strcmp(view->name, "statements_with_runtimes_in_95th_percentile") == 0 ||
                    strcmp(view->name, "x$statements_with_runtimes_in_95th_percentile") == 0 ||
                    strcmp(view->name, "statements_with_sorting") == 0 ||
                    strcmp(view->name, "x$statements_with_sorting") == 0 ||
                    strcmp(view->name, "statements_with_temp_tables") == 0 ||
                    strcmp(view->name, "x$statements_with_temp_tables") == 0 ||
                    strcmp(view->name, "schema_unused_indexes") == 0
                ? "YES"
                : "NO";
        const char *values[information_schema_views_column_count] = {
            "def",
            "sys",
            view->name,
            view->view_definition,
            "NONE",
            is_updatable,
            "mysql.sys@localhost",
            "INVOKER",
            "utf8mb4",
            "utf8mb4_0900_ai_ci",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_view_table_usage_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct builtin_view_dependency_row {
        const char *view_name;
        const char *table_schema;
        const char *table_name;
    };
    static const struct builtin_view_dependency_row dependency_rows[] = {
        {"host_summary", "performance_schema", "accounts"},
        {"host_summary", "sys", "x$host_summary_by_file_io"},
        {"host_summary", "sys", "x$host_summary_by_statement_latency"},
        {"host_summary", "sys", "x$memory_by_host_by_current_bytes"},
        {"host_summary_by_file_io",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"host_summary_by_file_io_type",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"host_summary_by_stages",
         "performance_schema",
         "events_stages_summary_by_host_by_event_name"},
        {"host_summary_by_statement_latency",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"host_summary_by_statement_type",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"user_summary", "performance_schema", "accounts"},
        {"user_summary", "sys", "x$memory_by_user_by_current_bytes"},
        {"user_summary", "sys", "x$user_summary_by_file_io"},
        {"user_summary", "sys", "x$user_summary_by_statement_latency"},
        {"user_summary_by_file_io",
         "performance_schema",
         "events_waits_summary_by_user_by_event_name"},
        {"user_summary_by_file_io_type",
         "performance_schema",
         "events_waits_summary_by_user_by_event_name"},
        {"user_summary_by_stages",
         "performance_schema",
         "events_stages_summary_by_user_by_event_name"},
        {"user_summary_by_statement_latency",
         "performance_schema",
         "events_statements_summary_by_user_by_event_name"},
        {"user_summary_by_statement_type",
         "performance_schema",
         "events_statements_summary_by_user_by_event_name"},
        {"wait_classes_global_by_avg_latency",
         "performance_schema",
         "events_waits_summary_global_by_event_name"},
        {"wait_classes_global_by_latency",
         "performance_schema",
         "events_waits_summary_global_by_event_name"},
        {"waits_by_host_by_latency",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"waits_by_user_by_latency",
         "performance_schema",
         "events_waits_summary_by_user_by_event_name"},
        {"waits_global_by_latency",
         "performance_schema",
         "events_waits_summary_global_by_event_name"},
        {"innodb_buffer_stats_by_schema", "information_schema", "INNODB_BUFFER_PAGE"},
        {"innodb_buffer_stats_by_table", "information_schema", "INNODB_BUFFER_PAGE"},
        {"memory_by_host_by_current_bytes",
         "performance_schema",
         "memory_summary_by_host_by_event_name"},
        {"memory_by_thread_by_current_bytes",
         "performance_schema",
         "memory_summary_by_thread_by_event_name"},
        {"memory_by_thread_by_current_bytes", "performance_schema", "threads"},
        {"memory_by_user_by_current_bytes",
         "performance_schema",
         "memory_summary_by_user_by_event_name"},
        {"memory_global_by_current_bytes",
         "performance_schema",
         "memory_summary_global_by_event_name"},
        {"memory_global_total", "performance_schema", "memory_summary_global_by_event_name"},
        {"metrics", "information_schema", "INNODB_METRICS"},
        {"metrics", "performance_schema", "global_status"},
        {"metrics", "performance_schema", "memory_summary_global_by_event_name"},
        {"metrics", "performance_schema", "setup_instruments"},
        {"processlist", "performance_schema", "events_stages_current"},
        {"processlist", "performance_schema", "events_statements_current"},
        {"processlist", "performance_schema", "events_transactions_current"},
        {"processlist", "performance_schema", "events_waits_current"},
        {"processlist", "performance_schema", "session_connect_attrs"},
        {"processlist", "performance_schema", "threads"},
        {"processlist", "sys", "sys_config"},
        {"processlist", "sys", "x$memory_by_thread_by_current_bytes"},
        {"session", "sys", "processlist"},
        {"session", "sys", "sys_config"},
        {"session_ssl_status", "performance_schema", "status_by_thread"},
        {"statement_analysis", "performance_schema", "events_statements_summary_by_digest"},
        {"statement_analysis", "sys", "sys_config"},
        {"statements_with_errors_or_warnings",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"statements_with_errors_or_warnings", "sys", "sys_config"},
        {"statements_with_full_table_scans",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"statements_with_full_table_scans", "sys", "sys_config"},
        {"statements_with_runtimes_in_95th_percentile",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"statements_with_runtimes_in_95th_percentile", "sys", "sys_config"},
        {"statements_with_runtimes_in_95th_percentile",
         "sys",
         "x$ps_digest_95th_percentile_by_avg_us"},
        {"statements_with_sorting",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"statements_with_sorting", "sys", "sys_config"},
        {"statements_with_temp_tables",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"statements_with_temp_tables", "sys", "sys_config"},
        {"innodb_lock_waits", "information_schema", "INNODB_TRX"},
        {"innodb_lock_waits", "performance_schema", "data_lock_waits"},
        {"innodb_lock_waits", "performance_schema", "data_locks"},
        {"innodb_lock_waits", "sys", "sys_config"},
        {"io_by_thread_by_latency",
         "performance_schema",
         "events_waits_summary_by_thread_by_event_name"},
        {"io_by_thread_by_latency", "performance_schema", "threads"},
        {"io_global_by_file_by_bytes", "performance_schema", "file_summary_by_instance"},
        {"io_global_by_file_by_bytes", "performance_schema", "global_variables"},
        {"io_global_by_file_by_latency", "performance_schema", "file_summary_by_instance"},
        {"io_global_by_file_by_latency", "performance_schema", "global_variables"},
        {"io_global_by_wait_by_bytes", "performance_schema", "file_summary_by_event_name"},
        {"io_global_by_wait_by_latency", "performance_schema", "file_summary_by_event_name"},
        {"latest_file_io", "information_schema", "PROCESSLIST"},
        {"latest_file_io", "performance_schema", "events_waits_history_long"},
        {"latest_file_io", "performance_schema", "global_variables"},
        {"latest_file_io", "performance_schema", "threads"},
        {"ps_check_lost_instrumentation", "performance_schema", "global_status"},
        {"schema_auto_increment_columns", "information_schema", "COLUMNS"},
        {"schema_auto_increment_columns", "information_schema", "TABLES"},
        {"schema_index_statistics", "performance_schema", "table_io_waits_summary_by_index_usage"},
        {"schema_object_overview", "information_schema", "EVENTS"},
        {"schema_object_overview", "information_schema", "ROUTINES"},
        {"schema_object_overview", "information_schema", "STATISTICS"},
        {"schema_object_overview", "information_schema", "TABLES"},
        {"schema_object_overview", "information_schema", "TRIGGERS"},
        {"schema_redundant_indexes", "sys", "x$schema_flattened_keys"},
        {"schema_table_lock_waits", "performance_schema", "events_statements_current"},
        {"schema_table_lock_waits", "performance_schema", "metadata_locks"},
        {"schema_table_lock_waits", "performance_schema", "threads"},
        {"schema_table_lock_waits", "sys", "sys_config"},
        {"schema_table_statistics", "performance_schema", "table_io_waits_summary_by_table"},
        {"schema_table_statistics", "sys", "x$ps_schema_table_statistics_io"},
        {"schema_table_statistics_with_buffer",
         "performance_schema",
         "table_io_waits_summary_by_table"},
        {"schema_table_statistics_with_buffer", "sys", "x$innodb_buffer_stats_by_table"},
        {"schema_table_statistics_with_buffer", "sys", "x$ps_schema_table_statistics_io"},
        {"schema_tables_with_full_table_scans",
         "performance_schema",
         "table_io_waits_summary_by_index_usage"},
        {"schema_unused_indexes", "information_schema", "STATISTICS"},
        {"schema_unused_indexes", "performance_schema", "table_io_waits_summary_by_index_usage"},
        {"x$host_summary", "performance_schema", "accounts"},
        {"x$host_summary", "sys", "x$host_summary_by_file_io"},
        {"x$host_summary", "sys", "x$host_summary_by_statement_latency"},
        {"x$host_summary", "sys", "x$memory_by_host_by_current_bytes"},
        {"x$host_summary_by_file_io",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"x$host_summary_by_file_io_type",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"x$host_summary_by_stages",
         "performance_schema",
         "events_stages_summary_by_host_by_event_name"},
        {"x$host_summary_by_statement_latency",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"x$host_summary_by_statement_type",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"x$user_summary", "performance_schema", "accounts"},
        {"x$user_summary", "sys", "x$memory_by_user_by_current_bytes"},
        {"x$user_summary", "sys", "x$user_summary_by_file_io"},
        {"x$user_summary", "sys", "x$user_summary_by_statement_latency"},
        {"x$user_summary_by_file_io",
         "performance_schema",
         "events_waits_summary_by_user_by_event_name"},
        {"x$user_summary_by_file_io_type",
         "performance_schema",
         "events_waits_summary_by_user_by_event_name"},
        {"x$user_summary_by_stages",
         "performance_schema",
         "events_stages_summary_by_user_by_event_name"},
        {"x$user_summary_by_statement_latency",
         "performance_schema",
         "events_statements_summary_by_user_by_event_name"},
        {"x$user_summary_by_statement_type",
         "performance_schema",
         "events_statements_summary_by_user_by_event_name"},
        {"x$wait_classes_global_by_avg_latency",
         "performance_schema",
         "events_waits_summary_global_by_event_name"},
        {"x$wait_classes_global_by_latency",
         "performance_schema",
         "events_waits_summary_global_by_event_name"},
        {"x$waits_by_host_by_latency",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"x$waits_by_user_by_latency",
         "performance_schema",
         "events_waits_summary_by_user_by_event_name"},
        {"x$waits_global_by_latency",
         "performance_schema",
         "events_waits_summary_global_by_event_name"},
        {"x$innodb_buffer_stats_by_schema", "information_schema", "INNODB_BUFFER_PAGE"},
        {"x$innodb_buffer_stats_by_table", "information_schema", "INNODB_BUFFER_PAGE"},
        {"x$memory_by_host_by_current_bytes",
         "performance_schema",
         "memory_summary_by_host_by_event_name"},
        {"x$memory_by_thread_by_current_bytes",
         "performance_schema",
         "memory_summary_by_thread_by_event_name"},
        {"x$memory_by_thread_by_current_bytes", "performance_schema", "threads"},
        {"x$memory_by_user_by_current_bytes",
         "performance_schema",
         "memory_summary_by_user_by_event_name"},
        {"x$memory_global_by_current_bytes",
         "performance_schema",
         "memory_summary_global_by_event_name"},
        {"x$memory_global_total", "performance_schema", "memory_summary_global_by_event_name"},
        {"x$innodb_lock_waits", "information_schema", "INNODB_TRX"},
        {"x$innodb_lock_waits", "performance_schema", "data_lock_waits"},
        {"x$innodb_lock_waits", "performance_schema", "data_locks"},
        {"x$io_by_thread_by_latency",
         "performance_schema",
         "events_waits_summary_by_thread_by_event_name"},
        {"x$io_by_thread_by_latency", "performance_schema", "threads"},
        {"x$io_global_by_file_by_bytes", "performance_schema", "file_summary_by_instance"},
        {"x$io_global_by_file_by_latency", "performance_schema", "file_summary_by_instance"},
        {"x$io_global_by_wait_by_bytes", "performance_schema", "file_summary_by_event_name"},
        {"x$io_global_by_wait_by_latency", "performance_schema", "file_summary_by_event_name"},
        {"x$latest_file_io", "information_schema", "PROCESSLIST"},
        {"x$latest_file_io", "performance_schema", "events_waits_history_long"},
        {"x$latest_file_io", "performance_schema", "threads"},
        {"x$processlist", "performance_schema", "events_stages_current"},
        {"x$processlist", "performance_schema", "events_statements_current"},
        {"x$processlist", "performance_schema", "events_transactions_current"},
        {"x$processlist", "performance_schema", "events_waits_current"},
        {"x$processlist", "performance_schema", "session_connect_attrs"},
        {"x$processlist", "performance_schema", "threads"},
        {"x$processlist", "sys", "x$memory_by_thread_by_current_bytes"},
        {"x$session", "sys", "x$processlist"},
        {"x$statement_analysis", "performance_schema", "events_statements_summary_by_digest"},
        {"x$statements_with_errors_or_warnings",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"x$statements_with_full_table_scans",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"x$statements_with_runtimes_in_95th_percentile",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"x$statements_with_runtimes_in_95th_percentile",
         "sys",
         "x$ps_digest_95th_percentile_by_avg_us"},
        {"x$statements_with_sorting",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"x$statements_with_temp_tables",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"x$ps_digest_95th_percentile_by_avg_us",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"x$ps_digest_95th_percentile_by_avg_us",
         "sys",
         "x$ps_digest_avg_latency_distribution"},
        {"x$ps_digest_avg_latency_distribution",
         "performance_schema",
         "events_statements_summary_by_digest"},
        {"x$ps_schema_table_statistics_io", "performance_schema", "file_summary_by_instance"},
        {"x$schema_flattened_keys", "information_schema", "STATISTICS"},
        {"x$schema_index_statistics",
         "performance_schema",
         "table_io_waits_summary_by_index_usage"},
        {"x$schema_table_lock_waits", "performance_schema", "events_statements_current"},
        {"x$schema_table_lock_waits", "performance_schema", "metadata_locks"},
        {"x$schema_table_lock_waits", "performance_schema", "threads"},
        {"x$schema_table_statistics", "performance_schema", "table_io_waits_summary_by_table"},
        {"x$schema_table_statistics", "sys", "x$ps_schema_table_statistics_io"},
        {"x$schema_table_statistics_with_buffer",
         "performance_schema",
         "table_io_waits_summary_by_table"},
        {"x$schema_table_statistics_with_buffer", "sys", "x$innodb_buffer_stats_by_table"},
        {"x$schema_table_statistics_with_buffer", "sys", "x$ps_schema_table_statistics_io"},
        {"x$schema_tables_with_full_table_scans",
         "performance_schema",
         "table_io_waits_summary_by_index_usage"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_view_table_usage_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.VIEW_TABLE_USAGE columns");
        return MYLITE_ERROR;
    }

    for (size_t dependency_index = 0U;
         rc == MYLITE_OK && dependency_index < sizeof(dependency_rows) / sizeof(dependency_rows[0]);
         ++dependency_index) {
        const struct builtin_view_dependency_row *dependency = &dependency_rows[dependency_index];
        const char *values[information_schema_view_table_usage_column_count] = {
            "def",
            "sys",
            dependency->view_name,
            "def",
            dependency->table_schema,
            dependency->table_name,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_view_routine_usage_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct builtin_view_routine_dependency_row {
        const char *view_name;
        const char *routine_name;
    };
    static const struct builtin_view_routine_dependency_row dependency_rows[] = {
        {"innodb_lock_waits", "format_statement"},
        {"innodb_lock_waits", "quote_identifier"},
        {"io_global_by_file_by_bytes", "format_path"},
        {"io_global_by_file_by_latency", "format_path"},
        {"latest_file_io", "format_path"},
        {"processlist", "format_statement"},
        {"schema_table_lock_waits", "format_statement"},
        {"schema_table_lock_waits", "ps_thread_account"},
        {"statement_analysis", "format_statement"},
        {"statements_with_errors_or_warnings", "format_statement"},
        {"statements_with_full_table_scans", "format_statement"},
        {"statements_with_runtimes_in_95th_percentile", "format_statement"},
        {"statements_with_sorting", "format_statement"},
        {"statements_with_temp_tables", "format_statement"},
        {"x$innodb_lock_waits", "quote_identifier"},
        {"x$ps_schema_table_statistics_io", "extract_schema_from_file_name"},
        {"x$ps_schema_table_statistics_io", "extract_table_from_file_name"},
        {"x$schema_table_lock_waits", "ps_thread_account"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_view_routine_usage_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE columns");
        return MYLITE_ERROR;
    }

    for (size_t dependency_index = 0U;
         rc == MYLITE_OK && dependency_index < sizeof(dependency_rows) / sizeof(dependency_rows[0]);
         ++dependency_index) {
        const struct builtin_view_routine_dependency_row *dependency =
            &dependency_rows[dependency_index];
        const char *values[information_schema_view_routine_usage_column_count] = {
            "def",
            "sys",
            dependency->view_name,
            "def",
            "sys",
            dependency->routine_name,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

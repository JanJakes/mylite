
static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum mylite_execution_system_variable_kind variable = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    const char *override_value = NULL;
    const char *default_value = NULL;
    uint64_t count = 0U;
    bool global_scope = false;
    int rc = resolve_session_system_variable(database, expression, &variable);

    if (rc != MYLITE_OK) {
        return rc;
    }
    global_scope = system_variable_expression_has_global_scope(expression);
    if (!global_scope && variable != MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT &&
        variable != MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR) {
        override_value = session_system_variable_override_value(database, variable);
        if (override_value != NULL) {
            out_cell->value = override_value;
            return MYLITE_OK;
        }
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ACCOUNTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_DIGESTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ERROR_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_HOSTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_INDEX_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MEMORY_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METADATA_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METER_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METRIC_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PREPARED_STATEMENTS_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PROGRAM_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STAGE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_STACK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_LOCK_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_USERS_SIZE:
        out_cell->value = performance_schema_scalar_system_variable_value(variable);
        return out_cell->value == NULL ? MYLITE_ERROR : MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        out_cell->value = database->session.character_set_client;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
        out_cell->value = database->session.character_set_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        out_cell->value = database->session.character_set_results;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        out_cell->value = database->session.collation_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
        out_cell->value = "utf8mb4";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER:
        out_cell->value = "utf8mb4_0900_ai_ci";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
        return database_character_set_system_variable_value(
            database,
            global_scope,
            out_cell->literal_text,
            sizeof(out_cell->literal_text),
            &out_cell->value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE:
        return database_collation_system_variable_value(
            database,
            global_scope,
            out_cell->literal_text,
            sizeof(out_cell->literal_text),
            &out_cell->value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
        out_cell->value = "InnoDB";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXTERNAL_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_HOST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
        override_value =
            global_scope ? NULL : session_system_variable_override_value(database, variable);
        out_cell->value =
            (override_value != NULL && strcmp(override_value, "0") == 0) ? "NO" : "YES";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
        out_cell->value = "utf8mb3";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
        out_cell->value = "binary";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE:
        out_cell->value = global_scope ? default_sql_mode_value() : database->session.sql_mode_text;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE:
        out_cell->value = "UTC";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE:
        out_cell->value = global_scope ? "SYSTEM" : database->session.time_zone;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE:
        out_cell->value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED:
        out_cell->value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
        out_cell->value = basedir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
        out_cell->value = datadir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
        out_cell->value = hostname_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
        out_cell->value = license_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG_FILE:
        out_cell->value = general_log_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR:
        out_cell->value = "stderr";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SERVICES:
        out_cell->value = "log_filter_internal; log_sink_internal";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SUPPRESSION_LIST:
        out_cell->value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_OUTPUT:
        out_cell->value = "FILE";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_TIMESTAMPS:
        out_cell->value = "UTC";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG_FILE:
        out_cell->value = slow_query_log_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
        out_cell->value = log_bin_basename_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
        out_cell->value = log_bin_index_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG:
        out_cell->value = relay_log_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME:
        out_cell->value = relay_log_basename_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX:
        out_cell->value = relay_log_index_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
        out_cell->value = pid_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
        out_cell->value = plugin_dir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
        out_cell->value = version_compile_machine_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
        out_cell->value = version_compile_os_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        out_cell->value = version_compile_zlib_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
        out_cell->value = server_uuid_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
        out_cell->value = socket_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIND_ADDRESS:
        out_cell->value = bind_address_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BACK_LOG:
        return format_session_scalar_uint64_value(
            database,
            back_log_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOST_CACHE_SIZE:
        return format_session_scalar_uint64_value(
            database,
            host_cache_size_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_LIMIT:
        return format_session_scalar_uint64_value(
            database,
            global_connection_memory_limit_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_FAILED_CONNECTIONS_THRESHOLD:
        return format_session_scalar_uint64_value(
            database,
            connection_control_failed_connections_threshold_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MAX_CONNECTION_DELAY:
        return format_session_scalar_uint64_value(
            database,
            connection_control_max_connection_delay_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MIN_CONNECTION_DELAY:
        return format_session_scalar_uint64_value(
            database,
            connection_control_min_connection_delay_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        return format_session_scalar_uint64_value(
            database,
            auto_increment_step_system_variable_value(database, variable, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        return format_session_scalar_uint64_value(
            database,
            timeout_system_variable_value(database, variable, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_LIMIT:
        return format_session_scalar_uint64_value(
            database,
            connection_memory_system_variable_value(database, variable, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERNAL_TMP_MEM_STORAGE_ENGINE:
        out_cell->value =
            internal_tmp_mem_storage_engine_system_variable_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_JOIN_BUFFER_SIZE:
        return format_session_scalar_uint64_value(
            database,
            join_buffer_size_system_variable_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
        return format_session_scalar_uint64_value(
            database,
            myisam_data_pointer_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
        return format_session_scalar_uint64_value(
            database,
            myisam_max_sort_file_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MMAP_SIZE:
        return format_session_scalar_uint64_value(
            database,
            myisam_mmap_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_RECOVER_OPTIONS:
        out_cell->value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_SORT_BUFFER_SIZE:
        return format_session_scalar_uint64_value(
            database,
            myisam_sort_buffer_size_system_variable_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_STATS_METHOD:
        out_cell->value = myisam_stats_method_system_variable_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
        return format_session_scalar_uint64_value(database, 0U, out_cell);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
        return format_session_scalar_uint64_value(
            database,
            innodb_adaptive_flushing_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
        return format_session_scalar_uint64_value(
            database,
            innodb_adaptive_flushing_lwm_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
        return format_session_scalar_uint64_value(
            database,
            innodb_adaptive_hash_index_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX_PARTS:
        return format_session_scalar_uint64_value(
            database,
            innodb_adaptive_hash_index_parts_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
        return format_session_scalar_uint64_value(
            database,
            innodb_adaptive_max_sleep_delay_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
        return format_session_scalar_uint64_value(
            database,
            innodb_autoextend_increment_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOINC_LOCK_MODE:
        return format_session_scalar_uint64_value(
            database,
            innodb_autoinc_lock_mode_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_CHUNK_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_chunk_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_dump_at_shutdown_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_dump_now_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_dump_pct_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
        out_cell->value = innodb_buffer_pool_filename_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_in_core_file_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_INSTANCES:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_instances_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_load_abort_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_AT_STARTUP:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_load_at_startup_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_load_now_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_buffer_pool_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_change_buffer_max_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
        out_cell->value = innodb_change_buffering_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
        out_cell->value = innodb_checksum_algorithm_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
        return format_session_scalar_uint64_value(
            database,
            innodb_cmp_per_index_enabled_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
        return format_session_scalar_uint64_value(
            database,
            innodb_commit_concurrency_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
        return format_session_scalar_uint64_value(
            database,
            innodb_compression_failure_threshold_pct_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
        return format_session_scalar_uint64_value(
            database,
            innodb_compression_level_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
        return format_session_scalar_uint64_value(
            database,
            innodb_compression_pad_pct_max_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
        return format_session_scalar_uint64_value(
            database,
            innodb_concurrency_tickets_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_FILE_PATH:
        out_cell->value = innodb_data_file_path_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_HOME_DIR:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_ddl_buffer_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
        return format_session_scalar_uint64_value(
            database,
            innodb_ddl_threads_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
        return format_session_scalar_uint64_value(
            database,
            innodb_deadlock_detect_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEDICATED_SERVER:
        return format_session_scalar_uint64_value(
            database,
            innodb_dedicated_server_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
        out_cell->value = innodb_default_row_format_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DIRECTORIES:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
        return format_session_scalar_uint64_value(
            database,
            innodb_disable_sort_file_cache_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
        out_cell->value = innodb_doublewrite_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_BATCH_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_doublewrite_batch_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_DIR:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_FILES:
        return format_session_scalar_uint64_value(
            database,
            innodb_doublewrite_files_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_PAGES:
        return format_session_scalar_uint64_value(
            database,
            innodb_doublewrite_pages_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_extend_and_initialize_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
        return format_session_scalar_uint64_value(
            database,
            innodb_fast_shutdown_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
        return format_session_scalar_uint64_value(
            database,
            innodb_file_per_table_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
        return format_session_scalar_uint64_value(
            database,
            innodb_fill_factor_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
        return format_session_scalar_uint64_value(
            database,
            innodb_flush_log_at_timeout_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
        return format_session_scalar_uint64_value(
            database,
            innodb_flush_log_at_trx_commit_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_METHOD:
        out_cell->value = innodb_flush_method_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
        return format_session_scalar_uint64_value(
            database,
            innodb_flush_neighbors_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
        return format_session_scalar_uint64_value(
            database,
            innodb_flush_sync_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
        return format_session_scalar_uint64_value(
            database,
            innodb_flushing_avg_loops_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_LOAD_CORRUPTED:
        return format_session_scalar_uint64_value(
            database,
            innodb_force_load_corrupted_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_RECOVERY:
        return format_session_scalar_uint64_value(
            database,
            innodb_force_recovery_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
        return format_session_scalar_uint64_value(
            database,
            innodb_fsync_threshold_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_CACHE_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_cache_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_enable_diag_print_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_enable_stopword_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MAX_TOKEN_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_max_token_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MIN_TOKEN_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_min_token_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_num_word_optimize_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_result_cache_limit_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SORT_PLL_DEGREE:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_sort_pll_degree_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_TOTAL_CACHE_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_ft_total_cache_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
        return format_session_scalar_uint64_value(
            database,
            innodb_idle_flush_pct_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
        return format_session_scalar_uint64_value(
            database,
            innodb_io_capacity_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
        return format_session_scalar_uint64_value(
            database,
            innodb_io_capacity_max_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_buffer_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_checksums_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_compressed_pages_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILE_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_file_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILES_IN_GROUP:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_files_in_group_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_GROUP_HOME_DIR:
        out_cell->value = innodb_log_group_home_dir_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_spin_cpu_abs_lwm_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_spin_cpu_pct_hwm_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_wait_for_flush_spin_hwm_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_write_ahead_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
        return format_session_scalar_uint64_value(
            database,
            innodb_log_writer_threads_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
        return format_session_scalar_uint64_value(
            database,
            innodb_lru_scan_depth_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
        out_cell->value = innodb_max_dirty_pages_pct_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
        out_cell->value = innodb_max_dirty_pages_pct_lwm_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
        return format_session_scalar_uint64_value(
            database,
            innodb_max_purge_lag_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_max_undo_log_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
        return format_session_scalar_uint64_value(
            database,
            innodb_old_blocks_pct_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
        return format_session_scalar_uint64_value(
            database,
            innodb_old_blocks_time_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_online_alter_log_max_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPEN_FILES:
        return format_session_scalar_uint64_value(
            database,
            innodb_open_files_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
        return format_session_scalar_uint64_value(
            database,
            innodb_optimize_fulltext_only_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_CLEANERS:
        return format_session_scalar_uint64_value(
            database,
            innodb_page_cleaners_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_page_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS:
        return format_session_scalar_uint64_value(
            database,
            innodb_parallel_read_threads_system_variable_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
        return format_session_scalar_uint64_value(
            database,
            innodb_print_all_deadlocks_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
        return format_session_scalar_uint64_value(
            database,
            innodb_print_ddl_logs_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_purge_batch_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
        return format_session_scalar_uint64_value(
            database,
            innodb_purge_rseg_truncate_frequency_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_THREADS:
        return format_session_scalar_uint64_value(
            database,
            innodb_purge_threads_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
        return format_session_scalar_uint64_value(
            database,
            innodb_random_read_ahead_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
        return format_session_scalar_uint64_value(
            database,
            innodb_read_ahead_threshold_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_IO_THREADS:
        return format_session_scalar_uint64_value(
            database,
            innodb_read_io_threads_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
        return format_session_scalar_uint64_value(
            database,
            innodb_redo_log_capacity_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
        return format_session_scalar_uint64_value(
            database,
            innodb_redo_log_encrypt_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
        return format_session_scalar_uint64_value(
            database,
            innodb_replication_delay_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_ON_TIMEOUT:
        return format_session_scalar_uint64_value(
            database,
            innodb_rollback_on_timeout_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
        return format_session_scalar_uint64_value(
            database,
            innodb_rollback_segments_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
        out_cell->value = innodb_segment_reserve_factor_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SORT_BUFFER_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_sort_buffer_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
        return format_session_scalar_uint64_value(
            database,
            innodb_spin_wait_delay_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
        return format_session_scalar_uint64_value(
            database,
            innodb_spin_wait_pause_multiplier_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
        return format_session_scalar_uint64_value(
            database,
            innodb_stats_auto_recalc_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
        return format_session_scalar_uint64_value(
            database,
            innodb_stats_include_delete_marked_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
        out_cell->value = innodb_stats_method_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
        return format_session_scalar_uint64_value(
            database,
            innodb_stats_on_metadata_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
        return format_session_scalar_uint64_value(
            database,
            innodb_stats_persistent_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
        return format_session_scalar_uint64_value(
            database,
            innodb_stats_persistent_sample_pages_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
        return format_session_scalar_uint64_value(
            database,
            innodb_stats_transient_sample_pages_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
        return format_session_scalar_uint64_value(
            database,
            innodb_status_output_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
        return format_session_scalar_uint64_value(
            database,
            innodb_status_output_locks_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
        return format_session_scalar_uint64_value(
            database,
            boolean_session_placeholder_system_variable_uint64_value(
                database,
                variable,
                global_scope
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_ARRAY_SIZE:
        return format_session_scalar_uint64_value(
            database,
            innodb_sync_array_size_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
        return format_session_scalar_uint64_value(
            database,
            innodb_sync_spin_loops_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
        return format_session_scalar_uint64_value(
            database,
            boolean_session_placeholder_system_variable_uint64_value(
                database,
                variable,
                global_scope
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_DATA_FILE_PATH:
        out_cell->value = innodb_temp_data_file_path_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_TABLESPACES_DIR:
        out_cell->value = innodb_temp_tablespaces_dir_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
        return format_session_scalar_uint64_value(
            database,
            innodb_thread_concurrency_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
        return format_session_scalar_uint64_value(
            database,
            innodb_thread_sleep_delay_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR:
        if (!global_scope) {
            override_value = session_system_variable_override_value(database, variable);
            if (override_value != NULL) {
                out_cell->value = override_value[0] == '\0' ? NULL : override_value;
                return MYLITE_OK;
            }
        }
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_DIRECTORY:
        out_cell->value = innodb_undo_directory_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
        return format_session_scalar_uint64_value(
            database,
            innodb_undo_log_encrypt_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
        return format_session_scalar_uint64_value(
            database,
            innodb_undo_log_truncate_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
        return format_session_scalar_uint64_value(
            database,
            innodb_undo_tablespaces_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
        return format_session_scalar_uint64_value(
            database,
            innodb_use_fdatasync_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_NATIVE_AIO:
        return format_session_scalar_uint64_value(
            database,
            innodb_use_native_aio_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VALIDATE_TABLESPACE_PATHS:
        return format_session_scalar_uint64_value(
            database,
            innodb_validate_tablespace_paths_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VERSION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_WRITE_IO_THREADS:
        return format_session_scalar_uint64_value(
            database,
            innodb_write_io_threads_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING:
        return format_session_scalar_uint64_value(
            database,
            global_connection_memory_tracking_system_variable_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SORT_BUFFER_SIZE:
        return format_session_scalar_uint64_value(
            database,
            sort_buffer_size_system_variable_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOW_PRIORITY_UPDATES:
        return format_session_scalar_uint64_value(
            database,
            low_priority_updates_system_variable_uint64_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        return format_session_scalar_uint64_value(
            database,
            global_scope ? group_concat_max_len_default_value
                         : database->session.group_concat_max_len,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_IDENTITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LAST_INSERT_ID:
        return format_session_scalar_uint64_value(
            database,
            database->session.last_insert_id,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        return format_session_scalar_uint64_value(
            database,
            information_schema_stats_expiry_system_variable_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LONG_QUERY_TIME:
        return format_long_query_time_microseconds(
            database,
            global_scope ? long_query_time_default_microseconds
                         : database->session.long_query_time_microseconds,
            out_cell->literal_text,
            sizeof(out_cell->literal_text),
            &out_cell->value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
        return format_session_scalar_uint64_value(
            database,
            global_scope ? max_error_count_default_value : database->session.max_error_count,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        return format_session_scalar_uint64_value(
            database,
            foreign_key_checks_system_variable_uint64_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
        return format_session_scalar_uint64_value(
            database,
            sql_require_primary_key_system_variable_uint64_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        return format_session_scalar_uint64_value(
            database,
            big_tables_system_variable_uint64_value(database, global_scope),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
        return format_session_scalar_uint64_value(
            database,
            global_scope || database->session.autocommit_enabled ? 1U : 0U,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_REPLICA_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG:
        rc = format_uint64(database, 1U, out_cell->integer_text, sizeof(out_cell->integer_text));
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_RAW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_ADMIN_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_EXTRA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_REPLICA_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_THROTTLE_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        rc = format_uint64(database, 0U, out_cell->integer_text, sizeof(out_cell->integer_text));
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        return format_session_scalar_uint64_value(
            database,
            server_id_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        return format_session_scalar_uint64_value(
            database,
            server_id_bits_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        return format_session_scalar_uint64_value(database, port_system_variable_value, out_cell);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_VERBOSITY:
        return format_session_scalar_uint64_value(database, 2U, out_cell);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_LAUNCH_TIME:
        return format_session_scalar_uint64_value(
            database,
            slow_launch_time_default_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        return format_session_scalar_uint64_value(
            database,
            protocol_version_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        rc = format_uint64(
            database,
            sql_select_limit_system_variable_value(database, global_scope),
            out_cell->integer_text,
            sizeof(out_cell->integer_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET:
        rc = format_uint64(
            database,
            max_allowed_packet_default_value,
            out_cell->integer_text,
            sizeof(out_cell->integer_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
        out_cell->value = transaction_isolation_system_variable_value(database, expression);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
        out_cell->value = transaction_read_only_system_variable_value(database, expression);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP:
        rc = format_timestamp_system_variable_value(
            database,
            out_cell->literal_text,
            sizeof(out_cell->literal_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->literal_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMMENT:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING;
        return MYLITE_OK;
    default:
        break;
    }

    default_value = mylite_execution_system_variable_default_value(variable);
    if (default_value != NULL) {
        out_cell->value = default_value;
        return MYLITE_OK;
    }

    rc = diagnostics_count_system_variable_value(
        system_variable_count_diagnostics(database),
        variable,
        &count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = format_uint64(database, count, out_cell->integer_text, sizeof(out_cell->integer_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

static const char *performance_schema_scalar_system_variable_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ACCOUNTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_HOSTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_INDEX_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METADATA_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PREPARED_STATEMENTS_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PROGRAM_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_LOCK_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_USERS_SIZE:
        return "-1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_DIGESTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG_SIZE:
        return "10000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ERROR_SIZE:
        return "5556";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_STACK:
        return "10";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_CLASSES:
        return "150";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH:
        return "1024";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE:
        return "60";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_CLASSES:
        return "80";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_HANDLES:
        return "32768";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MEMORY_CLASSES:
        return "470";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METER_CLASSES:
        return "30";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METRIC_CLASSES:
        return "600";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_CLASSES:
        return "350";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_CLASSES:
        return "100";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STAGE_CLASSES:
        return "175";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_CLASSES:
        return "220";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS_SIZE:
        return "512";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST:
        return "0";
    default:
        return NULL;
    }
}

static int diagnostics_count_system_variable_value(
    const struct mylite_diagnostics *diagnostics,
    enum mylite_execution_system_variable_kind variable,
    uint64_t *out_count
) {
    uint64_t warning_count = 0U;
    uint64_t condition_count = 0U;
    int rc = MYLITE_OK;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }
    if (variable == MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT) {
        return previous_diagnostics_error_count(diagnostics, out_count);
    }

    rc = previous_diagnostics_condition_count(diagnostics, &condition_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    warning_count = (uint64_t)mylite_diagnostics_warning_total_count(diagnostics);
    if (condition_count > UINT64_MAX - warning_count) {
        return MYLITE_NOMEM;
    }

    *out_count = condition_count + warning_count;
    return MYLITE_OK;
}

static uint64_t information_schema_stats_expiry_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope) {
        return information_schema_stats_expiry_default_value;
    }
    return database->session.information_schema_stats_expiry;
}

static int database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_execution_catalog_builtin_schema *builtin_schema = NULL;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    if (global_scope || database == NULL || !database->session.has_selected_schema) {
        *out_value = MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
        return MYLITE_OK;
    }
    builtin_schema = find_builtin_schema_descriptor(database->session.selected_schema);
    if (builtin_schema != NULL) {
        *out_value = builtin_schema->default_charset;
        return MYLITE_OK;
    }

    rc = resolve_schema_name(database, database->session.selected_schema, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strlen(schema.default_charset) >= buffer_size) {
        set_runtime_error(database, "system variable buffer is too small");
        return MYLITE_ERROR;
    }

    memcpy(buffer, schema.default_charset, strlen(schema.default_charset) + 1U);
    *out_value = buffer;
    return MYLITE_OK;
}

static int database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_execution_catalog_builtin_schema *builtin_schema = NULL;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    if (global_scope || database == NULL || !database->session.has_selected_schema) {
        *out_value = MYLITE_CATALOG_DEFAULT_TABLE_COLLATION;
        return MYLITE_OK;
    }
    builtin_schema = find_builtin_schema_descriptor(database->session.selected_schema);
    if (builtin_schema != NULL) {
        *out_value = builtin_schema->default_collation;
        return MYLITE_OK;
    }

    rc = resolve_schema_name(database, database->session.selected_schema, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strlen(schema.default_collation) >= buffer_size) {
        set_runtime_error(database, "system variable buffer is too small");
        return MYLITE_ERROR;
    }

    memcpy(buffer, schema.default_collation, strlen(schema.default_collation) + 1U);
    *out_value = buffer;
    return MYLITE_OK;
}

static int format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_uint64(database, value, out_cell->integer_text, sizeof(out_cell->integer_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

static int format_long_query_time_microseconds(
    struct mylite_db *database,
    uint64_t microseconds,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    uint64_t seconds = microseconds / microseconds_per_second;
    uint64_t fraction = microseconds % microseconds_per_second;
    int written = 0;

    if (buffer == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(buffer, buffer_size, "%" PRIu64 ".%06" PRIu64, seconds, fraction);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format long_query_time system variable");
        return MYLITE_ERROR;
    }
    *out_value = buffer;
    return MYLITE_OK;
}

static uint64_t boolean_session_placeholder_system_variable_uint64_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
) {
    const char *value = NULL;

    if (!global_scope && database != NULL) {
        value = session_system_variable_override_value(database, kind);
    }
    if (value == NULL) {
        value = mylite_execution_system_variable_default_value(kind);
    }
    if (value != NULL && strcmp(value, "1") == 0) {
        return 1U;
    }
    return 0U;
}

static uint64_t auto_increment_step_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return 1U;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT) {
        return database->session.auto_increment_increment;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET) {
        return database->session.auto_increment_offset;
    }
    return 1U;
}

static uint64_t sql_select_limit_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return UINT64_MAX;
    }
    return database->session.sql_select_limit;
}

static uint64_t timeout_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return timeout_system_variable_default_value_for_kind(kind);
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT) {
        return database->session.wait_timeout;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT) {
        return database->session.interactive_timeout;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT) {
        return database->session.lock_wait_timeout;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT) {
        return database->session.innodb_lock_wait_timeout;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT) {
        return database->session.net_read_timeout;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT) {
        return database->session.net_retry_count;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT) {
        return database->session.net_write_timeout;
    }
    return timeout_system_variable_default_value_for_kind(kind);
}

static uint64_t timeout_system_variable_default_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT) {
        return connect_timeout_default_value;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT) {
        return lock_wait_timeout_default_value;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT) {
        return MYLITE_SESSION_INNODB_LOCK_WAIT_TIMEOUT_DEFAULT_VALUE;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT) {
        return net_read_timeout_default_value;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT) {
        return net_retry_count_default_value;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT) {
        return net_write_timeout_default_value;
    }
    return timeout_system_variable_default_value;
}

static uint64_t timeout_system_variable_min_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT) {
        return connect_timeout_min_value;
    }
    return 1U;
}

static uint64_t timeout_system_variable_max_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT) {
        return UINT64_MAX;
    }
    return timeout_system_variable_max_value;
}

static uint64_t innodb_parallel_read_threads_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return innodb_parallel_read_threads_default_value;
    }
    return database->session.innodb_parallel_read_threads;
}

static uint64_t connection_memory_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE) {
            return connection_memory_chunk_size_default_value;
        }
        return connection_memory_limit_default_value;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE) {
        return database->session.connection_memory_chunk_size;
    }
    return database->session.connection_memory_limit;
}

static uint64_t global_connection_memory_tracking_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    const char *value = NULL;

    if (!global_scope && database != NULL) {
        value = session_system_variable_override_value(
            database,
            MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING
        );
    }
    if (value != NULL && strcmp(value, "1") == 0) {
        return 1U;
    }
    return 0U;
}

static const char *internal_tmp_mem_storage_engine_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return "TempTable";
    }
    if (database->session.internal_tmp_mem_storage_engine ==
        MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_MEMORY) {
        return "MEMORY";
    }
    return "TempTable";
}

static uint64_t join_buffer_size_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return join_buffer_size_default_value;
    }
    return database->session.join_buffer_size;
}

static uint64_t myisam_sort_buffer_size_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return myisam_sort_buffer_size_default_value;
    }
    return database->session.myisam_sort_buffer_size;
}

static const char *myisam_stats_method_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return "nulls_unequal";
    }
    return myisam_stats_method_text(database->session.myisam_stats_method);
}

static uint64_t sort_buffer_size_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return sort_buffer_size_default_value;
    }
    return database->session.sort_buffer_size;
}

static uint64_t low_priority_updates_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL || !database->session.low_priority_updates) {
        return 0U;
    }
    return 1U;
}

static const char *low_priority_updates_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
) {
    return low_priority_updates_system_variable_uint64_value(database, global_scope) != 0U ? "ON"
                                                                                           : "OFF";
}

static const char *transaction_isolation_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    if (system_variable_expression_has_global_scope(expression)) {
        return transaction_isolation_value_text(MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ);
    }
    return transaction_isolation_value_text(database->session.session_transaction_isolation);
}

static const char *transaction_read_only_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    if (system_variable_expression_has_global_scope(expression)) {
        return transaction_read_only_scalar_text(MYLITE_TRANSACTION_ACCESS_READ_WRITE);
    }
    return transaction_read_only_scalar_text(database->session.session_transaction_access_mode);
}

static bool system_variable_expression_has_global_scope(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    size_t offset = system_variable_body_offset;
    size_t scope_start = offset;
    size_t scope_length = 0U;

    if (span == NULL || span->text == NULL || span->length <= offset) {
        return false;
    }
    if (span->text[offset] == '`') {
        return false;
    }
    while (offset < span->length && span->text[offset] != '.') {
        ++offset;
    }
    if (offset == span->length) {
        return false;
    }

    scope_length = offset - scope_start;
    return sql_mode_token_matches(&span->text[scope_start], scope_length, "global");
}

static bool foreign_key_checks_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return true;
    }
    return database->session.foreign_key_checks_enabled;
}

static uint64_t foreign_key_checks_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (foreign_key_checks_system_variable_value(database, global_scope)) {
        return 1U;
    }
    return 0U;
}

static const char *foreign_key_checks_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (foreign_key_checks_system_variable_value(database, global_scope)) {
        return "ON";
    }
    return "OFF";
}

static uint64_t sql_require_primary_key_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL || !database->session.sql_require_primary_key) {
        return 0U;
    }
    return 1U;
}

static const char *sql_require_primary_key_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (sql_require_primary_key_system_variable_uint64_value(database, global_scope) == 0U) {
        return "OFF";
    }
    return "ON";
}

static uint64_t big_tables_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL || !database->session.big_tables) {
        return 0U;
    }
    return 1U;
}

static const char *big_tables_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (big_tables_system_variable_uint64_value(database, global_scope) == 0U) {
        return "OFF";
    }
    return "ON";
}

static const char *default_sql_mode_value(void) {
    return MYLITE_SESSION_SQL_MODE_DEFAULT_TEXT;
}

static const struct mylite_diagnostics *system_variable_count_diagnostics(
    const struct mylite_db *database
) {
    if (database == NULL) {
        return NULL;
    }
    if (mylite_diagnostics_errcode(&database->diagnostics) != MYLITE_OK ||
        mylite_diagnostics_warning_count(&database->diagnostics) > 0U) {
        return &database->diagnostics;
    }
    return &database->previous_diagnostics;
}

static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    struct system_variable_component first = {0};
    struct system_variable_component second = {0};
    const struct system_variable_component *name = &first;
    size_t offset = 2U;
    bool has_scope = false;
    int rc = MYLITE_OK;

    *out_kind = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    if (span == NULL || span->text == NULL || span->length < 3U || span->text[0] != '@' ||
        span->text[1] != '@') {
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    rc = parse_system_variable_component(database, span, &offset, &first);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (offset < span->length && span->text[offset] == '.') {
        has_scope = true;
        ++offset;
        if (first.quoted) {
            set_unsupported_error(database, "unsupported quoted system variable scope");
            return MYLITE_ERROR;
        }
        rc = parse_system_variable_component(database, span, &offset, &second);
        if (rc != MYLITE_OK) {
            return rc;
        }
        name = &second;
    }
    if (offset != span->length || system_variable_component_is_empty(name)) {
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    if (!resolve_system_variable_kind(name, out_kind)) {
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    if (!has_scope) {
        return MYLITE_OK;
    }
    if (system_variable_component_equals(&first, "global")) {
        if (mylite_execution_system_variable_allows_global_scope(*out_kind)) {
            return MYLITE_OK;
        }
        set_session_variable_only_error(database, name->text);
        return MYLITE_ERROR;
    }
    if (system_variable_component_equals(&first, "session") ||
        system_variable_component_equals(&first, "local")) {
        if (!mylite_execution_system_variable_allows_session_scope(*out_kind)) {
            set_global_variable_only_error(database, name->text);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    set_unknown_system_variable_error(database, expression);
    return MYLITE_ERROR;
}

static bool resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum mylite_execution_system_variable_kind *out_kind
) {
    const struct mylite_execution_system_variable_descriptor *descriptor =
        mylite_execution_system_variable_descriptor_by_name(name == NULL ? NULL : name->text);

    if (descriptor != NULL && system_variable_component_equals(name, descriptor->name)) {
        *out_kind = descriptor->kind;
        return true;
    }

    return false;
}

static int show_system_variable_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
) {
    if (out_value == NULL || mylite_execution_system_variable_descriptor_by_kind(kind) == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES descriptor");
        return MYLITE_ERROR;
    }
    *out_value = NULL;
    if (!global_scope) {
        *out_value = session_system_variable_override_show_value(database, kind);
        if (*out_value != NULL) {
            return MYLITE_OK;
        }
    }

    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ACCOUNTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_DIGESTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ERROR_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_HOSTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_INDEX_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MEMORY_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METADATA_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METER_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METRIC_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PREPARED_STATEMENTS_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PROGRAM_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STAGE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_STACK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_LOCK_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_USERS_SIZE:
        *out_value = performance_schema_show_system_variable_value(kind);
        return *out_value == NULL ? MYLITE_ERROR : MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        *out_value = database->session.character_set_client;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
        *out_value = database->session.character_set_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        *out_value = database->session.character_set_results;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        *out_value = database->session.collation_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
        *out_value = "utf8mb4";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
        return database_character_set_system_variable_value(
            database,
            global_scope,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER:
        *out_value = "utf8mb4_0900_ai_ci";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE:
        return database_collation_system_variable_value(
            database,
            global_scope,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
        *out_value = "InnoDB";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
        *out_value = "YES";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
        *out_value = "utf8mb3";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
        *out_value = "binary";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE:
        *out_value = global_scope ? default_sql_mode_value() : database->session.sql_mode_text;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE:
        *out_value = "UTC";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE:
        *out_value = global_scope ? "SYSTEM" : database->session.time_zone;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
        *out_value = basedir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
        *out_value = datadir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
        *out_value = hostname_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
        *out_value = license_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG_FILE:
        *out_value = general_log_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR:
        *out_value = "stderr";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SERVICES:
        *out_value = "log_filter_internal; log_sink_internal";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SUPPRESSION_LIST:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_OUTPUT:
        *out_value = "FILE";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_TIMESTAMPS:
        *out_value = "UTC";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
        *out_value = log_bin_basename_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
        *out_value = log_bin_index_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG:
        *out_value = relay_log_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME:
        *out_value = relay_log_basename_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX:
        *out_value = relay_log_index_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_HOST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_USER:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
        *out_value = pid_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
        *out_value = plugin_dir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
        *out_value = version_compile_machine_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
        *out_value = version_compile_os_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        *out_value = version_compile_zlib_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
        *out_value = server_uuid_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
        *out_value = socket_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIND_ADDRESS:
        *out_value = bind_address_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BLOCK_ENCRYPTION_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BUILD_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BULK_INSERT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SWITCH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_FEATURES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ERROR_ACTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_IMAGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_VALUE_OPTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION_LEVEL_ZSTD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE:
        *out_value = mylite_execution_system_variable_default_value(kind);
        return *out_value == NULL ? MYLITE_ERROR : MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG_FILE:
        *out_value = slow_query_log_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        return format_show_system_variable_uint64_value(
            database,
            auto_increment_step_system_variable_value(database, kind, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        return format_show_system_variable_uint64_value(
            database,
            timeout_system_variable_value(database, kind, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BACK_LOG:
        return format_show_system_variable_uint64_value(
            database,
            back_log_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOST_CACHE_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            host_cache_size_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_LIMIT:
        return format_show_system_variable_uint64_value(
            database,
            connection_memory_system_variable_value(database, kind, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERNAL_TMP_MEM_STORAGE_ENGINE:
        *out_value = internal_tmp_mem_storage_engine_system_variable_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_JOIN_BUFFER_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            join_buffer_size_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            myisam_data_pointer_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            myisam_max_sort_file_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MMAP_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            myisam_mmap_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_RECOVER_OPTIONS:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_SORT_BUFFER_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            myisam_sort_buffer_size_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_STATS_METHOD:
        *out_value = myisam_stats_method_system_variable_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
        return format_show_system_variable_uint64_value(
            database,
            innodb_adaptive_flushing_lwm_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX_PARTS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_adaptive_hash_index_parts_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_adaptive_max_sleep_delay_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_autoextend_increment_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOINC_LOCK_MODE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_autoinc_lock_mode_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_CHUNK_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_buffer_pool_chunk_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_buffer_pool_dump_pct_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
        *out_value = innodb_buffer_pool_filename_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_INSTANCES:
        return format_show_system_variable_uint64_value(
            database,
            innodb_buffer_pool_instances_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_AT_STARTUP:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_buffer_pool_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_change_buffer_max_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
        *out_value = innodb_change_buffering_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
        *out_value = innodb_checksum_algorithm_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_commit_concurrency_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_compression_failure_threshold_pct_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
        return format_show_system_variable_uint64_value(
            database,
            innodb_compression_level_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
        return format_show_system_variable_uint64_value(
            database,
            innodb_compression_pad_pct_max_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_concurrency_tickets_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_FILE_PATH:
        *out_value = innodb_data_file_path_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_HOME_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DIRECTORIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_DIR:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ddl_buffer_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ddl_threads_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEDICATED_SERVER:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
        *out_value = innodb_default_row_format_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
        *out_value = innodb_doublewrite_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_BATCH_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_doublewrite_batch_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_FILES:
        return format_show_system_variable_uint64_value(
            database,
            innodb_doublewrite_files_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_PAGES:
        return format_show_system_variable_uint64_value(
            database,
            innodb_doublewrite_pages_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
        return format_show_system_variable_uint64_value(
            database,
            innodb_fast_shutdown_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
        return format_show_system_variable_uint64_value(
            database,
            innodb_fill_factor_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_flush_log_at_timeout_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_flush_log_at_trx_commit_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_METHOD:
        *out_value = innodb_flush_method_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_flush_neighbors_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_flushing_avg_loops_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_LOAD_CORRUPTED:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_RECOVERY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_force_recovery_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
        return format_show_system_variable_uint64_value(
            database,
            innodb_fsync_threshold_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_CACHE_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ft_cache_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MAX_TOKEN_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ft_max_token_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MIN_TOKEN_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ft_min_token_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ft_num_word_optimize_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ft_result_cache_limit_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SORT_PLL_DEGREE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ft_sort_pll_degree_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_TOTAL_CACHE_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_ft_total_cache_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_idle_flush_pct_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_io_capacity_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
        return format_show_system_variable_uint64_value(
            database,
            innodb_io_capacity_max_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_log_buffer_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILE_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_log_file_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILES_IN_GROUP:
        return format_show_system_variable_uint64_value(
            database,
            innodb_log_files_in_group_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_GROUP_HOME_DIR:
        *out_value = innodb_log_group_home_dir_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
        return format_show_system_variable_uint64_value(
            database,
            innodb_log_spin_cpu_abs_lwm_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
        return format_show_system_variable_uint64_value(
            database,
            innodb_log_spin_cpu_pct_hwm_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
        return format_show_system_variable_uint64_value(
            database,
            innodb_log_wait_for_flush_spin_hwm_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_log_write_ahead_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
        return format_show_system_variable_uint64_value(
            database,
            innodb_lru_scan_depth_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
        *out_value = innodb_max_dirty_pages_pct_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
        *out_value = innodb_max_dirty_pages_pct_lwm_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
        return format_show_system_variable_uint64_value(
            database,
            innodb_max_purge_lag_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_max_undo_log_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
        return format_show_system_variable_uint64_value(
            database,
            innodb_old_blocks_pct_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
        return format_show_system_variable_uint64_value(
            database,
            innodb_old_blocks_time_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_online_alter_log_max_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPEN_FILES:
        return format_show_system_variable_uint64_value(
            database,
            innodb_open_files_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_CLEANERS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_page_cleaners_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_page_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_parallel_read_threads_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_purge_batch_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_purge_rseg_truncate_frequency_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_THREADS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_purge_threads_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
        return format_show_system_variable_uint64_value(
            database,
            innodb_read_ahead_threshold_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_IO_THREADS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_read_io_threads_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_redo_log_capacity_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_replication_delay_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_ON_TIMEOUT:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_rollback_segments_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
        *out_value = innodb_segment_reserve_factor_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SORT_BUFFER_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_sort_buffer_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_spin_wait_delay_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
        return format_show_system_variable_uint64_value(
            database,
            innodb_spin_wait_pause_multiplier_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
        *out_value = innodb_stats_method_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
        return format_show_system_variable_uint64_value(
            database,
            innodb_stats_persistent_sample_pages_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
        return format_show_system_variable_uint64_value(
            database,
            innodb_stats_transient_sample_pages_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
        *out_value = boolean_session_placeholder_system_variable_uint64_value(
                         database,
                         kind,
                         global_scope
                     ) == 0U
                         ? "OFF"
                         : "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_ARRAY_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            innodb_sync_array_size_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_sync_spin_loops_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
        *out_value = boolean_session_placeholder_system_variable_uint64_value(
                         database,
                         kind,
                         global_scope
                     ) == 0U
                         ? "OFF"
                         : "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_DATA_FILE_PATH:
        *out_value = innodb_temp_data_file_path_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_TABLESPACES_DIR:
        *out_value = innodb_temp_tablespaces_dir_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_thread_concurrency_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
        return format_show_system_variable_uint64_value(
            database,
            innodb_thread_sleep_delay_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_DIRECTORY:
        *out_value = innodb_undo_directory_default_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
        return format_show_system_variable_uint64_value(
            database,
            innodb_undo_tablespaces_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_NATIVE_AIO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VALIDATE_TABLESPACE_PATHS:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VERSION:
        *out_value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_WRITE_IO_THREADS:
        return format_show_system_variable_uint64_value(
            database,
            innodb_write_io_threads_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_LIMIT:
        return format_show_system_variable_uint64_value(
            database,
            global_connection_memory_limit_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_FAILED_CONNECTIONS_THRESHOLD:
        return format_show_system_variable_uint64_value(
            database,
            connection_control_failed_connections_threshold_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MAX_CONNECTION_DELAY:
        return format_show_system_variable_uint64_value(
            database,
            connection_control_max_connection_delay_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MIN_CONNECTION_DELAY:
        return format_show_system_variable_uint64_value(
            database,
            connection_control_min_connection_delay_default_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING:
        *out_value =
            global_connection_memory_tracking_system_variable_value(database, global_scope) != 0U
                ? "ON"
                : "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SORT_BUFFER_SIZE:
        return format_show_system_variable_uint64_value(
            database,
            sort_buffer_size_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOW_PRIORITY_UPDATES:
        *out_value = low_priority_updates_system_variable_show_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        return format_show_system_variable_uint64_value(
            database,
            global_scope ? group_concat_max_len_default_value
                         : database->session.group_concat_max_len,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        return format_show_system_variable_uint64_value(
            database,
            information_schema_stats_expiry_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LONG_QUERY_TIME:
        return format_long_query_time_microseconds(
            database,
            global_scope ? long_query_time_default_microseconds
                         : database->session.long_query_time_microseconds,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
        return format_show_system_variable_uint64_value(
            database,
            global_scope ? max_error_count_default_value : database->session.max_error_count,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        *out_value = foreign_key_checks_system_variable_show_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
        *out_value = sql_require_primary_key_system_variable_show_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        *out_value = big_tables_system_variable_show_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
        *out_value = global_scope || database->session.autocommit_enabled ? "ON" : "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_REPLICA_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_MODE:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_DIRECT_NON_TRANSACTIONAL_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROWS_QUERY_LOG_EVENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_RAW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_ADMIN_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_EXTRA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_REPLICA_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CREATE_ADMIN_LISTENER_THREAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_FIPS_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_THROTTLE_QUERIES_NOT_USING_INDEXES:
        *out_value = "0";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_LAUNCH_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_VERBOSITY:
        *out_value = "2";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        return format_show_system_variable_uint64_value(
            database,
            server_id_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        return format_show_system_variable_uint64_value(
            database,
            server_id_bits_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        return format_show_system_variable_uint64_value(
            database,
            port_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        return format_show_system_variable_uint64_value(
            database,
            protocol_version_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET: {
        int rc = format_uint64(
            database,
            max_allowed_packet_default_value,
            integer_buffer,
            integer_buffer_size
        );
        if (rc == MYLITE_OK) {
            *out_value = integer_buffer;
        }
        return rc;
    }
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
        *out_value = transaction_isolation_value_text(
            global_scope ? MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ
                         : database->session.session_transaction_isolation
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
        *out_value = transaction_read_only_show_text(
            global_scope ? MYLITE_TRANSACTION_ACCESS_READ_WRITE
                         : database->session.session_transaction_access_mode
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT: {
        int rc = format_uint64(
            database,
            sql_select_limit_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size
        );
        if (rc == MYLITE_OK) {
            *out_value = integer_buffer;
        }
        return rc;
    }
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_IDENTITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LAST_INSERT_ID:
        return format_show_system_variable_uint64_value(
            database,
            database->session.last_insert_id,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP: {
        int rc =
            format_timestamp_system_variable_value(database, integer_buffer, integer_buffer_size);
        if (rc == MYLITE_OK) {
            *out_value = integer_buffer;
        }
        return rc;
    }
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION:
        *out_value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMMENT:
        *out_value = MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_COMPRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_DYNAMIC_LOADING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_QUERY_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_RTREE_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_STATEMENT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_SYMLINK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COMPLETION_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONCURRENT_INSERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CTE_MAX_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAY_KEY_WRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISABLED_STORAGE_ENGINES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DIV_PRECISION_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ENFORCE_GTID_CONSISTENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EQ_RANGE_INDEX_DIVE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EVENT_SCHEDULER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_JSON_FORMAT_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXTERNAL_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_BOOLEAN_SYNTAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MAX_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MIN_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_QUERY_EXPANSION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_STOPWORD_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERATED_RANDOM_PASSWORD_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_REPLICATION_CONSISTENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED_COMPRESSION_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_NEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HISTOGRAM_GENERATION_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_IMMEDIATE_SERVER_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_CONNECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_REPLICA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECT_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_PREPARED_STMT_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_RELAY_LOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_WRITE_LOCK_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTHENTICATION_POLICY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_DIGEST_ROUNDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_FILES_SUPPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCKED_IN_MEMORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_BIND_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DOCUMENT_ID_UNIQUE_PREFIX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_IDLE_WORKER_THREAD_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MIN_WORKER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT_OPEN_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_READ_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WRITE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NGRAM_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OFFLINE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_ONLY_ADMIN_X509_SUBJECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_SENSITIVE_VARIABLES_IN_PLAINTEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSISTED_GLOBALS_LOAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_SPACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SCHEMA_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_SOURCE_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_ENCRYPTION_PRIVILEGE_CHECK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLESPACE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_STACK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_USE_SECONDARY_ENGINE:
        if (mylite_execution_system_variable_is_boolean_session_placeholder(kind)) {
            const char *value = mylite_execution_system_variable_default_value(kind);

            *out_value = (value != NULL && strcmp(value, "1") == 0) ? "ON" : "OFF";
            return MYLITE_OK;
        }
        *out_value = mylite_execution_system_variable_default_value(kind);
        return *out_value == NULL ? MYLITE_ERROR : MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE:
        break;
    }

    *out_value = mylite_execution_system_variable_default_value(kind);
    if (*out_value != NULL) {
        return MYLITE_OK;
    }

    set_runtime_error(database, "unsupported SHOW VARIABLES value");
    return MYLITE_ERROR;
}

static const char *performance_schema_show_system_variable_value(
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA) {
        return "ON";
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST) {
        return "OFF";
    }
    return performance_schema_scalar_system_variable_value(kind);
}

static int format_show_system_variable_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
) {
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_uint64(database, value, integer_buffer, integer_buffer_size);
    if (rc == MYLITE_OK) {
        *out_value = integer_buffer;
    }
    return rc;
}

static int format_timestamp_system_variable_value(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid timestamp system variable buffer");
        return MYLITE_ERROR;
    }
    written =
        snprintf(buffer, buffer_size, "%" PRId64 ".000000", current_timestamp_epoch(database));
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format timestamp system variable");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@binlog_format' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@binlog_max_flush_queue_time' is deprecated and will be removed in a future "
            "release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@delayed_insert_limit' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@delayed_insert_timeout' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@delayed_queue_size' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'temptable_use_mmap' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@max_delayed_threads' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@max_insert_delayed_threads' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@max_length_for_sort_data' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@sql_slave_skip_counter' is deprecated and will be removed in a future release. "
            "Please use sql_replica_skip_counter instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@init_slave' is deprecated and will be removed in a future release. Please use "
            "init_replica instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@log_bin_trust_function_creators' is deprecated and will be removed in a future "
            "release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@log_slave_updates' is deprecated and will be removed in a future release. "
            "Please use log_replica_updates instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@log_slow_slave_statements' is deprecated and will be removed in a future "
            "release. Please use log_slow_replica_statements instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@log_statements_unsafe_for_binlog' is deprecated and will be removed in a future "
            "release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@master_verify_checksum' is deprecated and will be removed in a future release. "
            "Please use source_verify_checksum instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@replica_parallel_type' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@rpl_stop_slave_timeout' is deprecated and will be removed in a future release. "
            "Please use rpl_stop_replica_timeout instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@skip_slave_start' is deprecated and will be removed in a future release. Please "
            "use skip_replica_start instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@sync_master_info' is deprecated and will be removed in a future release. Please "
            "use sync_source_info instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@sync_relay_log_info' is deprecated and will be removed in a future release."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_allow_batching' is deprecated and will be removed in a future release. "
            "Please use replica_allow_batching instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_checkpoint_group' is deprecated and will be removed in a future release. "
            "Please use replica_checkpoint_group instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_checkpoint_period' is deprecated and will be removed in a future release. "
            "Please use replica_checkpoint_period instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_compressed_protocol' is deprecated and will be removed in a future release. "
            "Please use replica_compressed_protocol instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_exec_mode' is deprecated and will be removed in a future release. Please "
            "use replica_exec_mode instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_load_tmpdir' is deprecated and will be removed in a future release. Please "
            "use replica_load_tmpdir instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_max_allowed_packet' is deprecated and will be removed in a future release. "
            "Please use replica_max_allowed_packet instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_net_timeout' is deprecated and will be removed in a future release. Please "
            "use replica_net_timeout instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_parallel_type' is deprecated and will be removed in a future release. "
            "Please use replica_parallel_type instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_parallel_workers' is deprecated and will be removed in a future release. "
            "Please use replica_parallel_workers instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_pending_jobs_size_max' is deprecated and will be removed in a future "
            "release. Please use replica_pending_jobs_size_max instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_preserve_commit_order' is deprecated and will be removed in a future "
            "release. Please use replica_preserve_commit_order instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_skip_errors' is deprecated and will be removed in a future release. Please "
            "use replica_skip_errors instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_sql_verify_checksum' is deprecated and will be removed in a future release. "
            "Please use replica_sql_verify_checksum instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_transaction_retries' is deprecated and will be removed in a future release. "
            "Please use replica_transaction_retries instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@slave_type_conversions' is deprecated and will be removed in a future release. "
            "Please use replica_type_conversions instead."
        );
    }

    return MYLITE_OK;
}

static int parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
) {
    size_t component_length = 0U;

    *out_component = (struct system_variable_component){0};
    if (*offset >= span->length) {
        return MYLITE_OK;
    }

    if (span->text[*offset] == '`') {
        out_component->quoted = true;
        ++*offset;
        while (*offset < span->length) {
            char value = span->text[*offset];

            if (value == '`') {
                ++*offset;
                if (*offset < span->length && span->text[*offset] == '`') {
                    int rc = append_quoted_system_variable_byte(
                        database,
                        out_component,
                        &component_length,
                        '`'
                    );
                    if (rc != MYLITE_OK) {
                        return rc;
                    }
                    ++*offset;
                    continue;
                }
                return MYLITE_OK;
            }

            {
                int rc = append_quoted_system_variable_byte(
                    database,
                    out_component,
                    &component_length,
                    value
                );
                if (rc != MYLITE_OK) {
                    return rc;
                }
            }
            ++*offset;
        }

        set_unsupported_error(database, "unterminated system variable identifier");
        return MYLITE_ERROR;
    }

    while (*offset < span->length && span->text[*offset] != '.') {
        if (component_length + 1U >= sizeof(out_component->text)) {
            set_unknown_system_variable_error(database, NULL);
            return MYLITE_ERROR;
        }
        out_component->text[component_length] = span->text[*offset];
        ++component_length;
        ++*offset;
    }
    out_component->text[component_length] = '\0';

    return MYLITE_OK;
}

static int append_quoted_system_variable_byte(
    struct mylite_db *database,
    struct system_variable_component *component,
    size_t *component_length,
    char value
) {
    if (*component_length + 1U >= sizeof(component->text)) {
        set_unknown_system_variable_error(database, NULL);
        return MYLITE_ERROR;
    }

    component->text[*component_length] = value;
    ++*component_length;
    component->text[*component_length] = '\0';
    return MYLITE_OK;
}

static bool system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
) {
    return text_equals_ascii_case_insensitive(component == NULL ? NULL : component->text, expected);
}

static bool system_variable_component_is_empty(const struct system_variable_component *component) {
    if (component == NULL) {
        return true;
    }
    return component->text[0] == '\0';
}

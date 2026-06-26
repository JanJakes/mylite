
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
    if (!global_scope && variable != MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT) {
        override_value = session_system_variable_override_value(database, variable);
        if (override_value != NULL) {
            out_cell->value = override_value;
            return MYLITE_OK;
        }
    }

    switch (variable) {
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
        return format_session_scalar_uint64_value(database, database->session.last_insert_id, out_cell);
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
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

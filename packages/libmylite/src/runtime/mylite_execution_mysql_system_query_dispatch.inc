static int execute_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_row_set rows = {0};
    struct information_schema_result_projection_buffers stream_buffers = {0};
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    bool stream_during_enumeration =
        !query->is_count_star && !query->has_group && !query->has_order &&
        query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES &&
        query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS;
    bool skip_row_enumeration =
        (query->limit.has_limit && query->limit.row_count == 0) ||
        (query->is_count_star && query->limit.has_offset && query->limit.offset != 0);
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_result_columns(database, result, query);
    }
    if (rc == MYLITE_OK && stream_during_enumeration && !skip_row_enumeration) {
        rc = information_schema_result_projection_buffers_init(database, query, &stream_buffers);
    }
    if (rc == MYLITE_OK && !skip_row_enumeration) {
        rc = build_mysql_system_table_rows(
            database,
            context,
            statement,
            query,
            definition,
            stream_during_enumeration ? result : NULL,
            stream_during_enumeration ? &stream_buffers : NULL,
            &rows
        );
    }
    if (rc == MYLITE_OK && query->is_count_star) {
        rc = information_schema_append_count_result(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    } else if (rc == MYLITE_OK && stream_during_enumeration) {
        read_row_count = rows.row_count;
    } else if (rc == MYLITE_OK) {
        rc = information_schema_append_result_rows(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    }
    information_schema_result_projection_buffers_deinit(&stream_buffers);
    information_schema_row_set_deinit(&rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    if (apply_sql_select_limit && !query->limit.has_limit) {
        apply_sql_select_limit_to_result(database, result);
    }
    return finish_successful_result(database, result, out_result);
}

static int build_mysql_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    mylite_result *stream_result,
    struct information_schema_result_projection_buffers *stream_buffers,
    struct information_schema_row_set *out_rows
) {
    bool has_no_rows = mysql_system_table_definition_has_no_rows(definition);
    bool has_catalog_rows = mysql_system_table_definition_has_catalog_rows(definition);
    int rc = MYLITE_OK;

    rc = information_schema_row_set_init(
        database,
        out_rows,
        statement,
        query,
        &definition->query_definition,
        stream_result,
        stream_buffers
    );
    if (rc == MYLITE_OK && out_rows->has_row_limit && out_rows->row_limit == 0U) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_system_table_system_rows(database, context, definition, out_rows);
    }
    if (rc == MYLITE_DONE) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && !has_no_rows && has_catalog_rows) {
        rc = append_mysql_system_table_catalog_rows(database, definition, out_rows);
    }
    if (rc == MYLITE_DONE) {
        return MYLITE_OK;
    }
    if (rc != MYLITE_OK) {
        information_schema_row_set_deinit(out_rows);
    }
    return rc;
}

static int append_mysql_system_table_system_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    if (mysql_system_table_definition_has_no_rows(definition)) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") == 0) {
        return append_mysql_schema_system_table_rows(database, definition, rows);
    }
    if (strcmp(definition->schema_name, "performance_schema") == 0) {
        return append_performance_schema_system_table_rows(database, context, definition, rows);
    }
    if (strcmp(definition->schema_name, "sys") == 0) {
        return append_sys_schema_system_table_rows(database, context, definition, rows);
    }

    set_runtime_error(database, "invalid mysql system table");
    return MYLITE_ERROR;
}

static int append_mysql_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    if (strcmp(definition->query_definition.name, "engine_cost") == 0) {
        return append_mysql_engine_cost_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "innodb_index_stats") == 0) {
        return append_mysql_innodb_index_stats_builtin_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "innodb_table_stats") == 0) {
        return append_mysql_innodb_table_stats_builtin_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "plugin") == 0) {
        return append_mysql_plugin_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "server_cost") == 0) {
        return append_mysql_server_cost_system_rows(database, rows);
    }

    set_runtime_error(database, "invalid mysql system table");
    return MYLITE_ERROR;
}

static int append_performance_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    if (strcmp(definition->query_definition.name, "accounts") == 0) {
        return append_performance_schema_accounts_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "cond_instances") == 0 ||
        strcmp(definition->query_definition.name, "binary_log_transaction_compression_stats") ==
            0 ||
        strcmp(definition->query_definition.name, "data_locks") == 0 ||
        strcmp(definition->query_definition.name, "data_lock_waits") == 0 ||
        strcmp(definition->query_definition.name, "error_log") == 0 ||
        strcmp(definition->query_definition.name, "events_errors_summary_by_account_by_error") ==
            0 ||
        strcmp(definition->query_definition.name, "events_errors_summary_by_host_by_error") == 0 ||
        strcmp(definition->query_definition.name, "events_errors_summary_by_thread_by_error") ==
            0 ||
        strcmp(definition->query_definition.name, "events_errors_summary_by_user_by_error") == 0 ||
        strcmp(definition->query_definition.name, "events_errors_summary_global_by_error") == 0 ||
        strcmp(definition->query_definition.name, "events_stages_current") == 0 ||
        strcmp(definition->query_definition.name, "events_stages_history") == 0 ||
        strcmp(definition->query_definition.name, "events_stages_history_long") == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_stages_summary_by_account_by_event_name"
        ) == 0 ||
        strcmp(definition->query_definition.name, "events_stages_summary_by_host_by_event_name") ==
            0 ||
        strcmp(
            definition->query_definition.name,
            "events_stages_summary_by_thread_by_event_name"
        ) == 0 ||
        strcmp(definition->query_definition.name, "events_stages_summary_by_user_by_event_name") ==
            0 ||
        strcmp(definition->query_definition.name, "events_stages_summary_global_by_event_name") ==
            0 ||
        strcmp(definition->query_definition.name, "events_statements_current") == 0 ||
        strcmp(definition->query_definition.name, "events_statements_histogram_by_digest") == 0 ||
        strcmp(definition->query_definition.name, "events_statements_histogram_global") == 0 ||
        strcmp(definition->query_definition.name, "events_statements_history") == 0 ||
        strcmp(definition->query_definition.name, "events_statements_history_long") == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_statements_summary_by_account_by_event_name"
        ) == 0 ||
        strcmp(definition->query_definition.name, "events_statements_summary_by_digest") == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_statements_summary_by_host_by_event_name"
        ) == 0 ||
        strcmp(definition->query_definition.name, "events_statements_summary_by_program") == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_statements_summary_by_thread_by_event_name"
        ) == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_statements_summary_by_user_by_event_name"
        ) == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_statements_summary_global_by_event_name"
        ) == 0 ||
        strcmp(definition->query_definition.name, "events_transactions_current") == 0 ||
        strcmp(definition->query_definition.name, "events_transactions_history") == 0 ||
        strcmp(definition->query_definition.name, "events_transactions_history_long") == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_transactions_summary_by_account_by_event_name"
        ) == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_transactions_summary_by_host_by_event_name"
        ) == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_transactions_summary_by_thread_by_event_name"
        ) == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_transactions_summary_by_user_by_event_name"
        ) == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_transactions_summary_global_by_event_name"
        ) == 0 ||
        strcmp(definition->query_definition.name, "events_waits_current") == 0 ||
        strcmp(definition->query_definition.name, "events_waits_history") == 0 ||
        strcmp(definition->query_definition.name, "events_waits_history_long") == 0 ||
        strcmp(
            definition->query_definition.name,
            "events_waits_summary_by_account_by_event_name"
        ) == 0 ||
        strcmp(definition->query_definition.name, "events_waits_summary_by_host_by_event_name") ==
            0 ||
        strcmp(definition->query_definition.name, "events_waits_summary_by_instance") == 0 ||
        strcmp(definition->query_definition.name, "events_waits_summary_by_thread_by_event_name") ==
            0 ||
        strcmp(definition->query_definition.name, "events_waits_summary_by_user_by_event_name") ==
            0 ||
        strcmp(definition->query_definition.name, "events_waits_summary_global_by_event_name") ==
            0 ||
        strcmp(definition->query_definition.name, "file_instances") == 0 ||
        strcmp(definition->query_definition.name, "file_summary_by_event_name") == 0 ||
        strcmp(definition->query_definition.name, "file_summary_by_instance") == 0 ||
        strcmp(definition->query_definition.name, "host_cache") == 0 ||
        strcmp(definition->query_definition.name, "keyring_component_status") == 0 ||
        strcmp(definition->query_definition.name, "keyring_keys") == 0 ||
        strcmp(definition->query_definition.name, "memory_summary_by_account_by_event_name") == 0 ||
        strcmp(definition->query_definition.name, "memory_summary_by_host_by_event_name") == 0 ||
        strcmp(definition->query_definition.name, "memory_summary_by_thread_by_event_name") == 0 ||
        strcmp(definition->query_definition.name, "memory_summary_by_user_by_event_name") == 0 ||
        strcmp(definition->query_definition.name, "memory_summary_global_by_event_name") == 0 ||
        strcmp(definition->query_definition.name, "mutex_instances") == 0 ||
        strcmp(definition->query_definition.name, "prepared_statements_instances") == 0 ||
        strcmp(definition->query_definition.name, "replication_applier_configuration") == 0 ||
        strcmp(definition->query_definition.name, "replication_applier_filters") == 0 ||
        strcmp(definition->query_definition.name, "replication_applier_global_filters") == 0 ||
        strcmp(definition->query_definition.name, "replication_applier_status") == 0 ||
        strcmp(definition->query_definition.name, "replication_applier_status_by_coordinator") ==
            0 ||
        strcmp(definition->query_definition.name, "replication_applier_status_by_worker") == 0 ||
        strcmp(definition->query_definition.name, "replication_asynchronous_connection_failover") ==
            0 ||
        strcmp(
            definition->query_definition.name,
            "replication_asynchronous_connection_failover_managed"
        ) == 0 ||
        strcmp(definition->query_definition.name, "replication_connection_configuration") == 0 ||
        strcmp(definition->query_definition.name, "replication_connection_status") == 0 ||
        strcmp(definition->query_definition.name, "replication_group_member_stats") == 0 ||
        strcmp(definition->query_definition.name, "replication_group_members") == 0 ||
        strcmp(definition->query_definition.name, "rwlock_instances") == 0 ||
        strcmp(definition->query_definition.name, "socket_instances") == 0 ||
        strcmp(definition->query_definition.name, "socket_summary_by_event_name") == 0 ||
        strcmp(definition->query_definition.name, "socket_summary_by_instance") == 0 ||
        strcmp(definition->query_definition.name, "table_io_waits_summary_by_index_usage") == 0 ||
        strcmp(definition->query_definition.name, "table_io_waits_summary_by_table") == 0 ||
        strcmp(definition->query_definition.name, "table_lock_waits_summary_by_table") == 0) {
        return MYLITE_OK;
    }
    if (strcmp(definition->query_definition.name, "global_variables") == 0) {
        return append_performance_schema_variable_rows(database, rows, true);
    }
    if (strcmp(definition->query_definition.name, "hosts") == 0) {
        return append_performance_schema_hosts_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "processlist") == 0) {
        return append_performance_schema_processlist_rows(database, context, rows);
    }
    if (strcmp(definition->query_definition.name, "session_variables") == 0) {
        return append_performance_schema_variable_rows(database, rows, false);
    }
    if (strcmp(definition->query_definition.name, "global_status") == 0) {
        return append_performance_schema_status_rows(database, rows, true);
    }
    if (strcmp(definition->query_definition.name, "variables_by_thread") == 0) {
        return append_performance_schema_variable_by_thread_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "variables_info") == 0) {
        return append_performance_schema_variables_info_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "status_by_account") == 0) {
        return append_performance_schema_status_by_account_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "status_by_host") == 0) {
        return append_performance_schema_status_by_host_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "status_by_thread") == 0) {
        return append_performance_schema_status_by_thread_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "status_by_user") == 0) {
        return append_performance_schema_status_by_user_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "performance_timers") == 0) {
        return append_performance_schema_performance_timer_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "persisted_variables") == 0) {
        return MYLITE_OK;
    }
    if (strcmp(definition->query_definition.name, "log_status") == 0) {
        return append_performance_schema_log_status_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_actors") == 0) {
        return append_performance_schema_setup_actor_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_consumers") == 0) {
        return append_performance_schema_setup_consumer_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_instruments") == 0) {
        return append_performance_schema_setup_instrument_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_loggers") == 0) {
        return append_performance_schema_setup_logger_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_meters") == 0) {
        return append_performance_schema_setup_meter_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_metrics") == 0) {
        return append_performance_schema_setup_metric_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_objects") == 0) {
        return append_performance_schema_setup_object_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "setup_threads") == 0) {
        return append_performance_schema_setup_thread_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "session_account_connect_attrs") == 0 ||
        strcmp(definition->query_definition.name, "session_connect_attrs") == 0) {
        return append_performance_schema_connection_attribute_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "metadata_locks") == 0 ||
        strcmp(definition->query_definition.name, "objects_summary_global_by_type") == 0 ||
        strcmp(definition->query_definition.name, "table_handles") == 0 ||
        strcmp(definition->query_definition.name, "tls_channel_status") == 0) {
        return MYLITE_OK;
    }
    if (strcmp(definition->query_definition.name, "threads") == 0) {
        return append_performance_schema_threads_rows(database, context, rows);
    }
    if (strcmp(definition->query_definition.name, "user_defined_functions") == 0) {
        return append_performance_schema_user_defined_function_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "user_variables_by_thread") == 0) {
        return append_performance_schema_user_variable_by_thread_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "users") == 0) {
        return append_performance_schema_users_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "session_status") == 0) {
        return append_performance_schema_status_rows(database, rows, false);
    }

    set_runtime_error(database, "invalid performance_schema system table");
    return MYLITE_ERROR;
}

static int append_performance_schema_accounts_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_accounts_column_count = 6 };

    char count_text[integer_text_capacity];
    const char *const values[performance_schema_accounts_column_count] = {
        "root",
        "%",
        count_text,
        count_text,
        "0",
        "0",
    };
    int rc = format_performance_schema_session_count(database, count_text, sizeof(count_text));

    if (rc == MYLITE_OK) {
        rc = append_performance_schema_connection_count_row(
            database,
            rows,
            values,
            performance_schema_accounts_column_count
        );
    }
    return rc;
}

static int append_performance_schema_hosts_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_hosts_column_count = 5 };

    char count_text[integer_text_capacity];
    const char *const values[performance_schema_hosts_column_count] = {
        "%",
        count_text,
        count_text,
        "0",
        "0",
    };
    int rc = format_performance_schema_session_count(database, count_text, sizeof(count_text));

    if (rc == MYLITE_OK) {
        rc = append_performance_schema_connection_count_row(
            database,
            rows,
            values,
            performance_schema_hosts_column_count
        );
    }
    return rc;
}

static int append_performance_schema_users_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_users_column_count = 5 };

    char count_text[integer_text_capacity];
    const char *const values[performance_schema_users_column_count] = {
        "root",
        count_text,
        count_text,
        "0",
        "0",
    };
    int rc = format_performance_schema_session_count(database, count_text, sizeof(count_text));

    if (rc == MYLITE_OK) {
        rc = append_performance_schema_connection_count_row(
            database,
            rows,
            values,
            performance_schema_users_column_count
        );
    }
    return rc;
}

static int append_performance_schema_processlist_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
) {
    enum {
        performance_schema_processlist_column_count = 9,
        performance_schema_processlist_info_column = 7,
    };
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_processlist_column_count) {
        set_runtime_error(database, "invalid performance_schema processlist columns");
        return MYLITE_ERROR;
    }

    rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to collect performance_schema.processlist sessions");
        return rc;
    }

    for (size_t session_index = 0U; rc == MYLITE_OK && session_index < session_count;
         ++session_index) {
        const struct mylite_processlist_session_snapshot *session = &sessions[session_index];
        char id_text[integer_text_capacity];
        char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
        char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
        char *info = NULL;
        const char *values[performance_schema_processlist_column_count] = {
            id_text,
            user,
            host,
            session->has_selected_schema ? session->selected_schema : NULL,
            session->is_current ? "Query" : "Sleep",
            "0",
            session->is_current ? "executing" : NULL,
            NULL,
            session->is_current ? "PRIMARY" : NULL,
        };
        int written = snprintf(id_text, sizeof(id_text), "%" PRIu64, session->connection_id);

        if (written < 0 || (size_t)written >= sizeof(id_text)) {
            set_runtime_error(database, "failed to format performance_schema.processlist id");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = format_processlist_user_host(
                database,
                session->client_user_identity,
                user,
                sizeof(user),
                host,
                sizeof(host)
            );
        }
        if (rc == MYLITE_OK && session->is_current) {
            rc = copy_information_schema_processlist_info(database, context, &info);
        }
        if (rc == MYLITE_OK) {
            values[performance_schema_processlist_info_column] = info;
            rc = append_information_schema_row(database, rows, values);
        }
        free(info);
    }

    free(sessions);
    return rc;
}

static int append_performance_schema_threads_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
) {
    enum {
        performance_schema_threads_column_count = 24,
        performance_schema_threads_processlist_info_column = 10,
    };
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_threads_column_count) {
        set_runtime_error(database, "invalid performance_schema threads columns");
        return MYLITE_ERROR;
    }

    rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to collect performance_schema.threads sessions");
        return rc;
    }

    for (size_t session_index = 0U; rc == MYLITE_OK && session_index < session_count;
         ++session_index) {
        const struct mylite_processlist_session_snapshot *session = &sessions[session_index];
        char id_text[integer_text_capacity];
        char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
        char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
        char *info = NULL;
        const char *values[performance_schema_threads_column_count] = {
            id_text,
            "thread/sql/one_connection",
            "FOREGROUND",
            id_text,
            user,
            host,
            session->has_selected_schema ? session->selected_schema : NULL,
            session->is_current ? "Query" : "Sleep",
            "0",
            session->is_current ? "executing" : NULL,
            NULL,
            NULL,
            NULL,
            "YES",
            "YES",
            NULL,
            NULL,
            NULL,
            session->is_current ? "PRIMARY" : NULL,
            "0",
            "0",
            "0",
            "0",
            "NO",
        };
        int written = snprintf(id_text, sizeof(id_text), "%" PRIu64, session->connection_id);

        if (written < 0 || (size_t)written >= sizeof(id_text)) {
            set_runtime_error(database, "failed to format performance_schema.threads id");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = format_processlist_user_host(
                database,
                session->client_user_identity,
                user,
                sizeof(user),
                host,
                sizeof(host)
            );
        }
        if (rc == MYLITE_OK && session->is_current) {
            rc = copy_information_schema_processlist_info(database, context, &info);
        }
        if (rc == MYLITE_OK) {
            values[performance_schema_threads_processlist_info_column] = info;
            rc = append_information_schema_row(database, rows, values);
        }
        free(info);
    }

    free(sessions);
    return rc;
}

static int append_performance_schema_connection_attribute_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum {
        performance_schema_connection_attribute_column_count = 4,
        performance_schema_connection_attribute_count = 3,
    };

    static const char *const attribute_names[performance_schema_connection_attribute_count] = {
        "_client_name",
        "_client_version",
        "program_name",
    };
    const char *const attribute_values[performance_schema_connection_attribute_count] = {
        "mylite",
        mylite_version(),
        "mylite",
    };
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_connection_attribute_column_count) {
        set_runtime_error(database, "invalid performance_schema connection attribute columns");
        return MYLITE_ERROR;
    }

    rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to collect performance_schema connection attributes");
        return rc;
    }

    for (size_t session_index = 0U; rc == MYLITE_OK && session_index < session_count;
         ++session_index) {
        const struct mylite_processlist_session_snapshot *session = &sessions[session_index];
        char id_text[integer_text_capacity];
        int written = snprintf(id_text, sizeof(id_text), "%" PRIu64, session->connection_id);

        if (written < 0 || (size_t)written >= sizeof(id_text)) {
            set_runtime_error(database, "failed to format performance_schema connection id");
            rc = MYLITE_ERROR;
        }

        for (size_t attribute_index = 0U;
             rc == MYLITE_OK && attribute_index < performance_schema_connection_attribute_count;
             ++attribute_index) {
            char ordinal_text[integer_text_capacity];
            const char *const values[performance_schema_connection_attribute_column_count] = {
                id_text,
                attribute_names[attribute_index],
                attribute_values[attribute_index],
                ordinal_text,
            };
            written = snprintf(ordinal_text, sizeof(ordinal_text), "%zu", attribute_index);
            if (written < 0 || (size_t)written >= sizeof(ordinal_text)) {
                set_runtime_error(
                    database,
                    "failed to format performance_schema attribute ordinal"
                );
                rc = MYLITE_ERROR;
            }
            if (rc == MYLITE_OK) {
                rc = append_information_schema_row(database, rows, values);
            }
        }
    }

    free(sessions);
    return rc;
}

static int append_performance_schema_connection_count_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *const *values,
    size_t expected_column_count
) {
    if (rows->definition->column_count != expected_column_count) {
        set_runtime_error(database, "invalid performance_schema connection count columns");
        return MYLITE_ERROR;
    }
    return append_information_schema_row(database, rows, values);
}

static int format_performance_schema_session_count(
    struct mylite_db *database,
    char *count_text,
    size_t count_text_size
) {
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = MYLITE_OK;
    int written = 0;

    if (count_text == NULL || count_text_size == 0U) {
        return MYLITE_MISUSE;
    }

    rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to collect performance_schema connection sessions");
        return rc;
    }

    written = snprintf(count_text, count_text_size, "%zu", session_count);
    free(sessions);
    if (written < 0 || (size_t)written >= count_text_size) {
        set_runtime_error(database, "failed to format performance_schema connection count");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_performance_schema_variable_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool global_scope
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != 2U) {
        set_runtime_error(database, "invalid performance_schema variable columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_system_variable_descriptor_count();
         ++index) {
        const struct mylite_execution_system_variable_descriptor *descriptor =
            mylite_execution_system_variable_descriptor_at(index);
        char integer_buffer[integer_text_capacity];
        const char *values[2] = {NULL, NULL};

        if (!show_variable_descriptor_visible(descriptor, global_scope)) {
            continue;
        }

        values[0] = descriptor->name;
        rc = show_system_variable_value(
            database,
            descriptor->kind,
            global_scope,
            integer_buffer,
            sizeof(integer_buffer),
            &values[1]
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_performance_schema_status_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool global_scope
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != 2U) {
        set_runtime_error(database, "invalid performance_schema status columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        const struct mylite_execution_show_status_descriptor *descriptor =
            mylite_execution_show_status_descriptor_at(index);
        const char *values[2] = {NULL, NULL};

        if (!performance_schema_show_status_descriptor_visible(descriptor, global_scope)) {
            continue;
        }

        values[0] = descriptor->name;
        values[1] = descriptor->value;
        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_performance_schema_variable_by_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_thread_variable_status_column_count = 3 };

    char thread_id_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_thread_variable_status_column_count) {
        set_runtime_error(database, "invalid performance_schema variables_by_thread columns");
        return MYLITE_ERROR;
    }
    rc = format_performance_schema_current_thread_id(
        database,
        thread_id_text,
        sizeof(thread_id_text)
    );
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_system_variable_descriptor_count();
         ++index) {
        const struct mylite_execution_system_variable_descriptor *descriptor =
            mylite_execution_system_variable_descriptor_at(index);
        char integer_buffer[integer_text_capacity];
        const char *values[performance_schema_thread_variable_status_column_count] = {
            thread_id_text,
            NULL,
            NULL,
        };

        if (!show_variable_descriptor_visible(descriptor, false)) {
            continue;
        }

        values[1] = descriptor->name;
        rc = show_system_variable_value(
            database,
            descriptor->kind,
            false,
            integer_buffer,
            sizeof(integer_buffer),
            &values[2]
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_performance_schema_variables_info_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_variables_info_column_count = 8 };

    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_variables_info_column_count) {
        set_runtime_error(database, "invalid performance_schema variables_info columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_system_variable_descriptor_count();
         ++index) {
        const struct mylite_execution_system_variable_descriptor *descriptor =
            mylite_execution_system_variable_descriptor_at(index);
        const char *const values[performance_schema_variables_info_column_count] = {
            descriptor == NULL ? NULL : descriptor->name,
            "COMPILED",
            "",
            "0",
            "0",
            NULL,
            "",
            "",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_performance_schema_status_by_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_thread_variable_status_column_count = 3 };

    char thread_id_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_thread_variable_status_column_count) {
        set_runtime_error(database, "invalid performance_schema status_by_thread columns");
        return MYLITE_ERROR;
    }
    rc = format_performance_schema_current_thread_id(
        database,
        thread_id_text,
        sizeof(thread_id_text)
    );
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        const struct mylite_execution_show_status_descriptor *descriptor =
            mylite_execution_show_status_descriptor_at(index);
        const char *const values[performance_schema_thread_variable_status_column_count] = {
            thread_id_text,
            descriptor == NULL ? NULL : descriptor->name,
            descriptor == NULL ? NULL : descriptor->value,
        };

        if (!performance_schema_show_status_descriptor_visible(descriptor, false)) {
            continue;
        }

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_performance_schema_status_by_account_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_status_by_account_column_count = 4 };

    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_status_by_account_column_count) {
        set_runtime_error(database, "invalid performance_schema status_by_account columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        const struct mylite_execution_show_status_descriptor *descriptor =
            mylite_execution_show_status_descriptor_at(index);
        const char *const values[performance_schema_status_by_account_column_count] = {
            "root",
            "%",
            descriptor == NULL ? NULL : descriptor->name,
            descriptor == NULL ? NULL : descriptor->value,
        };

        if (!performance_schema_show_status_descriptor_visible(descriptor, false)) {
            continue;
        }

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_performance_schema_status_by_host_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_status_by_host_column_count = 3 };

    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_status_by_host_column_count) {
        set_runtime_error(database, "invalid performance_schema status_by_host columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        const struct mylite_execution_show_status_descriptor *descriptor =
            mylite_execution_show_status_descriptor_at(index);
        const char *const values[performance_schema_status_by_host_column_count] = {
            "%",
            descriptor == NULL ? NULL : descriptor->name,
            descriptor == NULL ? NULL : descriptor->value,
        };

        if (!performance_schema_show_status_descriptor_visible(descriptor, false)) {
            continue;
        }

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_performance_schema_status_by_user_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_status_by_user_column_count = 3 };

    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_status_by_user_column_count) {
        set_runtime_error(database, "invalid performance_schema status_by_user columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        const struct mylite_execution_show_status_descriptor *descriptor =
            mylite_execution_show_status_descriptor_at(index);
        const char *const values[performance_schema_status_by_user_column_count] = {
            "root",
            descriptor == NULL ? NULL : descriptor->name,
            descriptor == NULL ? NULL : descriptor->value,
        };

        if (!performance_schema_show_status_descriptor_visible(descriptor, false)) {
            continue;
        }

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_performance_schema_performance_timer_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_performance_timer_column_count = 4 };

    static const char *const timer_rows[][performance_schema_performance_timer_column_count] = {
        {"CYCLE", "1000000000", "1", "1"},
        {"NANOSECOND", "1000000000", "1", "1"},
        {"MICROSECOND", "1000000", "1", "1"},
        {"MILLISECOND", "1000", "1", "1"},
        {"THREAD_CPU", "1000000000", "1", "1"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_performance_timer_column_count) {
        set_runtime_error(database, "invalid performance_schema performance_timers columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < sizeof(timer_rows) / sizeof(timer_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, timer_rows[index]);
    }
    return rc;
}

static int append_performance_schema_setup_actor_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_setup_actor_column_count = 5 };

    static const char *const default_actor_row[performance_schema_setup_actor_column_count] = {
        "%",
        "%",
        "%",
        "YES",
        "YES",
    };

    if (rows->definition->column_count != performance_schema_setup_actor_column_count) {
        set_runtime_error(database, "invalid performance_schema setup_actors columns");
        return MYLITE_ERROR;
    }
    return append_information_schema_row(database, rows, default_actor_row);
}

static int append_performance_schema_setup_consumer_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_setup_consumer_column_count = 2 };

    static const char *const consumer_rows[][performance_schema_setup_consumer_column_count] = {
        {"events_stages_current", "NO"},
        {"events_stages_history", "NO"},
        {"events_stages_history_long", "NO"},
        {"events_statements_cpu", "NO"},
        {"events_statements_current", "YES"},
        {"events_statements_history", "YES"},
        {"events_statements_history_long", "NO"},
        {"events_transactions_current", "YES"},
        {"events_transactions_history", "YES"},
        {"events_transactions_history_long", "NO"},
        {"events_waits_current", "NO"},
        {"events_waits_history", "NO"},
        {"events_waits_history_long", "NO"},
        {"global_instrumentation", "YES"},
        {"statements_digest", "YES"},
        {"thread_instrumentation", "YES"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_setup_consumer_column_count) {
        set_runtime_error(database, "invalid performance_schema setup_consumers columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < sizeof(consumer_rows) / sizeof(consumer_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, consumer_rows[index]);
    }
    return rc;
}

static int append_performance_schema_log_status_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_log_status_column_count = 4 };

    static const char *const log_status_row[performance_schema_log_status_column_count] = {
        server_uuid_system_variable_value,
        "{}",
        "{}",
        "{}",
    };

    if (rows->definition->column_count != performance_schema_log_status_column_count) {
        set_runtime_error(database, "invalid performance_schema log_status columns");
        return MYLITE_ERROR;
    }
    return append_information_schema_row(database, rows, log_status_row);
}

static int append_performance_schema_setup_instrument_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_setup_instrument_column_count = 7 };

    static const char setup_instrument_lock_doc[] =
        "Components can provide their own performance_schema tables. This lock protects the "
        "list of such tables definitions.";
    static const char *const instrument_rows[][performance_schema_setup_instrument_column_count] = {
        {"idle", "YES", "YES", "user", NULL, "0", NULL},
        {"memory/sql/TABLE",
         "YES",
         NULL,
         "global_statistics",
         "",
         "0",
         "Memory used by TABLE objects and their mem root."},
        {"stage/sql/starting", "NO", "NO", "", NULL, "0", NULL},
        {"statement/sql/select", "YES", "YES", "", NULL, "0", NULL},
        {"wait/io/socket/sql/client_connection", "NO", "NO", "user", NULL, "0", NULL},
        {"wait/synch/mutex/pfs/LOCK_pfs_share_list",
         "NO",
         "NO",
         "singleton",
         NULL,
         "1",
         setup_instrument_lock_doc},
        {"wait/synch/mutex/sql/MYSQL_BIN_LOG::LOCK_commit", "NO", "NO", "", NULL, "0", NULL},
        {"wait/synch/mutex/sql/TC_LOG_MMAP::LOCK_tc", "NO", "NO", "", NULL, "0", NULL},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_setup_instrument_column_count) {
        set_runtime_error(database, "invalid performance_schema setup_instruments columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < sizeof(instrument_rows) / sizeof(instrument_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, instrument_rows[index]);
    }
    return rc;
}

static int append_performance_schema_setup_logger_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_setup_logger_column_count = 3 };

    static const char *const logger_rows[][performance_schema_setup_logger_column_count] = {
        {"logger/error/error_log", "info", "MySQL error logger"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_setup_logger_column_count) {
        set_runtime_error(database, "invalid performance_schema setup_loggers columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < sizeof(logger_rows) / sizeof(logger_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, logger_rows[index]);
    }
    return rc;
}

static int append_performance_schema_setup_meter_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_setup_meter_column_count = 4 };

    static const char *const meter_rows[][performance_schema_setup_meter_column_count] = {
        {"mysql.inno", "10", "YES", "MySql InnoDB metrics"},
        {"mysql.inno.buffer_pool", "10", "YES", "MySql InnoDB buffer pool metrics"},
        {"mysql.inno.data", "10", "YES", "MySql InnoDB data metrics"},
        {"mysql.myisam", "10", "YES", "MySql MyISAM storage engine stats"},
        {"mysql.perf_schema", "10", "YES", "MySql performance_schema lost instruments"},
        {"mysql.stats", "10", "YES", "MySql core metrics"},
        {"mysql.stats.com", "10", "YES", "MySql command stats"},
        {"mysql.stats.connection", "10", "YES", "MySql connection stats"},
        {"mysql.stats.handler", "10", "YES", "MySql handler stats"},
        {"mysql.stats.ssl", "10", "YES", "MySql TLS related stats"},
        {"mysql.x", "10", "YES", "MySql X plugin metrics"},
        {"mysql.x.stmt", "10", "YES", "MySql X plugin statement statistics"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_setup_meter_column_count) {
        set_runtime_error(database, "invalid performance_schema setup_meters columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < sizeof(meter_rows) / sizeof(meter_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, meter_rows[index]);
    }
    return rc;
}

static int append_performance_schema_setup_object_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_setup_object_column_count = 5 };

    static const char *const object_rows[][performance_schema_setup_object_column_count] = {
        {"EVENT", "%", "%", "YES", "YES"},
        {"EVENT", "information_schema", "%", "NO", "NO"},
        {"EVENT", "mysql", "%", "NO", "NO"},
        {"EVENT", "performance_schema", "%", "NO", "NO"},
        {"FUNCTION", "%", "%", "YES", "YES"},
        {"FUNCTION", "information_schema", "%", "NO", "NO"},
        {"FUNCTION", "mysql", "%", "NO", "NO"},
        {"FUNCTION", "performance_schema", "%", "NO", "NO"},
        {"PROCEDURE", "%", "%", "YES", "YES"},
        {"PROCEDURE", "information_schema", "%", "NO", "NO"},
        {"PROCEDURE", "mysql", "%", "NO", "NO"},
        {"PROCEDURE", "performance_schema", "%", "NO", "NO"},
        {"TABLE", "%", "%", "YES", "YES"},
        {"TABLE", "information_schema", "%", "NO", "NO"},
        {"TABLE", "mysql", "%", "NO", "NO"},
        {"TABLE", "performance_schema", "%", "NO", "NO"},
        {"TRIGGER", "%", "%", "YES", "YES"},
        {"TRIGGER", "information_schema", "%", "NO", "NO"},
        {"TRIGGER", "mysql", "%", "NO", "NO"},
        {"TRIGGER", "performance_schema", "%", "NO", "NO"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_setup_object_column_count) {
        set_runtime_error(database, "invalid performance_schema setup_objects columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < sizeof(object_rows) / sizeof(object_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, object_rows[index]);
    }
    return rc;
}

static int append_performance_schema_setup_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_setup_thread_column_count = 6 };

    static const char *const thread_rows[][performance_schema_setup_thread_column_count] = {
        {"thread/innodb/buf_dump_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/buf_pool_create_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/buf_resize_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/bulk_alloc_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/bulk_flusher_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/clone_ddl_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/clone_gtid_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/ddl_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/dict_stats_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/fts_optimize_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/fts_parallel_merge_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/fts_parallel_tokenization_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/io_ibuf_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/io_read_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/io_write_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/log_archiver_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/log_checkpointer_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/log_files_governor_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/log_flush_notifier_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/log_flusher_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/log_write_notifier_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/log_writer_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/meb::redo_log_archive_consumer_thread", "YES", "YES", "singleton", "0", NULL
        },
        {"thread/innodb/page_archiver_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/page_flush_coordinator_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/page_flush_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/parallel_read_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/parallel_rseg_init_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/recv_writer_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/srv_error_monitor_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/srv_lock_timeout_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/srv_master_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/srv_monitor_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/srv_purge_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/srv_ts_alter_encrypt_thread", "YES", "YES", "singleton", "0", NULL},
        {"thread/innodb/srv_worker_thread", "YES", "YES", "", "0", NULL},
        {"thread/innodb/trx_recovery_rollback_thread", "YES", "YES", "", "0", NULL},
        {"thread/myisam/find_all_keys", "YES", "YES", "", "0", NULL},
        {"thread/mysqlx/acceptor_network", "YES", "YES", "", "0", NULL},
        {"thread/mysqlx/worker", "YES", "YES", "user", "0", NULL},
        {"thread/mysys/thread_timer_notifier", "YES", "YES", "singleton", "0", NULL},
        {"thread/performance_schema/setup", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/admin_interface", "YES", "YES", "user", "0", NULL},
        {"thread/sql/bootstrap", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/compress_gtid_table", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/event_scheduler", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/event_worker", "YES", "YES", "", "0", NULL},
        {"thread/sql/main", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/manager", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/one_connection", "YES", "YES", "user", "0", NULL},
        {"thread/sql/parser_service", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/replica_io", "YES", "YES", "", "0", NULL},
        {"thread/sql/replica_monitor", "YES", "YES", "singleton", "0", NULL},
        {"thread/sql/replica_sql", "YES", "YES", "", "0", NULL},
        {"thread/sql/replica_worker", "YES", "YES", "", "0", NULL},
        {"thread/sql/signal_handler", "YES", "YES", "singleton", "0", NULL},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_setup_thread_column_count) {
        set_runtime_error(database, "invalid performance_schema setup_threads columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < sizeof(thread_rows) / sizeof(thread_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, thread_rows[index]);
    }
    return rc;
}

static int append_performance_schema_user_defined_function_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_user_defined_function_column_count = 5 };

    static const char
        *const function_rows[][performance_schema_user_defined_function_column_count] = {
            {"asynchronous_connection_failover_add_managed", "char", "function", NULL, "1"},
            {"asynchronous_connection_failover_add_source", "char", "function", NULL, "1"},
            {"asynchronous_connection_failover_delete_managed", "char", "function", NULL, "1"},
            {"asynchronous_connection_failover_delete_source", "char", "function", NULL, "1"},
            {"asynchronous_connection_failover_reset", "char", "function", NULL, "1"},
            {"innodb_redo_log_archive_flush", "integer", "function", NULL, "1"},
            {"innodb_redo_log_archive_start", "integer", "function", NULL, "1"},
            {"innodb_redo_log_archive_stop", "integer", "function", NULL, "1"},
            {"innodb_redo_log_consumer_advance", "integer", "function", NULL, "1"},
            {"innodb_redo_log_consumer_register", "integer", "function", NULL, "1"},
            {"innodb_redo_log_consumer_unregister", "integer", "function", NULL, "1"},
            {"innodb_redo_log_sharp_checkpoint", "integer", "function", NULL, "1"},
            {"innodb_set_open_files_limit", "integer", "function", NULL, "1"},
            {"mysqlx_error", "char", "function", NULL, "1"},
            {"mysqlx_generate_document_id", "char", "function", NULL, "1"},
            {"mysqlx_get_prepared_statement_id", "integer", "function", NULL, "1"},
        };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_user_defined_function_column_count) {
        set_runtime_error(database, "invalid performance_schema user_defined_functions columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < sizeof(function_rows) / sizeof(function_rows[0]);
         ++index) {
        rc = append_information_schema_row(database, rows, function_rows[index]);
    }
    return rc;
}

static int append_performance_schema_user_variable_by_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    enum { performance_schema_user_variable_by_thread_column_count = 3 };

    char thread_id_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != performance_schema_user_variable_by_thread_column_count) {
        set_runtime_error(database, "invalid performance_schema user_variables_by_thread columns");
        return MYLITE_ERROR;
    }

    rc = format_performance_schema_current_thread_id(
        database,
        thread_id_text,
        sizeof(thread_id_text)
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < database->session.user_variable_count;
         ++index) {
        const struct mylite_session_user_variable *variable =
            &database->session.user_variables[index];
        const char *display_name =
            variable->display_name[0] == '\0' ? variable->name : variable->display_name;
        const char *const values[performance_schema_user_variable_by_thread_column_count] = {
            thread_id_text,
            display_name,
            variable->is_null ? NULL : variable->value,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int format_performance_schema_current_thread_id(
    struct mylite_db *database,
    char *thread_id_text,
    size_t thread_id_text_size
) {
    int written = 0;

    if (thread_id_text == NULL || thread_id_text_size == 0U) {
        return MYLITE_MISUSE;
    }

    written =
        snprintf(thread_id_text, thread_id_text_size, "%" PRIu64, database->session.connection_id);
    if (written < 0 || (size_t)written >= thread_id_text_size) {
        set_runtime_error(database, "failed to format performance_schema thread id");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool performance_schema_show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL ||
        (performance_schema_status_name_is_command_counter(descriptor->name) &&
         strcmp(descriptor->name, "Com_stmt_reprepare") != 0)) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global &&
               !performance_schema_status_name_is_session_only(descriptor->name);
    }
    return descriptor->show_session;
}

static bool performance_schema_status_name_is_command_counter(const char *name) {
    return name != NULL && strncmp(name, "Com_", 4U) == 0;
}

static bool performance_schema_status_name_is_session_only(const char *name) {
    static const char *const session_only_names[] = {
        "Compression",
        "Compression_algorithm",
        "Compression_level",
        "Last_query_cost",
        "Last_query_partial_plans",
        "Tls_sni_server_name",
    };

    if (name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(session_only_names) / sizeof(session_only_names[0]);
         ++index) {
        if (strcmp(name, session_only_names[index]) == 0) {
            return true;
        }
    }
    return false;
}

static int append_sys_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    int status = MYLITE_OK;

    if (strcmp(definition->query_definition.name, "sys_config") == 0) {
        return append_sys_sys_config_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "version") == 0) {
        return append_sys_version_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "metrics") == 0) {
        return append_sys_metrics_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "processlist") == 0) {
        return append_sys_processlist_system_rows(database, context, rows);
    }
    if (strcmp(definition->query_definition.name, "x$processlist") == 0) {
        return append_sys_processlist_system_rows(database, context, rows);
    }
    if (strcmp(definition->query_definition.name, "session") == 0) {
        return append_sys_processlist_system_rows(database, context, rows);
    }
    if (strcmp(definition->query_definition.name, "x$session") == 0) {
        return append_sys_processlist_system_rows(database, context, rows);
    }
    if (strcmp(definition->query_definition.name, "session_ssl_status") == 0) {
        return append_sys_session_ssl_status_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "statement_analysis") == 0 ||
        strcmp(definition->query_definition.name, "statements_with_errors_or_warnings") == 0 ||
        strcmp(definition->query_definition.name, "statements_with_full_table_scans") == 0 ||
        strcmp(definition->query_definition.name, "statements_with_runtimes_in_95th_percentile") ==
            0 ||
        strcmp(definition->query_definition.name, "statements_with_sorting") == 0 ||
        strcmp(definition->query_definition.name, "statements_with_temp_tables") == 0) {
        return append_sys_statement_digest_system_rows(
            database,
            rows,
            definition->query_definition.name
        );
    }
    if (strcmp(definition->query_definition.name, "innodb_lock_waits") == 0) {
        return append_sys_innodb_lock_waits_system_rows(database, rows);
    }
    if (append_sys_schema_summary_system_table_rows(database, definition, rows, &status)) {
        return status;
    }
    if (strcmp(definition->query_definition.name, "io_by_thread_by_latency") == 0) {
        return append_sys_io_by_thread_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_file_by_bytes") == 0) {
        return append_sys_io_global_by_file_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_file_by_latency") == 0) {
        return append_sys_io_global_by_file_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_wait_by_bytes") == 0) {
        return append_sys_io_global_by_wait_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_wait_by_latency") == 0) {
        return append_sys_io_global_by_wait_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "latest_file_io") == 0) {
        return append_sys_latest_file_io_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "ps_check_lost_instrumentation") == 0) {
        return append_sys_ps_check_lost_instrumentation_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_auto_increment_columns") == 0) {
        return append_sys_schema_auto_increment_columns_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_index_statistics") == 0) {
        return append_sys_schema_index_statistics_system_rows(database, rows, true);
    }
    if (strcmp(definition->query_definition.name, "schema_object_overview") == 0) {
        return append_sys_schema_object_overview_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_redundant_indexes") == 0) {
        return append_sys_schema_redundant_indexes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_table_lock_waits") == 0) {
        return append_sys_schema_table_lock_waits_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_table_statistics") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, true, false);
    }
    if (strcmp(definition->query_definition.name, "schema_table_statistics_with_buffer") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, true, true);
    }
    if (strcmp(definition->query_definition.name, "schema_tables_with_full_table_scans") == 0) {
        return append_sys_schema_tables_with_full_table_scans_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_unused_indexes") == 0) {
        return append_sys_schema_unused_indexes_system_rows(database, rows);
    }

    return append_sys_schema_x_system_table_rows(database, definition, rows);
}

static bool append_sys_schema_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
) {
    if (strcmp(definition->query_definition.name, "host_summary") == 0) {
        *out_status = append_sys_host_summary_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_file_io") == 0) {
        *out_status = append_sys_host_summary_by_file_io_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_file_io_type") == 0) {
        *out_status = append_sys_host_summary_by_file_io_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_stages") == 0) {
        *out_status = append_sys_host_summary_by_stages_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_statement_latency") == 0) {
        *out_status = append_sys_host_summary_by_statement_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_statement_type") == 0) {
        *out_status = append_sys_host_summary_by_statement_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "user_summary") == 0) {
        *out_status = append_sys_user_summary_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "user_summary_by_file_io") == 0) {
        *out_status = append_sys_user_summary_by_file_io_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "user_summary_by_file_io_type") == 0) {
        *out_status = append_sys_user_summary_by_file_io_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "user_summary_by_stages") == 0) {
        *out_status = append_sys_user_summary_by_stages_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "user_summary_by_statement_latency") == 0) {
        *out_status = append_sys_user_summary_by_statement_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "user_summary_by_statement_type") == 0) {
        *out_status = append_sys_user_summary_by_statement_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "wait_classes_global_by_avg_latency") == 0) {
        *out_status = append_sys_wait_classes_global_by_avg_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "wait_classes_global_by_latency") == 0) {
        *out_status = append_sys_wait_classes_global_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "waits_by_host_by_latency") == 0) {
        *out_status = append_sys_waits_by_host_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "waits_by_user_by_latency") == 0) {
        *out_status = append_sys_waits_by_user_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "waits_global_by_latency") == 0) {
        *out_status = append_sys_waits_global_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_by_host_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_host_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_by_thread_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_thread_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_by_user_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_user_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_global_by_current_bytes") == 0) {
        *out_status = append_sys_memory_global_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_global_total") == 0) {
        *out_status = append_sys_memory_global_total_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "innodb_buffer_stats_by_schema") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_schema_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "innodb_buffer_stats_by_table") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_table_system_rows(database, rows);
        return true;
    }

    return false;
}

static int append_sys_schema_x_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    int status = MYLITE_OK;

    if (strcmp(definition->query_definition.name, "x$schema_flattened_keys") == 0) {
        return append_sys_x_schema_flattened_keys_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$innodb_lock_waits") == 0) {
        return append_sys_innodb_lock_waits_system_rows(database, rows);
    }
    if (append_sys_schema_x_summary_system_table_rows(database, definition, rows, &status)) {
        return status;
    }
    if (strcmp(definition->query_definition.name, "x$io_by_thread_by_latency") == 0) {
        return append_sys_io_by_thread_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_file_by_bytes") == 0) {
        return append_sys_io_global_by_file_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_file_by_latency") == 0) {
        return append_sys_io_global_by_file_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_wait_by_bytes") == 0) {
        return append_sys_io_global_by_wait_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_wait_by_latency") == 0) {
        return append_sys_io_global_by_wait_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$latest_file_io") == 0) {
        return append_sys_latest_file_io_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$ps_digest_avg_latency_distribution") == 0) {
        return append_sys_x_ps_digest_avg_latency_distribution_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$ps_digest_95th_percentile_by_avg_us") == 0) {
        return append_sys_x_ps_digest_95th_percentile_by_avg_us_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$statement_analysis") == 0 ||
        strcmp(definition->query_definition.name, "x$statements_with_errors_or_warnings") == 0 ||
        strcmp(definition->query_definition.name, "x$statements_with_full_table_scans") == 0 ||
        strcmp(
            definition->query_definition.name,
            "x$statements_with_runtimes_in_95th_percentile"
        ) == 0 ||
        strcmp(definition->query_definition.name, "x$statements_with_sorting") == 0 ||
        strcmp(definition->query_definition.name, "x$statements_with_temp_tables") == 0) {
        return append_sys_statement_digest_system_rows(
            database,
            rows,
            definition->query_definition.name
        );
    }
    if (strcmp(definition->query_definition.name, "x$ps_schema_table_statistics_io") == 0) {
        return append_sys_x_ps_schema_table_statistics_io_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$schema_index_statistics") == 0) {
        return append_sys_schema_index_statistics_system_rows(database, rows, false);
    }
    if (strcmp(definition->query_definition.name, "x$schema_table_lock_waits") == 0) {
        return append_sys_schema_table_lock_waits_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$schema_table_statistics") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, false, false);
    }
    if (strcmp(definition->query_definition.name, "x$schema_table_statistics_with_buffer") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, false, true);
    }
    if (strcmp(definition->query_definition.name, "x$schema_tables_with_full_table_scans") == 0) {
        return append_sys_schema_tables_with_full_table_scans_system_rows(database, rows);
    }

    set_runtime_error(database, "invalid mysql system table");
    return MYLITE_ERROR;
}

static bool append_sys_schema_x_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
) {
    if (strcmp(definition->query_definition.name, "x$host_summary") == 0) {
        *out_status = append_sys_host_summary_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_file_io") == 0) {
        *out_status = append_sys_host_summary_by_file_io_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_file_io_type") == 0) {
        *out_status = append_sys_host_summary_by_file_io_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_stages") == 0) {
        *out_status = append_sys_host_summary_by_stages_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_statement_latency") == 0) {
        *out_status = append_sys_host_summary_by_statement_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_statement_type") == 0) {
        *out_status = append_sys_host_summary_by_statement_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$user_summary") == 0) {
        *out_status = append_sys_user_summary_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$user_summary_by_file_io") == 0) {
        *out_status = append_sys_user_summary_by_file_io_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$user_summary_by_file_io_type") == 0) {
        *out_status = append_sys_user_summary_by_file_io_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$user_summary_by_stages") == 0) {
        *out_status = append_sys_user_summary_by_stages_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$user_summary_by_statement_latency") == 0) {
        *out_status = append_sys_user_summary_by_statement_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$user_summary_by_statement_type") == 0) {
        *out_status = append_sys_user_summary_by_statement_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$wait_classes_global_by_avg_latency") == 0) {
        *out_status = append_sys_wait_classes_global_by_avg_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$wait_classes_global_by_latency") == 0) {
        *out_status = append_sys_wait_classes_global_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$waits_by_host_by_latency") == 0) {
        *out_status = append_sys_waits_by_host_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$waits_by_user_by_latency") == 0) {
        *out_status = append_sys_waits_by_user_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$waits_global_by_latency") == 0) {
        *out_status = append_sys_waits_global_by_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_by_host_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_host_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_by_thread_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_thread_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_by_user_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_user_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_global_by_current_bytes") == 0) {
        *out_status = append_sys_memory_global_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_global_total") == 0) {
        *out_status = append_sys_memory_global_total_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$innodb_buffer_stats_by_schema") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_schema_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$innodb_buffer_stats_by_table") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_table_system_rows(database, rows);
        return true;
    }

    return false;
}

static bool mysql_system_table_definition_has_no_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    return definition != NULL && strcmp(definition->schema_name, "mysql") == 0 &&
           (strcmp(definition->query_definition.name, "component") == 0 ||
            strcmp(definition->query_definition.name, "columns_priv") == 0 ||
            strcmp(definition->query_definition.name, "db") == 0 ||
            strcmp(definition->query_definition.name, "default_roles") == 0 ||
            strcmp(definition->query_definition.name, "func") == 0 ||
            strcmp(definition->query_definition.name, "general_log") == 0 ||
            strcmp(definition->query_definition.name, "global_grants") == 0 ||
            strcmp(definition->query_definition.name, "gtid_executed") == 0 ||
            strcmp(definition->query_definition.name, "help_category") == 0 ||
            strcmp(definition->query_definition.name, "help_keyword") == 0 ||
            strcmp(definition->query_definition.name, "help_relation") == 0 ||
            strcmp(definition->query_definition.name, "help_topic") == 0 ||
            strcmp(definition->query_definition.name, "ndb_binlog_index") == 0 ||
            strcmp(definition->query_definition.name, "password_history") == 0 ||
            strcmp(definition->query_definition.name, "procs_priv") == 0 ||
            strcmp(definition->query_definition.name, "proxies_priv") == 0 ||
            strcmp(definition->query_definition.name, "role_edges") == 0 ||
            strcmp(definition->query_definition.name, "servers") == 0 ||
            strcmp(definition->query_definition.name, "slave_master_info") == 0 ||
            strcmp(definition->query_definition.name, "slave_relay_log_info") == 0 ||
            strcmp(definition->query_definition.name, "slave_worker_info") == 0 ||
            strcmp(definition->query_definition.name, "slow_log") == 0 ||
            strcmp(definition->query_definition.name, "tables_priv") == 0 ||
            strcmp(definition->query_definition.name, "time_zone") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_leap_second") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_name") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_transition") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_transition_type") == 0 ||
            strcmp(definition->query_definition.name, "user") == 0);
}

static bool mysql_system_table_definition_has_catalog_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    return definition != NULL && strcmp(definition->schema_name, "mysql") == 0 &&
           (strcmp(definition->query_definition.name, "innodb_index_stats") == 0 ||
            strcmp(definition->query_definition.name, "innodb_table_stats") == 0);
}

static int append_mysql_engine_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct mysql_engine_cost_row {
        const char *engine_name;
        const char *device_type;
        const char *cost_name;
        const char *cost_value;
        const char *comment;
        const char *default_value;
    };
    static const struct mysql_engine_cost_row cost_rows[] = {
        {"default", "0", "io_block_read_cost", NULL, NULL, "1"},
        {"default", "0", "memory_block_read_cost", NULL, NULL, "0.25"},
    };
    char last_update[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_engine_cost_column_count) {
        set_runtime_error(database, "invalid mysql.engine_cost columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(cost_rows) / sizeof(cost_rows[0]);
         ++row_index) {
        const struct mysql_engine_cost_row *cost_row = &cost_rows[row_index];
        const char *values[mysql_engine_cost_column_count] = {
            cost_row->engine_name,
            cost_row->device_type,
            cost_row->cost_name,
            cost_row->cost_value,
            last_update,
            cost_row->comment,
            cost_row->default_value,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_mysql_plugin_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    static const char *const plugin_rows[][mysql_plugin_column_count] = {
        {"CONNECTION_CONTROL", "connection_control.so"},
        {"CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS", "connection_control.so"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_plugin_column_count) {
        set_runtime_error(database, "invalid mysql.plugin columns");
        return MYLITE_ERROR;
    }

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(plugin_rows) / sizeof(plugin_rows[0]);
         ++row_index) {
        rc = append_information_schema_row(database, rows, plugin_rows[row_index]);
    }
    return rc;
}

static int append_mysql_server_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct mysql_server_cost_row {
        const char *cost_name;
        const char *cost_value;
        const char *comment;
        const char *default_value;
    };
    static const struct mysql_server_cost_row cost_rows[] = {
        {"disk_temptable_create_cost", NULL, NULL, "20"},
        {"disk_temptable_row_cost", NULL, NULL, "0.5"},
        {"key_compare_cost", NULL, NULL, "0.05"},
        {"memory_temptable_create_cost", NULL, NULL, "1"},
        {"memory_temptable_row_cost", NULL, NULL, "0.1"},
        {"row_evaluate_cost", NULL, NULL, "0.1"},
    };
    char last_update[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_server_cost_column_count) {
        set_runtime_error(database, "invalid mysql.server_cost columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(cost_rows) / sizeof(cost_rows[0]);
         ++row_index) {
        const struct mysql_server_cost_row *cost_row = &cost_rows[row_index];
        const char *values[mysql_server_cost_column_count] = {
            cost_row->cost_name,
            cost_row->cost_value,
            last_update,
            cost_row->comment,
            cost_row->default_value,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_sys_sys_config_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_config_row {
        const char *variable;
        const char *value;
    };
    static const struct sys_config_row config_rows[] = {
        {"diagnostics.allow_i_s_tables", "OFF"},
        {"diagnostics.include_raw", "OFF"},
        {"ps_thread_trx_info.max_length", "65535"},
        {"statement_performance_analyzer.limit", "100"},
        {"statement_performance_analyzer.view", NULL},
        {"statement_truncate_len", "64"},
    };
    char set_time[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_sys_config_column_count) {
        set_runtime_error(database, "invalid sys.sys_config columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, set_time, sizeof(set_time));
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(config_rows) / sizeof(config_rows[0]);
         ++row_index) {
        const struct sys_config_row *config_row = &config_rows[row_index];
        const char *values[sys_sys_config_column_count] = {
            config_row->variable,
            config_row->value,
            set_time,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_sys_version_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[sys_version_column_count] = {
        "2.1.3",
        MYLITE_MYSQL_SERVER_VERSION_STRING,
    };

    if (rows->definition->column_count != sys_version_column_count) {
        set_runtime_error(database, "invalid sys.version columns");
        return MYLITE_ERROR;
    }

    return append_information_schema_row(database, rows, values);
}

static int append_sys_metrics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_metrics_column_count) {
        set_runtime_error(database, "invalid sys.metrics columns");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        const struct mylite_execution_show_status_descriptor *descriptor =
            mylite_execution_show_status_descriptor_at(index);
        char variable_name[sys_metrics_variable_name_capacity];
        size_t name_length = 0U;
        const char *values[sys_metrics_column_count] = {
            variable_name,
            NULL,
            "Global Status",
            "YES",
        };

        if (descriptor == NULL || !descriptor->show_global) {
            continue;
        }

        name_length = strlen(descriptor->name);
        if (name_length >= sizeof(variable_name)) {
            set_runtime_error(database, "sys.metrics status variable name too long");
            return MYLITE_ERROR;
        }
        for (size_t offset = 0U; offset < name_length; ++offset) {
            variable_name[offset] = ascii_lower((unsigned char)descriptor->name[offset]);
        }
        variable_name[name_length] = '\0';
        values[1] = descriptor->value;

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_sys_processlist_system_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
) {
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);

    if (rows->definition->column_count != sys_processlist_column_count) {
        set_runtime_error(database, "invalid sys.processlist columns");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to collect sys.processlist sessions");
        return rc;
    }

    for (size_t session_index = 0U; rc == MYLITE_OK && session_index < session_count;
         ++session_index) {
        const struct mylite_processlist_session_snapshot *session = &sessions[session_index];
        char thd_id_text[integer_text_capacity];
        char conn_id_text[integer_text_capacity];
        char *current_statement = NULL;
        const char *selected_schema = NULL;
        const char *values[sys_processlist_column_count] = {
            thd_id_text,
            conn_id_text,
            session->client_user_identity,
            NULL,
            session->is_current ? "Query" : "Sleep",
            session->is_current ? "executing" : NULL,
            "0",
            NULL,
            session->is_current ? "PRIMARY" : NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            "0",
            "0",
            "0",
            "0",
            "0",
            "NO",
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
        };
        int thd_written =
            snprintf(thd_id_text, sizeof(thd_id_text), "%" PRIu64, session->connection_id);
        int conn_written =
            snprintf(conn_id_text, sizeof(conn_id_text), "%" PRIu64, session->connection_id);

        if (thd_written < 0 || (size_t)thd_written >= sizeof(thd_id_text) || conn_written < 0 ||
            (size_t)conn_written >= sizeof(conn_id_text)) {
            set_runtime_error(database, "failed to format sys.processlist connection id");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK && session->has_selected_schema) {
            selected_schema = session->selected_schema;
        }
        values[sys_processlist_db_column] = selected_schema;
        if (rc == MYLITE_OK && session->is_current) {
            rc = copy_information_schema_processlist_info(database, context, &current_statement);
        }
        if (rc == MYLITE_OK) {
            values[sys_processlist_current_statement_column] = current_statement;
            values[sys_processlist_trx_state_column] =
                session->is_current || session->user_transaction_active ? "ACTIVE" : NULL;
            values[sys_processlist_trx_autocommit_column] =
                session->autocommit_enabled ? "YES" : "NO";
            rc = append_information_schema_row(database, rows, values);
        }
        free(current_statement);
    }

    free(sessions);
    return rc;
}

static int append_sys_session_ssl_status_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_session_ssl_status_column_count) {
        set_runtime_error(database, "invalid sys.session_ssl_status columns");
        return MYLITE_ERROR;
    }

    rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to collect sys.session_ssl_status sessions");
        return rc;
    }

    for (size_t session_index = 0U; rc == MYLITE_OK && session_index < session_count;
         ++session_index) {
        const struct mylite_processlist_session_snapshot *session = &sessions[session_index];
        char thread_id_text[integer_text_capacity];
        const char *values[sys_session_ssl_status_column_count] = {
            thread_id_text,
            "",
            "",
            "0",
        };
        int written =
            snprintf(thread_id_text, sizeof(thread_id_text), "%" PRIu64, session->connection_id);

        if (written < 0 || (size_t)written >= sizeof(thread_id_text)) {
            set_runtime_error(database, "failed to format sys.session_ssl_status thread id");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    free(sessions);
    return rc;
}

static int append_sys_innodb_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_innodb_lock_waits_column_count) {
        set_runtime_error(database, "invalid sys.innodb_lock_waits columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_column_count) {
        set_runtime_error(database, "invalid sys.host_summary columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_file_io_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_file_io columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_file_io_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_file_io_type_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_file_io_type columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_stages_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_stages_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_stages columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_statement_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_statement_latency_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_statement_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_statement_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_statement_type_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_statement_type columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_user_summary_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_user_summary_column_count) {
        set_runtime_error(database, "invalid sys.user_summary columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_user_summary_by_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_user_summary_by_file_io_column_count) {
        set_runtime_error(database, "invalid sys.user_summary_by_file_io columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_user_summary_by_file_io_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_user_summary_by_file_io_type_column_count) {
        set_runtime_error(database, "invalid sys.user_summary_by_file_io_type columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_user_summary_by_stages_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_user_summary_by_stages_column_count) {
        set_runtime_error(database, "invalid sys.user_summary_by_stages columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_user_summary_by_statement_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_user_summary_by_statement_latency_column_count) {
        set_runtime_error(database, "invalid sys.user_summary_by_statement_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_user_summary_by_statement_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_user_summary_by_statement_type_column_count) {
        set_runtime_error(database, "invalid sys.user_summary_by_statement_type columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_wait_classes_global_by_avg_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_wait_classes_global_by_avg_latency_column_count) {
        set_runtime_error(database, "invalid sys.wait_classes_global_by_avg_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_wait_classes_global_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_wait_classes_global_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.wait_classes_global_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_waits_by_host_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_waits_by_host_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.waits_by_host_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_waits_by_user_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_waits_by_user_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.waits_by_user_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_waits_global_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_waits_global_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.waits_global_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_by_host_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_by_host_by_current_bytes_column_count) {
        set_runtime_error(database, "invalid sys.memory_by_host_by_current_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_by_thread_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_by_thread_by_current_bytes_column_count) {
        set_runtime_error(database, "invalid sys.memory_by_thread_by_current_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_by_user_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_by_user_by_current_bytes_column_count) {
        set_runtime_error(database, "invalid sys.memory_by_user_by_current_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_global_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_global_by_current_bytes_column_count) {
        set_runtime_error(database, "invalid sys.memory_global_by_current_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_global_total_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_global_total_column_count) {
        set_runtime_error(database, "invalid sys.memory_global_total columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_x_ps_digest_avg_latency_distribution_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_x_ps_digest_avg_latency_distribution_column_count) {
        set_runtime_error(database, "invalid sys.x$ps_digest_avg_latency_distribution columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_x_ps_digest_95th_percentile_by_avg_us_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_x_ps_digest_95th_percentile_by_avg_us_column_count) {
        set_runtime_error(database, "invalid sys.x$ps_digest_95th_percentile_by_avg_us columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_statement_digest_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *view_name
) {
    size_t expected_column_count = 0U;

    if (strcmp(view_name, "statement_analysis") == 0) {
        expected_column_count = sys_statement_analysis_column_count;
    } else if (strcmp(view_name, "x$statement_analysis") == 0) {
        expected_column_count = sys_x_statement_analysis_column_count;
    } else if (strcmp(view_name, "statements_with_errors_or_warnings") == 0 ||
               strcmp(view_name, "x$statements_with_errors_or_warnings") == 0) {
        expected_column_count = sys_statements_with_errors_or_warnings_column_count;
    } else if (strcmp(view_name, "statements_with_full_table_scans") == 0 ||
               strcmp(view_name, "x$statements_with_full_table_scans") == 0) {
        expected_column_count = sys_statements_with_full_table_scans_column_count;
    } else if (strcmp(view_name, "statements_with_runtimes_in_95th_percentile") == 0 ||
               strcmp(view_name, "x$statements_with_runtimes_in_95th_percentile") == 0) {
        expected_column_count = sys_statements_with_runtimes_in_95th_percentile_column_count;
    } else if (strcmp(view_name, "statements_with_sorting") == 0 ||
               strcmp(view_name, "x$statements_with_sorting") == 0) {
        expected_column_count = sys_statements_with_sorting_column_count;
    } else if (strcmp(view_name, "statements_with_temp_tables") == 0 ||
               strcmp(view_name, "x$statements_with_temp_tables") == 0) {
        expected_column_count = sys_statements_with_temp_tables_column_count;
    }

    if (rows->definition->column_count != expected_column_count) {
        set_runtime_error(database, "invalid sys statement digest view columns");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_sys_innodb_buffer_stats_by_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_innodb_buffer_stats_by_schema_column_count) {
        set_runtime_error(database, "invalid sys.innodb_buffer_stats_by_schema columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_innodb_buffer_stats_by_table_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_innodb_buffer_stats_by_table_column_count) {
        set_runtime_error(database, "invalid sys.innodb_buffer_stats_by_table columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_by_thread_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_by_thread_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.io_by_thread_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_file_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_file_by_bytes_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_file_by_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_file_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_file_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_file_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_wait_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_wait_by_bytes_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_wait_by_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_wait_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_wait_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_wait_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_latest_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_latest_file_io_column_count) {
        set_runtime_error(database, "invalid sys.latest_file_io columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_ps_check_lost_instrumentation_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_ps_check_lost_instrumentation_column_count) {
        set_runtime_error(database, "invalid sys.ps_check_lost_instrumentation columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

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
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    int rc = build_mysql_system_table_rows(database, context, definition, &rows);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_result_columns(database, result, query);
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
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *out_rows
) {
    bool has_no_rows = mysql_system_table_definition_has_no_rows(definition);
    bool has_catalog_rows = mysql_system_table_definition_has_catalog_rows(definition);
    int rc = MYLITE_OK;

    *out_rows = (struct information_schema_row_set){.definition = &definition->query_definition};
    rc = append_mysql_system_table_system_rows(database, context, definition, out_rows);
    if (rc == MYLITE_OK && !has_no_rows && has_catalog_rows) {
        rc = append_mysql_system_table_catalog_rows(database, definition, out_rows);
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
        return append_performance_schema_system_table_rows(database, definition, rows);
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
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    if (strcmp(definition->query_definition.name, "global_variables") == 0) {
        return append_performance_schema_variable_rows(database, rows, true);
    }
    if (strcmp(definition->query_definition.name, "session_variables") == 0) {
        return append_performance_schema_variable_rows(database, rows, false);
    }
    if (strcmp(definition->query_definition.name, "global_status") == 0) {
        return append_performance_schema_status_rows(database, rows, true);
    }
    if (strcmp(definition->query_definition.name, "performance_timers") == 0) {
        return append_performance_schema_performance_timer_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "session_status") == 0) {
        return append_performance_schema_status_rows(database, rows, false);
    }

    set_runtime_error(database, "invalid performance_schema system table");
    return MYLITE_ERROR;
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
        strcmp(
            definition->query_definition.name,
            "statements_with_runtimes_in_95th_percentile"
        ) == 0 ||
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

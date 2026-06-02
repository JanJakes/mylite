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
    (void)context;

    if (mysql_system_table_definition_has_no_rows(definition)) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") == 0) {
        return append_mysql_schema_system_table_rows(database, definition, rows);
    }
    if (strcmp(definition->schema_name, "sys") == 0) {
        return append_sys_schema_system_table_rows(database, definition, rows);
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

static int append_sys_schema_system_table_rows(
    struct mylite_db *database,
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

enum sys_schema_auto_increment_columns_column_index {
    sys_schema_auto_increment_columns_table_schema_column = 0,
    sys_schema_auto_increment_columns_table_name_column = 1,
    sys_schema_auto_increment_columns_column_name_column = 2,
    sys_schema_auto_increment_columns_max_value_column = 7,
    sys_schema_auto_increment_columns_auto_increment_ratio_column = 9,
};

static int append_sys_schema_auto_increment_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_auto_increment_columns_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_auto_increment_columns_column_count) {
        set_runtime_error(database, "invalid sys.schema_auto_increment_columns columns");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_for_each_schema(
        database,
        append_sys_schema_auto_increment_columns_schema_rows,
        &context
    );
    if (rc == MYLITE_OK) {
        sort_sys_schema_auto_increment_columns_default_order(rows);
    }
    return rc;
}

static int append_sys_schema_auto_increment_columns_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_auto_increment_columns_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (strcmp(schema->name, "mysql") == 0 || strcmp(schema->name, "sys") == 0 ||
        strcmp(schema->name, "performance_schema") == 0 ||
        schema_name_is_information_schema(schema->name)) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_auto_increment_columns_table_rows,
        context
    );
}

static int append_sys_schema_auto_increment_columns_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_auto_increment_columns_context *context = user_data;
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (!columns[column_index].is_auto_increment) {
            continue;
        }
        rc = append_sys_schema_auto_increment_columns_column_row(
            context,
            table,
            &columns[column_index]
        );
    }
    free(columns);
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(context->database)) == MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else {
            set_runtime_error(
                context->database,
                "failed to build sys.schema_auto_increment_columns rows"
            );
        }
    }
    return rc;
}

static int append_sys_schema_auto_increment_columns_column_row(
    struct sys_schema_auto_increment_columns_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    char column_type_text[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    char max_value_text[integer_text_capacity];
    char auto_increment_text[integer_text_capacity];
    char ratio_text[integer_text_capacity];
    const char *column_type = NULL;
    const char *max_value_cell = NULL;
    const char *auto_increment_cell = NULL;
    const char *auto_increment_ratio_cell = NULL;
    const char *data_type = information_schema_data_type_for_descriptor(column);
    bool is_unsigned =
        information_schema_innodb_column_logical_type_is_unsigned(column->logical_type);
    uint64_t max_value = 0U;
    int64_t row_count = 0;
    int rc = show_column_type_text(
        context->database,
        column->logical_type,
        column_type_text,
        sizeof(column_type_text),
        &column_type
    );

    if (rc == MYLITE_OK &&
        sys_schema_auto_increment_columns_max_value(data_type, is_unsigned, &max_value)) {
        rc = format_uint64(context->database, max_value, max_value_text, sizeof(max_value_text));
        if (rc == MYLITE_OK) {
            max_value_cell = max_value_text;
        }
    }
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(context->database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(
                context->database,
                "failed to read sys.schema_auto_increment_columns row count"
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && max_value_cell != NULL &&
        !(row_count == 0 && table->auto_increment_next <= 1)) {
        rc = information_schema_format_i64(
            context->database,
            table->auto_increment_next,
            auto_increment_text,
            sizeof(auto_increment_text)
        );
        if (rc == MYLITE_OK) {
            auto_increment_cell = auto_increment_text;
            rc = format_sys_schema_auto_increment_columns_ratio(
                context->database,
                table->auto_increment_next,
                max_value,
                ratio_text,
                sizeof(ratio_text)
            );
        }
        if (rc == MYLITE_OK) {
            auto_increment_ratio_cell = ratio_text;
        }
    }
    if (rc == MYLITE_OK) {
        const char *values[sys_schema_auto_increment_columns_column_count] = {
            context->schema->name,
            table->name,
            column->name,
            data_type,
            column_type,
            is_unsigned ? "0" : "1",
            is_unsigned ? "1" : "0",
            max_value_cell,
            auto_increment_cell,
            auto_increment_ratio_cell,
        };

        rc = append_information_schema_row(context->database, context->rows, values);
    }
    return rc;
}

static bool sys_schema_auto_increment_columns_max_value(
    const char *data_type,
    bool is_unsigned,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (out_value == NULL) {
        return false;
    }
    *out_value = 0U;
    if (data_type == NULL) {
        return false;
    }
    if (strcmp(data_type, "tinyint") == 0) {
        value = UINT64_C(255);
    } else if (strcmp(data_type, "smallint") == 0) {
        value = UINT64_C(65535);
    } else if (strcmp(data_type, "mediumint") == 0) {
        value = UINT64_C(16777215);
    } else if (strcmp(data_type, "int") == 0) {
        value = UINT64_C(4294967295);
    } else if (strcmp(data_type, "bigint") == 0) {
        value = UINT64_MAX;
    } else {
        return false;
    }

    *out_value = is_unsigned ? value : value >> 1U;
    return true;
}

static int format_sys_schema_auto_increment_columns_ratio(
    struct mylite_db *database,
    int64_t auto_increment,
    uint64_t max_value,
    char *buffer,
    size_t buffer_size
) {
    int written = 0;

    if (auto_increment < 0 || max_value == 0U) {
        set_runtime_error(database, "invalid sys.schema_auto_increment_columns ratio");
        return MYLITE_ERROR;
    }

    written =
        snprintf(buffer, buffer_size, "%.4f", (double)(uint64_t)auto_increment / (double)max_value);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format sys.schema_auto_increment_columns ratio");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void sort_sys_schema_auto_increment_columns_default_order(
    struct information_schema_row_set *rows
) {
    if (rows == NULL || rows->row_count < 2U) {
        return;
    }

    for (size_t index = 1U; index < rows->row_count; ++index) {
        char **row = rows->rows[index];
        size_t insert = index;

        while (insert > 0U &&
               compare_sys_schema_auto_increment_columns_rows(row, rows->rows[insert - 1U]) < 0) {
            rows->rows[insert] = rows->rows[insert - 1U];
            --insert;
        }
        rows->rows[insert] = row;
    }
}

static int compare_sys_schema_auto_increment_columns_rows(char *const *left, char *const *right) {
    const char *left_ratio = left[sys_schema_auto_increment_columns_auto_increment_ratio_column];
    const char *right_ratio = right[sys_schema_auto_increment_columns_auto_increment_ratio_column];
    int text_compare = 0;

    if (left_ratio == NULL && right_ratio != NULL) {
        return 1;
    }
    if (left_ratio != NULL && right_ratio == NULL) {
        return -1;
    }
    if (left_ratio != NULL && right_ratio != NULL) {
        double left_value = strtod(left_ratio, NULL);
        double right_value = strtod(right_ratio, NULL);

        if (left_value > right_value) {
            return -1;
        }
        if (left_value < right_value) {
            return 1;
        }
    }

    if (left[sys_schema_auto_increment_columns_max_value_column] != NULL &&
        right[sys_schema_auto_increment_columns_max_value_column] != NULL) {
        uint64_t left_max =
            strtoull(left[sys_schema_auto_increment_columns_max_value_column], NULL, decimal_base);
        uint64_t right_max =
            strtoull(right[sys_schema_auto_increment_columns_max_value_column], NULL, decimal_base);

        if (left_max < right_max) {
            return -1;
        }
        if (left_max > right_max) {
            return 1;
        }
    }

    text_compare = compare_optional_text(
        left[sys_schema_auto_increment_columns_table_schema_column],
        right[sys_schema_auto_increment_columns_table_schema_column]
    );
    if (text_compare != 0) {
        return text_compare;
    }
    text_compare = compare_optional_text(
        left[sys_schema_auto_increment_columns_table_name_column],
        right[sys_schema_auto_increment_columns_table_name_column]
    );
    if (text_compare != 0) {
        return text_compare;
    }
    return compare_optional_text(
        left[sys_schema_auto_increment_columns_column_name_column],
        right[sys_schema_auto_increment_columns_column_name_column]
    );
}

static int compare_optional_text(const char *left, const char *right) {
    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        return 1;
    }
    if (right == NULL) {
        return -1;
    }
    return strcmp(left, right);
}

static int append_sys_schema_index_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_latency
) {
    struct sys_schema_index_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_latency = formatted_latency,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_index_statistics_column_count) {
        set_runtime_error(database, "invalid sys.schema_index_statistics columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_schema_index_statistics_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_index_statistics_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_mysql_system_rows(
    struct sys_schema_index_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_schema_index_statistics_mysql_system_table_rows(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_mysql_system_table_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            definition->schema_name,
            definition->query_definition.name,
            "PRIMARY"
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            definition->schema_name,
            definition->query_definition.name,
            definition->secondary_indexes[index].name
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_index_statistics_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_index_statistics_table_rows,
        context
    );
}

static int append_sys_schema_index_statistics_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_index_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_index_statistics_base_index_rows(context, table);
}

static int append_sys_schema_index_statistics_base_index_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            context->schema->name,
            table->name,
            indexes[index].index.name
        );
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_schema_index_statistics_index_row(
    struct sys_schema_index_statistics_context *context,
    const char *schema_name,
    const char *table_name,
    const char *index_name
) {
    static const char formatted_zero_latency[] = "  0 ps";
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL || index_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *latency = context->formatted_latency ? formatted_zero_latency : zero;
    const char *values[sys_schema_index_statistics_column_count] = {
        schema_name,
        table_name,
        index_name,
        zero,
        latency,
        zero,
        latency,
        zero,
        latency,
        zero,
        latency,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_x_ps_schema_table_statistics_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_table_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_metrics = false,
        .include_buffer_metrics = false,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_x_ps_schema_table_statistics_io_column_count) {
        set_runtime_error(database, "invalid sys.x$ps_schema_table_statistics_io columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_x_ps_schema_table_statistics_io_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (definition == NULL) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") != 0 &&
        (strcmp(definition->schema_name, "sys") != 0 ||
         strcmp(definition->query_definition.name, "sys_config") != 0)) {
        return MYLITE_OK;
    }
    return append_sys_x_ps_schema_table_statistics_io_table_row(
        context,
        definition->schema_name,
        definition->query_definition.name
    );
}

static int append_sys_x_ps_schema_table_statistics_io_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog,
        context
    );
}

static int append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_x_ps_schema_table_statistics_io_table_row(
        context,
        context->schema->name,
        table->name
    );
}

static int append_sys_x_ps_schema_table_statistics_io_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
) {
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *values[sys_x_ps_schema_table_statistics_io_column_count] = {
        schema_name,
        table_name,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_table_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_metrics,
    bool include_buffer_metrics
) {
    struct sys_schema_table_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_metrics = formatted_metrics,
        .include_buffer_metrics = include_buffer_metrics,
        .schema = NULL,
    };
    size_t expected_column_count = include_buffer_metrics
                                       ? sys_schema_table_statistics_with_buffer_column_count
                                       : sys_schema_table_statistics_column_count;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != expected_column_count) {
        set_runtime_error(database, "invalid sys.schema_table_statistics columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_schema_table_statistics_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_table_statistics_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_schema_table_statistics_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_schema_table_statistics_mysql_system_table_row(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_schema_table_statistics_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (definition == NULL) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") != 0 &&
        (strcmp(definition->schema_name, "sys") != 0 ||
         strcmp(definition->query_definition.name, "sys_config") != 0)) {
        return MYLITE_OK;
    }
    return append_sys_schema_table_statistics_table_row(
        context,
        definition->schema_name,
        definition->query_definition.name
    );
}

static int append_sys_schema_table_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_table_statistics_table_row_from_catalog,
        context
    );
}

static int append_sys_schema_table_statistics_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_table_statistics_table_row(
        context,
        context->schema->name,
        table->name
    );
}

static int append_sys_schema_table_statistics_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
) {
    static const char formatted_zero_bytes[] = "   0 bytes";
    static const char formatted_zero_latency[] = "  0 ps";
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *latency = context->formatted_metrics ? formatted_zero_latency : zero;
    const char *bytes = context->formatted_metrics ? formatted_zero_bytes : zero;
    if (context->include_buffer_metrics) {
        const char *values[sys_schema_table_statistics_with_buffer_column_count] = {
            schema_name, table_name, zero,  latency, zero, latency, zero,    latency, zero,
            latency,     zero,       bytes, latency, zero, bytes,   latency, zero,    latency,
            bytes,       bytes,      bytes, zero,    zero, zero,    zero,
        };

        return append_information_schema_row(context->database, context->rows, values);
    }

    const char *values[sys_schema_table_statistics_column_count] = {
        schema_name, table_name, latency, zero,    latency, zero,  latency, zero, latency, zero,
        latency,     zero,       bytes,   latency, zero,    bytes, latency, zero, latency,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_redundant_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_redundant_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_schema_redundant_indexes_column_count) {
        set_runtime_error(database, "invalid sys.schema_redundant_indexes columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_schema_redundant_indexes_schema_rows,
        &context
    );
}

static int append_sys_x_schema_flattened_keys_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_redundant_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_x_schema_flattened_keys_column_count) {
        set_runtime_error(database, "invalid sys.x$schema_flattened_keys columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_x_schema_flattened_keys_schema_rows,
        &context
    );
}

static int append_sys_schema_redundant_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_redundant_indexes_table_rows,
        context
    );
}

static int append_sys_x_schema_flattened_keys_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_x_schema_flattened_keys_table_rows,
        context
    );
}

static int append_sys_schema_redundant_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;
    struct sys_flattened_key_row_set key_rows = {0};
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = build_sys_flattened_key_rows(context, table, &key_rows);
    if (rc == MYLITE_OK) {
        rc = append_sys_schema_redundant_index_rows(context, &key_rows);
    }
    sys_flattened_key_rows_deinit(&key_rows);
    return rc;
}

static int append_sys_x_schema_flattened_keys_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;
    struct sys_flattened_key_row_set key_rows = {0};
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = build_sys_flattened_key_rows(context, table, &key_rows);
    if (rc == MYLITE_OK) {
        rc = append_sys_x_schema_flattened_key_rows(context, &key_rows);
    }
    sys_flattened_key_rows_deinit(&key_rows);
    return rc;
}

static int build_sys_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    struct sys_flattened_key_row_set *out_rows
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    *out_rows = (struct sys_flattened_key_row_set){0};
    rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (sys_flattened_key_index_supported(&indexes[index])) {
            rc = append_sys_flattened_key_row_from_index(context, table, &indexes[index], out_rows);
        }
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_x_schema_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < key_rows->count; ++index) {
        rc = append_sys_x_schema_flattened_key_row(context, &key_rows->rows[index]);
    }
    return rc;
}

static int append_sys_schema_redundant_index_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
) {
    int rc = MYLITE_OK;

    for (size_t redundant_index = 0U; rc == MYLITE_OK && redundant_index < key_rows->count;
         ++redundant_index) {
        const struct sys_flattened_key_row *redundant = &key_rows->rows[redundant_index];

        for (size_t dominant_index = 0U; rc == MYLITE_OK && dominant_index < key_rows->count;
             ++dominant_index) {
            const struct sys_flattened_key_row *dominant = &key_rows->rows[dominant_index];

            if (sys_schema_redundant_index_pair_matches(redundant, dominant)) {
                rc = append_sys_schema_redundant_index_row(context, redundant, dominant);
            }
        }
    }
    return rc;
}

static int append_sys_x_schema_flattened_key_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *key_row
) {
    const char *non_unique = key_row->non_unique ? "1" : "0";
    const char *subpart_exists = key_row->subpart_exists ? "1" : "0";
    const char *values[sys_x_schema_flattened_keys_column_count] = {
        key_row->schema_name,
        key_row->table_name,
        key_row->index_name,
        non_unique,
        subpart_exists,
        key_row->index_columns,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_redundant_index_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
) {
    char *drop_index_sql = NULL;
    const char *redundant_non_unique = redundant->non_unique ? "1" : "0";
    const char *dominant_non_unique = dominant->non_unique ? "1" : "0";
    const char *subpart_exists = redundant->subpart_exists || dominant->subpart_exists ? "1" : "0";
    int rc = build_sys_redundant_drop_index_sql(redundant, &drop_index_sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        }
        return rc;
    }

    const char *values[sys_schema_redundant_indexes_column_count] = {
        redundant->schema_name,
        redundant->table_name,
        redundant->index_name,
        redundant->index_columns,
        redundant_non_unique,
        dominant->index_name,
        dominant->index_columns,
        dominant_non_unique,
        subpart_exists,
        drop_index_sql,
    };

    rc = append_information_schema_row(context->database, context->rows, values);
    free(drop_index_sql);
    return rc;
}

static bool sys_schema_redundant_index_pair_matches(
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
) {
    const char *redundant_sort_name = NULL;
    const char *dominant_sort_name = NULL;

    if (redundant == NULL || dominant == NULL ||
        strcmp(redundant->index_name, dominant->index_name) == 0) {
        return false;
    }
    if (strcmp(redundant->index_columns, dominant->index_columns) == 0) {
        if (redundant->non_unique && !dominant->non_unique) {
            return true;
        }
        redundant_sort_name =
            strcmp(redundant->index_name, "PRIMARY") == 0 ? "" : redundant->index_name;
        dominant_sort_name =
            strcmp(dominant->index_name, "PRIMARY") == 0 ? "" : dominant->index_name;
        if (redundant->non_unique == dominant->non_unique &&
            strcmp(redundant_sort_name, dominant_sort_name) > 0) {
            return true;
        }
    }
    if (redundant->non_unique && sys_schema_redundant_columns_left_prefix(
                                     redundant->index_columns,
                                     dominant->index_columns
                                 )) {
        return true;
    }
    return !dominant->non_unique && sys_schema_redundant_columns_left_prefix(
                                        dominant->index_columns,
                                        redundant->index_columns
                                    );
}

static bool sys_schema_redundant_columns_left_prefix(const char *left, const char *right) {
    size_t left_length = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    left_length = strlen(left);
    if (left_length == 0U || strncmp(left, right, left_length) != 0) {
        return false;
    }
    return right[left_length] == ',';
}

static int append_sys_flattened_key_row_from_index(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index,
    struct sys_flattened_key_row_set *rows
) {
    struct sys_flattened_key_row row = {0};
    int written = snprintf(row.schema_name, sizeof(row.schema_name), "%s", context->schema->name);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(row.schema_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys schema name");
        return MYLITE_ERROR;
    }
    written = snprintf(row.table_name, sizeof(row.table_name), "%s", table->name);
    if (written < 0 || (size_t)written >= sizeof(row.table_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys table name");
        return MYLITE_ERROR;
    }
    written = snprintf(row.index_name, sizeof(row.index_name), "%s", index->index.name);
    if (written < 0 || (size_t)written >= sizeof(row.index_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys index name");
        return MYLITE_ERROR;
    }

    row.non_unique = !index->index.is_unique;
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        row.subpart_exists =
            row.subpart_exists || index->parts[part_index].index_column.has_prefix_length;
    }

    rc = build_sys_flattened_key_index_columns(index, &row.index_columns);
    if (rc == MYLITE_OK) {
        rc = reserve_sys_flattened_key_rows(rows, rows->count + 1U);
    }
    if (rc != MYLITE_OK) {
        free(row.index_columns);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        }
        return rc;
    }

    rows->rows[rows->count] = row;
    ++rows->count;
    return MYLITE_OK;
}

static bool sys_flattened_key_index_supported(const struct loaded_index_info *index) {
    if (index == NULL || index->part_count == 0U) {
        return false;
    }
    return index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY;
}

static int build_sys_flattened_key_index_columns(
    const struct loaded_index_info *index,
    char **out_columns
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_columns = NULL;
    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, index->parts[part_index].column.name);
        }
    }
    if (rc == MYLITE_OK) {
        *out_columns = mylite_dynamic_string_take(&string);
        if (*out_columns == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int reserve_sys_flattened_key_rows(
    struct sys_flattened_key_row_set *rows,
    size_t required_capacity
) {
    struct sys_flattened_key_row *resized = NULL;
    size_t capacity =
        rows->capacity == 0U ? sys_flattened_key_row_initial_capacity : rows->capacity;

    if (required_capacity <= rows->capacity) {
        return MYLITE_OK;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(rows->rows[0])) {
        return MYLITE_NOMEM;
    }

    resized = realloc(rows->rows, capacity * sizeof(rows->rows[0]));
    if (resized == NULL) {
        return MYLITE_NOMEM;
    }
    rows->rows = resized;
    rows->capacity = capacity;
    return MYLITE_OK;
}

static void sys_flattened_key_rows_deinit(struct sys_flattened_key_row_set *rows) {
    if (rows == NULL) {
        return;
    }
    for (size_t index = 0U; index < rows->count; ++index) {
        free(rows->rows[index].index_columns);
    }
    free(rows->rows);
    *rows = (struct sys_flattened_key_row_set){0};
}

static int build_sys_redundant_drop_index_sql(
    const struct sys_flattened_key_row *redundant,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->schema_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " DROP INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->index_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_sys_schema_table_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_schema_table_lock_waits_column_count) {
        set_runtime_error(database, "invalid sys.schema_table_lock_waits columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_schema_tables_with_full_table_scans_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_schema_tables_with_full_table_scans_column_count) {
        set_runtime_error(database, "invalid sys.schema_tables_with_full_table_scans columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_schema_unused_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_unused_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_schema_unused_indexes_column_count) {
        set_runtime_error(database, "invalid sys.schema_unused_indexes columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_schema_unused_indexes_schema_rows,
        &context
    );
}

static int append_sys_schema_unused_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_unused_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_unused_indexes_table_rows,
        context
    );
}

static int append_sys_schema_unused_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_unused_indexes_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_unused_indexes_index_rows(context, table);
}

static int append_sys_schema_unused_indexes_index_rows(
    struct sys_schema_unused_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (sys_schema_unused_indexes_index_supported(&indexes[index])) {
            rc = append_sys_schema_unused_indexes_index_row(context, table->name, &indexes[index]);
        }
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_schema_unused_indexes_index_row(
    struct sys_schema_unused_indexes_context *context,
    const char *table_name,
    const struct loaded_index_info *index
) {
    const char *values[sys_schema_unused_indexes_column_count] = {
        context->schema->name,
        table_name,
        index->index.name,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static bool sys_schema_unused_indexes_index_supported(const struct loaded_index_info *index) {
    if (index == NULL || index->part_count == 0U || index->index.is_unique) {
        return false;
    }
    return index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL;
}

static int append_sys_schema_object_overview_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_object_overview_context context = {
        .database = database,
        .rows = rows,
        .groups = NULL,
        .group_count = 0U,
        .group_capacity = 0U,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_object_overview_column_count) {
        set_runtime_error(database, "invalid sys.schema_object_overview columns");
        return MYLITE_ERROR;
    }

    rc = collect_sys_schema_object_overview_builtin_groups(&context);
    if (rc == MYLITE_OK) {
        rc = collect_sys_schema_object_overview_mysql_system_index_groups(&context);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_object_overview_schema_groups,
            &context
        );
    }
    if (rc == MYLITE_OK) {
        rc = increment_sys_schema_object_overview_group(
            &context,
            "sys",
            "TRIGGER",
            mylite_execution_catalog_sys_sys_config_trigger_count()
        );
    }
    if (rc == MYLITE_OK) {
        sort_sys_schema_object_overview_groups(&context);
        rc = append_sys_schema_object_overview_group_rows(&context);
    }

    free(context.groups);
    return rc;
}

static int collect_sys_schema_object_overview_builtin_groups(
    struct sys_schema_object_overview_context *context
) {
    int rc = MYLITE_OK;

    for (size_t directory_index = 0U;
         rc == MYLITE_OK &&
         directory_index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++directory_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_at(directory_index);

        for (size_t table_index = 0U; rc == MYLITE_OK && table_index < directory->table_count;
             ++table_index) {
            const char *table_type =
                builtin_schema_table_type(directory, directory->table_names[table_index]);

            rc = increment_sys_schema_object_overview_group(
                context,
                directory->schema_name,
                table_type,
                1U
            );
        }
    }
    return rc;
}

static int collect_sys_schema_object_overview_mysql_system_index_groups(
    struct sys_schema_object_overview_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        const struct mylite_execution_catalog_mysql_system_table *definition =
            mylite_execution_catalog_mysql_system_table_definition_at(index);
        size_t count = mysql_system_table_primary_key_column_count(definition);

        if (SIZE_MAX - count < definition->secondary_index_count) {
            set_runtime_error(context->database, "sys.schema_object_overview count overflow");
            return MYLITE_ERROR;
        }
        count += definition->secondary_index_count;
        if (count > 0U) {
            rc = increment_sys_schema_object_overview_group(
                context,
                definition->schema_name,
                "INDEX (BTREE)",
                (uint64_t)count
            );
        }
    }
    return rc;
}

static int append_sys_schema_object_overview_schema_groups(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_object_overview_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_object_overview_table_groups,
        context
    );
}

static int append_sys_schema_object_overview_table_groups(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_object_overview_context *context = user_data;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }

    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        rc = increment_sys_schema_object_overview_group(
            context,
            context->schema->name,
            "BASE TABLE",
            1U
        );
        if (rc == MYLITE_OK) {
            rc = append_sys_schema_object_overview_base_table_index_groups(context, table);
        }
    } else if (table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        rc = increment_sys_schema_object_overview_group(context, context->schema->name, "VIEW", 1U);
    }

    return rc;
}

static int append_sys_schema_object_overview_base_table_index_groups(
    struct sys_schema_object_overview_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        char object_type[sizeof("INDEX (FULLTEXT)")];
        int written = snprintf(
            object_type,
            sizeof(object_type),
            "INDEX (%s)",
            show_index_index_type_text(&indexes[index])
        );

        if (written < 0 || (size_t)written >= sizeof(object_type)) {
            set_runtime_error(context->database, "failed to format sys.schema_object_overview row");
            rc = MYLITE_ERROR;
        } else {
            rc = increment_sys_schema_object_overview_group(
                context,
                context->schema->name,
                object_type,
                indexes[index].part_count
            );
        }
    }

    free(columns);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(context->database)) == MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else {
            set_runtime_error(context->database, "failed to build sys.schema_object_overview rows");
        }
    }
    return rc;
}

static int increment_sys_schema_object_overview_group(
    struct sys_schema_object_overview_context *context,
    const char *schema_name,
    const char *object_type,
    uint64_t count
) {
    if (context == NULL || context->database == NULL || schema_name == NULL ||
        object_type == NULL) {
        return MYLITE_MISUSE;
    }
    if (count == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < context->group_count; ++index) {
        struct sys_schema_object_overview_group *group = &context->groups[index];

        if (strcmp(group->db, schema_name) == 0 && strcmp(group->object_type, object_type) == 0) {
            if (UINT64_MAX - group->count < count) {
                set_runtime_error(context->database, "sys.schema_object_overview count overflow");
                return MYLITE_ERROR;
            }
            group->count += count;
            return MYLITE_OK;
        }
    }

    if (context->group_count == context->group_capacity) {
        int rc = grow_sys_schema_object_overview_groups(context);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    struct sys_schema_object_overview_group *group = &context->groups[context->group_count++];
    int db_written = snprintf(group->db, sizeof(group->db), "%s", schema_name);
    int object_type_written =
        snprintf(group->object_type, sizeof(group->object_type), "%s", object_type);

    if (db_written < 0 || (size_t)db_written >= sizeof(group->db) || object_type_written < 0 ||
        (size_t)object_type_written >= sizeof(group->object_type)) {
        set_runtime_error(context->database, "failed to format sys.schema_object_overview group");
        --context->group_count;
        return MYLITE_ERROR;
    }
    group->count = count;
    return MYLITE_OK;
}

static int grow_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
) {
    size_t new_capacity = context->group_capacity == 0U
                              ? sys_schema_object_overview_initial_group_capacity
                              : context->group_capacity * 2U;
    struct sys_schema_object_overview_group *groups = NULL;

    if (new_capacity < context->group_capacity ||
        new_capacity > SIZE_MAX / sizeof(*context->groups)) {
        set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    groups = realloc(context->groups, new_capacity * sizeof(*groups));
    if (groups == NULL) {
        set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->groups = groups;
    context->group_capacity = new_capacity;
    return MYLITE_OK;
}

static void sort_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
) {
    if (context == NULL || context->group_count < 2U) {
        return;
    }

    for (size_t index = 1U; index < context->group_count; ++index) {
        struct sys_schema_object_overview_group group = context->groups[index];
        size_t insert = index;

        while (insert > 0U &&
               compare_sys_schema_object_overview_groups(&group, &context->groups[insert - 1U]) <
                   0) {
            context->groups[insert] = context->groups[insert - 1U];
            --insert;
        }
        context->groups[insert] = group;
    }
}

static int compare_sys_schema_object_overview_groups(
    const struct sys_schema_object_overview_group *left,
    const struct sys_schema_object_overview_group *right
) {
    int db_compare = strcmp(left->db, right->db);

    if (db_compare != 0) {
        return db_compare;
    }
    return strcmp(left->object_type, right->object_type);
}

static int append_sys_schema_object_overview_group_rows(
    struct sys_schema_object_overview_context *context
) {
    char count_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < context->group_count; ++index) {
        const struct sys_schema_object_overview_group *group = &context->groups[index];
        const char *values[sys_schema_object_overview_column_count] = {
            group->db,
            group->object_type,
            count_text,
        };

        rc = format_uint64(context->database, group->count, count_text, sizeof(count_text));
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(context->database, context->rows, values);
        }
    }
    return rc;
}

static const char *mysql_system_table_column_comment(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t column_index
) {
    if (definition == NULL || definition->column_comments == NULL ||
        column_index >= definition->query_definition.column_count) {
        return "";
    }
    return definition->column_comments[column_index];
}

static const char *mysql_system_table_column_generation_expression(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t column_index
) {
    if (definition == NULL || definition->column_generation_expressions == NULL ||
        column_index >= definition->query_definition.column_count) {
        return "";
    }
    return definition->column_generation_expressions[column_index];
}

static size_t mysql_system_table_primary_key_column_count(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    size_t count = 0U;

    if (definition == NULL || definition->column_keys == NULL) {
        return 0U;
    }
    if (definition->primary_key_column_count > 0U) {
        return definition->primary_key_column_count;
    }
    for (size_t column_index = 0U; column_index < definition->query_definition.column_count;
         ++column_index) {
        if (strcmp(definition->column_keys[column_index], "PRI") == 0) {
            ++count;
        }
    }
    return count;
}

static const struct mylite_execution_catalog_column_definition *mysql_system_table_primary_key_column(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t key_index
) {
    size_t sequence = 0U;

    if (definition == NULL ||
        key_index >= mysql_system_table_primary_key_column_count(definition)) {
        return NULL;
    }
    if (definition->primary_key_column_count > 0U) {
        size_t column_index = definition->primary_key_column_indexes[key_index];

        if (column_index >= definition->query_definition.column_count) {
            return NULL;
        }
        return &definition->query_definition.columns[column_index];
    }
    for (size_t column_index = 0U; column_index < definition->query_definition.column_count;
         ++column_index) {
        if (strcmp(definition->column_keys[column_index], "PRI") != 0) {
            continue;
        }
        if (sequence == key_index) {
            return &definition->query_definition.columns[column_index];
        }
        ++sequence;
    }
    return NULL;
}

static int append_mysql_innodb_index_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = append_mysql_innodb_index_stats_builtin_index_rows(
        database,
        rows,
        "mysql",
        "component",
        "PRIMARY",
        "0",
        "component_id"
    );

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_builtin_index_rows(
            database,
            rows,
            "sys",
            "sys_config",
            "PRIMARY",
            "6",
            "variable"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_builtin_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *distinct_count,
    const char *stat_description
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = append_mysql_innodb_index_stats_row(
        database,
        rows,
        schema_name,
        table_name,
        index_name,
        "n_diff_pfx01",
        distinct_count,
        "1",
        stat_description
    );
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema_name,
            table_name,
            index_name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema_name,
            table_name,
            index_name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_table_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = append_mysql_innodb_table_stats_builtin_row(database, rows, "mysql", "component", "0");

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_table_stats_builtin_row(database, rows, "sys", "sys_config", "6");
    }
    return rc;
}

static int append_mysql_innodb_table_stats_builtin_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *row_count
) {
    char last_update[datetime_text_length + 1U];
    int rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));

    if (rows->definition->column_count != mysql_innodb_table_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_table_stats columns");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_table_stats_column_count] = {
            schema_name,
            table_name,
            last_update,
            row_count,
            "1",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_mysql_system_table_catalog_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    struct mysql_system_table_catalog_context context = {
        .database = database,
        .rows = rows,
        .definition = definition,
        .schema = NULL,
    };

    return mylite_catalog_for_each_schema(
        database,
        append_mysql_system_table_catalog_schema,
        &context
    );
}

static int append_mysql_system_table_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct mysql_system_table_catalog_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->definition == NULL) {
        return MYLITE_MISUSE;
    }
    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_mysql_system_table_catalog_table,
        context
    );
}

static int append_mysql_system_table_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct mysql_system_table_catalog_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->definition == NULL || context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    if (strcmp(context->definition->schema_name, "mysql") == 0 &&
        strcmp(context->definition->query_definition.name, "innodb_index_stats") == 0) {
        return append_mysql_innodb_index_stats_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    }
    if (strcmp(context->definition->schema_name, "mysql") == 0 &&
        strcmp(context->definition->query_definition.name, "innodb_table_stats") == 0) {
        return append_mysql_innodb_table_stats_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    }

    set_runtime_error(context->database, "invalid mysql system table catalog row");
    return MYLITE_ERROR;
}

static int append_mysql_innodb_index_stats_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    const struct loaded_index_info *clustered_index = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        clustered_index = information_schema_innodb_clustered_index(indexes, index_count);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_mysql_innodb_index_stats_index_rows(
            database,
            rows,
            schema,
            table,
            clustered_index,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK && clustered_index == NULL) {
        rc = append_mysql_innodb_index_stats_generated_cluster_rows(database, rows, schema, table);
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_mysql_innodb_index_stats_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
) {
    size_t prefix_count = 0U;
    int rc = mysql_innodb_index_stats_prefix_count(database, clustered_index, index, &prefix_count);

    if (rc == MYLITE_OK && prefix_count == 0U) {
        return MYLITE_OK;
    }
    for (size_t prefix = 1U; rc == MYLITE_OK && prefix <= prefix_count; ++prefix) {
        rc = append_mysql_innodb_index_stats_diff_row(
            database,
            rows,
            schema,
            table,
            clustered_index,
            index,
            prefix
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_generated_cluster_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    int rc = append_mysql_innodb_index_stats_generated_diff_row(database, rows, schema, table);

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count
) {
    char stat_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char stat_value[integer_text_capacity];
    char *stat_description = NULL;
    int64_t distinct_count = 0;
    int written = snprintf(stat_name, sizeof(stat_name), "n_diff_pfx%02zu", prefix_count);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(stat_name)) {
        set_runtime_error(database, "failed to format mysql.innodb_index_stats stat name");
        return MYLITE_ERROR;
    }

    rc = read_mysql_innodb_index_stats_distinct_count(
        database,
        table,
        clustered_index,
        index,
        prefix_count,
        &distinct_count
    );
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, distinct_count, stat_value, sizeof(stat_value));
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_prefix_description(
            database,
            clustered_index,
            index,
            prefix_count,
            &stat_description
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            stat_name,
            stat_value,
            "1",
            stat_description
        );
    }

    free(stat_description);
    return rc;
}

static int append_mysql_innodb_index_stats_generated_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char stat_value[integer_text_capacity];
    int64_t row_count = 0;
    int rc = read_show_table_status_row_count(database, table, &row_count);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read mysql.innodb_index_stats rows");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, row_count, stat_value, sizeof(stat_value));
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "n_diff_pfx01",
            stat_value,
            "1",
            "DB_ROW_ID"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_page_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_description
) {
    return append_mysql_innodb_index_stats_row(
        database,
        rows,
        schema_name,
        table_name,
        index_name,
        stat_name,
        "1",
        NULL,
        stat_description
    );
}

static int append_mysql_innodb_index_stats_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_value,
    const char *sample_size,
    const char *stat_description
) {
    char last_update[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_index_stats_column_count] = {
            schema_name,
            table_name,
            index_name,
            last_update,
            stat_name,
            stat_value,
            sample_size,
            stat_description,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int mysql_innodb_index_stats_prefix_count(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t *out_count
) {
    size_t clustered_part_count = 0U;
    size_t prefix_count = 0U;
    int rc = MYLITE_OK;

    if (out_count == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0U;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
        index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return MYLITE_OK;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY || index == clustered_index ||
        index->index.is_unique) {
        *out_count = index->part_count;
        return MYLITE_OK;
    }
    if (index->index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats index type");
        return MYLITE_ERROR;
    }

    prefix_count = index->part_count;
    if (clustered_index == NULL) {
        if (prefix_count == SIZE_MAX) {
            set_runtime_error(database, "invalid mysql.innodb_index_stats prefix count");
            return MYLITE_ERROR;
        }
        *out_count = prefix_count + 1U;
        return MYLITE_OK;
    }

    rc = information_schema_innodb_secondary_clustered_part_count(
        database,
        index,
        clustered_index,
        &clustered_part_count
    );
    if (rc == MYLITE_OK) {
        if (prefix_count > SIZE_MAX - clustered_part_count) {
            set_runtime_error(database, "invalid mysql.innodb_index_stats prefix count");
            return MYLITE_ERROR;
        }
        *out_count = prefix_count + clustered_part_count;
    }
    return rc;
}

static int read_mysql_innodb_index_stats_distinct_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_mysql_innodb_index_stats_distinct_sql(
        database,
        table,
        clustered_index,
        index,
        prefix_count,
        &sql
    );

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    return rc;
}

static int build_mysql_innodb_index_stats_distinct_sql(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (table == NULL || index == NULL || prefix_count == 0U || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM (SELECT DISTINCT ");
    for (size_t prefix_index = 0U; rc == MYLITE_OK && prefix_index < prefix_count; ++prefix_index) {
        if (prefix_index > 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_mysql_innodb_index_stats_prefix_expression(
                database,
                &string,
                clustered_index,
                index,
                prefix_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, table->physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ")");
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_mysql_innodb_index_stats_prefix_expression(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
) {
    const struct loaded_index_part *part = NULL;

    if (prefix_index < index->part_count) {
        return append_loaded_key_part_sql(string, &index->parts[prefix_index], NULL);
    }
    if (clustered_index == NULL) {
        return mylite_dynamic_string_append(string, "rowid");
    }

    part = mysql_innodb_index_stats_missing_clustered_part(
        clustered_index,
        index,
        prefix_index - index->part_count
    );
    if (part == NULL) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats prefix column");
        return MYLITE_ERROR;
    }
    return append_loaded_key_part_sql(string, part, NULL);
}

static int append_mysql_innodb_index_stats_prefix_description(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_description
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (index == NULL || prefix_count == 0U || out_description == NULL) {
        return MYLITE_MISUSE;
    }
    *out_description = NULL;
    mylite_dynamic_string_init(&string);

    for (size_t prefix_index = 0U; rc == MYLITE_OK && prefix_index < prefix_count; ++prefix_index) {
        if (prefix_index > 0U) {
            rc = mylite_dynamic_string_append(&string, ",");
        }
        if (rc == MYLITE_OK) {
            rc = append_mysql_innodb_index_stats_prefix_name(
                database,
                &string,
                clustered_index,
                index,
                prefix_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        *out_description = mylite_dynamic_string_take(&string);
        if (*out_description == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_mysql_innodb_index_stats_prefix_name(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
) {
    const struct loaded_index_part *part = NULL;

    if (prefix_index < index->part_count) {
        return mylite_dynamic_string_append(string, index->parts[prefix_index].column.name);
    }
    if (clustered_index == NULL) {
        return mylite_dynamic_string_append(string, "DB_ROW_ID");
    }

    part = mysql_innodb_index_stats_missing_clustered_part(
        clustered_index,
        index,
        prefix_index - index->part_count
    );
    if (part == NULL) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats prefix description");
        return MYLITE_ERROR;
    }
    return mylite_dynamic_string_append(string, part->column.name);
}

static const struct loaded_index_part *mysql_innodb_index_stats_missing_clustered_part(
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t missing_index
) {
    size_t current = 0U;

    if (clustered_index == NULL || index == NULL) {
        return NULL;
    }
    for (size_t part_index = 0U; part_index < clustered_index->part_count; ++part_index) {
        if (information_schema_innodb_index_contains_column(
                index,
                clustered_index->parts[part_index].column.column_id
            )) {
            continue;
        }
        if (current == missing_index) {
            return &clustered_index->parts[part_index];
        }
        ++current;
    }
    return NULL;
}

static int append_mysql_innodb_table_stats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char last_update[datetime_text_length + 1U];
    char row_count_text[integer_text_capacity];
    char other_index_size_text[integer_text_capacity];
    int64_t row_count = 0;
    int64_t other_index_size = 0;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_table_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_table_stats columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read mysql.innodb_table_stats rows");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_count_nonprimary_indexes(
            database,
            table->table_id,
            &other_index_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            row_count,
            row_count_text,
            sizeof(row_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            other_index_size,
            other_index_size_text,
            sizeof(other_index_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_table_stats_column_count] = {
            schema->name,
            table->name,
            last_update,
            row_count_text,
            "1",
            other_index_size_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int mysql_system_current_timestamp(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    return format_session_timestamp_epoch_text(
        database,
        current_wall_clock_epoch(),
        buffer,
        buffer_size
    );
}

static int mysql_system_current_timestamp2(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    char timestamp[datetime_text_length + 1U];
    int rc = mysql_system_current_timestamp(database, timestamp, sizeof(timestamp));

    if (rc == MYLITE_OK) {
        int written = snprintf(buffer, buffer_size, "%s.00", timestamp);

        if (written < 0 || (size_t)written >= buffer_size) {
            set_runtime_error(database, "failed to format timestamp(2) metadata");
            rc = MYLITE_ERROR;
        }
    }
    return rc;
}

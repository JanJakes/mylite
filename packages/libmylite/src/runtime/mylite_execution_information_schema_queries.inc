static int resolve_information_schema_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int rc = MYLITE_OK;

    *out_query = (struct information_schema_query){0};
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT supports only non-distinct metadata queries"
        );
        return MYLITE_ERROR;
    }

    rc = information_schema_resolve_source(database, from_clause, out_query);
    while (rc == MYLITE_OK && optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports only WHERE, ORDER BY, and LIMIT"
            );
            rc = MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_projection(database, select_list, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_where(database, where_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        out_query->limit = (struct planned_select_limit){
            .has_limit = false,
            .row_count = 0,
            .has_offset = false,
            .offset = 0,
        };
        rc = information_schema_plan_order(database, order_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_limit(database, limit_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }

    information_schema_query_deinit(out_query);
    return rc;
}

static void information_schema_query_deinit(struct information_schema_query *query) {
    if (query == NULL) {
        return;
    }
    for (size_t index = 0U; index < query->projection_count; ++index) {
        free(query->projection_names[index]);
    }
    free((void *)query->projection_names);
    free(query->projection_indexes);
    *query = (struct information_schema_query){0};
}

static int execute_information_schema_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_row_set rows = {0};
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    int rc = build_information_schema_rows(database, context, query->definition, &rows);

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
    if (rc == MYLITE_OK && query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST &&
        read_row_count > 0U) {
        rc = append_show_processlist_warning(database);
    }
    if (rc == MYLITE_OK && query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROFILING &&
        !(query->limit.has_limit && query->limit.row_count == 0)) {
        rc = append_information_schema_profiling_warning(database);
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

static int build_information_schema_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    struct information_schema_row_set *out_rows
) {
    int rc = MYLITE_OK;

    *out_rows = (struct information_schema_row_set){.definition = definition};
    if (definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST) {
        rc = append_information_schema_processlist_system_row(database, context, out_rows);
    } else {
        rc = append_information_schema_system_rows(database, out_rows);
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_catalog_rows(database, out_rows);
    }
    if (rc != MYLITE_OK) {
        information_schema_row_set_deinit(out_rows);
    }
    return rc;
}

static void information_schema_row_set_deinit(struct information_schema_row_set *rows) {
    if (rows == NULL) {
        return;
    }
    for (size_t row_index = 0U; row_index < rows->row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < rows->definition->column_count;
             ++column_index) {
            free(rows->rows[row_index][column_index]);
        }
        free((void *)rows->rows[row_index]);
    }
    free((void *)rows->rows);
    *rows = (struct information_schema_row_set){0};
}

static int append_information_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
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
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_TYPE:
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
                    strcmp(view->name, "x$io_global_by_file_by_bytes") == 0 ||
                    strcmp(view->name, "x$io_global_by_file_by_latency") == 0 ||
                    strcmp(view->name, "x$io_global_by_wait_by_bytes") == 0 ||
                    strcmp(view->name, "x$io_global_by_wait_by_latency") == 0 ||
                    strcmp(view->name, "schema_index_statistics") == 0 ||
                    strcmp(view->name, "x$schema_index_statistics") == 0 ||
                    strcmp(view->name, "schema_tables_with_full_table_scans") == 0 ||
                    strcmp(view->name, "x$schema_tables_with_full_table_scans") == 0 ||
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
        {"schema_table_lock_waits", "format_statement"},
        {"schema_table_lock_waits", "ps_thread_account"},
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

static int append_information_schema_catalog_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct information_schema_catalog_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    switch (rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_CHARACTER_SETS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ADMINISTRABLE_ROLE_AUTHORIZATIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_APPLICABLE_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATION_CHARACTER_SET_APPLICABILITY:
    case MYLITE_EXECUTION_CATALOG_TABLE_ENABLED_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_ENGINES:
    case MYLITE_EXECUTION_CATALOG_TABLE_EVENTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_KEYWORDS:
    case MYLITE_EXECUTION_CATALOG_TABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PARAMETERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PLUGINS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROFILING:
    case MYLITE_EXECUTION_CATALOG_TABLE_RESOURCE_GROUPS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROUTINES:
    case MYLITE_EXECUTION_CATALOG_TABLE_FILES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_DATAFILES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_BEING_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_CACHE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DEFAULT_STOPWORD:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES_BRIEF:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_SESSION_TEMP_TABLESPACES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TRX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE_LRU:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_POOL_STATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CACHED_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_METRICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TEMP_TABLE_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_SPATIAL_REFERENCE_SYSTEMS:
    case MYLITE_EXECUTION_CATALOG_TABLE_CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_UNITS_OF_MEASURE:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_COLUMN_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_ROUTINE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_TABLE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMA_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TRIGGERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_ATTRIBUTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_ROUTINE_USAGE:
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
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_TYPE:
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
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_INDEX_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS:
        return MYLITE_OK;
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLESPACES_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEWS:
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_CHECK_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_KEY_COLUMN_USAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PARTITIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_REFERENTIAL_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FIELDS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN_COLS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_VIRTUAL:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_GEOMETRY_COLUMNS:
        break;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_information_schema_catalog_schema,
        &context
    );
}

static int append_information_schema_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct information_schema_catalog_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (context->rows->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA) {
        return append_information_schema_schemata_schema_row(
            context->database,
            context->rows,
            schema
        );
    }
    if (context->rows->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA_EXTENSIONS) {
        return append_information_schema_schemata_extensions_schema_row(
            context->database,
            context->rows,
            schema
        );
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_information_schema_catalog_table,
        context
    );
}

static int append_information_schema_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct information_schema_catalog_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        return append_information_schema_catalog_view_table(context, table);
    }
    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        return append_information_schema_catalog_base_table(context, table);
    }

    return MYLITE_OK;
}

static int append_information_schema_catalog_view_table(
    struct information_schema_catalog_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    switch (context->rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
        return append_information_schema_tables_view_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
        return append_information_schema_tables_extensions_table_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
        return append_information_schema_columns_view_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
        return append_information_schema_columns_extensions_table_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEWS:
        return append_information_schema_views_view_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE:
        return append_information_schema_view_table_usage_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    default:
        return MYLITE_OK;
    }
}

static int append_information_schema_catalog_base_table(
    struct information_schema_catalog_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    switch (context->rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
        return append_information_schema_tables_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
        return append_information_schema_tables_extensions_table_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
        return append_information_schema_columns_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
        return append_information_schema_columns_extensions_table_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLESPACES_EXTENSIONS:
        return append_information_schema_tablespaces_extensions_table_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS:
        return append_information_schema_table_constraints_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS_EXTENSIONS:
        return append_information_schema_table_constraints_extensions_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FIELDS:
        return append_information_schema_innodb_fields_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN:
        return append_information_schema_innodb_foreign_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN_COLS:
        return append_information_schema_innodb_foreign_cols_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_COLUMNS:
        return append_information_schema_innodb_columns_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES:
        return append_information_schema_innodb_indexes_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLES:
        return append_information_schema_innodb_tables_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS:
        return append_information_schema_innodb_tablestats_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_VIRTUAL:
        return append_information_schema_innodb_virtual_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_CHECK_CONSTRAINTS:
        return append_information_schema_check_constraints_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_KEY_COLUMN_USAGE:
        return append_information_schema_key_column_usage_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS:
        return append_information_schema_statistics_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_PARTITIONS:
        return append_information_schema_partitions_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_REFERENTIAL_CONSTRAINTS:
        return append_information_schema_referential_constraints_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_GEOMETRY_COLUMNS:
        return append_information_schema_st_geometry_columns_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    default:
        return MYLITE_OK;
    }
}

static int append_information_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *const *values
) {
    char ***grown_rows = NULL;
    char **row = NULL;
    size_t required_count = rows->row_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_rows)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    grown_rows = (char ***)realloc((void *)rows->rows, required_count * sizeof(*grown_rows));
    if (grown_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    rows->rows = grown_rows;

    row = (char **)calloc(rows->definition->column_count, sizeof(*row));
    if (row == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < rows->definition->column_count; ++column_index) {
        if (values[column_index] == NULL) {
            continue;
        }
        int rc = duplicate_text(database, values[column_index], &row[column_index]);

        if (rc != MYLITE_OK) {
            for (size_t rollback_index = 0U; rollback_index < column_index; ++rollback_index) {
                free(row[rollback_index]);
            }
            free((void *)row);
            return rc;
        }
    }

    rows->rows[rows->row_count] = row;
    rows->row_count = required_count;
    return MYLITE_OK;
}

static int append_information_schema_schemata_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_builtin_schema_count();
         ++index) {
        const struct mylite_execution_catalog_builtin_schema *schema =
            mylite_execution_catalog_builtin_schema_at(index);
        const char *values[information_schema_schemata_column_count] = {
            "def",
            schema->name,
            schema->default_charset,
            schema->default_collation,
            NULL,
            "NO",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_schemata_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
) {
    const char *values[information_schema_schemata_column_count] = {
        "def",
        schema->name,
        schema->default_charset,
        schema->default_collation,
        NULL,
        "NO",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_schemata_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_builtin_schema_count();
         ++index) {
        const struct mylite_execution_catalog_builtin_schema *schema =
            mylite_execution_catalog_builtin_schema_at(index);
        const char *values[information_schema_schemata_extensions_column_count] = {
            "def",
            schema->name,
            "",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_schemata_extensions_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
) {
    const char *values[information_schema_schemata_extensions_column_count] = {
        "def",
        schema->name,
        "",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_character_sets_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_character_set_count();
         ++row_index) {
        const struct mylite_execution_catalog_character_set *charset =
            mylite_execution_catalog_mysql_character_set_at(row_index);
        const char *values[information_schema_character_sets_column_count] = {
            charset->name,
            charset->default_collation,
            charset->description,
            charset->maxlen,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_collations_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *values[information_schema_collations_column_count] = {
            collation->name,
            collation->character_set,
            collation->id,
            collation->is_default,
            collation->compiled,
            collation->sortlen,
            collation->pad_attribute,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_collation_applicability_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *values[information_schema_collation_applicability_column_count] = {
            collation->name,
            collation->character_set,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_engines_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_engines_column_count] = {
        "InnoDB",
        "DEFAULT",
        "Supports transactions, row-level locking, and foreign keys",
        "YES",
        "YES",
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_plugins_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_plugins_column_count] = {
        "InnoDB",
        "8.4",
        "ACTIVE",
        "STORAGE ENGINE",
        "80409.0",
        NULL,
        NULL,
        "Oracle Corporation",
        "Supports transactions, row-level locking, and foreign keys",
        "GPL",
        "FORCE",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_keywords_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_keyword_count();
         ++row_index) {
        const struct mylite_execution_catalog_keyword *keyword =
            mylite_execution_catalog_keyword_at(row_index);
        const char *values[information_schema_keywords_column_count] = {
            keyword->word,
            keyword->reserved,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_user_privileges_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(embedded_root_global_privileges) /
                                            sizeof(embedded_root_global_privileges[0]);
         ++row_index) {
        const char *values[information_schema_user_privileges_column_count] = {
            "'root'@'%'",
            "def",
            embedded_root_global_privileges[row_index],
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_user_attributes_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_user_attributes_column_count] = {
        "root",
        "%",
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_resource_groups_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char vcpu_ids[integer_text_capacity];
    const char *user_values[information_schema_resource_groups_column_count] = {
        "USR_default",
        "USER",
        "1",
        vcpu_ids,
        "0",
    };
    const char *system_values[information_schema_resource_groups_column_count] = {
        "SYS_default",
        "SYSTEM",
        "1",
        vcpu_ids,
        "0",
    };
    int rc =
        format_information_schema_resource_group_vcpu_ids(database, vcpu_ids, sizeof(vcpu_ids));

    if (rc == MYLITE_OK) {
        rc = append_information_schema_row(database, rows, user_values);
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_row(database, rows, system_values);
    }
    return rc;
}

static int append_information_schema_st_units_of_measure_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_st_unit_of_measure_row_count();
         ++row_index) {
        const struct mylite_execution_catalog_st_unit_of_measure_row *unit =
            mylite_execution_catalog_st_unit_of_measure_row_at(row_index);
        const char *values[information_schema_st_units_of_measure_column_count] = {
            unit->unit_name,
            "LINEAR",
            unit->conversion_factor,
            "",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int format_information_schema_resource_group_vcpu_ids(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    long cpu_count = information_schema_online_processor_count();
    int written = 0;

    if (cpu_count <= 1) {
        written = snprintf(buffer, buffer_size, "0");
    } else {
        written = snprintf(buffer, buffer_size, "0-%ld", cpu_count - 1);
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format resource group vcpu ids");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static long information_schema_online_processor_count(void) {
#ifdef _WIN32
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors > 0U) {
        return (long)info.dwNumberOfProcessors;
    }
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    if (count > 0) {
        return count;
    }
#endif
    return 1;
}

static int append_information_schema_processlist_system_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
) {
    char connection_id_text[integer_text_capacity];
    char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char *info = NULL;
    const char *selected_schema = NULL;
    const char *values[information_schema_processlist_column_count] = {
        connection_id_text,
        user,
        host,
        NULL,
        "Query",
        "0",
        "executing",
        NULL,
    };
    int written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        database->session.connection_id
    );
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA.PROCESSLIST ID");
        return MYLITE_ERROR;
    }
    if (database->session.has_selected_schema) {
        selected_schema = database->session.selected_schema;
    }
    values[information_schema_processlist_db_column] = selected_schema;

    rc = format_show_processlist_user_host(database, user, sizeof(user), host, sizeof(host));
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_processlist_info(database, context, &info);
    }
    if (rc == MYLITE_OK) {
        values[information_schema_processlist_info_column] = info;
        rc = append_information_schema_row(database, rows, values);
    }

    free(info);
    return rc;
}

static int copy_information_schema_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    char **out_info
) {
    const char *sql = NULL;
    size_t sql_size = 0U;
    size_t length = 0U;
    char *info = NULL;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;
    if (context == NULL) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    sql = mylite_statement_context_sql(context);
    sql_size = mylite_statement_context_sql_size(context);
    if (sql == NULL) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    length = statement_info_length_without_terminator(sql, sql_size);
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    info = malloc(length + 1U);
    if (info == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length > 0U) {
        memcpy(info, sql, length);
    }
    info[length] = '\0';
    *out_info = info;
    return MYLITE_OK;
}

static int append_information_schema_partitions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        const struct mylite_execution_catalog_table_definition *definition =
            mylite_execution_catalog_information_schema_table_definition_at(table_index);
        const char *values[information_schema_partitions_column_count] = {
            "def",
            "information_schema",
            definition->name,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            "0",
            "0",
            "0",
            "0",
            "0",
            "0",
            NULL,
            NULL,
            NULL,
            NULL,
            "",
            "",
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_tables_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++table_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_at(table_index);

        for (size_t name_index = 0U; rc == MYLITE_OK && name_index < directory->table_count;
             ++name_index) {
            rc = append_information_schema_builtin_table_row(
                database,
                rows,
                directory,
                directory->table_names[name_index]
            );
        }
    }

    return rc;
}

static int append_information_schema_tables_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++table_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_at(table_index);

        for (size_t name_index = 0U; rc == MYLITE_OK && name_index < directory->table_count;
             ++name_index) {
            rc = append_information_schema_builtin_table_extension_row(
                database,
                rows,
                directory,
                directory->table_names[name_index]
            );
        }
    }

    return rc;
}

static int append_information_schema_builtin_table_extension_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *values[information_schema_tables_extensions_column_count] = {
        "def",
        directory->schema_name,
        table_name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tables_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    const char *values[information_schema_tables_extensions_column_count] = {
        "def",
        schema->name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tablespaces_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_tablespaces_extensions_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK &&
         index < mylite_execution_catalog_builtin_tablespace_extension_name_count();
         ++index) {
        const char *values[information_schema_tablespaces_extensions_column_count] = {
            mylite_execution_catalog_builtin_tablespace_extension_name_at(index),
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_tablespaces_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char *tablespace_name = NULL;
    const char *values[information_schema_tablespaces_extensions_column_count] = {
        NULL,
        NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_tablespaces_extensions_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS columns");
        return MYLITE_ERROR;
    }
    rc = copy_information_schema_tablespace_name(database, schema, table, &tablespace_name);
    if (rc == MYLITE_OK) {
        values[0] = tablespace_name;
        rc = append_information_schema_row(database, rows, values);
    }

    free(tablespace_name);
    return rc;
}

static int append_information_schema_files_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_files_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.FILES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_information_schema_files_row_count();
         ++index) {
        const struct mylite_execution_catalog_files_row *file =
            mylite_execution_catalog_information_schema_files_row_at(index);
        const char *values[information_schema_files_column_count] = {
            file->file_id,
            file->file_name,
            file->file_type,
            file->tablespace_name,
            "",
            NULL,
            NULL,
            NULL,
            NULL,
            "InnoDB",
            NULL,
            NULL,
            NULL,
            file->free_extents,
            file->total_extents,
            "1048576",
            file->initial_size,
            NULL,
            file->autoextend_size,
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
            NULL,
            file->data_free,
            NULL,
            NULL,
            NULL,
            NULL,
            "NORMAL",
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_datafiles_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_datafiles_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_DATAFILES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_row_at(index);
        const char *values[information_schema_innodb_datafiles_column_count] = {
            tablespace->space,
            tablespace->path,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tablespaces_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_full_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_full_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_full_row_at(index);
        const char *values[information_schema_innodb_tablespaces_column_count] = {
            tablespace->space,
            tablespace->name,
            tablespace->flag,
            tablespace->row_format,
            tablespace->page_size,
            tablespace->zip_page_size,
            tablespace->space_type,
            tablespace->fs_block_size,
            tablespace->file_size,
            tablespace->allocated_size,
            tablespace->autoextend_size,
            tablespace->server_version,
            tablespace->space_version,
            tablespace->encryption,
            tablespace->state,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_session_temp_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char connection_id_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_session_temp_tablespaces_column_count) {
        set_runtime_error(
            database,
            "invalid INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES columns"
        );
        return MYLITE_ERROR;
    }
    rc = format_uint64(
        database,
        database->session.connection_id,
        connection_id_text,
        sizeof(connection_id_text)
    );
    for (size_t index = 0U;
         rc == MYLITE_OK &&
         index < mylite_execution_catalog_innodb_session_temp_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_session_temp_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_session_temp_tablespace_row_at(index);
        const char *values[information_schema_innodb_session_temp_tablespaces_column_count] = {
            strcmp(tablespace->state, "ACTIVE") == 0 ? connection_id_text : "0",
            tablespace->space,
            tablespace->path,
            "81920",
            tablespace->state,
            tablespace->purpose,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_cmp_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_cmp_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_CMP columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_compressed_page_size_count();
         ++index) {
        const char *values[information_schema_innodb_cmp_column_count] = {
            mylite_execution_catalog_innodb_compressed_page_size_at(index),
            "0",
            "0",
            "0",
            "0",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_buffer_pool_stats_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_innodb_buffer_pool_stats_column_count] = {
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
    };

    if (rows->definition->column_count !=
        information_schema_innodb_buffer_pool_stats_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS columns");
        return MYLITE_ERROR;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_innodb_cmpmem_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_cmpmem_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_CMPMEM columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_compressed_page_size_count();
         ++index) {
        const char *values[information_schema_innodb_cmpmem_column_count] = {
            mylite_execution_catalog_innodb_compressed_page_size_at(index),
            "0",
            "0",
            "0",
            "0",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_ft_default_stopword_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_ft_default_stopword_column_count) {
        set_runtime_error(
            database,
            "invalid INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD columns"
        );
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_ft_default_stopword_count();
         ++index) {
        const char *values[information_schema_innodb_ft_default_stopword_column_count] = {
            mylite_execution_catalog_innodb_ft_default_stopword_at(index),
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_tablespaces_brief_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_tablespaces_brief_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_row_at(index);
        const char *values[information_schema_innodb_tablespaces_brief_column_count] = {
            tablespace->space,
            tablespace->name,
            tablespace->path,
            tablespace->flag,
            tablespace->space_type,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int copy_information_schema_tablespace_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
) {
    size_t schema_length = strlen(schema->name);
    size_t table_length = strlen(table->name);
    size_t required_length = 0U;
    char *name = NULL;

    *out_name = NULL;
    if (schema_length > SIZE_MAX - table_length - tablespace_name_separator_size -
                            tablespace_name_terminator_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    required_length = schema_length + table_length + tablespace_name_separator_size +
                      tablespace_name_terminator_size;
    name = (char *)malloc(required_length);
    if (name == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(name, schema->name, schema_length);
    name[schema_length] = '/';
    memcpy(&name[schema_length + tablespace_name_separator_size], table->name, table_length);
    name[required_length - tablespace_name_terminator_size] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int append_information_schema_builtin_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    struct table_status_values status = {0};
    const char *table_type = builtin_schema_table_type(directory, table_name);
    int rc = load_builtin_table_status_values(database, directory, table_name, &status);

    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *values[information_schema_tables_column_count] = {
        "def",
        directory->schema_name,
        table_name,
        table_type,
        builtin_schema_table_engine(directory, table_name),
        builtin_schema_table_version(directory, table_name),
        builtin_schema_table_row_format(directory, table_name),
        builtin_schema_table_rows(directory, table_name),
        builtin_schema_table_average_row_length(directory, table_name),
        builtin_schema_table_data_length(directory, table_name),
        table_type == NULL || strcmp(table_type, "VIEW") == 0 ? NULL : "0",
        builtin_schema_table_index_length(directory, table_name),
        builtin_schema_table_data_free(directory, table_name),
        builtin_schema_table_auto_increment(directory, table_name),
        status.create_time,
        status.update_time,
        NULL,
        builtin_schema_table_collation(directory, table_name),
        NULL,
        builtin_schema_table_create_options(directory, table_name),
        builtin_schema_table_comment(directory, table_name),
    };

    return append_information_schema_row(database, rows, values);
}

static int load_builtin_table_status_values(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct table_status_values *status
) {
    int rc = format_builtin_table_status_timestamp(database, status);

    if (rc == MYLITE_OK && builtin_schema_table_has_update_time(directory, table_name)) {
        memcpy(
            status->update_time_text,
            status->create_time_text,
            sizeof(status->update_time_text)
        );
        status->update_time = status->update_time_text;
    }
    return rc;
}

static const struct mylite_execution_catalog_builtin_schema_table_directory *find_builtin_schema_table_directory(
    const char *schema_name
) {
    return mylite_execution_catalog_builtin_schema_table_directory_by_name(schema_name);
}

static const char *builtin_schema_table_type(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return "SYSTEM VIEW";
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") != 0) {
        return "VIEW";
    }
    return "BASE TABLE";
}

static const char *builtin_schema_table_engine(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return strcmp(table_name, "sys_config") == 0 ? "InnoDB" : NULL;
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        return "PERFORMANCE_SCHEMA";
    }
    if (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0) {
        return "CSV";
    }
    return "InnoDB";
}

static const char *builtin_schema_table_version(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    return "10";
}

static const char *builtin_schema_table_row_format(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return strcmp(table_name, "sys_config") == 0 ? "Dynamic" : NULL;
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        if (strcmp(table_name, "accounts") == 0 ||
            strcmp(table_name, "events_statements_histogram_global") == 0 ||
            strcmp(table_name, "hosts") == 0 || strcmp(table_name, "performance_timers") == 0 ||
            strcmp(table_name, "replication_applier_status") == 0 ||
            strcmp(table_name, "replication_asynchronous_connection_failover") == 0 ||
            strcmp(table_name, "replication_group_members") == 0 ||
            strcmp(table_name, "setup_actors") == 0 || strcmp(table_name, "users") == 0) {
            return "Fixed";
        }
    }
    return "Dynamic";
}

static const char *builtin_schema_table_rows(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        const char *rows = builtin_mysql_table_rows(table_name);

        if (rows != NULL) {
            return rows;
        }
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 &&
        strcmp(table_name, "setup_actors") == 0) {
        return "128";
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") == 0) {
        return "6";
    }
    return "0";
}

static const char *builtin_mysql_table_rows(const char *table_name) {
    static const struct mysql_table_row_count {
        const char *table_name;
        const char *rows;
    } row_counts[] = {
        {"db", "2"},
        {"engine_cost", "2"},
        {"general_log", "2"},
        {"global_grants", "103"},
        {"help_category", "53"},
        {"help_keyword", "1142"},
        {"help_relation", "1608"},
        {"help_topic", "902"},
        {"innodb_index_stats", "6"},
        {"innodb_table_stats", "2"},
        {"plugin", "2"},
        {"proxies_priv", "1"},
        {"server_cost", "6"},
        {"slow_log", "2"},
        {"tables_priv", "2"},
        {"time_zone", "1457"},
        {"time_zone_leap_second", "0"},
        {"time_zone_name", "1712"},
        {"time_zone_transition", "119074"},
        {"time_zone_transition_type", "10529"},
        {"user", "5"},
    };

    for (size_t index = 0U; index < sizeof(row_counts) / sizeof(row_counts[0]); ++index) {
        if (strcmp(table_name, row_counts[index].table_name) == 0) {
            return row_counts[index].rows;
        }
    }
    return NULL;
}

static const char *builtin_schema_table_average_row_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (builtin_schema_table_is_mysql_stats(directory, table_name)) {
        return strcmp(table_name, "innodb_index_stats") == 0 ? "2730" : "8192";
    }
    if (builtin_schema_table_is_mysql_cost(directory, table_name)) {
        return strcmp(table_name, "server_cost") == 0 ? "2730" : "8192";
    }
    if (builtin_schema_table_is_mysql_plugin(directory, table_name)) {
        return "8192";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 &&
        (strcmp(table_name, "db") == 0 || strcmp(table_name, "tables_priv") == 0)) {
        return "8192";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "proxies_priv") == 0) {
        return "16384";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "global_grants") == 0) {
        return "795";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "user") == 0) {
        return "3276";
    }
    if (builtin_schema_table_is_mysql_help(directory, table_name)) {
        return builtin_schema_mysql_help_average_row_length(table_name);
    }
    if (builtin_schema_table_is_mysql_time_zone(directory, table_name)) {
        return builtin_schema_mysql_time_zone_average_row_length(table_name);
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") == 0) {
        return "2730";
    }
    return "0";
}

static const char *builtin_schema_mysql_help_average_row_length(const char *table_name) {
    if (strcmp(table_name, "help_category") == 0) {
        return "309";
    }
    if (strcmp(table_name, "help_keyword") == 0) {
        return "114";
    }
    if (strcmp(table_name, "help_relation") == 0) {
        return "50";
    }
    return "1761";
}

static const char *builtin_schema_mysql_time_zone_average_row_length(const char *table_name) {
    if (strcmp(table_name, "time_zone") == 0) {
        return "56";
    }
    if (strcmp(table_name, "time_zone_name") == 0) {
        return "153";
    }
    if (strcmp(table_name, "time_zone_transition") == 0) {
        return "39";
    }
    if (strcmp(table_name, "time_zone_transition_type") == 0) {
        return "45";
    }
    return "0";
}

static const char *builtin_schema_table_data_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0 &&
        (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0)) {
        return "0";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "global_grants") == 0) {
        return "81920";
    }
    if (builtin_schema_table_is_mysql_time_zone(directory, table_name)) {
        if (strcmp(table_name, "time_zone") == 0) {
            return "81920";
        }
        if (strcmp(table_name, "time_zone_name") == 0) {
            return "262144";
        }
        if (strcmp(table_name, "time_zone_transition") == 0) {
            return "4734976";
        }
        if (strcmp(table_name, "time_zone_transition_type") == 0) {
            return "475136";
        }
    }
    if (builtin_schema_table_is_mysql_help(directory, table_name)) {
        if (strcmp(table_name, "help_category") == 0) {
            return "16384";
        }
        if (strcmp(table_name, "help_keyword") == 0) {
            return "131072";
        }
        if (strcmp(table_name, "help_relation") == 0) {
            return "81920";
        }
        return "1589248";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 ||
        strcmp(directory->schema_name, "information_schema") == 0) {
        return "0";
    }
    return "16384";
}

static const char *builtin_schema_table_index_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (!builtin_schema_table_is_mysql_help(directory, table_name)) {
        return strcmp(directory->schema_name, "mysql") == 0 &&
                       (strcmp(table_name, "db") == 0 || strcmp(table_name, "procs_priv") == 0 ||
                        strcmp(table_name, "proxies_priv") == 0 ||
                        strcmp(table_name, "tables_priv") == 0)
                   ? "16384"
                   : "0";
    }
    if (strcmp(table_name, "help_category") == 0) {
        return "16384";
    }
    if (strcmp(table_name, "help_keyword") == 0) {
        return "147456";
    }
    if (strcmp(table_name, "help_topic") == 0) {
        return "98304";
    }
    return "0";
}

static const char *builtin_schema_table_data_free(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    return builtin_schema_table_is_mysql_stats(directory, table_name) ||
                   builtin_schema_table_is_mysql_cost(directory, table_name) ||
                   builtin_schema_table_is_mysql_component(directory, table_name) ||
                   builtin_schema_table_is_mysql_func(directory, table_name) ||
                   builtin_schema_table_is_mysql_gtid_executed(directory, table_name) ||
                   builtin_schema_table_is_mysql_help(directory, table_name) ||
                   builtin_schema_table_is_mysql_ndb_binlog_index(directory, table_name) ||
                   builtin_schema_table_is_mysql_plugin(directory, table_name) ||
                   builtin_schema_table_is_mysql_replication_metadata(directory, table_name) ||
                   builtin_schema_table_is_mysql_servers(directory, table_name) ||
                   builtin_schema_table_is_mysql_time_zone(directory, table_name) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "columns_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "default_roles") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "password_history") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "role_edges") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "procs_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "proxies_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    (strcmp(table_name, "db") == 0 || strcmp(table_name, "tables_priv") == 0)) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "global_grants") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "user") == 0)
               ? "4194304"
               : "0";
}

static const char *builtin_schema_table_auto_increment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (builtin_schema_table_is_mysql_component(directory, table_name)) {
        return "1";
    }
    return builtin_schema_table_is_mysql_time_zone(directory, table_name) &&
                   strcmp(table_name, "time_zone") == 0
               ? "1796"
               : NULL;
}

static const char *builtin_schema_table_collation(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0 ||
        strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        if (strcmp(table_name, "keyring_keys") == 0 ||
            strcmp(table_name, "session_account_connect_attrs") == 0 ||
            strcmp(table_name, "session_connect_attrs") == 0) {
            return "utf8mb4_bin";
        }
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(table_name, "ndb_binlog_index") == 0) {
        return "latin1_swedish_ci";
    }
    if (strcmp(table_name, "gtid_executed") == 0 ||
        strcmp(table_name, "replication_group_configuration_version") == 0 ||
        strcmp(table_name, "replication_group_member_actions") == 0) {
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(table_name, "columns_priv") == 0 || strcmp(table_name, "db") == 0 ||
        strcmp(table_name, "default_roles") == 0 || strcmp(table_name, "func") == 0 ||
        strcmp(table_name, "global_grants") == 0 || strcmp(table_name, "innodb_index_stats") == 0 ||
        strcmp(table_name, "innodb_table_stats") == 0 ||
        strcmp(table_name, "password_history") == 0 || strcmp(table_name, "procs_priv") == 0 ||
        strcmp(table_name, "proxies_priv") == 0 || strcmp(table_name, "role_edges") == 0 ||
        strcmp(table_name, "tables_priv") == 0 || strcmp(table_name, "user") == 0) {
        return "utf8mb3_bin";
    }
    return "utf8mb3_general_ci";
}

static const char *builtin_schema_table_create_options(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        if (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0) {
            return "";
        }
        if (strcmp(table_name, "component") == 0) {
            return "row_format=DYNAMIC";
        }
        return "row_format=DYNAMIC stats_persistent=0";
    }
    return strcmp(builtin_schema_table_type(directory, table_name), "VIEW") == 0 ? NULL : "";
}

static const char *builtin_schema_table_comment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(builtin_schema_table_type(directory, table_name), "VIEW") == 0) {
        return "VIEW";
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        const char *comment = builtin_mysql_table_comment(table_name);

        if (comment != NULL) {
            return comment;
        }
    }
    return "";
}

static const char *builtin_mysql_table_comment(const char *table_name) {
    static const struct mysql_table_comment {
        const char *table_name;
        const char *comment;
    } comments[] = {
        {"columns_priv", "Column privileges"},
        {"component", "Components"},
        {"db", "Database privileges"},
        {"default_roles", "Default roles"},
        {"func", "User defined functions"},
        {"general_log", "General log"},
        {"global_grants", "Extended global grants"},
        {"help_category", "help categories"},
        {"help_keyword", "help keywords"},
        {"help_relation", "keyword-topic relation"},
        {"help_topic", "help topics"},
        {"password_history", "Password history for user accounts"},
        {"plugin", "MySQL plugins"},
        {"procs_priv", "Procedure privileges"},
        {"proxies_priv", "User proxy privileges"},
        {"role_edges", "Role hierarchy and role grants"},
        {"servers", "MySQL Foreign Servers table"},
        {"slave_master_info", "Master Information"},
        {"slave_relay_log_info", "Relay Log Information"},
        {"slave_worker_info", "Worker Information"},
        {"slow_log", "Slow log"},
        {"tables_priv", "Table privileges"},
        {"time_zone", "Time zones"},
        {"time_zone_leap_second", "Leap seconds information for time zones"},
        {"time_zone_name", "Time zone names"},
        {"time_zone_transition", "Time zone transitions"},
        {"time_zone_transition_type", "Time zone transition types"},
        {"user", "Users and global privileges"},
    };

    for (size_t index = 0U; index < sizeof(comments) / sizeof(comments[0]); ++index) {
        if (strcmp(table_name, comments[index].table_name) == 0) {
            return comments[index].comment;
        }
    }
    return NULL;
}

static bool builtin_schema_table_has_update_time(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return builtin_schema_table_is_mysql_stats(directory, table_name) ||
           builtin_schema_table_is_mysql_plugin(directory, table_name);
}

static bool builtin_schema_table_is_mysql_stats(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "innodb_index_stats") == 0 ||
            strcmp(table_name, "innodb_table_stats") == 0);
}

static bool builtin_schema_table_is_mysql_cost(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "engine_cost") == 0 || strcmp(table_name, "server_cost") == 0);
}

static bool builtin_schema_table_is_mysql_component(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "component") == 0;
}

static bool builtin_schema_table_is_mysql_func(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "func") == 0;
}

static bool builtin_schema_table_is_mysql_gtid_executed(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "gtid_executed") == 0;
}

static bool builtin_schema_table_is_mysql_help(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "help_category") == 0 || strcmp(table_name, "help_keyword") == 0 ||
            strcmp(table_name, "help_relation") == 0 || strcmp(table_name, "help_topic") == 0);
}

static bool builtin_schema_table_is_mysql_ndb_binlog_index(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           strcmp(table_name, "ndb_binlog_index") == 0;
}

static bool builtin_schema_table_is_mysql_replication_metadata(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "slave_master_info") == 0 ||
            strcmp(table_name, "slave_relay_log_info") == 0 ||
            strcmp(table_name, "slave_worker_info") == 0);
}

static bool builtin_schema_table_is_mysql_plugin(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "plugin") == 0;
}

static bool builtin_schema_table_is_mysql_servers(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "servers") == 0;
}

static bool builtin_schema_table_is_mysql_time_zone(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "time_zone") == 0 ||
            strcmp(table_name, "time_zone_leap_second") == 0 ||
            strcmp(table_name, "time_zone_name") == 0 ||
            strcmp(table_name, "time_zone_transition") == 0 ||
            strcmp(table_name, "time_zone_transition_type") == 0);
}

static int append_information_schema_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = load_table_status_values(database, table, &status);

    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *values[information_schema_tables_column_count] = {
        "def",
        schema->name,
        table->name,
        "BASE TABLE",
        "InnoDB",
        "10",
        status.row_format,
        status.row_count_text,
        status.average_row_length_text,
        "16384",
        "0",
        status.index_length,
        "0",
        status.auto_increment,
        status.create_time,
        status.update_time,
        NULL,
        table->default_collation,
        NULL,
        status.create_options,
        table->comment,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tables_view_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = format_table_status_timestamp(
        database,
        table->created_time_utc_epoch,
        status.create_time_text,
        sizeof(status.create_time_text),
        &status.create_time
    );
    const char *values[information_schema_tables_column_count] = {
        "def",  schema->name, table->name, "VIEW", NULL,
        NULL,   NULL,         NULL,        NULL,   NULL,
        NULL,   NULL,         NULL,        NULL,   status.create_time,
        NULL,   NULL,         NULL,        NULL,   NULL,
        "VIEW",
    };

    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_partitions_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = load_table_status_values(database, table, &status);
    const char *values[information_schema_partitions_column_count] = {
        "def",
        schema->name,
        table->name,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        status.row_count_text,
        status.average_row_length_text,
        "16384",
        "0",
        status.index_length,
        "0",
        status.create_time,
        status.update_time,
        NULL,
        NULL,
        "",
        "",
        NULL,
    };

    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_information_schema_row(database, rows, values);
}

static int load_table_status_values(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    int64_t row_count = 0;
    int64_t average_row_length = 0;
    int64_t index_length = 0;
    bool has_auto_increment = false;
    int rc = MYLITE_OK;

    if (out_values == NULL) {
        return MYLITE_MISUSE;
    }
    *out_values = (struct table_status_values){0};

    rc = read_show_table_status_row_count(database, table, &row_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read table status row count");
        return MYLITE_ERROR;
    }
    if (row_count > 0) {
        average_row_length = show_table_status_data_length / row_count;
    }
    rc = format_show_table_status_integer(
        database,
        row_count,
        out_values->row_count_text,
        sizeof(out_values->row_count_text)
    );
    if (rc == MYLITE_OK) {
        rc = format_show_table_status_integer(
            database,
            average_row_length,
            out_values->average_row_length_text,
            sizeof(out_values->average_row_length_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = table_status_index_length(database, table, &index_length);
    }
    if (rc == MYLITE_OK) {
        rc = format_show_table_status_integer(
            database,
            index_length,
            out_values->index_length_text,
            sizeof(out_values->index_length_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_values->index_length = out_values->index_length_text;
        rc = table_status_has_auto_increment(database, table, &has_auto_increment);
    }
    if (rc == MYLITE_OK && has_auto_increment) {
        rc = format_show_table_status_integer(
            database,
            table->auto_increment_next,
            out_values->auto_increment_text,
            sizeof(out_values->auto_increment_text)
        );
        if (rc == MYLITE_OK) {
            out_values->auto_increment = out_values->auto_increment_text;
        }
    }
    if (rc == MYLITE_OK) {
        rc = format_table_status_timestamp(
            database,
            table->created_time_utc_epoch,
            out_values->create_time_text,
            sizeof(out_values->create_time_text),
            &out_values->create_time
        );
    }
    if (rc == MYLITE_OK) {
        rc = format_table_status_timestamp(
            database,
            table->updated_time_utc_epoch,
            out_values->update_time_text,
            sizeof(out_values->update_time_text),
            &out_values->update_time
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_status_row_format(database, table, out_values);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_status_create_options(database, table, out_values);
    }

    return rc;
}

static int load_table_status_row_format(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    const char *row_format = "Dynamic";

    if (table == NULL || out_values == NULL) {
        return MYLITE_MISUSE;
    }
    if (table_status_row_format_is_compressed(table)) {
        row_format = "Compressed";
    } else if (text_equals_ascii_case_insensitive(table->row_format_option, "COMPACT")) {
        row_format = "Compact";
    } else if (text_equals_ascii_case_insensitive(table->row_format_option, "REDUNDANT")) {
        row_format = "Redundant";
    } else if (
        table->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(table->row_format_option, "DYNAMIC")
    ) {
        set_runtime_error(database, "invalid table row format descriptor");
        return MYLITE_ERROR;
    }
    snprintf(out_values->row_format_text, sizeof(out_values->row_format_text), "%s", row_format);
    out_values->row_format = out_values->row_format_text;
    return MYLITE_OK;
}

static bool table_status_row_format_is_compressed(
    const struct mylite_catalog_table_descriptor *table
) {
    if (table == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(table->row_format_option, "COMPRESSED")) {
        return true;
    }
    if (table->row_format_option[0] != '\0') {
        return false;
    }
    if (table->key_block_size > 0) {
        return true;
    }
    return false;
}

static int load_table_status_create_options(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    bool has_option = false;
    int rc = MYLITE_OK;

    if (table == NULL || out_values == NULL) {
        return MYLITE_MISUSE;
    }
    out_values->create_options_text[0] = '\0';
    out_values->create_options = out_values->create_options_text;

    if (table->row_format_option[0] != '\0') {
        char text[sizeof("row_format=") + MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int written = snprintf(text, sizeof(text), "row_format=%s", table->row_format_option);

        if (written < 0 || (size_t)written >= sizeof(text)) {
            set_runtime_error(database, "failed to format table create option");
            return MYLITE_ERROR;
        }
        rc = append_table_status_create_option(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            text
        );
    }
    if (rc == MYLITE_OK && table->stats_sample_pages > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_sample_pages=",
            table->stats_sample_pages
        );
    }
    if (rc == MYLITE_OK && table->stats_auto_recalc != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_auto_recalc=",
            table->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK && table->key_block_size > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "KEY_BLOCK_SIZE=",
            table->key_block_size
        );
    }
    if (rc == MYLITE_OK && table->stats_persistent != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_persistent=",
            table->stats_persistent
        );
    }
    if (rc == MYLITE_OK && table->pack_keys != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "pack_keys=",
            table->pack_keys
        );
    }
    if (rc == MYLITE_OK && table->checksum != 0) {
        rc = append_table_status_create_option(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "checksum=1"
        );
    }
    return rc;
}

static int append_table_status_create_option(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *text
) {
    size_t current_length = 0U;
    size_t separator_length = 0U;
    size_t text_length = 0U;

    if (buffer == NULL || buffer_size == 0U || has_option == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    current_length = strlen(buffer);
    text_length = strlen(text);
    if (*has_option) {
        separator_length = 1U;
    }
    if (current_length + separator_length + text_length >= buffer_size) {
        set_runtime_error(database, "failed to format table create options");
        return MYLITE_ERROR;
    }
    if (*has_option) {
        buffer[current_length] = ' ';
        ++current_length;
    }
    memcpy(buffer + current_length, text, text_length + 1U);
    *has_option = true;
    return MYLITE_OK;
}

static int append_table_status_create_option_i64(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *prefix,
    int64_t value
) {
    char text[integer_text_capacity + sizeof("stats_sample_pages=")];
    int written = snprintf(text, sizeof(text), "%s%" PRId64, prefix, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        set_runtime_error(database, "failed to format table create option");
        return MYLITE_ERROR;
    }
    return append_table_status_create_option(database, buffer, buffer_size, has_option, text);
}

static int table_status_has_auto_increment(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_has_auto_increment
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (out_has_auto_increment == NULL) {
        return MYLITE_MISUSE;
    }
    *out_has_auto_increment = false;

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (column_descriptor_is_auto_increment(&columns[column_index])) {
            *out_has_auto_increment = true;
            break;
        }
    }
    free(columns);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read table status columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int table_status_index_length(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_index_length
) {
    struct nonprimary_index_presence_context context = {.has_nonprimary_index = false};
    int rc = MYLITE_OK;

    if (out_index_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index_length = 0;
    rc = mylite_catalog_for_each_index_in_table(
        database,
        table->table_id,
        note_nonprimary_index_presence,
        &context
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read table status index descriptors");
        return MYLITE_ERROR;
    }
    if (context.has_nonprimary_index) {
        *out_index_length = show_table_status_data_length;
    }
    return MYLITE_OK;
}

static int note_nonprimary_index_presence(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct nonprimary_index_presence_context *context = user_data;

    if (context == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        context->has_nonprimary_index = true;
    }

    return MYLITE_OK;
}

static int format_table_status_timestamp(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size,
    const char **out_timestamp
) {
    if (out_timestamp == NULL) {
        return MYLITE_MISUSE;
    }
    *out_timestamp = NULL;
    if (epoch <= 0) {
        return MYLITE_OK;
    }
    int rc = format_session_timestamp_epoch_text(database, epoch, buffer, buffer_size);

    if (rc == MYLITE_OK) {
        *out_timestamp = buffer;
    }
    return rc;
}

static int append_information_schema_catalog_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct information_schema_catalog_filter *filter
) {
    struct information_schema_catalog_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
        .filter = filter,
    };

    if (!information_schema_catalog_filter_allows_user_catalog_rows(rows->definition, filter)) {
        return MYLITE_OK;
    }

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
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_GLOBAL_STATUS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_GLOBAL_VARIABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_PERFORMANCE_TIMERS:
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
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAIT_CLASSES_GLOBAL_BY_AVG_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAIT_CLASSES_GLOBAL_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_BY_HOST_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_BY_USER_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_GLOBAL_BY_LATENCY:
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
    if (!information_schema_catalog_filter_allows_schema(
            context->rows->definition,
            context->filter,
            schema->name
        )) {
        return MYLITE_OK;
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
    if (!information_schema_catalog_filter_allows_table(
            context->rows->definition,
            context->filter,
            table->name
        )) {
        return MYLITE_OK;
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

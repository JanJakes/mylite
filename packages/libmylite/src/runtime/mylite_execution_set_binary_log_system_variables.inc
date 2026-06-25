static const uint64_t binary_log_cache_size_default_value = 32768ULL;
static const uint64_t binary_log_expire_logs_seconds_default_value = 2592000ULL;
static const uint64_t binary_log_row_event_max_size_default_value = 8192ULL;
static const uint64_t binary_log_transaction_dependency_history_size_default_value = 25000ULL;

static int apply_set_binary_log_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_binary_log(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (!mylite_execution_system_variable_allows_session_scope(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = validate_set_binary_log_system_variable_value(database, target, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_binary_log_set_warning_if_needed(database, target->kind);
}

static int apply_set_binary_log_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_execution_system_variable_allows_session_scope(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET binary log system variables from user variables is not supported"
    );
    return MYLITE_ERROR;
}

static int validate_set_binary_log_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    const char *default_value = NULL;
    bool expected_boolean_value = false;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    default_value = mylite_execution_system_variable_default_value(target->kind);
    if (default_value == NULL) {
        set_runtime_error(database, "invalid binary log system variable");
        return MYLITE_ERROR;
    }
    if (binary_log_system_variable_is_boolean(target->kind)) {
        expected_boolean_value = strcmp(default_value, "1") == 0;
        return validate_set_fixed_boolean_value(database, value_node, expected_boolean_value);
    }
    if (binary_log_system_variable_is_uint64(target->kind)) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            binary_log_system_variable_uint64_value(target->kind),
            "SET binary log system variables support only fixed no-op assignments"
        );
    }
    return validate_set_fixed_text_value(database, value_node, default_value);
}

static bool binary_log_system_variable_is_boolean(enum mylite_execution_system_variable_kind kind) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_DIRECT_NON_TRANSACTIONAL_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROWS_QUERY_LOG_EVENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION:
        return true;
    default:
        return false;
    }
}

static bool binary_log_system_variable_is_uint64(enum mylite_execution_system_variable_kind kind) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION_LEVEL_ZSTD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE:
        return true;
    default:
        return false;
    }
}

static uint64_t binary_log_system_variable_uint64_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE:
        return binary_log_cache_size_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS:
        return binary_log_expire_logs_seconds_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME:
        return 0ULL;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
        return binary_log_row_event_max_size_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION_LEVEL_ZSTD:
        return 3ULL;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE:
        return binary_log_transaction_dependency_history_size_default_value;
    default:
        return 0ULL;
    }
}

static int append_binary_log_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT ||
        kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME) {
        return append_system_variable_read_warning(database, kind);
    }
    return MYLITE_OK;
}

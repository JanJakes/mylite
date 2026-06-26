static int apply_set_innodb_storage_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
        *out_handled = true;
        return validate_set_innodb_storage_fixed_value(database, target, value_node);
    default:
        return MYLITE_OK;
    }
}

static int apply_set_innodb_storage_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
        *out_handled = true;
        return validate_set_innodb_storage_fixed_cell_value(
            database,
            target,
            value,
            value_kind
        );
    default:
        return MYLITE_OK;
    }
}

static int validate_set_innodb_storage_fixed_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE &&
        target->kind != MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
        return validate_set_fixed_text_value(
            database,
            value_node,
            innodb_checksum_algorithm_default_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_commit_concurrency_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_compression_failure_threshold_pct_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_compression_level_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_compression_pad_pct_max_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_concurrency_tickets_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_ddl_buffer_size_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_ddl_threads_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
        return validate_set_fixed_text_value(
            database,
            value_node,
            innodb_default_row_format_default_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
        return validate_set_fixed_text_value(database, value_node, innodb_doublewrite_default_value);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_fast_shutdown_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_fill_factor_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_flush_log_at_timeout_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_flush_log_at_trx_commit_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_flush_neighbors_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_flushing_avg_loops_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_fsync_threshold_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    default:
        return MYLITE_OK;
    }
}

static int validate_set_innodb_storage_fixed_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind != MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE &&
        target->kind != MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
        return validate_set_innodb_storage_fixed_text_cell_value(
            database,
            target,
            innodb_checksum_algorithm_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_commit_concurrency_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_compression_failure_threshold_pct_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_compression_level_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_compression_pad_pct_max_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_concurrency_tickets_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_ddl_buffer_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_ddl_threads_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
        return validate_set_innodb_storage_fixed_text_cell_value(
            database,
            target,
            innodb_default_row_format_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
        return validate_set_innodb_storage_fixed_text_cell_value(
            database,
            target,
            innodb_doublewrite_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_fast_shutdown_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_fill_factor_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_flush_log_at_timeout_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_flush_log_at_trx_commit_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_flush_neighbors_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_flushing_avg_loops_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_fsync_threshold_default_value,
            value,
            value_kind
        );
    default:
        return MYLITE_OK;
    }
}

static int validate_set_innodb_storage_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t actual = 0U;
    struct mylite_sql_source_span unsigned_span;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U || text[0] == '-' || text[0] == '+') {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, &actual) != MYLITE_OK ||
        actual != expected_value) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_set_innodb_storage_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;

    if (target == NULL || expected_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (!text_equals_ascii_case_insensitive(text, expected_value)) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

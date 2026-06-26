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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
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
    if (!innodb_storage_system_variable_allows_session_assignment(target->kind) &&
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
        return validate_set_innodb_storage_nullable_table_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return validate_set_fixed_boolean_value(database, value_node, true);
        }
        return apply_set_session_placeholder_system_variable_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_ft_num_word_optimize_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_ft_result_cache_limit_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE:
        return validate_set_innodb_storage_nullable_table_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_idle_flush_pct_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_io_capacity_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_io_capacity_max_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_log_buffer_size_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_log_spin_cpu_abs_lwm_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_log_spin_cpu_pct_hwm_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_log_wait_for_flush_spin_hwm_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_log_write_ahead_size_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_lru_scan_depth_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
        return validate_set_fixed_decimal_value(
            database,
            value_node,
            innodb_max_dirty_pages_pct_integral_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
        return validate_set_fixed_decimal_value(
            database,
            value_node,
            innodb_max_dirty_pages_pct_lwm_integral_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_max_purge_lag_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_max_undo_log_size_default_value,
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
    if (!innodb_storage_system_variable_allows_session_assignment(target->kind) &&
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
        return validate_set_innodb_storage_nullable_table_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
        }
        return apply_set_session_placeholder_system_variable_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_ft_num_word_optimize_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_ft_result_cache_limit_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE:
        return validate_set_innodb_storage_nullable_table_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_idle_flush_pct_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_io_capacity_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_io_capacity_max_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_log_buffer_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_log_spin_cpu_abs_lwm_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_log_spin_cpu_pct_hwm_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_log_wait_for_flush_spin_hwm_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_log_write_ahead_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_lru_scan_depth_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
        return validate_set_innodb_storage_fixed_decimal_cell_value(
            database,
            target,
            innodb_max_dirty_pages_pct_integral_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
        return validate_set_innodb_storage_fixed_decimal_cell_value(
            database,
            target,
            innodb_max_dirty_pages_pct_lwm_integral_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_max_purge_lag_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_max_undo_log_size_default_value,
            value,
            value_kind
        );
    default:
        return MYLITE_OK;
    }
}

static bool innodb_storage_system_variable_allows_session_assignment(
    enum mylite_execution_system_variable_kind kind
) {
    return kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE;
}

static int validate_set_innodb_storage_nullable_table_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, target->name, "");
    return MYLITE_ERROR;
}

static int validate_set_innodb_storage_nullable_table_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_NULL || text == NULL) {
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, target->name, text);
    return MYLITE_ERROR;
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

static int validate_set_innodb_storage_fixed_decimal_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_integral_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;
    struct mylite_sql_source_span value_span = {0};
    size_t text_size = 0U;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL ||
        (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER &&
         value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL)) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    value_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (text_size == 0U || text[0] == '-' || text[0] == '+' ||
        !fixed_decimal_text_matches_uint64(value_span, expected_integral_value)) {
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

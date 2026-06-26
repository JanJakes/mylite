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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
        *out_handled = true;
        return validate_set_innodb_storage_fixed_cell_value(database, target, value, value_kind);
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
        return validate_set_fixed_text_value(
            database,
            value_node,
            innodb_doublewrite_default_value
        );
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
        return validate_set_innodb_storage_monitor_switch_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_old_blocks_pct_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_old_blocks_time_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_online_alter_log_max_size_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return validate_set_fixed_uint64_value(
                database,
                value_node,
                innodb_parallel_read_threads_default_value,
                "SET supports only fixed no-op system variable assignments"
            );
        }
        return apply_set_innodb_parallel_read_threads_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_purge_batch_size_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_purge_rseg_truncate_frequency_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_read_ahead_threshold_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
        return validate_set_fixed_null_value(
            database,
            value_node,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_redo_log_capacity_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_replication_delay_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_rollback_segments_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
        return validate_set_innodb_storage_fixed_decimal_text_value(
            database,
            value_node,
            innodb_segment_reserve_factor_exact_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_spin_wait_delay_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_spin_wait_pause_multiplier_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
        return validate_set_fixed_text_value(
            database,
            value_node,
            innodb_stats_method_default_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_stats_persistent_sample_pages_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_stats_transient_sample_pages_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return validate_set_fixed_boolean_value(database, value_node, true);
        }
        return apply_set_session_placeholder_system_variable_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_sync_spin_loops_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return validate_set_fixed_boolean_value(database, value_node, true);
        }
        return apply_set_session_placeholder_system_variable_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_thread_concurrency_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_thread_sleep_delay_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return validate_set_fixed_null_value(
                database,
                value_node,
                "SET supports only fixed no-op system variable assignments"
            );
        }
        return apply_set_innodb_tmpdir_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_undo_tablespaces_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
        return validate_set_fixed_boolean_value(database, value_node, true);
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
            return apply_set_fixed_boolean_system_variable_cell_value(
                database,
                target,
                value,
                true
            );
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
        return validate_set_innodb_storage_monitor_switch_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_old_blocks_pct_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_old_blocks_time_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_online_alter_log_max_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return validate_set_innodb_storage_fixed_uint64_cell_value(
                database,
                target,
                innodb_parallel_read_threads_default_value,
                value,
                value_kind
            );
        }
        return apply_set_innodb_parallel_read_threads_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_purge_batch_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_purge_rseg_truncate_frequency_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_read_ahead_threshold_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
        return validate_set_innodb_storage_fixed_null_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_redo_log_capacity_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_replication_delay_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_rollback_segments_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
        return validate_set_innodb_storage_fixed_decimal_text_cell_value(
            database,
            target,
            innodb_segment_reserve_factor_exact_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_spin_wait_delay_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_spin_wait_pause_multiplier_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
        return validate_set_innodb_storage_fixed_text_cell_value(
            database,
            target,
            innodb_stats_method_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_stats_persistent_sample_pages_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_stats_transient_sample_pages_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return apply_set_fixed_boolean_system_variable_cell_value(
                database,
                target,
                value,
                true
            );
        }
        return apply_set_session_placeholder_system_variable_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_sync_spin_loops_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return apply_set_fixed_boolean_system_variable_cell_value(
                database,
                target,
                value,
                true
            );
        }
        return apply_set_session_placeholder_system_variable_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_thread_concurrency_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_thread_sleep_delay_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR:
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            return validate_set_innodb_storage_fixed_null_cell_value(
                database,
                target,
                value,
                value_kind
            );
        }
        return apply_set_innodb_tmpdir_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
        return validate_set_innodb_storage_fixed_uint64_cell_value(
            database,
            target,
            innodb_undo_tablespaces_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
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
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR;
}

static int apply_set_innodb_parallel_read_threads_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = innodb_parallel_read_threads_default_value;
    int rc = MYLITE_OK;

    if (database == NULL || target == NULL) {
        return MYLITE_MISUSE;
    }
    rc = parse_set_innodb_parallel_read_threads_value(database, target, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }

    database->session.innodb_parallel_read_threads = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_innodb_parallel_read_threads_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t integer_value = innodb_parallel_read_threads_default_value;
    int rc = MYLITE_OK;

    if (database == NULL || target == NULL) {
        return MYLITE_MISUSE;
    }
    rc = parse_set_innodb_parallel_read_threads_cell_value(
        database,
        target,
        value,
        value_kind,
        &integer_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    database->session.innodb_parallel_read_threads = integer_value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_innodb_parallel_read_threads_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target == NULL) {
        set_runtime_error(database, "invalid innodb_parallel_read_threads target");
        return MYLITE_ERROR;
    }
    *out_value = innodb_parallel_read_threads_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = MYLITE_SESSION_INNODB_PARALLEL_READ_THREADS_MIN_VALUE;
        return append_truncated_incorrect_system_variable_warning(database, target->name, "0");
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_innodb_parallel_read_threads_integer(
        database,
        target,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_set_innodb_parallel_read_threads_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target == NULL) {
        set_runtime_error(database, "invalid innodb_parallel_read_threads target");
        return MYLITE_ERROR;
    }
    *out_value = innodb_parallel_read_threads_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    return parse_innodb_parallel_read_threads_integer(
        database,
        target,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int parse_innodb_parallel_read_threads_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (target == NULL) {
        set_runtime_error(database, "invalid innodb_parallel_read_threads target");
        return MYLITE_ERROR;
    }
    if (negative) {
        *out_value = MYLITE_SESSION_INNODB_PARALLEL_READ_THREADS_MIN_VALUE;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK) {
        *out_value = MYLITE_SESSION_INNODB_PARALLEL_READ_THREADS_MAX_VALUE;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }
    if (magnitude > MYLITE_SESSION_INNODB_PARALLEL_READ_THREADS_MAX_VALUE) {
        *out_value = MYLITE_SESSION_INNODB_PARALLEL_READ_THREADS_MAX_VALUE;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }
    if (magnitude < MYLITE_SESSION_INNODB_PARALLEL_READ_THREADS_MIN_VALUE) {
        *out_value = MYLITE_SESSION_INNODB_PARALLEL_READ_THREADS_MIN_VALUE;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_innodb_tmpdir_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    char *decoded = NULL;
    size_t decoded_size = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE ||
        (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_LITERAL &&
         mylite_sql_ast_node_literal_kind(literal_node) == MYLITE_SQL_AST_LITERAL_NULL)) {
        return set_session_system_variable_override(database, target->kind, "", 0U);
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    rc = decode_sql_string_literal(
        database,
        literal_node,
        "SET innodb_tmpdir supports only string literals",
        "SET innodb_tmpdir string values do not support NUL bytes",
        &decoded,
        &decoded_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!innodb_tmpdir_text_is_supported(decoded, decoded_size)) {
        set_system_variable_cant_be_set_value_error(database, target->name, decoded);
        free(decoded);
        return MYLITE_ERROR;
    }

    rc = set_session_system_variable_override(database, target->kind, decoded, decoded_size);
    free(decoded);
    return rc;
}

static int apply_set_innodb_tmpdir_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_NULL || text == NULL) {
        return set_session_system_variable_override(database, target->kind, "", 0U);
    }
    if (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (!innodb_tmpdir_text_is_supported(text, text_size)) {
        set_system_variable_cant_be_set_value_error(database, target->name, text);
        return MYLITE_ERROR;
    }
    return set_session_system_variable_override(database, target->kind, text, text_size);
}

static bool innodb_tmpdir_text_is_supported(const char *text, size_t text_size) {
    if (text == NULL || text_size == 0U || text[0] != '/') {
        return false;
    }
    for (size_t index = 0U; index < text_size; ++index) {
        if (text[index] == '\0') {
            return false;
        }
    }
    return true;
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

static int validate_set_innodb_storage_monitor_switch_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    char switch_identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *switch_text = NULL;
    size_t switch_text_size = 0U;
    int rc = MYLITE_OK;

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
    if (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc = copy_identifier_text(
            literal_node,
            switch_identifier,
            sizeof(switch_identifier),
            database
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (innodb_monitor_switch_text_is_supported(switch_identifier)) {
            return MYLITE_OK;
        }
        set_system_variable_cant_be_set_value_error(database, target->name, switch_identifier);
        return MYLITE_ERROR;
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        set_system_variable_cant_be_set_value_error(database, target->name, "NULL");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    rc = decode_sql_string_literal(
        database,
        literal_node,
        "SET system variables support only string literals",
        "SET system variable string values do not support NUL bytes",
        &switch_text,
        &switch_text_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (innodb_monitor_switch_text_is_supported(switch_text)) {
        free(switch_text);
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, target->name, switch_text);
    free(switch_text);
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

static int validate_set_innodb_storage_monitor_switch_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (innodb_monitor_switch_text_is_supported(text)) {
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, target->name, text);
    return MYLITE_ERROR;
}

static bool innodb_monitor_switch_text_is_supported(const char *text) {
    return text_equals_ascii_case_insensitive(text, "all") ||
           text_equals_ascii_case_insensitive(text, "latch") ||
           text_equals_ascii_case_insensitive(text, "module_buffer") ||
           text_equals_ascii_case_insensitive(text, "buffer%");
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

static int validate_set_innodb_storage_fixed_null_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_NULL) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, "SET supports only fixed no-op system variable assignments");
    return MYLITE_ERROR;
}

static int validate_set_innodb_storage_fixed_decimal_text_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *expected_value
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (value_node == NULL || expected_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal_node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "SET supports only fixed no-op system variable assignments"
            );
            return MYLITE_ERROR;
        }
        literal_node = unwrap_parenthesized_expression(child_at(literal_node, 0U));
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if ((literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) ||
        !fixed_decimal_text_matches_ascii(literal_node->span, expected_value)) {
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

static int validate_set_innodb_storage_fixed_decimal_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;
    struct mylite_sql_source_span value_span = {0};
    size_t text_size = 0U;

    if (target == NULL || expected_value == NULL) {
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
        !fixed_decimal_text_matches_ascii(value_span, expected_value)) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool fixed_decimal_text_matches_ascii(
    struct mylite_sql_source_span text,
    const char *expected_value
) {
    size_t expected_size = 0U;

    if (text.text == NULL || text.length == 0U || expected_value == NULL) {
        return false;
    }
    expected_size = strlen(expected_value);
    if (text.length < expected_size || strncmp(text.text, expected_value, expected_size) != 0) {
        return false;
    }
    for (size_t index = expected_size; index < text.length; ++index) {
        if (text.text[index] != '0') {
            return false;
        }
    }
    return true;
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

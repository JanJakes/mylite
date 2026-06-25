static int finish_failed_statement(struct mylite_db *database, int rc, mylite_result **out_result) {
    int snapshot_rc = MYLITE_OK;

    database->session.previous_row_count = -1;
    set_internal_error_if_clear(database, rc, "statement execution failed");
    mylite_result_free(*out_result);
    *out_result = NULL;

    snapshot_rc = snapshot_current_diagnostics(database);
    return snapshot_rc == MYLITE_OK ? rc : snapshot_rc;
}

static int finish_completed_statement(
    struct mylite_db *database,
    bool completed_statement_is_select,
    int64_t completed_row_count,
    bool preserve_diagnostics_snapshot,
    mylite_result **out_result
) {
    int rc = MYLITE_OK;

    if (!preserve_diagnostics_snapshot) {
        mylite_diagnostics_clear_condition(mylite_connection_diagnostics(database));
        rc = snapshot_current_diagnostics(database);
    }
    if (rc != MYLITE_OK) {
        database->session.previous_row_count = -1;
        mylite_result_free(*out_result);
        *out_result = NULL;
        return rc;
    }

    database->session.previous_row_count = completed_row_count;
    update_found_rows_for_completed_statement(database, completed_statement_is_select, *out_result);
    return MYLITE_OK;
}

static void update_found_rows_for_completed_statement(
    struct mylite_db *database,
    bool completed_statement_is_select,
    const mylite_result *result
) {
    if (database == NULL || !completed_statement_is_select || result == NULL) {
        return;
    }

    if (mylite_result_has_found_row_count(result)) {
        database->session.found_rows = mylite_result_found_row_count(result);
        return;
    }

    database->session.found_rows = (uint64_t)mylite_result_row_count(result);
}

static bool statement_result_is_select(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
) {
    if (statement == NULL) {
        return false;
    }
    if (statement->kind == MYLITE_SQL_AST_SELECT_STATEMENT ||
        statement->kind == MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT ||
        statement->kind == MYLITE_SQL_AST_PARENTHESIZED_QUERY_EXPRESSION ||
        statement->kind == MYLITE_SQL_AST_EXPLAIN_STATEMENT) {
        return true;
    }
    if (statement->kind != MYLITE_SQL_AST_EXECUTE_STATEMENT || result == NULL) {
        return false;
    }
    return mylite_result_column_count(result) != 0U;
}

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    int rc = MYLITE_OK;

    if (statement == NULL) {
        set_unsupported_error(database, "empty statement is not supported");
        return MYLITE_ERROR;
    }
    database->session.active_statement_time = (int64_t)mylite_statement_context_time(context);

    rc = prepare_statement_transaction_boundary(database, statement);
    if (rc != MYLITE_OK) {
        return rc;
    }
    {
        rc = validate_alter_table_algorithm_lock_options(database, statement);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
        return execute_prepare_statement(database, statement, out_result);
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
        return execute_execute_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
        return execute_deallocate_prepare_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_VIEW_STATEMENT:
    case MYLITE_SQL_AST_VALUES_STATEMENT:
        return execute_non_prepared_statement(database, context, statement, out_result);
    case MYLITE_SQL_AST_SPATIAL_TYPE:
    case MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION:
    case MYLITE_SQL_AST_MULTI_VALUED_INDEX_PART:
    case MYLITE_SQL_AST_QUALIFIED_WILDCARD:
    case MYLITE_SQL_AST_GROUP_BY_ITEM_LIST:
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXPORT_SET_FUNCTION:
    case MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKE_SET_FUNCTION:
    case MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ANY_VALUE_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_STD_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_POP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_SAMP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VAR_POP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VAR_SAMP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VARIANCE_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_COMPRESS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_VALUES_ROW_LIST:
    case MYLITE_SQL_AST_VALUES_ROW:
    case MYLITE_SQL_AST_SELECT_INTO_LIST:
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION:
    case MYLITE_SQL_AST_CREATE_OR_REPLACE_CLAUSE:
    case MYLITE_SQL_AST_VIEW_OPTION_LIST:
    case MYLITE_SQL_AST_VIEW_ALGORITHM_OPTION:
    case MYLITE_SQL_AST_VIEW_DEFINER_OPTION:
    case MYLITE_SQL_AST_VIEW_SECURITY_OPTION:
    case MYLITE_SQL_AST_VIEW_DEFINER_ACCOUNT:
    case MYLITE_SQL_AST_VIEW_CHECK_OPTION:
    case MYLITE_SQL_AST_GROUP_BY_ROLLUP_MODIFIER:
    case MYLITE_SQL_AST_SHOW_GRANTS_ACCOUNT:
    case MYLITE_SQL_AST_SHOW_GRANTS_ROLE_LIST:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_GROUP_CONCAT_DISTINCT_MODIFIER:
    case MYLITE_SQL_AST_AGGREGATE_DISTINCT_MODIFIER:
    case MYLITE_SQL_AST_CHECKSUM_TABLE_QUICK_OPTION:
    case MYLITE_SQL_AST_CHECKSUM_TABLE_EXTENDED_OPTION:
    case MYLITE_SQL_AST_SHOW_LOG_EVENTS_IN_OPTION:
    case MYLITE_SQL_AST_SHOW_LOG_EVENTS_FROM_OPTION:
    case MYLITE_SQL_AST_SHOW_LOG_EVENTS_LIMIT_OPTION:
        break;
    default:
        return execute_non_prepared_statement(database, context, statement, out_result);
    }

    return execute_non_prepared_statement(database, context, statement, out_result);
}

static int execute_non_prepared_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    switch (statement->kind) {
    case MYLITE_SQL_AST_USE_STATEMENT:
        return execute_use_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        return execute_set_names_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
        return execute_set_character_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SET_STATEMENT:
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
        return execute_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT:
        return execute_admin_noop_statement(database, out_result);
    case MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT:
        return execute_unsupported_stored_program_statement(database);
    case MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT:
        return execute_utility_noop_statement(database, out_result);
    case MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT:
        return execute_unsupported_utility_statement(database);
    case MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT:
        return execute_set_transaction_statement(database, statement, out_result);
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        return execute_start_transaction_statement(database, statement, out_result);
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
        return execute_commit_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        return execute_rollback_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        return execute_savepoint_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        return execute_rollback_to_savepoint_statement(database, statement, out_result);
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return execute_release_savepoint_statement(database, statement, out_result);
    case MYLITE_SQL_AST_LOCK_TABLES_STATEMENT:
        return execute_lock_tables_statement(database, statement, out_result);
    case MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT:
        return execute_unlock_tables_statement(database, out_result);
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        return execute_create_schema_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT:
        return execute_alter_schema_default_charset_collation_statement(
            database,
            statement,
            out_result
        );
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        return execute_drop_schema_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return execute_create_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT:
        return execute_create_temporary_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT:
        return execute_create_table_like_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT:
        return execute_create_temporary_table_like_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT:
        return execute_create_table_select_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT:
        return execute_create_temporary_table_select_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_VIEW_STATEMENT:
        return execute_create_view_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_VIEW_STATEMENT:
        return execute_alter_view_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_PROCEDURE_STATEMENT:
        return execute_create_procedure_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT:
        return execute_create_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST:
    case MYLITE_SQL_AST_CREATE_OR_REPLACE_CLAUSE:
    case MYLITE_SQL_AST_VIEW_OPTION_LIST:
    case MYLITE_SQL_AST_VIEW_ALGORITHM_OPTION:
    case MYLITE_SQL_AST_VIEW_DEFINER_OPTION:
    case MYLITE_SQL_AST_VIEW_SECURITY_OPTION:
    case MYLITE_SQL_AST_VIEW_DEFINER_ACCOUNT:
    case MYLITE_SQL_AST_VIEW_CHECK_OPTION:
    case MYLITE_SQL_AST_SPATIAL_TYPE:
    case MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION:
    case MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER:
    case MYLITE_SQL_AST_SELECT_INTO_LIST:
    case MYLITE_SQL_AST_GROUP_CONCAT_DISTINCT_MODIFIER:
    case MYLITE_SQL_AST_AGGREGATE_DISTINCT_MODIFIER:
    case MYLITE_SQL_AST_CHECKSUM_TABLE_QUICK_OPTION:
    case MYLITE_SQL_AST_CHECKSUM_TABLE_EXTENDED_OPTION:
    case MYLITE_SQL_AST_SHOW_LOG_EVENTS_IN_OPTION:
    case MYLITE_SQL_AST_SHOW_LOG_EVENTS_FROM_OPTION:
    case MYLITE_SQL_AST_SHOW_LOG_EVENTS_LIMIT_OPTION:
        break;
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
        return execute_drop_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return execute_drop_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT:
        return execute_drop_temporary_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_VIEW_STATEMENT:
        return execute_drop_view_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_PROCEDURE_STATEMENT:
        return execute_drop_procedure_statement(database, statement, out_result);
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
        return execute_truncate_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
        return execute_rename_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
        return execute_alter_table_rename_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT:
        return execute_alter_table_multi_action_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
        return execute_alter_table_add_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
        return execute_alter_table_add_primary_key_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
        return execute_alter_table_add_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
        return execute_alter_table_add_foreign_key_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
        return execute_alter_table_drop_foreign_key_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT:
        return execute_alter_table_drop_constraint_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
        return execute_alter_table_drop_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
        return execute_alter_table_rename_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
        return execute_alter_table_index_visibility_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT:
        return execute_alter_table_add_check_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT:
        return execute_alter_table_drop_check_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT:
        return execute_alter_table_alter_check_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
        return execute_alter_table_drop_primary_key_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT:
        return execute_alter_table_auto_increment_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
        return execute_alter_table_drop_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
        return execute_alter_table_rename_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
        return execute_alter_table_modify_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
        return execute_alter_table_change_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
        return execute_alter_table_set_default_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
        return execute_alter_table_drop_default_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
        return execute_alter_table_column_visibility_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT:
        return execute_alter_table_default_charset_collation_statement(
            database,
            statement,
            out_result
        );
    case MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT:
        return execute_alter_table_convert_character_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
        return execute_alter_table_comment_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_STORAGE_STATISTICS_STATEMENT:
        return execute_alter_table_storage_statistics_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT:
        return execute_alter_table_order_by_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
        return execute_alter_table_force_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT:
        return execute_alter_table_disable_keys_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT:
        return execute_alter_table_enable_keys_statement(database, statement, out_result);
    case MYLITE_SQL_AST_INSERT_STATEMENT:
        return execute_insert_statement(database, statement, out_result);
    case MYLITE_SQL_AST_INSERT_SELECT_STATEMENT:
        return execute_insert_select_statement(database, statement, out_result);
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
        return execute_replace_values_statement(database, statement, out_result);
    case MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT:
        return execute_replace_select_statement(database, statement, out_result);
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
        return execute_insert_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
        return execute_replace_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT:
        return execute_load_data_infile_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_DELETE_STATEMENT:
        return execute_delete_statement(database, statement, out_result);
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT:
        return execute_update_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DO_STATEMENT:
        return execute_do_statement(database, statement, out_result);
    case MYLITE_SQL_AST_VALUES_STATEMENT:
        return execute_values_statement(database, statement, out_result);
    case MYLITE_SQL_AST_PARENTHESIZED_QUERY_EXPRESSION:
        return execute_parenthesized_query_expression(database, context, statement, out_result);
    case MYLITE_SQL_AST_EXPLAIN_STATEMENT:
        return execute_explain_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CALL_STATEMENT:
        return execute_call_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SELECT_STATEMENT: {
        int rc = select_statement_into_list(statement) == NULL
                     ? execute_select_statement(database, context, statement, true, out_result)
                     : execute_select_into_user_variables_statement(
                           database,
                           context,
                           statement,
                           out_result
                       );

        if (rc == MYLITE_OK) {
            clear_select_consumed_next_transaction_characteristics(database);
        }
        return rc;
    }
    case MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT: {
        int rc = execute_compound_select_statement(database, context, statement, out_result);

        if (rc == MYLITE_OK) {
            clear_select_consumed_next_transaction_characteristics(database);
        }
        return rc;
    }
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
        return execute_show_tables_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
        return execute_show_table_status_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
        return execute_show_character_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
        return execute_show_collation_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
        return execute_show_variables_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
        return execute_show_status_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT:
        return execute_show_triggers_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT:
        return execute_show_events_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT:
        return execute_show_open_tables_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT:
        return execute_show_routine_status_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT:
        return execute_show_processlist_statement(database, context, statement, out_result);
    case MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT:
        return execute_show_grants_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT:
        return execute_show_privileges_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT:
        return execute_show_binary_log_status_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT:
        return execute_show_binary_logs_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_BINLOG_EVENTS_STATEMENT:
        return execute_show_binlog_events_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_RELAYLOG_EVENTS_STATEMENT:
        return execute_show_relaylog_events_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_REPLICA_STATUS_STATEMENT:
        return execute_show_replica_status_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_REPLICAS_STATEMENT:
        return execute_show_replicas_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT:
        return execute_show_warnings_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT:
        return execute_show_count_warnings_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT:
        return execute_show_errors_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT:
        return execute_show_count_errors_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT:
        return execute_show_columns_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
        return execute_show_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
        return execute_show_create_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_VIEW_STATEMENT:
        return execute_show_create_view_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_PROCEDURE_STATEMENT:
        return execute_show_create_procedure_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_FUNCTION_STATEMENT:
        return execute_show_create_function_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_TRIGGER_STATEMENT:
        return execute_show_create_trigger_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_EVENT_STATEMENT:
        return execute_show_create_event_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_USER_STATEMENT:
        return execute_show_create_user_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT:
        return execute_show_create_database_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
        return execute_show_engines_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT:
        return execute_show_engine_status_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_PLUGINS_STATEMENT:
        return execute_show_plugins_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT:
        return execute_show_databases_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CHECK_TABLE_STATEMENT:
    case MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT:
        return execute_table_maintenance_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CHECKSUM_TABLE_STATEMENT:
        return execute_checksum_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST:
    case MYLITE_SQL_AST_LOCK_TABLE_TARGET:
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK:
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK:
    case MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_UNION_TERM_LIST:
    case MYLITE_SQL_AST_UNION_TERM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_USER_VARIABLE:
    case MYLITE_SQL_AST_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_USING_LIST:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_QUALIFIED_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_DML_DEFAULT_VALUE:
    case MYLITE_SQL_AST_DEFAULT_FUNCTION:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
    case MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_TIME_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION:
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATE_FUNCTION:
    case MYLITE_SQL_AST_TIME_FUNCTION:
    case MYLITE_SQL_AST_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_QUARTER_FUNCTION:
    case MYLITE_SQL_AST_WEEK_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_YEAR_FUNCTION:
    case MYLITE_SQL_AST_MONTH_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAY_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFMONTH_FUNCTION:
    case MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_HOUR_FUNCTION:
    case MYLITE_SQL_AST_MINUTE_FUNCTION:
    case MYLITE_SQL_AST_SECOND_FUNCTION:
    case MYLITE_SQL_AST_MICROSECOND_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_POSITION_FIRST:
    case MYLITE_SQL_AST_COLUMN_POSITION_AFTER:
    case MYLITE_SQL_AST_INTEGER_TYPE:
    case MYLITE_SQL_AST_VARCHAR_TYPE:
    case MYLITE_SQL_AST_CHAR_TYPE:
    case MYLITE_SQL_AST_TEXT_TYPE:
    case MYLITE_SQL_AST_JSON_TYPE:
    case MYLITE_SQL_AST_ENUM_TYPE:
    case MYLITE_SQL_AST_ENUM_LABEL_LIST:
    case MYLITE_SQL_AST_SET_TYPE:
    case MYLITE_SQL_AST_SET_MEMBER_LIST:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE:
    case MYLITE_SQL_AST_BIT_TYPE:
    case MYLITE_SQL_AST_DECIMAL_TYPE:
    case MYLITE_SQL_AST_APPROXIMATE_TYPE:
    case MYLITE_SQL_AST_YEAR_TYPE:
    case MYLITE_SQL_AST_DATE_TYPE:
    case MYLITE_SQL_AST_DATETIME_TYPE:
    case MYLITE_SQL_AST_TIME_TYPE:
    case MYLITE_SQL_AST_TIMESTAMP_TYPE:
    case MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION:
    case MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST:
    case MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION:
    case MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION:
    case MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST:
    case MYLITE_SQL_AST_SECONDARY_INDEX_PART:
    case MYLITE_SQL_AST_MULTI_VALUED_INDEX_PART:
    case MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION:
    case MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST:
    case MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME:
    case MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_RESTRICT:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_NO_ACTION:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL:
    case MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION:
    case MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED:
    case MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED:
    case MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION:
    case MYLITE_SQL_AST_INLINE_PRIMARY_KEY:
    case MYLITE_SQL_AST_INLINE_UNIQUE_KEY:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT:
    case MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE:
    case MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE:
    case MYLITE_SQL_AST_COLUMN_BINARY_COLLATION_ATTRIBUTE:
    case MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE:
    case MYLITE_SQL_AST_COLUMN_VISIBILITY_ATTRIBUTE:
    case MYLITE_SQL_AST_COLUMN_SRID_ATTRIBUTE:
    case MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE:
    case MYLITE_SQL_AST_GENERATED_COLUMN_VIRTUAL:
    case MYLITE_SQL_AST_GENERATED_COLUMN_STORED:
    case MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION:
    case MYLITE_SQL_AST_NULLABILITY:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
    case MYLITE_SQL_AST_IDENTIFIER_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_VALUES_ROW_LIST:
    case MYLITE_SQL_AST_VALUES_ROW:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_VALUES_REFERENCE:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_JOIN:
    case MYLITE_SQL_AST_FROM_DERIVED:
    case MYLITE_SQL_AST_JOIN_USING_CLAUSE:
    case MYLITE_SQL_AST_INDEX_HINT_LIST:
    case MYLITE_SQL_AST_USE_INDEX_HINT:
    case MYLITE_SQL_AST_FORCE_INDEX_HINT:
    case MYLITE_SQL_AST_IGNORE_INDEX_HINT:
    case MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN:
    case MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY:
    case MYLITE_SQL_AST_INDEX_HINT_FOR_GROUP_BY:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_ITEM_LIST:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_COMPARISON_PREDICATE:
    case MYLITE_SQL_AST_IS_NULL_PREDICATE:
    case MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE:
    case MYLITE_SQL_AST_AND_PREDICATE:
    case MYLITE_SQL_AST_OR_PREDICATE:
    case MYLITE_SQL_AST_XOR_PREDICATE:
    case MYLITE_SQL_AST_NOT_PREDICATE:
    case MYLITE_SQL_AST_BETWEEN_PREDICATE:
    case MYLITE_SQL_AST_IN_PREDICATE:
    case MYLITE_SQL_AST_EXISTS_PREDICATE:
    case MYLITE_SQL_AST_PREDICATE_VALUE_LIST:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_DIRECTION:
    case MYLITE_SQL_AST_ROW_NUMBER_FUNCTION:
    case MYLITE_SQL_AST_RANK_FUNCTION:
    case MYLITE_SQL_AST_DENSE_RANK_FUNCTION:
    case MYLITE_SQL_AST_PERCENT_RANK_FUNCTION:
    case MYLITE_SQL_AST_CUME_DIST_FUNCTION:
    case MYLITE_SQL_AST_NTILE_FUNCTION:
    case MYLITE_SQL_AST_LAG_FUNCTION:
    case MYLITE_SQL_AST_LEAD_FUNCTION:
    case MYLITE_SQL_AST_FIRST_VALUE_FUNCTION:
    case MYLITE_SQL_AST_LAST_VALUE_FUNCTION:
    case MYLITE_SQL_AST_NTH_VALUE_FUNCTION:
    case MYLITE_SQL_AST_WINDOW_SPEC:
    case MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_ORDER_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_REFERENCE:
    case MYLITE_SQL_AST_WINDOW_DEFINITION:
    case MYLITE_SQL_AST_WINDOW_DEFINITION_LIST:
    case MYLITE_SQL_AST_WINDOW_FRAME_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_FRAME_BOUND:
    case MYLITE_SQL_AST_WINDOW_RESPECT_NULLS:
    case MYLITE_SQL_AST_WINDOW_IGNORE_NULLS:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_EXPLAIN_FORMAT:
    case MYLITE_SQL_AST_EXPLAIN_ANALYZE:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_TABLE_ENGINE_OPTION:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_CHARSET_OPTION:
    case MYLITE_SQL_AST_TABLE_COLLATION_OPTION:
    case MYLITE_SQL_AST_TABLE_COMMENT_OPTION:
    case MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION:
    case MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION:
    case MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION:
    case MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION:
    case MYLITE_SQL_AST_TABLE_MIN_ROWS_OPTION:
    case MYLITE_SQL_AST_TABLE_MAX_ROWS_OPTION:
    case MYLITE_SQL_AST_TABLE_AVG_ROW_LENGTH_OPTION:
    case MYLITE_SQL_AST_TABLE_DELAY_KEY_WRITE_OPTION:
    case MYLITE_SQL_AST_TABLE_TABLESPACE_OPTION:
    case MYLITE_SQL_AST_TABLE_UNION_OPTION:
    case MYLITE_SQL_AST_TABLE_INSERT_METHOD_OPTION:
    case MYLITE_SQL_AST_TABLE_STORAGE_OPTION:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_TYPE_OPTION:
    case MYLITE_SQL_AST_INDEX_COMMENT_OPTION:
    case MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION:
    case MYLITE_SQL_AST_ORDER_BY_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_BY_ITEM:
    case MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_INSERT_STRING_FUNCTION:
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHARSET_FUNCTION:
    case MYLITE_SQL_AST_COLLATION_FUNCTION:
    case MYLITE_SQL_AST_COERCIBILITY_FUNCTION:
    case MYLITE_SQL_AST_ELT_FUNCTION:
    case MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXPORT_SET_FUNCTION:
    case MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKE_SET_FUNCTION:
    case MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FIELD_FUNCTION:
    case MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INTERVAL_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_SEARCH_FUNCTION:
    case MYLITE_SQL_AST_JSON_SEARCH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_OVERLAPS_FUNCTION:
    case MYLITE_SQL_AST_JSON_OVERLAPS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_MEMBER_OF_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_DEPTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_DEPTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_STORAGE_SIZE_FUNCTION:
    case MYLITE_SQL_AST_JSON_STORAGE_SIZE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_STORAGE_FREE_FUNCTION:
    case MYLITE_SQL_AST_JSON_STORAGE_FREE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_PRETTY_FUNCTION:
    case MYLITE_SQL_AST_JSON_PRETTY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_MERGE_FUNCTION:
    case MYLITE_SQL_AST_JSON_MERGE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION:
    case MYLITE_SQL_AST_JSON_MERGE_PATCH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION:
    case MYLITE_SQL_AST_JSON_MERGE_PRESERVE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MOD_FUNCTION:
    case MYLITE_SQL_AST_ABS_FUNCTION:
    case MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SIGN_FUNCTION:
    case MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CEIL_FUNCTION:
    case MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CEILING_FUNCTION:
    case MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ROUND_FUNCTION:
    case MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_OCT_FUNCTION:
    case MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONV_FUNCTION:
    case MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PI_FUNCTION:
    case MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
    case MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SQRT_FUNCTION:
    case MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
    case MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
    case MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ASIN_FUNCTION:
    case MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SIN_FUNCTION:
    case MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COS_FUNCTION:
    case MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TAN_FUNCTION:
    case MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COT_FUNCTION:
    case MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ATAN_FUNCTION:
    case MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
    case MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXP_FUNCTION:
    case MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LN_FUNCTION:
    case MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOG_FUNCTION:
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
    case MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POWER_FUNCTION:
    case MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
    case MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CRC32_FUNCTION:
    case MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
    case MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_COMPRESS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_SHORT_FUNCTION:
    case MYLITE_SQL_AST_UUID_SHORT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHAR_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED:
    case MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
    case MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ASCII_FUNCTION:
    case MYLITE_SQL_AST_ORD_FUNCTION:
    case MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
    case MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
    case MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
    case MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
    case MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
    case MYLITE_SQL_AST_LEFT_FUNCTION:
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
    case MYLITE_SQL_AST_LPAD_FUNCTION:
    case MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RPAD_FUNCTION:
    case MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_SPACE_FUNCTION:
    case MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
    case MYLITE_SQL_AST_LOCATE_FUNCTION:
    case MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INSTR_FUNCTION:
    case MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POSITION_FUNCTION:
    case MYLITE_SQL_AST_FIND_IN_SET_FUNCTION:
    case MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_STRCMP_FUNCTION:
    case MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN_CLAUSE:
    case MYLITE_SQL_AST_CASE_ELSE_CLAUSE:
    case MYLITE_SQL_AST_DO_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_GENERIC_FUNCTION:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COUNT_STAR_FUNCTION:
    case MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION:
    case MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION:
    case MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STD_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_POP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_SAMP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VAR_POP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VAR_SAMP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VARIANCE_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_GROUP_BY_ROLLUP_MODIFIER:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION:
    case MYLITE_SQL_AST_SHOW_GRANTS_ACCOUNT:
    case MYLITE_SQL_AST_SHOW_GRANTS_ROLE_LIST:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET:
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET:
    case MYLITE_SQL_AST_SET_DEFAULT_VALUE:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE:
    case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE:
    case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY:
    case MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT:
    case MYLITE_SQL_AST_TRANSACTION_CHAIN_COMPLETION:
    case MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER:
    case MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER:
    case MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER:
    case MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER:
        break;
    }

    set_unsupported_error(database, "statement is not supported by this MyLite build");

    return MYLITE_ERROR;
}

static int execute_parenthesized_query_expression(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *inner_statement = child_at(statement, 0U);

    if (inner_statement == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (const struct mylite_sql_ast_node *clause = child_at(statement, 1U); clause != NULL;
         clause = clause->next_sibling) {
        if (clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
            clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            set_unsupported_error(
                database,
                "parenthesized query expressions do not yet support outer ORDER BY or LIMIT"
            );
            return MYLITE_ERROR;
        }
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (inner_statement->kind) {
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT:
    case MYLITE_SQL_AST_VALUES_STATEMENT:
    case MYLITE_SQL_AST_PARENTHESIZED_QUERY_EXPRESSION:
        return execute_non_prepared_statement(database, context, inner_statement, out_result);
    default:
        break;
    }

    set_unsupported_error(database, "parenthesized query expression operand is not yet supported");
    return MYLITE_ERROR;
}

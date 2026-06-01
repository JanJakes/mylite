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
        statement->kind == MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT) {
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
    case MYLITE_SQL_AST_VALUES_ROW_LIST:
    case MYLITE_SQL_AST_VALUES_ROW:
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
    case MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT:
        return execute_set_transaction_statement(database, statement, out_result);
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        return execute_start_transaction_statement(database, statement, out_result);
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
        return execute_commit_statement(database, out_result);
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        return execute_rollback_statement(database, out_result);
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
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT:
        return execute_create_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST:
    case MYLITE_SQL_AST_SPATIAL_TYPE:
    case MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION:
    case MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER:
        break;
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
        return execute_drop_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return execute_drop_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT:
        return execute_drop_temporary_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_VIEW_STATEMENT:
        return execute_drop_view_statement(database, statement, out_result);
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
    case MYLITE_SQL_AST_SELECT_STATEMENT: {
        int rc = execute_select_statement(database, context, statement, true, out_result);

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
        return execute_show_grants_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT:
        return execute_show_privileges_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT:
        return execute_show_binary_log_status_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT:
        return execute_show_binary_logs_statement(database, out_result);
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
    case MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE:
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
    case MYLITE_SQL_AST_WINDOW_SPEC:
    case MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_ORDER_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
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
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR:
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
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
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
    case MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
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

static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_use_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_execution_catalog_builtin_schema *builtin_schema = NULL;
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    int written = 0;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK) {
        builtin_schema = find_builtin_schema_descriptor(schema_name);
    }
    if (rc == MYLITE_OK && builtin_schema != NULL) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && result == NULL && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && result == NULL) {
        rc = resolve_schema_name(database, schema_name, &schema);
    }
    if (rc == MYLITE_OK && result == NULL) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(
        database->session.selected_schema,
        sizeof(database->session.selected_schema),
        "%s",
        builtin_schema != NULL ? builtin_schema->name : schema.name
    );
    if (written < 0 || (size_t)written >= sizeof(database->session.selected_schema)) {
        mylite_result_free(result);
        set_identifier_too_long_error(database, "database");
        return MYLITE_ERROR;
    }
    database->session.has_selected_schema = true;

    return finish_successful_result(database, result, out_result);
}

static int execute_set_names_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_set_connection_character_set_statement(database, statement, true, out_result);
}

static int execute_set_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_set_connection_character_set_statement(database, statement, false, out_result);
}

static int execute_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = apply_set_statement(database, statement);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *source_sql = NULL;
    mylite_result *result = NULL;
    size_t source_sql_size = 0U;
    size_t parameter_count = 0U;
    size_t existing_index = 0U;
    int rc = MYLITE_OK;

    rc = copy_prepared_statement_name(database, child_at(statement, 0U), name, sizeof(name));
    if (rc == MYLITE_OK) {
        rc = prepare_statement_source_sql(
            database,
            child_at(statement, 1U),
            &source_sql,
            &source_sql_size
        );
    }
    if (rc == MYLITE_OK) {
        existing_index = find_session_prepared_statement_index(&database->session, name);
        if (existing_index != SIZE_MAX) {
            remove_session_prepared_statement_at(&database->session, existing_index);
        }
        rc = validate_prepared_statement_sql(
            database,
            source_sql,
            source_sql_size,
            &parameter_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = ensure_session_prepared_statement_capacity(database);
    }
    if (rc == MYLITE_OK) {
        struct mylite_session_prepared_statement *prepared =
            &database->session.prepared_statements[database->session.prepared_statement_count];

        *prepared = (struct mylite_session_prepared_statement){0};
        memcpy(prepared->name, name, strlen(name) + 1U);
        prepared->sql = source_sql;
        prepared->sql_size = source_sql_size;
        prepared->parameter_count = parameter_count;
        source_sql = NULL;
        ++database->session.prepared_statement_count;
    }

    free(source_sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_execute_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_session_prepared_statement *prepared = NULL;
    const struct mylite_sql_ast_node *using_list = NULL;
    struct prepared_statement_expanded_sql expanded_sql = {0};
    size_t parameter_count = 0U;
    int rc = copy_prepared_statement_name(database, child_at(statement, 0U), name, sizeof(name));

    if (rc != MYLITE_OK) {
        return rc;
    }

    prepared = find_session_prepared_statement(&database->session, name);
    if (prepared == NULL) {
        set_unknown_prepared_statement_error(database, name, "EXECUTE");
        return MYLITE_ERROR;
    }

    using_list = child_at(statement, 1U);
    parameter_count = using_list == NULL ? 0U : mylite_sql_ast_node_child_count(using_list);
    if (parameter_count != prepared->parameter_count) {
        set_prepared_statement_argument_count_error(database);
        return MYLITE_ERROR;
    }

    rc = build_prepared_statement_sql(
        database,
        prepared->sql,
        prepared->sql_size,
        using_list,
        &expanded_sql
    );
    if (rc == MYLITE_OK) {
        rc = execute_expanded_prepared_statement_sql(
            database,
            expanded_sql.text,
            expanded_sql.text_size,
            out_result
        );
    }

    free(expanded_sql.text);
    return rc;
}

static int execute_expanded_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    const struct mylite_sql_ast_node *statement = NULL;
    int64_t completed_row_count = -1;
    size_t statement_count = 0U;
    bool preserve_diagnostics_snapshot = false;
    bool completed_statement_is_select = false;
    int rc = MYLITE_OK;

    if (database == NULL || sql == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;

    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(&context, database, sql, sql_size);
    if (rc != MYLITE_OK) {
        mylite_statement_context_deinit(&context);
        return rc;
    }
    mylite_statement_context_set_previous_row_count(&context, database->session.previous_row_count);
    mylite_statement_context_set_previous_found_rows(&context, database->session.found_rows);

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sql_size,
            .modes = lexer_modes_for_session_sql_mode(&database->session),
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        (void)mylite_statement_context_end(&context, rc);
        mylite_statement_context_deinit(&context);
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 0U) {
        rc = execute_empty_statement(database, out_result);
    } else if (rc == MYLITE_OK && statement_count == 1U) {
        statement = child_at(parse_result.root, 0U);
        if (prepared_statement_disallows_statement(statement)) {
            set_prepared_statement_command_unsupported_error(database);
            rc = MYLITE_ERROR;
        }
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "multiple statements are not supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && statement != NULL) {
        database->session.active_statement_time = (int64_t)mylite_statement_context_time(&context);
        rc = prepare_statement_transaction_boundary(database, statement);
        if (rc == MYLITE_OK) {
            rc = validate_alter_table_algorithm_lock_options(database, statement);
        }
        if (rc == MYLITE_OK) {
            rc = execute_non_prepared_statement(database, &context, statement, out_result);
        }
    }
    if (rc == MYLITE_OK) {
        completed_row_count = row_count_for_completed_statement(statement, *out_result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(statement);
        completed_statement_is_select = statement_result_is_select(statement, *out_result);
    }

    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
            completed_statement_is_select,
            completed_row_count,
            preserve_diagnostics_snapshot,
            out_result
        );
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);
    return rc;
}

static int execute_deallocate_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    size_t existing_index = 0U;
    int rc = copy_prepared_statement_name(database, child_at(statement, 0U), name, sizeof(name));

    if (rc != MYLITE_OK) {
        return rc;
    }

    existing_index = find_session_prepared_statement_index(&database->session, name);
    if (existing_index == SIZE_MAX) {
        set_unknown_prepared_statement_error(database, name, "DEALLOCATE PREPARE");
        return MYLITE_ERROR;
    }
    remove_session_prepared_statement_at(&database->session, existing_index);

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = apply_set_transaction_statement(database, statement);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int apply_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *characteristic_list = first_child;
    struct transaction_characteristics characteristics = {
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    char scope_text[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        characteristic_list = child_at(statement, 1U);
    }
    rc = collect_transaction_characteristics(database, characteristic_list, &characteristics);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (scope == NULL) {
        return apply_next_transaction_characteristics(database, &characteristics, false);
    }

    rc = copy_identifier_text(scope, scope_text, sizeof(scope_text), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(scope_text, "session")) {
        return apply_session_transaction_characteristics(database, &characteristics);
    }
    if (text_equals_ascii_case_insensitive(scope_text, "global")) {
        set_unsupported_error(database, "SET GLOBAL TRANSACTION is not supported");
        return MYLITE_ERROR;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int collect_transaction_characteristics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *list,
    struct transaction_characteristics *out_characteristics
) {
    const struct mylite_sql_ast_node *characteristic = NULL;

    *out_characteristics = (struct transaction_characteristics){
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    if (list == NULL || list->kind != MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    characteristic = child_at(list, 0U);
    while (characteristic != NULL) {
        switch (characteristic->kind) {
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_READ_COMMITTED;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_READ_UNCOMMITTED;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_SERIALIZABLE;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE:
            if (out_characteristics->has_access_mode &&
                out_characteristics->access_mode != MYLITE_TRANSACTION_ACCESS_READ_WRITE) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            out_characteristics->has_access_mode = true;
            out_characteristics->access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY:
            if (out_characteristics->has_access_mode &&
                out_characteristics->access_mode != MYLITE_TRANSACTION_ACCESS_READ_ONLY) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            out_characteristics->has_access_mode = true;
            out_characteristics->access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
            break;
        case MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT:
            out_characteristics->has_consistent_snapshot = true;
            break;
        default:
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        characteristic = characteristic->next_sibling;
    }

    return MYLITE_OK;
}

static int apply_session_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics
) {
    if (characteristics->has_isolation) {
        database->session.session_transaction_isolation = characteristics->isolation;
        if (!database->session.user_transaction_active) {
            database->session.has_next_transaction_isolation = false;
            database->session.next_transaction_isolation_from_system_variable = false;
        }
    }
    if (characteristics->has_access_mode) {
        database->session.session_transaction_access_mode = characteristics->access_mode;
        if (!database->session.user_transaction_active) {
            database->session.has_next_transaction_access_mode = false;
            database->session.next_transaction_access_mode_from_system_variable = false;
        }
    }
    return MYLITE_OK;
}

static int apply_next_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics,
    bool from_system_variable
) {
    if (database->session.user_transaction_active) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_transaction_characteristics_changed,
            "25001",
            "Transaction characteristics can't be changed while a transaction is in progress"
        );
        return MYLITE_ERROR;
    }
    if (characteristics->has_isolation) {
        database->session.has_next_transaction_isolation = true;
        database->session.next_transaction_isolation = characteristics->isolation;
        database->session.next_transaction_isolation_from_system_variable = from_system_variable;
    }
    if (characteristics->has_access_mode) {
        database->session.has_next_transaction_access_mode = true;
        database->session.next_transaction_access_mode = characteristics->access_mode;
        database->session.next_transaction_access_mode_from_system_variable = from_system_variable;
    }
    return MYLITE_OK;
}

static int apply_transaction_system_variable_characteristics(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct transaction_characteristics *characteristics
) {
    if (target == NULL || characteristics == NULL) {
        set_runtime_error(database, "invalid transaction system variable assignment");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return MYLITE_OK;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_NONE && target->is_system_variable_target) {
        return apply_next_transaction_characteristics(database, characteristics, true);
    }
    return apply_session_transaction_characteristics(database, characteristics);
}

static int parse_set_transaction_isolation_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_isolation *out_isolation
) {
    char *decoded = NULL;
    char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *value = NULL;
    int rc = MYLITE_OK;

    if (out_isolation == NULL) {
        set_runtime_error(database, "invalid transaction_isolation output");
        return MYLITE_ERROR;
    }
    *out_isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc = copy_set_transaction_identifier_value(
            database,
            value_node,
            identifier,
            sizeof(identifier)
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        value = identifier;
    } else if (
        value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING
    ) {
        rc = decode_set_transaction_string_value(
            database,
            value_node,
            "transaction_isolation",
            &decoded
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        value = decoded;
    } else {
        return set_transaction_variable_invalid_node_value_error(
            database,
            "transaction_isolation",
            value_node
        );
    }

    if (text_equals_ascii_case_insensitive(value, "REPEATABLE-READ")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    } else if (text_equals_ascii_case_insensitive(value, "READ-COMMITTED")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_READ_COMMITTED;
    } else if (text_equals_ascii_case_insensitive(value, "READ-UNCOMMITTED")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_READ_UNCOMMITTED;
    } else if (text_equals_ascii_case_insensitive(value, "SERIALIZABLE")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_SERIALIZABLE;
    } else {
        set_system_variable_cant_be_set_value_error(database, "transaction_isolation", value);
        rc = MYLITE_ERROR;
    }

    free(decoded);
    return rc;
}

static int parse_set_transaction_read_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_access_mode *out_access_mode
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *decoded = NULL;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (out_access_mode == NULL) {
        set_runtime_error(database, "invalid transaction_read_only output");
        return MYLITE_ERROR;
    }
    *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_transaction_variable_invalid_node_value_error(
            database,
            "transaction_read_only",
            value_node
        );
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (parse_unsigned_integer_literal(&value_node->span, &magnitude) != MYLITE_OK ||
            magnitude > 1U) {
            return set_transaction_variable_invalid_node_value_error(
                database,
                "transaction_read_only",
                value_node
            );
        }
        if (magnitude == 1U) {
            *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
        }
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_set_transaction_string_value(
            database,
            value_node,
            "transaction_read_only",
            &decoded
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (text_equals_ascii_case_insensitive(decoded, "ON")) {
            *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
        } else if (!text_equals_ascii_case_insensitive(decoded, "OFF")) {
            set_system_variable_cant_be_set_value_error(database, "transaction_read_only", decoded);
            rc = MYLITE_ERROR;
        }
        free(decoded);
        return rc;
    }

    return set_transaction_variable_invalid_node_value_error(
        database,
        "transaction_read_only",
        value_node
    );
}

static int decode_set_transaction_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *variable_name,
    char **out_value
) {
    size_t text_length = 0U;
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    int unsupported_written = snprintf(
        unsupported_message,
        sizeof(unsupported_message),
        "SET %s supports only literal values",
        variable_name == NULL ? "transaction variable" : variable_name
    );
    int nul_written = snprintf(
        nul_message,
        sizeof(nul_message),
        "SET %s does not support NUL bytes",
        variable_name == NULL ? "transaction variable" : variable_name
    );

    if (unsupported_written < 0) {
        unsupported_message[0] = '\0';
    }
    if (nul_written < 0) {
        nul_message[0] = '\0';
    }
    return decode_sql_string_literal(
        database,
        value_node,
        unsupported_message,
        nul_message,
        out_value,
        &text_length
    );
}

static int copy_set_transaction_identifier_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char *value,
    size_t value_size
) {
    if (value == NULL || value_size == 0U) {
        set_runtime_error(database, "invalid transaction system variable value buffer");
        return MYLITE_ERROR;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_identifier_text(value_node, value, value_size, database);
}

static int set_transaction_variable_invalid_node_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node
) {
    char value[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t copied = 0U;

    if (value_node == NULL || value_node->span.text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    copied = value_node->span.length < sizeof(value) ? value_node->span.length : sizeof(value) - 1U;
    memcpy(value, value_node->span.text, copied);
    value[copied] = '\0';
    set_system_variable_cant_be_set_value_error(database, variable_name, value);
    return MYLITE_ERROR;
}

static const char *transaction_isolation_value_text(enum mylite_transaction_isolation isolation) {
    switch (isolation) {
    case MYLITE_TRANSACTION_ISOLATION_READ_COMMITTED:
        return "READ-COMMITTED";
    case MYLITE_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
        return "READ-UNCOMMITTED";
    case MYLITE_TRANSACTION_ISOLATION_SERIALIZABLE:
        return "SERIALIZABLE";
    case MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ:
    default:
        return "REPEATABLE-READ";
    }
}

static const char *transaction_read_only_scalar_text(
    enum mylite_transaction_access_mode access_mode
) {
    return access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY ? "1" : "0";
}

static const char *transaction_read_only_show_text(
    enum mylite_transaction_access_mode access_mode
) {
    return access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY ? "ON" : "OFF";
}

static enum mylite_transaction_access_mode current_transaction_access_mode(
    const struct mylite_db *database
) {
    if (database->session.has_next_transaction_access_mode) {
        return database->session.next_transaction_access_mode;
    }
    return database->session.session_transaction_access_mode;
}

static enum mylite_transaction_isolation current_transaction_isolation(
    const struct mylite_db *database
) {
    if (database->session.has_next_transaction_isolation) {
        return database->session.next_transaction_isolation;
    }
    return database->session.session_transaction_isolation;
}

static enum mylite_transaction_access_mode effective_start_transaction_access_mode(
    const struct mylite_db *database,
    const struct transaction_characteristics *characteristics
) {
    if (characteristics->has_access_mode) {
        return characteristics->access_mode;
    }
    return current_transaction_access_mode(database);
}

static int reserve_consistent_snapshot_ignored_warning(struct mylite_db *database) {
    struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    size_t warning_count = mylite_diagnostics_warning_count(diagnostics);

    if (warning_count == SIZE_MAX) {
        mylite_diagnostics_set_error(diagnostics, MYLITE_NOMEM, "HY001", "too many warnings");
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return mylite_diagnostics_reserve_warning_capacity(diagnostics, warning_count + 1U);
}

static int append_consistent_snapshot_ignored_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_consistent_snapshot_ignored,
        "HY000",
        "InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only be used "
        "with REPEATABLE READ isolation level."
    );
}

static void clear_next_transaction_characteristics(struct mylite_db *database) {
    database->session.has_next_transaction_isolation = false;
    database->session.has_next_transaction_access_mode = false;
    database->session.next_transaction_isolation_from_system_variable = false;
    database->session.next_transaction_access_mode_from_system_variable = false;
}

static void clear_active_transaction_characteristics(struct mylite_db *database) {
    database->session.active_transaction_read_only = false;
}

static void clear_next_transaction_characteristics_before_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    if (statement_consumes_next_characteristics_before_execution(statement)) {
        clear_next_transaction_characteristics(database);
    }
}

static int prepare_statement_transaction_boundary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    bool needs_implicit_commit = statement_requires_implicit_user_transaction_commit(statement);

    if (needs_implicit_commit && database->session.user_transaction_active &&
        statement->kind == MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT &&
        alter_table_comment_targets_existing_temporary_table(database, statement)) {
        needs_implicit_commit = false;
    }
    if (!needs_implicit_commit) {
        return MYLITE_OK;
    }

    clear_next_transaction_characteristics_before_statement(database, statement);
    return commit_active_user_transaction_for_ddl(database);
}

static bool statement_consumes_next_characteristics_before_execution(
    const struct mylite_sql_ast_node *statement
) {
    return statement_requires_implicit_user_transaction_commit(statement);
}

static bool alter_table_comment_targets_existing_temporary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_table_descriptor temporary_table = {0};
    const char *schema_name = NULL;
    const char *table_name = NULL;
    size_t part_count = 0U;
    bool found = false;
    int rc = MYLITE_OK;

    if (database == NULL || statement == NULL ||
        statement->kind != MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT) {
        return false;
    }

    rc = collect_identifier_parts(
        child_at(statement, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return false;
    }
    if (part_count == 1U) {
        if (!database->session.has_selected_schema) {
            return false;
        }
        schema_name = database->session.selected_schema;
        table_name = parts[0];
    } else if (part_count == 2U) {
        schema_name = parts[0];
        table_name = parts[1];
    } else {
        return false;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        schema_name,
        table_name,
        &temporary_table,
        &found
    );
    if (rc != MYLITE_OK) {
        return false;
    }
    if (!found) {
        return false;
    }
    if (temporary_table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        return false;
    }
    return true;
}

static void clear_select_consumed_next_transaction_characteristics(struct mylite_db *database) {
    if (!database->session.next_transaction_isolation_from_system_variable) {
        database->session.has_next_transaction_isolation = false;
        database->session.next_transaction_isolation_from_system_variable = false;
    }
    if (!database->session.next_transaction_access_mode_from_system_variable) {
        database->session.has_next_transaction_access_mode = false;
        database->session.next_transaction_access_mode_from_system_variable = false;
    }
}

static void clear_next_transaction_characteristics_after_statement(struct mylite_db *database) {
    clear_next_transaction_characteristics(database);
}

static int reject_read_only_persistent_write(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table
) {
    bool is_temporary = false;
    int rc = MYLITE_OK;

    if (database->session.user_transaction_active) {
        if (!database->session.active_transaction_read_only) {
            return MYLITE_OK;
        }
    } else if (current_transaction_access_mode(database) != MYLITE_TRANSACTION_ACCESS_READ_ONLY) {
        return MYLITE_OK;
    }

    rc = table_resolution_is_temporary(database, target, table, &is_temporary);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_temporary) {
        return MYLITE_OK;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    return MYLITE_ERROR;
}

static int table_resolution_is_temporary(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_is_temporary
) {
    struct mylite_catalog_table_descriptor temporary_table = {0};
    bool found_temporary = false;
    int rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        target->schema.name,
        target->table_name,
        &temporary_table,
        &found_temporary
    );

    *out_is_temporary = false;
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (found_temporary && temporary_table.table_id == table->table_id) {
        *out_is_temporary = true;
    }
    return MYLITE_OK;
}

static int execute_start_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *characteristic_list = child_at(statement, 0U);
    struct transaction_characteristics characteristics = {
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    enum mylite_transaction_isolation effective_isolation =
        MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    enum mylite_transaction_access_mode effective_access_mode =
        MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    bool needs_consistent_snapshot_warning = false;
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (characteristic_list != NULL) {
        rc = collect_transaction_characteristics(database, characteristic_list, &characteristics);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }
    if (characteristics.has_isolation) {
        effective_isolation = characteristics.isolation;
    } else {
        effective_isolation = current_transaction_isolation(database);
    }
    effective_access_mode = effective_start_transaction_access_mode(database, &characteristics);
    if (characteristics.has_consistent_snapshot) {
        if (effective_isolation != MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ) {
            needs_consistent_snapshot_warning = true;
        }
    }
    if (needs_consistent_snapshot_warning) {
        rc = reserve_consistent_snapshot_ignored_warning(database);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }

    clear_session_table_locks(database);
    if (database->session.user_transaction_active) {
        rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = normalize_sqlite_control_rc(
            database,
            execute_sqlite_control_sql(database, "BEGIN IMMEDIATE")
        );
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = true;
            database->session.active_transaction_read_only =
                effective_access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY;
            clear_next_transaction_characteristics(database);
        }
    }
    if (rc == MYLITE_OK && needs_consistent_snapshot_warning) {
        rc = append_consistent_snapshot_ignored_warning(database);
    }
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_commit_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (database->session.user_transaction_active) {
        rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_rollback_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (database->session.user_transaction_active) {
        rc =
            normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "ROLLBACK"));
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char savepoint_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_savepoint_name(
        database,
        child_at(statement, 0U),
        savepoint_name,
        sizeof(savepoint_name)
    );
    if (rc == MYLITE_OK && database->session.user_transaction_active) {
        rc = create_or_replace_user_savepoint(database, savepoint_name);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_rollback_to_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char savepoint_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_savepoint_name(
        database,
        child_at(statement, 0U),
        savepoint_name,
        sizeof(savepoint_name)
    );
    if (rc == MYLITE_OK) {
        rc = rollback_to_user_savepoint(database, savepoint_name);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_release_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char savepoint_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_savepoint_name(
        database,
        child_at(statement, 0U),
        savepoint_name,
        sizeof(savepoint_name)
    );
    if (rc == MYLITE_OK) {
        rc = release_user_savepoint(database, savepoint_name);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_lock_tables plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = commit_active_user_transaction_for_ddl(database);
    if (rc == MYLITE_OK) {
        clear_session_table_locks(database);
        rc = plan_lock_tables_statement(database, statement, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_lock_tables_plan(database, &plan);
    }
    planned_lock_tables_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_unlock_tables_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (database->session.table_lock_count != 0U && database->session.user_transaction_active) {
        rc = commit_active_user_transaction_for_ddl(database);
    }
    if (rc == MYLITE_OK) {
        clear_session_table_locks(database);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int plan_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_lock_tables *out_plan
) {
    const struct mylite_sql_ast_node *target_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *target_node = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_lock_tables){0};
    if (target_list == NULL || target_list->kind != MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->lock_count = mylite_sql_ast_node_child_count(target_list);
    if (out_plan->lock_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->lock_count > SIZE_MAX / sizeof(*out_plan->locks)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->locks = calloc(out_plan->lock_count, sizeof(*out_plan->locks));
    if (out_plan->locks == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    target_node = child_at(target_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < out_plan->lock_count;
         ++target_index) {
        rc = plan_lock_table_target(database, target_node, out_plan, target_index);
        if (target_node != NULL) {
            target_node = target_node->next_sibling;
        }
    }
    if (rc != MYLITE_OK) {
        planned_lock_tables_deinit(out_plan);
    }
    return rc;
}

static int plan_lock_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_lock_tables *plan,
    size_t target_index
) {
    struct mylite_session_table_lock *lock = &plan->locks[target_index];
    struct table_name_resolution resolution = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const size_t child_count = mylite_sql_ast_node_child_count(target_node);
    const struct mylite_sql_ast_node *table_node = child_at(target_node, 0U);
    const struct mylite_sql_ast_node *alias_node = NULL;
    const struct mylite_sql_ast_node *mode_node = NULL;
    int rc = MYLITE_OK;

    if (target_node == NULL || target_node->kind != MYLITE_SQL_AST_LOCK_TABLE_TARGET ||
        child_count < 2U || child_count > 3U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    alias_node = child_count == 3U ? child_at(target_node, 1U) : NULL;
    mode_node = child_at(target_node, child_count - 1U);

    rc = resolve_visible_table_reference(database, table_node, &resolution, &table);
    if (rc != MYLITE_OK) {
        return rc;
    }

    memcpy(lock->schema_name, resolution.schema.name, sizeof(lock->schema_name));
    memcpy(lock->table_name, resolution.table_name, sizeof(lock->table_name));
    lock->is_temporary = table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY;
    rc = copy_lock_table_alias(database, alias_node, lock);
    if (rc == MYLITE_OK) {
        rc = set_lock_table_mode(database, mode_node, lock);
    }
    if (rc == MYLITE_OK) {
        rc = check_lock_table_duplicate_targets(database, plan, target_index);
    }
    return rc;
}

static int copy_lock_table_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias_node,
    struct mylite_session_table_lock *out_lock
) {
    int rc = MYLITE_OK;

    if (alias_node == NULL) {
        memcpy(out_lock->effective_name, out_lock->table_name, sizeof(out_lock->effective_name));
        return MYLITE_OK;
    }

    rc = copy_identifier_text(alias_node, out_lock->alias, sizeof(out_lock->alias), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_lock->has_alias = true;
    memcpy(out_lock->effective_name, out_lock->alias, sizeof(out_lock->effective_name));
    return MYLITE_OK;
}

static int set_lock_table_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *mode_node,
    struct mylite_session_table_lock *out_lock
) {
    if (mode_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (mode_node->kind) {
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK:
        out_lock->mode = MYLITE_SESSION_TABLE_LOCK_READ;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK:
        out_lock->mode = MYLITE_SESSION_TABLE_LOCK_READ_LOCAL;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK:
        out_lock->mode = MYLITE_SESSION_TABLE_LOCK_WRITE;
        return MYLITE_OK;
    default:
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
}

static int check_lock_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_lock_tables *plan,
    size_t target_index
) {
    const struct mylite_session_table_lock *target = &plan->locks[target_index];

    for (size_t previous_index = 0U; previous_index < target_index; ++previous_index) {
        const struct mylite_session_table_lock *previous = &plan->locks[previous_index];

        if (strcmp(previous->effective_name, target->effective_name) == 0) {
            set_duplicate_table_alias_error(database, target->effective_name);
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int apply_lock_tables_plan(struct mylite_db *database, struct planned_lock_tables *plan) {
    free(database->session.table_locks);
    database->session.table_locks = plan->locks;
    database->session.table_lock_count = plan->lock_count;
    database->session.table_lock_capacity = plan->lock_count;
    plan->locks = NULL;
    plan->lock_count = 0U;
    return MYLITE_OK;
}

static void planned_lock_tables_deinit(struct planned_lock_tables *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->locks);
    plan->locks = NULL;
    plan->lock_count = 0U;
}

static void clear_session_table_locks(struct mylite_db *database) {
    if (database == NULL || database->session.table_locks == NULL) {
        return;
    }

    memset(
        database->session.table_locks,
        0,
        database->session.table_lock_count * sizeof(*database->session.table_locks)
    );
    database->session.table_lock_count = 0U;
}

static bool statement_requires_implicit_user_transaction_commit(
    const struct mylite_sql_ast_node *statement
) {
    if (statement == NULL) {
        return false;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT:
    case MYLITE_SQL_AST_CREATE_VIEW_STATEMENT:
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_VIEW_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
    case MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CHECK_TABLE_STATEMENT:
    case MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CHECKSUM_TABLE_STATEMENT:
        return true;
    default:
        return false;
    }
}

static int commit_active_user_transaction_for_ddl(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (!database->session.user_transaction_active) {
        return MYLITE_OK;
    }

    rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
    if (rc == MYLITE_OK) {
        database->session.user_transaction_active = false;
        clear_active_transaction_characteristics(database);
        clear_user_savepoints(database);
    }
    return rc;
}

static void clear_user_savepoints(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    database->session.savepoint_count = 0U;
}

static int create_or_replace_user_savepoint(struct mylite_db *database, const char *name) {
    char sqlite_name[MYLITE_SESSION_SAVEPOINT_INTERNAL_NAME_CAPACITY];
    size_t existing_index = 0U;
    bool has_existing = false;
    int rc = MYLITE_OK;

    has_existing = find_user_savepoint(&database->session, name, &existing_index);
    rc = reserve_user_savepoints(database, database->session.savepoint_count + 1U);
    if (rc == MYLITE_OK) {
        rc = format_next_sqlite_savepoint_name(database, sqlite_name, sizeof(sqlite_name));
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_savepoint_control(database, "SAVEPOINT ", sqlite_name);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (has_existing) {
        remove_user_savepoint_at(&database->session, existing_index);
    }

    rc = append_user_savepoint(
        database,
        (struct user_savepoint_values){
            .name = name,
            .sqlite_name = sqlite_name,
        }
    );
    if (rc == MYLITE_OK) {
        ++database->session.next_savepoint_id;
    }
    return rc;
}

static int rollback_to_user_savepoint(struct mylite_db *database, const char *name) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (!database->session.user_transaction_active ||
        !find_user_savepoint(&database->session, name, &index)) {
        set_savepoint_does_not_exist_error(database, name);
        return MYLITE_ERROR;
    }

    rc = execute_sqlite_savepoint_control(
        database,
        "ROLLBACK TO SAVEPOINT ",
        database->session.savepoints[index].sqlite_name
    );
    if (rc == MYLITE_OK) {
        database->session.savepoint_count = index + 1U;
    }
    return rc;
}

static int release_user_savepoint(struct mylite_db *database, const char *name) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (!database->session.user_transaction_active ||
        !find_user_savepoint(&database->session, name, &index)) {
        set_savepoint_does_not_exist_error(database, name);
        return MYLITE_ERROR;
    }

    rc = execute_sqlite_savepoint_control(
        database,
        "RELEASE SAVEPOINT ",
        database->session.savepoints[index].sqlite_name
    );
    if (rc == MYLITE_OK) {
        database->session.savepoint_count = index;
    }
    return rc;
}

static int copy_savepoint_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
) {
    return copy_identifier_text(node, destination, destination_size, database);
}

static void fold_savepoint_name(const char *source, char *destination, size_t destination_size) {
    size_t index = 0U;

    if (destination == NULL || destination_size == 0U) {
        return;
    }
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    for (; index + 1U < destination_size && source[index] != '\0'; ++index) {
        destination[index] = ascii_lower((unsigned char)source[index]);
    }
    destination[index] = '\0';
}

static bool find_user_savepoint(
    const struct mylite_session_state *session,
    const char *name,
    size_t *out_index
) {
    char folded_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];

    fold_savepoint_name(name, folded_name, sizeof(folded_name));
    for (size_t index = 0U; index < session->savepoint_count; ++index) {
        if (strcmp(session->savepoints[index].folded_name, folded_name) == 0) {
            if (out_index != NULL) {
                *out_index = index;
            }
            return true;
        }
    }
    return false;
}

static int reserve_user_savepoints(struct mylite_db *database, size_t required_capacity) {
    struct mylite_session_state *session = &database->session;
    struct mylite_session_savepoint *savepoints = NULL;
    size_t new_capacity = 0U;

    if (required_capacity <= session->savepoint_capacity) {
        return MYLITE_OK;
    }

    new_capacity = session->savepoint_capacity == 0U ? 4U : session->savepoint_capacity;
    while (new_capacity < required_capacity) {
        if (new_capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        new_capacity *= 2U;
    }

    savepoints = realloc(session->savepoints, new_capacity * sizeof(session->savepoints[0]));
    if (savepoints == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    session->savepoints = savepoints;
    session->savepoint_capacity = new_capacity;
    return MYLITE_OK;
}

static int format_next_sqlite_savepoint_name(
    struct mylite_db *database,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (database->session.next_savepoint_id == UINT64_MAX) {
        set_runtime_error(database, "too many savepoints in this transaction");
        return MYLITE_ERROR;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_savepoint_%" PRIu64 "_%" PRIu64,
        database->session.connection_id,
        database->session.next_savepoint_id
    );
    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "failed to format savepoint name");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int execute_sqlite_savepoint_control(
    struct mylite_db *database,
    const char *prefix,
    const char *sqlite_name
) {
    char *sql = NULL;
    int rc = build_savepoint_control_sql(
        (struct savepoint_control_sql_request){
            .prefix = prefix,
            .sqlite_name = sqlite_name,
        },
        &sql
    );

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else {
            set_runtime_error(database, "failed to build savepoint control SQL");
        }
        return rc;
    }

    rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, sql));
    free(sql);
    return rc;
}

static int build_savepoint_control_sql(
    struct savepoint_control_sql_request request,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, request.prefix);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, request.sqlite_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static void remove_user_savepoint_at(struct mylite_session_state *session, size_t index) {
    if (index >= session->savepoint_count) {
        return;
    }

    if (index + 1U < session->savepoint_count) {
        memmove(
            &session->savepoints[index],
            &session->savepoints[index + 1U],
            (session->savepoint_count - index - 1U) * sizeof(session->savepoints[0])
        );
    }
    --session->savepoint_count;
}

static int append_user_savepoint(struct mylite_db *database, struct user_savepoint_values values) {
    struct mylite_session_state *session = &database->session;
    struct mylite_session_savepoint *savepoint = NULL;
    int written = 0;

    if (session->savepoint_count >= session->savepoint_capacity) {
        set_runtime_error(database, "savepoint registry capacity was not reserved");
        return MYLITE_ERROR;
    }

    savepoint = &session->savepoints[session->savepoint_count];
    written = snprintf(savepoint->name, sizeof(savepoint->name), "%s", values.name);
    if (written < 0 || (size_t)written >= sizeof(savepoint->name)) {
        set_identifier_too_long_error(database, "savepoint");
        return MYLITE_ERROR;
    }
    fold_savepoint_name(savepoint->name, savepoint->folded_name, sizeof(savepoint->folded_name));
    written =
        snprintf(savepoint->sqlite_name, sizeof(savepoint->sqlite_name), "%s", values.sqlite_name);
    if (written < 0 || (size_t)written >= sizeof(savepoint->sqlite_name)) {
        set_runtime_error(database, "failed to store savepoint name");
        return MYLITE_ERROR;
    }

    ++session->savepoint_count;
    return MYLITE_OK;
}

static int begin_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    int rc = MYLITE_OK;

    transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
    transaction->active = false;
    if (database->session.user_transaction_active) {
        rc = normalize_sqlite_control_rc(
            database,
            execute_sqlite_control_sql(database, "SAVEPOINT _mylite_statement")
        );
        if (rc == MYLITE_OK) {
            transaction->kind = MYLITE_STATEMENT_TRANSACTION_SAVEPOINT;
            transaction->active = true;
        }
        return rc;
    }

    rc = normalize_sqlite_control_rc(
        database,
        execute_sqlite_control_sql(database, "BEGIN IMMEDIATE")
    );
    if (rc == MYLITE_OK) {
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_DIRECT;
        transaction->active = true;
    }
    return rc;
}

static int commit_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    int rc = MYLITE_OK;

    if (!transaction->active) {
        return MYLITE_OK;
    }

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        rc = normalize_sqlite_control_rc(
            database,
            execute_sqlite_control_sql(database, "RELEASE SAVEPOINT _mylite_statement")
        );
    } else {
        rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
    }
    if (rc == MYLITE_OK) {
        transaction->active = false;
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
    }
    return rc;
}

static void rollback_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    if (!transaction->active) {
        return;
    }

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK TO SAVEPOINT _mylite_statement");
        (void)execute_sqlite_control_sql(database, "RELEASE SAVEPOINT _mylite_statement");
    } else {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    transaction->active = false;
    transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
}

static int normalize_sqlite_control_rc(struct mylite_db *database, int rc) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    set_physical_sqlite_error(database);
    return MYLITE_ERROR;
}

static int execute_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = apply_set_connection_character_set_statement(database, statement, allow_collation);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int apply_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation
) {
    const struct mylite_sql_ast_node *collation = NULL;
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_collation *collation_descriptor = NULL;
    int rc = validate_set_connection_character_set_statement(database, statement, allow_collation);

    if (rc != MYLITE_OK) {
        return rc;
    }

    collation = child_at(statement, 1U);
    collation_descriptor = utf8mb4_collation_by_name(MYLITE_CATALOG_DEFAULT_TABLE_COLLATION);
    if (collation != NULL) {
        rc = copy_table_option_name_text(
            database,
            collation,
            collation_name,
            sizeof(collation_name),
            (struct table_option_name_policy){
                .identifier_kind = "collation",
                .nul_message = "SET collation names do not support NUL bytes",
            }
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        collation_descriptor = utf8mb4_collation_by_name(collation_name);
        if (collation_descriptor == NULL) {
            set_unknown_collation_error(database, collation_name);
            return MYLITE_ERROR;
        }
    }
    if (collation_descriptor == NULL) {
        set_runtime_error(database, "invalid default utf8mb4 collation");
        return MYLITE_ERROR;
    }

    return set_session_connection_character_set(database, collation_descriptor->name);
}

static int validate_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *collation = NULL;
    int rc = MYLITE_OK;

    if (statement == NULL || (statement->kind != MYLITE_SQL_AST_SET_NAMES_STATEMENT &&
                              statement->kind != MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target = child_at(statement, 0U);
    collation = child_at(statement, 1U);
    if (collation != NULL && !allow_collation) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = validate_set_connection_character_set_target(database, target);
    if (rc == MYLITE_OK && collation != NULL) {
        rc = validate_set_names_collation_target(database, collation);
    }
    return rc;
}

static int validate_set_connection_character_set_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET) {
        return MYLITE_OK;
    }

    rc = copy_table_option_name_text(
        database,
        target,
        charset_name,
        sizeof(charset_name),
        (struct table_option_name_policy){
            .identifier_kind = "character set",
            .nul_message = "SET character set names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(charset_name, "utf8mb4")) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_set_names_collation_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = copy_table_option_name_text(
        database,
        target,
        collation_name,
        sizeof(collation_name),
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "SET collation names do not support NUL bytes",
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (utf8mb4_collation_by_name(collation_name) == NULL) {
        if (collation_name_is_known_unsupported(collation_name) ||
            collation_by_name(collation_name) != NULL) {
            set_collation_not_valid_for_charset_error(database, collation_name, "utf8mb4");
            return MYLITE_ERROR;
        }
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int set_session_connection_character_set(
    struct mylite_db *database,
    const char *collation_name
) {
    const char *charset_name = MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
    size_t charset_length = strlen(charset_name);
    size_t collation_length = strlen(collation_name);

    if (database == NULL || collation_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (charset_length >= sizeof(database->session.character_set_client) ||
        collation_length >= sizeof(database->session.collation_connection)) {
        set_runtime_error(database, "connection character set state is too small");
        return MYLITE_ERROR;
    }

    memcpy(database->session.character_set_client, charset_name, charset_length + 1U);
    memcpy(database->session.character_set_connection, charset_name, charset_length + 1U);
    memcpy(database->session.character_set_results, charset_name, charset_length + 1U);
    memcpy(database->session.collation_connection, collation_name, collation_length + 1U);
    database->session.character_set_state_is_placeholder = false;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    struct set_session_snapshot snapshot = {0};
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int rc = MYLITE_OK;

    if (statement == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (statement->kind == MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT) {
        return apply_set_system_variable_assignment(database, statement);
    }
    if (statement->kind != MYLITE_SQL_AST_SET_STATEMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_set_session_snapshot(database, &snapshot);
    if (rc != MYLITE_OK) {
        return rc;
    }

    assignment_list = child_at(statement, 0U);
    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_SET_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        restore_set_session_snapshot(database, &snapshot);
        return MYLITE_ERROR;
    }
    assignment = child_at(assignment_list, 0U);
    while (rc == MYLITE_OK && assignment != NULL) {
        rc = apply_set_assignment(database, assignment);
        assignment = assignment->next_sibling;
    }
    if (rc != MYLITE_OK) {
        restore_set_session_snapshot(database, &snapshot);
        return rc;
    }

    deinit_set_session_snapshot(&snapshot);
    return MYLITE_OK;
}

static int apply_set_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
) {
    const struct mylite_sql_ast_node *target = NULL;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target = child_at(assignment, 0U);
    if (target != NULL && target->kind == MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET) {
        return apply_set_system_variable_assignment(database, assignment);
    }
    if (target != NULL && target->kind == MYLITE_SQL_AST_USER_VARIABLE) {
        return apply_set_user_variable_assignment(database, assignment);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_set_system_variable_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
) {
    struct resolved_set_system_variable_target target = {0};
    const struct mylite_sql_ast_node *target_node = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    bool expected_boolean_value = false;
    bool handled = false;
    int rc = MYLITE_OK;

    if (assignment == NULL || (assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT &&
                               assignment->kind != MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target_node = child_at(assignment, 0U);
    value_node = child_at(assignment, 1U);
    rc = resolve_set_system_variable_target(database, target_node, &target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        apply_set_system_variable_user_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM ||
        target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES ||
        target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY ||
        mylite_execution_system_variable_is_read_only_server_build(target.kind) ||
        mylite_execution_system_variable_is_read_only_server_environment(target.kind)) {
        set_read_only_system_variable_error(database, target.name);
        return MYLITE_ERROR;
    }
    rc = apply_set_server_identity_binary_log_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_system_variable_is_global_read_only_toggle(target.kind)) {
        if (target.scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target.name);
            return MYLITE_ERROR;
        }
        return validate_set_fixed_boolean_value(database, value_node, false);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION ||
        target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY) {
        return apply_set_transaction_system_variable_value(database, &target, value_node);
    }
    rc = apply_set_integer_system_variable_value(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS) {
        return apply_set_foreign_key_checks_value(database, &target, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY) {
        return apply_set_sql_require_primary_key_value(database, &target, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES) {
        return apply_set_big_tables_value(database, &target, value_node);
    }
    if (target.scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }
    if (mylite_execution_system_variable_fixed_boolean_value(
            target.kind,
            &expected_boolean_value
        )) {
        return validate_set_fixed_boolean_value(database, value_node, expected_boolean_value);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE) {
        return apply_set_sql_mode_value(database, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP) {
        return apply_set_timestamp_value(database, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE) {
        return apply_set_time_zone_value(database, value_node);
    }

    set_read_only_system_variable_error(database, target.name);
    return MYLITE_ERROR;
}

static int apply_set_server_identity_binary_log_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    uint64_t expected_value = 0U;
    const char *unsupported_message = NULL;
    int rc = MYLITE_OK;

    *out_handled = false;
    if (mylite_execution_system_variable_is_read_only_server_identity_binary_log(target->kind)) {
        *out_handled = true;
        set_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (fixed_global_uint64_server_identity_system_variable_value(
            target->kind,
            &expected_value,
            &unsupported_message
        )) {
        *out_handled = true;
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            expected_value,
            unsupported_message
        );
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) {
        *out_handled = true;
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        rc = validate_set_fixed_boolean_value(database, value_node, false);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_system_variable_read_warning(database, target->kind);
    }

    return MYLITE_OK;
}

static int apply_set_system_variable_user_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(value_node);
    struct session_scalar_cell value = {0};
    enum mylite_session_user_variable_value_kind value_kind =
        MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (unwrapped == NULL || unwrapped->kind != MYLITE_SQL_AST_USER_VARIABLE) {
        return MYLITE_OK;
    }
    *out_handled = true;
    rc = session_user_variable_value(database, unwrapped, &value);
    if (rc == MYLITE_OK) {
        rc = session_user_variable_value_kind(database, unwrapped, &value_kind);
    }
    if (rc == MYLITE_OK) {
        rc = apply_set_system_variable_cell_value(database, target, &value, value_kind);
    }
    session_scalar_cell_deinit(&value);
    return rc;
}

static int apply_set_integer_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = true;
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET) {
        return apply_set_max_allowed_packet_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT ||
        target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET) {
        return apply_set_auto_increment_step_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT) {
        return apply_set_sql_select_limit_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN) {
        return apply_set_group_concat_max_len_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY) {
        return apply_set_information_schema_stats_expiry_value(database, target, value_node);
    }
    if (mylite_execution_system_variable_is_timeout(target->kind)) {
        return apply_set_timeout_system_variable_value(database, target, value_node);
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int apply_set_user_variable_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    struct session_scalar_cell value = {0};
    enum mylite_session_user_variable_value_kind value_kind =
        MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    int rc = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target = child_at(assignment, 0U);
    value_node = child_at(assignment, 1U);
    rc = evaluate_user_variable_assignment_value(database, value_node, &value, &value_kind);
    if (rc == MYLITE_OK) {
        rc = set_session_user_variable(database, target, &value, value_kind);
    }
    session_scalar_cell_deinit(&value);
    return rc;
}

static int evaluate_user_variable_assignment_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct session_scalar_cell *out_cell,
    enum mylite_session_user_variable_value_kind *out_value_kind
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL || out_value_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    *out_value_kind = MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    if (unwrapped == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(unwrapped);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal(
                database,
                unwrapped,
                "SET user variables support only string literals",
                "SET user variable string values do not support NUL bytes",
                &out_cell->owned_text,
                &decoded_length
            );
            if (rc == MYLITE_OK) {
                out_cell->value = out_cell->owned_text;
                out_cell->value_size = decoded_length;
                out_cell->has_value_size = true;
                *out_value_kind = MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL) {
            return copy_user_variable_source_text_value(
                database,
                unwrapped,
                MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL,
                out_cell,
                out_value_kind
            );
        }
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        const struct mylite_sql_ast_node *unary_value = child_at(unwrapped, 0U);

        if (unary_value != NULL && unary_value->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(unary_value) == MYLITE_SQL_AST_LITERAL_DECIMAL) {
            return copy_user_variable_source_text_value(
                database,
                mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_POSITIVE
                    ? unary_value
                    : unwrapped,
                MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL,
                out_cell,
                out_value_kind
            );
        }
    }

    switch (unwrapped->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_USER_VARIABLE:
        rc = session_scalar_value(database, unwrapped, out_cell);
        if (rc == MYLITE_OK) {
            *out_value_kind = infer_user_variable_value_kind(unwrapped, out_cell);
            if (unwrapped->kind == MYLITE_SQL_AST_USER_VARIABLE) {
                rc = session_user_variable_value_kind(database, unwrapped, out_value_kind);
            }
        }
        return rc;
    default:
        set_unsupported_error(
            database,
            "SET user variables support only integer, decimal, string, boolean, NULL, and "
            "variable values"
        );
        return MYLITE_ERROR;
    }
}

static int copy_user_variable_source_text_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_session_user_variable_value_kind value_kind,
    struct session_scalar_cell *out_cell,
    enum mylite_session_user_variable_value_kind *out_value_kind
) {
    const struct mylite_sql_source_span *span = value_node == NULL ? NULL : &value_node->span;

    if (out_cell == NULL || out_value_kind == NULL) {
        return MYLITE_MISUSE;
    }
    if (span == NULL || span->text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_cell->owned_text = (char *)malloc(span->length + 1U);
    if (out_cell->owned_text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, span->text, span->length);
    out_cell->owned_text[span->length] = '\0';
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = span->length;
    out_cell->has_value_size = true;
    *out_value_kind = value_kind;
    return MYLITE_OK;
}

static int set_session_user_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    const char *source_value = value == NULL ? NULL : value->value;
    size_t source_size = 0U;
    char *owned_value = NULL;
    int rc = copy_user_variable_name(database, target, name, sizeof(name));

    if (rc != MYLITE_OK) {
        return rc;
    }

    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL) {
        rc = ensure_session_user_variable_capacity(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        variable = &database->session.user_variables[database->session.user_variable_count];
        *variable = (struct mylite_session_user_variable){0};
        memcpy(variable->name, name, strlen(name) + 1U);
        ++database->session.user_variable_count;
    }

    if (source_value != NULL) {
        if (value->has_value_size) {
            source_size = value->value_size;
        } else {
            source_size = strlen(source_value);
        }
        owned_value = (char *)malloc(source_size + 1U);
        if (owned_value == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(owned_value, source_value, source_size);
        owned_value[source_size] = '\0';
    }

    free(variable->value);
    variable->value = owned_value;
    variable->value_size = source_size;
    variable->is_null = source_value == NULL;
    variable->value_kind =
        source_value == NULL ? MYLITE_SESSION_USER_VARIABLE_VALUE_NULL : value_kind;
    return MYLITE_OK;
}

static int session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = copy_user_variable_name(database, node, name, sizeof(name));
    if (rc != MYLITE_OK) {
        return rc;
    }
    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL || variable->is_null) {
        return MYLITE_OK;
    }

    out_cell->value = variable->value;
    out_cell->value_size = variable->value_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int session_user_variable_value_kind(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    enum mylite_session_user_variable_value_kind *out_value_kind
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    int rc = MYLITE_OK;

    if (out_value_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value_kind = MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    rc = copy_user_variable_name(database, node, name, sizeof(name));
    if (rc != MYLITE_OK) {
        return rc;
    }
    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL || variable->is_null) {
        return MYLITE_OK;
    }

    *out_value_kind = variable->value_kind;
    return MYLITE_OK;
}

static int copy_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size
) {
    const struct mylite_sql_source_span *span = node == NULL ? NULL : &node->span;
    char quote = '\0';
    int rc = MYLITE_OK;

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    buffer[0] = '\0';
    if (node == NULL || node->kind != MYLITE_SQL_AST_USER_VARIABLE || span == NULL ||
        span->text == NULL || span->length < 2U || span->text[0] != '@') {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    quote = span->text[1];
    if (quote == '\'' || quote == '"' || quote == '`') {
        rc = copy_quoted_user_variable_name(database, span, quote, buffer, buffer_size);
    } else {
        rc = copy_unquoted_user_variable_name(database, span, buffer, buffer_size);
    }
    if (rc == MYLITE_OK) {
        fold_user_variable_name(buffer);
    }
    return rc;
}

static int copy_unquoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size
) {
    size_t name_length = span == NULL ? 0U : span->length - 1U;

    if (span == NULL || span->text == NULL || name_length == 0U || name_length >= buffer_size) {
        return set_illegal_user_variable_name_error(
            database,
            span == NULL || span->text == NULL ? "" : span->text + 1U,
            name_length
        );
    }
    memcpy(buffer, span->text + 1U, name_length);
    buffer[name_length] = '\0';
    return validate_user_variable_name(database, buffer, name_length);
}

static int copy_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size
) {
    if (span == NULL || span->text == NULL || span->length < 3U) {
        return set_illegal_user_variable_name_error(database, "", 0U);
    }
    if (quote == '\'' || quote == '"') {
        return copy_string_quoted_user_variable_name(database, span, buffer, buffer_size);
    }
    return copy_identifier_quoted_user_variable_name(database, span, quote, buffer, buffer_size);
}

static int copy_string_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    struct mylite_sql_ast_node literal = {
        .kind = MYLITE_SQL_AST_LITERAL,
        .span = {.text = span->text + 1U, .length = span->length - 1U},
        .payload = {.literal = {.kind = MYLITE_SQL_AST_LITERAL_STRING}},
    };
    int rc = decode_sql_string_literal(
        database,
        &literal,
        "SET user variable names support only quoted variable names",
        "SET user variable names do not support NUL bytes",
        &decoded,
        &decoded_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (decoded_length == 0U || decoded_length >= buffer_size) {
        rc = set_illegal_user_variable_name_error(database, decoded, decoded_length);
        free(decoded);
        return rc;
    }
    memcpy(buffer, decoded, decoded_length);
    buffer[decoded_length] = '\0';
    rc = validate_user_variable_name(database, buffer, decoded_length);
    free(decoded);
    return rc;
}

static int copy_identifier_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size
) {
    size_t name_length = 0U;

    for (size_t index = 2U; index < span->length; ++index) {
        char byte = span->text[index];

        if (byte == quote) {
            if (index + 1U < span->length && span->text[index + 1U] == quote) {
                int rc = append_user_variable_name_byte(
                    database,
                    buffer,
                    buffer_size,
                    &name_length,
                    byte
                );

                if (rc != MYLITE_OK) {
                    return rc;
                }
                ++index;
                continue;
            }
            if (name_length == 0U || index + 1U != span->length) {
                return set_illegal_user_variable_name_error(database, buffer, name_length);
            }
            buffer[name_length] = '\0';
            return validate_user_variable_name(database, buffer, name_length);
        }
        {
            int rc =
                append_user_variable_name_byte(database, buffer, buffer_size, &name_length, byte);

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
    }

    return set_illegal_user_variable_name_error(database, buffer, name_length);
}

static int append_user_variable_name_byte(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    size_t *inout_length,
    char byte
) {
    if (buffer == NULL || inout_length == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    if (byte == '\0' || *inout_length + 1U >= buffer_size) {
        return set_illegal_user_variable_name_error(database, buffer, *inout_length);
    }

    buffer[*inout_length] = byte;
    ++*inout_length;
    buffer[*inout_length] = '\0';
    return MYLITE_OK;
}

static int validate_user_variable_name(
    struct mylite_db *database,
    const char *name,
    size_t name_length
) {
    size_t character_count = 0U;

    if (name == NULL) {
        return MYLITE_MISUSE;
    }
    if (name_length == 0U || name_length > MYLITE_SESSION_USER_VARIABLE_NAME_MAX_BYTES ||
        validate_utf8_text(name, name_length, &character_count) != MYLITE_OK ||
        character_count == 0U ||
        character_count > MYLITE_SESSION_USER_VARIABLE_NAME_MAX_CHARACTERS) {
        return set_illegal_user_variable_name_error(database, name, name_length);
    }
    return MYLITE_OK;
}

static int set_illegal_user_variable_name_error(
    struct mylite_db *database,
    const char *name,
    size_t name_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int printable_length = name_length > INT_MAX ? INT_MAX : (int)name_length;
    int written = snprintf(
        message,
        sizeof(message),
        "User variable name '%.*s' is illegal",
        printable_length,
        name == NULL ? "" : name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_illegal_user_variable_name,
        "42000",
        message
    );
    return MYLITE_ERROR;
}

static void fold_user_variable_name(char *text) {
    if (text == NULL) {
        return;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        text[index] = ascii_lower((unsigned char)text[index]);
    }
}

static struct mylite_session_user_variable *find_session_user_variable(
    struct mylite_session_state *session,
    const char *name
) {
    if (session == NULL || name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < session->user_variable_count; ++index) {
        if (strcmp(session->user_variables[index].name, name) == 0) {
            return &session->user_variables[index];
        }
    }
    return NULL;
}

static bool text_is_decimal_integer_literal(const char *text, size_t text_size) {
    size_t index = 0U;

    if (text == NULL || text_size == 0U) {
        return false;
    }
    if (text[index] == '+' || text[index] == '-') {
        ++index;
        if (index == text_size) {
            return false;
        }
    }
    for (; index < text_size; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}

static enum mylite_session_user_variable_value_kind infer_user_variable_value_kind(
    const struct mylite_sql_ast_node *value_node,
    const struct session_scalar_cell *value
) {
    const struct mylite_sql_ast_node *literal = NULL;
    size_t value_size = 0U;

    if (value == NULL || value->value == NULL) {
        return MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    }

    value_node = unwrap_parenthesized_expression(value_node);
    literal = value_node;
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
        switch (mylite_sql_ast_node_literal_kind(literal)) {
        case MYLITE_SQL_AST_LITERAL_INTEGER:
        case MYLITE_SQL_AST_LITERAL_TRUE:
        case MYLITE_SQL_AST_LITERAL_FALSE:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER;
        case MYLITE_SQL_AST_LITERAL_NULL:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
        case MYLITE_SQL_AST_LITERAL_STRING:
        case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        case MYLITE_SQL_AST_LITERAL_HEX:
        case MYLITE_SQL_AST_LITERAL_BIT:
        case MYLITE_SQL_AST_LITERAL_NONE:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
        case MYLITE_SQL_AST_LITERAL_DECIMAL:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL;
        case MYLITE_SQL_AST_LITERAL_FLOAT:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
        }
    }

    if (value->has_value_size) {
        value_size = value->value_size;
    } else {
        value_size = strlen(value->value);
    }
    if (text_is_decimal_integer_literal(value->value, value_size)) {
        return MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER;
    }
    return MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
}

static int ensure_session_user_variable_capacity(struct mylite_db *database) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *variables = NULL;
    size_t new_capacity = 0U;

    if (session == NULL) {
        return MYLITE_MISUSE;
    }
    if (session->user_variable_count < session->user_variable_capacity) {
        return MYLITE_OK;
    }
    new_capacity =
        session->user_variable_capacity == 0U ? 4U : session->user_variable_capacity * 2U;
    if (new_capacity < session->user_variable_capacity ||
        new_capacity > SIZE_MAX / sizeof(*variables)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    variables = (struct mylite_session_user_variable *)
        realloc(session->user_variables, new_capacity * sizeof(*variables));
    if (variables == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memset(
        variables + session->user_variable_capacity,
        0,
        (new_capacity - session->user_variable_capacity) * sizeof(*variables)
    );
    session->user_variables = variables;
    session->user_variable_capacity = new_capacity;
    return MYLITE_OK;
}

static int prepare_statement_source_sql(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    char **out_sql,
    size_t *out_sql_size
) {
    struct session_scalar_cell value = {0};
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    source = unwrap_parenthesized_expression(source);
    if (source == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (source->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(source) == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_sql_string_literal(
            database,
            source,
            "PREPARE supports only string literal or user-variable source text",
            "PREPARE source text does not support NUL bytes",
            out_sql,
            out_sql_size
        );
    }
    if (source->kind != MYLITE_SQL_AST_USER_VARIABLE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = session_user_variable_value(database, source, &value);
    if (rc == MYLITE_OK) {
        rc = copy_session_scalar_sql_text(database, &value, "NULL", out_sql, &decoded_length);
    }
    if (rc == MYLITE_OK) {
        *out_sql_size = decoded_length;
    }
    session_scalar_cell_deinit(&value);
    return rc;
}

static int copy_session_scalar_sql_text(
    struct mylite_db *database,
    const struct session_scalar_cell *value,
    const char *null_text,
    char **out_sql,
    size_t *out_sql_size
) {
    const char *source = NULL;
    size_t source_size = 0U;
    char *copy = NULL;

    if (out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    if (value == NULL || value->value == NULL) {
        source = null_text;
        source_size = strlen(null_text);
    } else {
        source = value->value;
        if (value->has_value_size) {
            source_size = value->value_size;
        } else {
            source_size = strlen(value->value);
        }
    }
    if (source_size > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy = (char *)malloc(source_size + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, source, source_size);
    copy[source_size] = '\0';

    *out_sql = copy;
    *out_sql_size = source_size;
    return MYLITE_OK;
}

static int validate_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    size_t *out_parameter_count
) {
    struct mylite_sql_parse_result parse_result;
    struct prepared_statement_expanded_sql normalized_sql = {0};
    const struct mylite_sql_ast_node *prepared_statement = NULL;
    size_t statement_count = 0U;
    int rc = MYLITE_OK;

    if (out_parameter_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_parameter_count = 0U;

    rc = build_prepared_statement_sql(database, sql, sql_size, NULL, &normalized_sql);
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_parameter_count = normalized_sql.parameter_count;

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = normalized_sql.text,
            .length = normalized_sql.text_size,
            .modes = lexer_modes_for_session_sql_mode(&database->session),
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        free(normalized_sql.text);
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count != 1U) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        prepared_statement = child_at(parse_result.root, 0U);
        if (prepared_statement_disallows_statement(prepared_statement)) {
            set_prepared_statement_command_unsupported_error(database);
            rc = MYLITE_ERROR;
        }
    }

    mylite_sql_parse_result_deinit(&parse_result);
    free(normalized_sql.text);
    return rc;
}

static int build_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    const struct mylite_sql_ast_node *using_list,
    struct prepared_statement_expanded_sql *out_sql
) {
    struct mylite_sql_lexer lexer;
    struct mylite_sql_token token;
    struct mylite_dynamic_string output;
    size_t cursor = 0U;
    size_t parameter_index = 0U;
    int rc = MYLITE_OK;

    if (sql == NULL || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = (struct prepared_statement_expanded_sql){0};

    mylite_dynamic_string_init(&output);
    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = sql,
            .length = sql_size,
            .modes = lexer_modes_for_session_sql_mode(&database->session),
        }
    );

    for (;;) {
        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            break;
        }
        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            struct mylite_sql_parse_result parse_result = {0};

            parse_result.error_token = token;
            set_parse_error(database, &parse_result);
            rc = MYLITE_ERROR;
            break;
        }
        if (token.kind != MYLITE_SQL_TOKEN_PARAMETER) {
            continue;
        }

        rc = mylite_dynamic_string_append_bytes(&output, &sql[cursor], token.offset - cursor);
        if (rc == MYLITE_OK) {
            if (using_list == NULL) {
                rc = mylite_dynamic_string_append(&output, "NULL");
            } else {
                rc = append_execute_parameter_sql(
                    database,
                    &output,
                    child_at(using_list, parameter_index)
                );
            }
        }
        if (rc != MYLITE_OK) {
            break;
        }
        cursor = token.offset + token.length;
        ++parameter_index;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&output, &sql[cursor], sql_size - cursor);
    }
    if (rc == MYLITE_OK) {
        out_sql->text_size = output.length;
        out_sql->parameter_count = parameter_index;
        out_sql->text = mylite_dynamic_string_take(&output);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    mylite_dynamic_string_deinit(&output);
    return rc;
}

static int append_execute_parameter_sql(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *variable
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *value = NULL;
    int rc = copy_user_variable_name(database, variable, name, sizeof(name));

    if (rc != MYLITE_OK) {
        return rc;
    }

    value = find_session_user_variable(&database->session, name);
    if (value == NULL || value->is_null || value->value == NULL) {
        return mylite_dynamic_string_append(string, "NULL");
    }
    if (value->value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER &&
        text_is_decimal_integer_literal(value->value, value->value_size)) {
        return mylite_dynamic_string_append_bytes(string, value->value, value->value_size);
    }
    if (value->value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL) {
        return mylite_dynamic_string_append_bytes(string, value->value, value->value_size);
    }

    return append_execute_string_literal(database, string, value->value, value->value_size);
}

static int append_execute_string_literal(
    const struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
) {
    bool no_backslash_escapes = false;
    int rc = mylite_dynamic_string_append_char(string, '\'');

    if (database != NULL) {
        no_backslash_escapes =
            (database->session.sql_mode & MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES) != 0U;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < text_size; ++index) {
        if (text[index] == '\'') {
            rc = mylite_dynamic_string_append(string, "''");
        } else if (!no_backslash_escapes && text[index] == '\\') {
            rc = mylite_dynamic_string_append(string, "\\\\");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static bool prepared_statement_disallows_statement(const struct mylite_sql_ast_node *statement) {
    if (statement == NULL) {
        return false;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
        return true;
    default:
        return false;
    }
}

static int copy_prepared_statement_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size
) {
    int rc = copy_identifier_text(node, buffer, buffer_size, database);

    if (rc == MYLITE_OK) {
        fold_user_variable_name(buffer);
    }
    return rc;
}

static struct mylite_session_prepared_statement *find_session_prepared_statement(
    struct mylite_session_state *session,
    const char *name
) {
    size_t index = find_session_prepared_statement_index(session, name);

    return index == SIZE_MAX ? NULL : &session->prepared_statements[index];
}

static size_t find_session_prepared_statement_index(
    const struct mylite_session_state *session,
    const char *name
) {
    if (session == NULL || name == NULL) {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < session->prepared_statement_count; ++index) {
        if (strcmp(session->prepared_statements[index].name, name) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void remove_session_prepared_statement_at(
    struct mylite_session_state *session,
    size_t index
) {
    if (session == NULL || index >= session->prepared_statement_count) {
        return;
    }

    deinit_session_prepared_statement(&session->prepared_statements[index]);
    for (size_t move_index = index + 1U; move_index < session->prepared_statement_count;
         ++move_index) {
        session->prepared_statements[move_index - 1U] = session->prepared_statements[move_index];
    }
    --session->prepared_statement_count;
    session->prepared_statements[session->prepared_statement_count] =
        (struct mylite_session_prepared_statement){0};
}

static void deinit_session_prepared_statement(struct mylite_session_prepared_statement *statement) {
    if (statement == NULL) {
        return;
    }

    free(statement->sql);
    *statement = (struct mylite_session_prepared_statement){0};
}

static int ensure_session_prepared_statement_capacity(struct mylite_db *database) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_prepared_statement *statements = NULL;
    size_t new_capacity = 0U;

    if (session == NULL) {
        return MYLITE_MISUSE;
    }
    if (session->prepared_statement_count < session->prepared_statement_capacity) {
        return MYLITE_OK;
    }
    new_capacity =
        session->prepared_statement_capacity == 0U ? 4U : session->prepared_statement_capacity * 2U;
    if (new_capacity < session->prepared_statement_capacity ||
        new_capacity > SIZE_MAX / sizeof(*statements)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    statements = (struct mylite_session_prepared_statement *)
        realloc(session->prepared_statements, new_capacity * sizeof(*statements));
    if (statements == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memset(
        statements + session->prepared_statement_capacity,
        0,
        (new_capacity - session->prepared_statement_capacity) * sizeof(*statements)
    );
    session->prepared_statements = statements;
    session->prepared_statement_capacity = new_capacity;
    return MYLITE_OK;
}

static void set_prepared_statement_argument_count_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_arguments,
        "HY000",
        "Incorrect arguments to EXECUTE"
    );
}

static void set_unknown_prepared_statement_error(
    struct mylite_db *database,
    const char *statement_name,
    const char *command_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Unknown prepared statement handler (%s) given to %s",
        statement_name == NULL ? "" : statement_name,
        command_name == NULL ? "" : command_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_prepared_statement_handler,
        "HY000",
        message
    );
}

static void set_prepared_statement_command_unsupported_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_prepared_command_not_supported,
        "HY000",
        "This command is not supported in the prepared statement protocol yet"
    );
}

static int copy_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *out_snapshot
) {
    const struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *variables = NULL;
    int rc = MYLITE_OK;

    if (session == NULL || out_snapshot == NULL) {
        return MYLITE_MISUSE;
    }
    *out_snapshot = (struct set_session_snapshot){0};
    rc = copy_session_user_variables(database, session, &variables);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_snapshot->sql_mode = session->sql_mode;
    out_snapshot->auto_increment_increment = session->auto_increment_increment;
    out_snapshot->auto_increment_offset = session->auto_increment_offset;
    out_snapshot->sql_select_limit = session->sql_select_limit;
    out_snapshot->group_concat_max_len = session->group_concat_max_len;
    out_snapshot->information_schema_stats_expiry = session->information_schema_stats_expiry;
    out_snapshot->wait_timeout = session->wait_timeout;
    out_snapshot->interactive_timeout = session->interactive_timeout;
    out_snapshot->timestamp_override = session->timestamp_override;
    out_snapshot->time_zone_offset_minutes = session->time_zone_offset_minutes;
    out_snapshot->session_transaction_isolation = session->session_transaction_isolation;
    out_snapshot->session_transaction_access_mode = session->session_transaction_access_mode;
    out_snapshot->next_transaction_isolation = session->next_transaction_isolation;
    out_snapshot->next_transaction_access_mode = session->next_transaction_access_mode;
    out_snapshot->sql_mode_is_placeholder = session->sql_mode_is_placeholder;
    out_snapshot->time_zone_is_placeholder = session->time_zone_is_placeholder;
    out_snapshot->system_variables_are_placeholder = session->system_variables_are_placeholder;
    out_snapshot->big_tables = session->big_tables;
    out_snapshot->foreign_key_checks_enabled = session->foreign_key_checks_enabled;
    out_snapshot->sql_require_primary_key = session->sql_require_primary_key;
    out_snapshot->has_next_transaction_isolation = session->has_next_transaction_isolation;
    out_snapshot->has_next_transaction_access_mode = session->has_next_transaction_access_mode;
    out_snapshot->next_transaction_isolation_from_system_variable =
        session->next_transaction_isolation_from_system_variable;
    out_snapshot->next_transaction_access_mode_from_system_variable =
        session->next_transaction_access_mode_from_system_variable;
    out_snapshot->has_timestamp_override = session->has_timestamp_override;
    memcpy(out_snapshot->time_zone, session->time_zone, sizeof(out_snapshot->time_zone));
    memcpy(
        out_snapshot->sql_mode_text,
        session->sql_mode_text,
        sizeof(out_snapshot->sql_mode_text)
    );
    out_snapshot->user_variables = variables;
    out_snapshot->user_variable_count = session->user_variable_count;
    out_snapshot->user_variable_capacity = session->user_variable_capacity;
    return MYLITE_OK;
}

static int copy_session_user_variables(
    struct mylite_db *database,
    const struct mylite_session_state *source,
    struct mylite_session_user_variable **out_variables
) {
    struct mylite_session_user_variable *copy = NULL;
    const struct mylite_session_user_variable *variables = NULL;
    size_t count = 0U;
    size_t capacity = 0U;

    if (source == NULL || out_variables == NULL) {
        return MYLITE_MISUSE;
    }
    *out_variables = NULL;
    variables = source->user_variables;
    count = source->user_variable_count;
    capacity = source->user_variable_capacity;
    if (capacity == 0U) {
        return MYLITE_OK;
    }
    if (capacity > SIZE_MAX / sizeof(*copy)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy = (struct mylite_session_user_variable *)calloc(capacity, sizeof(*copy));
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < count; ++index) {
        copy[index] = variables[index];
        copy[index].value = NULL;
        if (!variables[index].is_null && variables[index].value != NULL) {
            copy[index].value = (char *)malloc(variables[index].value_size + 1U);
            if (copy[index].value == NULL) {
                free_session_user_variables(copy, index);
                set_nomem_error(database);
                return MYLITE_NOMEM;
            }
            memcpy(copy[index].value, variables[index].value, variables[index].value_size);
            copy[index].value[variables[index].value_size] = '\0';
        }
    }

    *out_variables = copy;
    return MYLITE_OK;
}

static void restore_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *snapshot
) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *current_variables = NULL;
    size_t current_variable_count = 0U;

    if (session == NULL || snapshot == NULL) {
        return;
    }

    current_variables = session->user_variables;
    current_variable_count = session->user_variable_count;

    session->sql_mode = snapshot->sql_mode;
    session->auto_increment_increment = snapshot->auto_increment_increment;
    session->auto_increment_offset = snapshot->auto_increment_offset;
    session->sql_select_limit = snapshot->sql_select_limit;
    session->group_concat_max_len = snapshot->group_concat_max_len;
    session->information_schema_stats_expiry = snapshot->information_schema_stats_expiry;
    session->wait_timeout = snapshot->wait_timeout;
    session->interactive_timeout = snapshot->interactive_timeout;
    session->timestamp_override = snapshot->timestamp_override;
    session->time_zone_offset_minutes = snapshot->time_zone_offset_minutes;
    session->session_transaction_isolation = snapshot->session_transaction_isolation;
    session->session_transaction_access_mode = snapshot->session_transaction_access_mode;
    session->next_transaction_isolation = snapshot->next_transaction_isolation;
    session->next_transaction_access_mode = snapshot->next_transaction_access_mode;
    session->sql_mode_is_placeholder = snapshot->sql_mode_is_placeholder;
    session->time_zone_is_placeholder = snapshot->time_zone_is_placeholder;
    session->system_variables_are_placeholder = snapshot->system_variables_are_placeholder;
    session->big_tables = snapshot->big_tables;
    session->foreign_key_checks_enabled = snapshot->foreign_key_checks_enabled;
    session->sql_require_primary_key = snapshot->sql_require_primary_key;
    session->has_next_transaction_isolation = snapshot->has_next_transaction_isolation;
    session->has_next_transaction_access_mode = snapshot->has_next_transaction_access_mode;
    session->next_transaction_isolation_from_system_variable =
        snapshot->next_transaction_isolation_from_system_variable;
    session->next_transaction_access_mode_from_system_variable =
        snapshot->next_transaction_access_mode_from_system_variable;
    session->has_timestamp_override = snapshot->has_timestamp_override;
    memcpy(session->time_zone, snapshot->time_zone, sizeof(session->time_zone));
    memcpy(session->sql_mode_text, snapshot->sql_mode_text, sizeof(session->sql_mode_text));
    session->user_variables = snapshot->user_variables;
    session->user_variable_count = snapshot->user_variable_count;
    session->user_variable_capacity = snapshot->user_variable_capacity;

    snapshot->user_variables = NULL;
    snapshot->user_variable_count = 0U;
    snapshot->user_variable_capacity = 0U;
    free_session_user_variables(current_variables, current_variable_count);
}

static void deinit_set_session_snapshot(struct set_session_snapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    free_session_user_variables(snapshot->user_variables, snapshot->user_variable_count);
    *snapshot = (struct set_session_snapshot){0};
}

static void free_session_user_variables(
    struct mylite_session_user_variable *variables,
    size_t count
) {
    if (variables == NULL) {
        return;
    }
    for (size_t index = 0U; index < count; ++index) {
        free(variables[index].value);
    }
    free(variables);
}

static int apply_set_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (reject_invalid_read_only_system_variable_cell_target(database, target)) {
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE) {
        return apply_set_sql_mode_cell_value(database, value);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE) {
        return apply_set_time_zone_cell_value(database, value);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT) {
        return apply_set_sql_select_limit_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN) {
        return apply_set_group_concat_max_len_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY) {
        return apply_set_information_schema_stats_expiry_cell_value(
            database,
            target,
            value,
            value_kind
        );
    }
    if (mylite_execution_system_variable_is_timeout(target->kind)) {
        return apply_set_timeout_system_variable_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS) {
        rc = parse_set_boolean_cell_value(database, target->name, value, &boolean_value);
        if (rc == MYLITE_OK) {
            database->session.foreign_key_checks_enabled = boolean_value;
            database->session.system_variables_are_placeholder = false;
        }
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY) {
        return apply_set_sql_require_primary_key_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES) {
        return apply_set_big_tables_cell_value(database, target, value, value_kind);
    }
    if (fixed_global_uint64_server_identity_system_variable_value(target->kind, NULL, NULL)) {
        set_unsupported_error(
            database,
            "SET server identity system variables from user variables are not supported"
        );
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) {
        rc = parse_set_boolean_cell_value(database, target->name, value, &boolean_value);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (boolean_value) {
            set_unsupported_error(
                database,
                "SET supports only fixed no-op system variable assignments"
            );
            return MYLITE_ERROR;
        }
        return append_system_variable_read_warning(database, target->kind);
    }
    if (mylite_execution_system_variable_fixed_boolean_value(target->kind, &boolean_value)) {
        bool actual = false;

        rc = parse_set_boolean_cell_value(database, target->name, value, &actual);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (actual != boolean_value) {
            set_unsupported_error(
                database,
                "SET supports only fixed no-op system variable assignments"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "SET system variable assignments from user variables are not supported for this variable"
    );
    return MYLITE_ERROR;
}

static int apply_set_sql_mode_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
) {
    uint64_t modes = 0U;
    int rc = MYLITE_OK;

    if (value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->value == NULL) {
        set_unsupported_error(database, "SET sql_mode from NULL user variables is not supported");
        return MYLITE_ERROR;
    }
    rc = parse_sql_mode_text(database, value->value, &modes);
    if (rc == MYLITE_OK) {
        rc = append_set_sql_mode_warnings(database, modes);
    }
    if (rc == MYLITE_OK) {
        rc = set_session_sql_mode(database, modes);
    }
    return rc;
}

static int apply_set_time_zone_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
) {
    if (value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->value == NULL) {
        set_system_variable_cant_be_set_value_error(database, "time_zone", "NULL");
        return MYLITE_ERROR;
    }
    return set_session_time_zone(database, value->value);
}

static bool reject_invalid_read_only_system_variable_cell_target(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_server_environment(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_server_build(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_server_identity_binary_log(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if ((fixed_global_uint64_server_identity_system_variable_value(target->kind, NULL, NULL) ||
         target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_global_read_only_toggle(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }

    return false;
}

static int apply_set_sql_select_limit_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t sql_select_limit = UINT64_MAX;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL sql_select_limit assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = parse_set_sql_select_limit_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &sql_select_limit
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sql_select_limit = sql_select_limit;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_timeout_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t timeout = timeout_system_variable_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL timeout system variable assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_timeout_system_variable_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &timeout
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_timeout_system_variable_value(database, target, timeout);
}

static int parse_set_boolean_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    bool *out_value
) {
    if (out_value == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = false;
    if (value->value == NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (strcmp(value->value, "1") == 0 || text_equals_ascii_case_insensitive(value->value, "ON") ||
        text_equals_ascii_case_insensitive(value->value, "TRUE")) {
        *out_value = true;
        return MYLITE_OK;
    }
    if (strcmp(value->value, "0") == 0 || text_equals_ascii_case_insensitive(value->value, "OFF") ||
        text_equals_ascii_case_insensitive(value->value, "FALSE")) {
        *out_value = false;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, value->value);
    return MYLITE_ERROR;
}

static int resolve_set_system_variable_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    struct resolved_set_system_variable_target *out_target
) {
    const struct mylite_sql_ast_node *first = NULL;
    const struct mylite_sql_ast_node *second = NULL;
    size_t child_count = 0U;

    if (out_target == NULL) {
        set_runtime_error(database, "invalid SET system variable target");
        return MYLITE_ERROR;
    }
    *out_target = (struct resolved_set_system_variable_target){0};
    if (target == NULL || target->kind != MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(target);
    first = child_at(target, 0U);
    second = child_at(target, 1U);
    if (child_count == 1U && first != NULL && first->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        return resolve_set_system_variable_system_target(database, first, out_target);
    }
    if (child_count == 1U) {
        return resolve_set_system_variable_identifier_target(database, NULL, first, out_target);
    }
    if (child_count == 2U) {
        return resolve_set_system_variable_identifier_target(database, first, second, out_target);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int resolve_set_system_variable_identifier_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scope_node,
    const struct mylite_sql_ast_node *name_node,
    struct resolved_set_system_variable_target *out_target
) {
    struct system_variable_component name = {0};
    char scope[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (name_node == NULL || name_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (scope_node == NULL) {
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_NONE;
    } else {
        rc = copy_identifier_text(scope_node, scope, sizeof(scope), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (text_equals_ascii_case_insensitive(scope, "session")) {
            out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_SESSION;
        } else if (text_equals_ascii_case_insensitive(scope, "local")) {
            out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_LOCAL;
        } else if (text_equals_ascii_case_insensitive(scope, "global")) {
            out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_GLOBAL;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    rc = copy_identifier_text(name_node, out_target->name, sizeof(out_target->name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    memcpy(name.text, out_target->name, strlen(out_target->name) + 1U);
    if (!resolve_system_variable_kind(&name, &out_target->kind)) {
        set_unknown_system_variable_name_error(database, out_target->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_set_system_variable_system_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct resolved_set_system_variable_target *out_target
) {
    const struct mylite_sql_source_span *span = name_node == NULL ? NULL : &name_node->span;
    struct system_variable_component first = {0};
    struct system_variable_component second = {0};
    const struct system_variable_component *name = &first;
    size_t offset = 2U;
    bool has_scope = false;
    int rc = MYLITE_OK;

    if (span == NULL || span->text == NULL || span->length < 3U || span->text[0] != '@' ||
        span->text[1] != '@') {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }

    rc = parse_system_variable_component(database, span, &offset, &first);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (offset < span->length && span->text[offset] == '.') {
        has_scope = true;
        ++offset;
        if (first.quoted) {
            set_unsupported_error(database, "unsupported quoted system variable scope");
            return MYLITE_ERROR;
        }
        rc = parse_system_variable_component(database, span, &offset, &second);
        if (rc != MYLITE_OK) {
            return rc;
        }
        name = &second;
    }
    if (offset != span->length || system_variable_component_is_empty(name)) {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }

    if (!resolve_system_variable_kind(name, &out_target->kind)) {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }
    memcpy(out_target->name, name->text, strlen(name->text) + 1U);

    if (!has_scope) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_NONE;
        return MYLITE_OK;
    }
    if (system_variable_component_equals(&first, "global")) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_GLOBAL;
        return MYLITE_OK;
    }
    if (system_variable_component_equals(&first, "session")) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_SESSION;
    } else if (system_variable_component_equals(&first, "local")) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_LOCAL;
    } else {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool fixed_global_uint64_server_identity_system_variable_value(
    enum mylite_execution_system_variable_kind kind,
    uint64_t *out_value,
    const char **out_unsupported_message
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        if (out_value != NULL) {
            *out_value = server_id_system_variable_value;
        }
        if (out_unsupported_message != NULL) {
            *out_unsupported_message = "SET server_id supports only fixed no-op global assignments";
        }
        return true;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        if (out_value != NULL) {
            *out_value = server_id_bits_system_variable_value;
        }
        if (out_unsupported_message != NULL) {
            *out_unsupported_message =
                "SET server_id_bits supports only fixed no-op global assignments";
        }
        return true;
    default:
        return false;
    }
}

static int validate_set_fixed_boolean_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool expected_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool actual_value = false;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        actual_value = true;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        actual_value = false;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (parse_unsigned_integer_literal(&value_node->span, &magnitude) != MYLITE_OK ||
            magnitude > 1U) {
            set_unsupported_error(
                database,
                "SET supports only fixed no-op system variable assignments"
            );
            return MYLITE_ERROR;
        }
        actual_value = magnitude == 1U;
    } else {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    if (actual_value != expected_value) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int apply_set_sql_require_primary_key_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid sql_require_primary_key system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value) {
            set_unsupported_error(
                database,
                "SET GLOBAL sql_require_primary_key assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.sql_require_primary_key = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_sql_require_primary_key_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid sql_require_primary_key system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_cell_value(database, target->name, value, value_kind, &boolean_value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (boolean_value) {
            set_unsupported_error(
                database,
                "SET GLOBAL sql_require_primary_key assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.sql_require_primary_key = boolean_value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_big_tables_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid big_tables system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value) {
            set_unsupported_error(
                database,
                "SET big_tables supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.big_tables = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_big_tables_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool big_tables = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_big_tables_cell_value(database, target->name, value, value_kind, &big_tables);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (big_tables) {
            set_unsupported_error(
                database,
                "SET big_tables supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.big_tables = big_tables;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_big_tables_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    bool *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = false;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        const struct mylite_sql_ast_node *inner_node =
            unwrap_parenthesized_expression(child_at(value_node, 0U));

        if (inner_node != NULL && inner_node->kind == MYLITE_SQL_AST_LITERAL &&
            (mylite_sql_ast_node_literal_kind(inner_node) == MYLITE_SQL_AST_LITERAL_TRUE ||
             mylite_sql_ast_node_literal_kind(inner_node) == MYLITE_SQL_AST_LITERAL_FALSE)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    literal_node = unwrap_big_tables_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        char *decoded = NULL;
        size_t decoded_size = 0U;
        int rc = decode_sql_string_literal(
            database,
            literal_node,
            "SET big_tables supports only string literals",
            "SET big_tables string values do not support NUL bytes",
            &decoded,
            &decoded_size
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (text_equals_ascii_case_insensitive(decoded, "ON")) {
            *out_value = true;
            free(decoded);
            return MYLITE_OK;
        }
        if (text_equals_ascii_case_insensitive(decoded, "OFF")) {
            free(decoded);
            return MYLITE_OK;
        }
        free(decoded);
        copy_big_tables_value_text(value_node, value_text, sizeof(value_text));
        set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_big_tables_value_text(value_node, value_text, sizeof(value_text));
    return finish_parse_set_big_tables_integer(
        database,
        variable_name,
        magnitude,
        negative,
        value_text,
        out_value
    );
}

static int parse_set_big_tables_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t magnitude = 0U;
    bool negative = false;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = false;
    if (value == NULL || text == NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        if (text_equals_ascii_case_insensitive(text, "ON")) {
            *out_value = true;
            return MYLITE_OK;
        }
        if (text_equals_ascii_case_insensitive(text, "OFF")) {
            return MYLITE_OK;
        }
        set_system_variable_cant_be_set_value_error(database, variable_name, text);
        return MYLITE_ERROR;
    }
    if (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER || text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    return finish_parse_set_big_tables_integer(
        database,
        variable_name,
        magnitude,
        negative,
        value->value,
        out_value
    );
}

static int finish_parse_set_big_tables_integer(
    struct mylite_db *database,
    const char *variable_name,
    uint64_t magnitude,
    bool negative,
    const char *value_text,
    bool *out_value
) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (magnitude == 0U) {
        *out_value = false;
        return MYLITE_OK;
    }
    if (!negative && magnitude == 1U) {
        *out_value = true;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
    return MYLITE_ERROR;
}

static const struct mylite_sql_ast_node *unwrap_big_tables_value_literal(
    const struct mylite_sql_ast_node *value_node,
    bool *out_negative
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);

    if (out_negative != NULL) {
        *out_negative = false;
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return literal_node;
    }

    switch (mylite_sql_ast_node_operator(literal_node)) {
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        if (out_negative != NULL) {
            *out_negative = true;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        break;
    default:
        return NULL;
    }

    return unwrap_parenthesized_expression(child_at(literal_node, 0U));
}

static void copy_big_tables_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
) {
    const struct mylite_sql_source_span *span = value_node == NULL ? NULL : &value_node->span;
    size_t source_offset = 0U;
    size_t source_length = 0U;
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    if (span != NULL && span->text != NULL) {
        source_length = span->length;
        if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING &&
            source_length >= 2U) {
            source_offset = 1U;
            source_length -= 2U;
        }
        copied = source_length < buffer_size ? source_length : buffer_size - 1U;
        memcpy(buffer, &span->text[source_offset], copied);
    }
    buffer[copied] = '\0';
}

static int apply_set_foreign_key_checks_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = true;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid foreign_key_checks system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL foreign_key_checks assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_foreign_key_checks_value(database, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.foreign_key_checks_enabled = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_foreign_key_checks_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid foreign_key_checks output");
        return MYLITE_ERROR;
    }
    *out_value = true;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        *out_value = false;
        return MYLITE_OK;
    }

    literal_node = unwrap_foreign_key_checks_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        copy_foreign_key_checks_value_text(value_node, value_text, sizeof(value_text));
        set_system_variable_cant_be_set_value_error(database, "foreign_key_checks", value_text);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = false;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER && !negative &&
        parse_unsigned_integer_literal(&literal_node->span, &magnitude) == MYLITE_OK &&
        magnitude <= 1U) {
        *out_value = magnitude == 1U;
        return MYLITE_OK;
    }

    copy_foreign_key_checks_value_text(value_node, value_text, sizeof(value_text));
    set_system_variable_cant_be_set_value_error(database, "foreign_key_checks", value_text);
    return MYLITE_ERROR;
}

static const struct mylite_sql_ast_node *unwrap_foreign_key_checks_value_literal(
    const struct mylite_sql_ast_node *value_node,
    bool *out_negative
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);

    if (out_negative != NULL) {
        *out_negative = false;
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return literal_node;
    }

    switch (mylite_sql_ast_node_operator(literal_node)) {
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        if (out_negative != NULL) {
            *out_negative = true;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        break;
    default:
        return NULL;
    }

    return unwrap_parenthesized_expression(child_at(literal_node, 0U));
}

static void copy_foreign_key_checks_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
) {
    const struct mylite_sql_source_span *span = value_node == NULL ? NULL : &value_node->span;
    size_t source_offset = 0U;
    size_t source_length = 0U;
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    if (span != NULL && span->text != NULL) {
        source_length = span->length;
        if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING &&
            source_length >= 2U) {
            source_offset = 1U;
            source_length -= 2U;
        }
        copied = source_length < buffer_size ? source_length : buffer_size - 1U;
        memcpy(buffer, &span->text[source_offset], copied);
    }
    buffer[copied] = '\0';
}

static int apply_set_max_allowed_packet_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    if (target == NULL) {
        set_runtime_error(database, "invalid max_allowed_packet target");
        return MYLITE_ERROR;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }

    return validate_set_fixed_uint64_value(
        database,
        value_node,
        max_allowed_packet_default_value,
        "SET max_allowed_packet supports only fixed no-op global assignments"
    );
}

static int validate_set_fixed_uint64_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t expected_value,
    const char *unsupported_message
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t actual_value = 0U;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal_node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        literal_node = unwrap_parenthesized_expression(child_at(literal_node, 0U));
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal_node->span, &actual_value) != MYLITE_OK ||
        actual_value != expected_value) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int apply_set_transaction_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    struct transaction_characteristics characteristics = {
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid transaction system variable target");
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION) {
        rc =
            parse_set_transaction_isolation_value(database, value_node, &characteristics.isolation);
        if (rc != MYLITE_OK) {
            return rc;
        }
        characteristics.has_isolation = true;
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL &&
            characteristics.isolation != MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ) {
            set_unsupported_error(
                database,
                "SET transaction_isolation supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY) {
        rc = parse_set_transaction_read_only_value(
            database,
            value_node,
            &characteristics.access_mode
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        characteristics.has_access_mode = true;
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL &&
            characteristics.access_mode != MYLITE_TRANSACTION_ACCESS_READ_WRITE) {
            set_unsupported_error(
                database,
                "SET transaction_read_only supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
    } else {
        set_runtime_error(database, "invalid transaction system variable kind");
        return MYLITE_ERROR;
    }

    return apply_transaction_system_variable_characteristics(database, target, &characteristics);
}

static int apply_set_auto_increment_step_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = 1U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid auto-increment system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL auto-increment system variable assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_auto_increment_step_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT) {
        database->session.auto_increment_increment = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET) {
        database->session.auto_increment_offset = value;
    } else {
        set_runtime_error(database, "invalid auto-increment system variable kind");
        return MYLITE_ERROR;
    }
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_auto_increment_step_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    enum { max_auto_increment_step_value = 65535U };
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid auto-increment system variable output");
        return MYLITE_ERROR;
    }
    *out_value = 1U;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return append_truncated_incorrect_auto_increment_warning(database, variable_name, "0");
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));

    if (negative) {
        return append_truncated_incorrect_auto_increment_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK ||
        magnitude > max_auto_increment_step_value) {
        *out_value = max_auto_increment_step_value;
        return append_truncated_incorrect_auto_increment_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (magnitude == 0U) {
        return append_truncated_incorrect_auto_increment_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *unwrap_auto_increment_step_value_literal(
    const struct mylite_sql_ast_node *value_node,
    bool *out_negative
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);

    if (out_negative != NULL) {
        *out_negative = false;
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return literal_node;
    }

    switch (mylite_sql_ast_node_operator(literal_node)) {
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        if (out_negative != NULL) {
            *out_negative = true;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        break;
    default:
        return NULL;
    }

    return unwrap_parenthesized_expression(child_at(literal_node, 0U));
}

static void copy_auto_increment_step_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
) {
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    if (value_node != NULL && value_node->span.text != NULL) {
        copied = value_node->span.length < buffer_size ? value_node->span.length : buffer_size - 1U;
        memcpy(buffer, value_node->span.text, copied);
    }
    buffer[copied] = '\0';
}

static int set_incorrect_system_variable_argument_type_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect argument type to variable '%s'",
        variable_name == NULL ? "unknown" : variable_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_argument_type,
        "42000",
        message
    );
    return MYLITE_ERROR;
}

static int append_truncated_incorrect_auto_increment_warning(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect %s value: '%s'",
        variable_name == NULL ? "auto_increment" : variable_name,
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_auto_increment,
        "HY000",
        message
    );
}

static int apply_set_sql_select_limit_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = UINT64_MAX;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid sql_select_limit system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL sql_select_limit assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = parse_set_sql_select_limit_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sql_select_limit = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_sql_select_limit_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid sql_select_limit output");
        return MYLITE_ERROR;
    }
    *out_value = UINT64_MAX;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    if (parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int parse_set_sql_select_limit_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid sql_select_limit output");
        return MYLITE_ERROR;
    }
    *out_value = UINT64_MAX;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, out_value) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value->value
        );
    }

    return MYLITE_OK;
}

static int apply_set_group_concat_max_len_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = group_concat_max_len_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid group_concat_max_len system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL group_concat_max_len assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_group_concat_max_len_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.group_concat_max_len = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_group_concat_max_len_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid group_concat_max_len output");
        return MYLITE_ERROR;
    }
    *out_value = group_concat_max_len_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        magnitude = 1U;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        magnitude = 0U;
    } else if (
        literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK
    ) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    if (negative || magnitude < group_concat_max_len_minimum_value) {
        *out_value = group_concat_max_len_minimum_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_group_concat_max_len_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t group_concat_max_len = group_concat_max_len_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL group_concat_max_len assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_group_concat_max_len_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &group_concat_max_len
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.group_concat_max_len = group_concat_max_len;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_group_concat_max_len_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid group_concat_max_len output");
        return MYLITE_ERROR;
    }
    *out_value = group_concat_max_len_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, out_value) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative || *out_value < group_concat_max_len_minimum_value) {
        *out_value = group_concat_max_len_minimum_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value->value
        );
    }

    return MYLITE_OK;
}

static int apply_set_information_schema_stats_expiry_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = information_schema_stats_expiry_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            information_schema_stats_expiry_default_value,
            "SET information_schema_stats_expiry supports only fixed no-op global assignments"
        );
    }

    rc =
        parse_set_information_schema_stats_expiry_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.information_schema_stats_expiry = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_information_schema_stats_expiry_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry output");
        return MYLITE_ERROR;
    }
    *out_value = information_schema_stats_expiry_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_information_schema_stats_expiry_integer(
        database,
        variable_name,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_information_schema_stats_expiry_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry output");
        return MYLITE_ERROR;
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (magnitude > information_schema_stats_expiry_max_value) {
        *out_value = information_schema_stats_expiry_max_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_information_schema_stats_expiry_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t stats_expiry = information_schema_stats_expiry_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL information_schema_stats_expiry assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_information_schema_stats_expiry_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &stats_expiry
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.information_schema_stats_expiry = stats_expiry;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_information_schema_stats_expiry_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry output");
        return MYLITE_ERROR;
    }
    *out_value = information_schema_stats_expiry_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    return parse_information_schema_stats_expiry_integer(
        database,
        variable_name,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int apply_set_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = timeout_system_variable_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid timeout system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            timeout_system_variable_default_value,
            "SET timeout system variable supports only fixed no-op global assignments"
        );
    }

    rc = parse_set_timeout_system_variable_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_timeout_system_variable_value(database, target, value);
}

static int apply_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
) {
    if (database == NULL || target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT) {
        database->session.wait_timeout = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT) {
        database->session.interactive_timeout = value;
    } else {
        set_runtime_error(database, "invalid timeout system variable kind");
        return MYLITE_ERROR;
    }
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_timeout_system_variable_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid timeout system variable output");
        return MYLITE_ERROR;
    }
    *out_value = timeout_system_variable_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 1U;
        return append_truncated_incorrect_system_variable_warning(database, variable_name, "0");
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_timeout_system_variable_integer(
        database,
        variable_name,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_timeout_system_variable_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid timeout system variable output");
        return MYLITE_ERROR;
    }
    if (negative) {
        *out_value = 1U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK ||
        magnitude > timeout_system_variable_max_value) {
        *out_value = timeout_system_variable_max_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (magnitude == 0U) {
        *out_value = 1U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int parse_set_timeout_system_variable_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid timeout system variable output");
        return MYLITE_ERROR;
    }
    *out_value = timeout_system_variable_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    return parse_timeout_system_variable_integer(
        database,
        variable_name,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int append_truncated_incorrect_system_variable_warning(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect %s value: '%s'",
        variable_name == NULL ? "system variable" : variable_name,
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_auto_increment,
        "HY000",
        message
    );
}

static int apply_set_sql_mode_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    char *decoded = NULL;
    uint64_t modes = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return set_session_sql_mode(database, MYLITE_SESSION_SQL_MODE_DEFAULT_BITS);
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(
        database,
        value_node,
        &decoded,
        (struct table_option_name_policy){
            .identifier_kind = "sql_mode",
            .nul_message = "SET sql_mode does not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = parse_sql_mode_text(database, decoded, &modes);
    if (rc == MYLITE_OK) {
        rc = append_set_sql_mode_warnings(database, modes);
    }
    if (rc == MYLITE_OK) {
        rc = set_session_sql_mode(database, modes);
    }
    free(decoded);
    return rc;
}

static int apply_set_timestamp_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        database->session.has_timestamp_override = false;
        database->session.timestamp_override = 0;
        return MYLITE_OK;
    }
    if (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            database->session.has_timestamp_override = false;
            database->session.timestamp_override = 0;
            return MYLITE_OK;
        }
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "SET timestamp supports only integer values");
            return MYLITE_ERROR;
        }
        literal_node = unwrap_parenthesized_expression(child_at(literal_node, 0U));
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "SET timestamp supports only integer values");
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&literal_node->span, &magnitude);
    if (rc != MYLITE_OK || magnitude > INT32_MAX) {
        char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
        size_t copied = value_node->span.length < sizeof(value_text) ? value_node->span.length
                                                                     : sizeof(value_text) - 1U;

        if (value_node->span.text == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        memcpy(value_text, value_node->span.text, copied);
        value_text[copied] = '\0';
        set_system_variable_cant_be_set_value_error(database, "timestamp", value_text);
        return MYLITE_ERROR;
    }
    if (magnitude == 0U) {
        database->session.has_timestamp_override = false;
        database->session.timestamp_override = 0;
        return MYLITE_OK;
    }

    database->session.has_timestamp_override = true;
    database->session.timestamp_override = (int64_t)magnitude;
    return MYLITE_OK;
}

static int apply_set_time_zone_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    char *decoded = NULL;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return set_session_time_zone(database, "SYSTEM");
    }
    rc = copy_set_time_zone_value_text(database, value_node, &decoded);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = set_session_time_zone(database, decoded);
    free(decoded);
    return rc;
}

static int copy_set_time_zone_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid time_zone value");
        return MYLITE_ERROR;
    }
    *out_value = NULL;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        char value[MYLITE_SESSION_TIME_ZONE_CAPACITY];
        size_t value_length = 0U;

        rc = copy_identifier_text(value_node, value, sizeof(value), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        value_length = strlen(value);
        *out_value = (char *)malloc(value_length + 1U);
        if (*out_value == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(*out_value, value, value_length + 1U);
        return MYLITE_OK;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_time_zone_incorrect_argument_type_error(database);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_table_option_string_literal(
            database,
            value_node,
            out_value,
            (struct table_option_name_policy){
                .identifier_kind = "time_zone",
                .nul_message = "SET time_zone does not support NUL bytes",
            }
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        set_system_variable_cant_be_set_value_error(database, "time_zone", "NULL");
        return MYLITE_ERROR;
    }

    set_time_zone_incorrect_argument_type_error(database);
    return MYLITE_ERROR;
}

static int set_session_time_zone(struct mylite_db *database, const char *text) {
    char canonical[MYLITE_SESSION_TIME_ZONE_CAPACITY];
    int offset_minutes = 0;
    int rc = MYLITE_OK;

    if (database == NULL || text == NULL) {
        set_runtime_error(database, "invalid time_zone state");
        return MYLITE_ERROR;
    }

    rc = parse_time_zone_text(database, text, canonical, sizeof(canonical), &offset_minutes);
    if (rc != MYLITE_OK) {
        return rc;
    }

    memcpy(database->session.time_zone, canonical, strlen(canonical) + 1U);
    database->session.time_zone_offset_minutes = offset_minutes;
    database->session.time_zone_is_placeholder = false;
    return MYLITE_OK;
}

static int parse_time_zone_text(
    struct mylite_db *database,
    const char *text,
    char *canonical,
    size_t canonical_size,
    int *out_offset_minutes
) {
    if (text == NULL || canonical == NULL || canonical_size == 0U || out_offset_minutes == NULL) {
        set_runtime_error(database, "invalid time_zone value");
        return MYLITE_ERROR;
    }
    if (sql_mode_token_matches(text, strlen(text), "SYSTEM")) {
        if (sizeof("SYSTEM") > canonical_size) {
            set_runtime_error(database, "time_zone state is too small");
            return MYLITE_ERROR;
        }
        memcpy(canonical, "SYSTEM", sizeof("SYSTEM"));
        *out_offset_minutes = 0;
        return MYLITE_OK;
    }
    if (sql_mode_token_matches(text, strlen(text), "UTC")) {
        if (sizeof("UTC") > canonical_size) {
            set_runtime_error(database, "time_zone state is too small");
            return MYLITE_ERROR;
        }
        memcpy(canonical, "UTC", sizeof("UTC"));
        *out_offset_minutes = 0;
        return MYLITE_OK;
    }
    if (parse_time_zone_offset(text, canonical, canonical_size, out_offset_minutes)) {
        return MYLITE_OK;
    }

    set_unknown_or_incorrect_time_zone_error(database, text);
    return MYLITE_ERROR;
}

static bool parse_time_zone_offset(
    const char *text,
    char *canonical,
    size_t canonical_size,
    int *out_offset_minutes
) {
    size_t offset = 0U;
    int sign = 1;
    int hours = 0;
    int minutes = 0;
    int total_minutes = 0;
    int written = 0;

    if (text == NULL || canonical == NULL || out_offset_minutes == NULL) {
        return false;
    }
    if (text[offset] == '+') {
        sign = 1;
    } else if (text[offset] == '-') {
        sign = -1;
    } else {
        return false;
    }
    ++offset;
    if (text[offset] < '0' || text[offset] > '9') {
        return false;
    }
    hours = text[offset] - '0';
    ++offset;
    if (text[offset] >= '0' && text[offset] <= '9') {
        hours = (hours * decimal_base) + (text[offset] - '0');
        ++offset;
    }
    if (text[offset] != ':') {
        return false;
    }
    ++offset;
    if (text[offset] < '0' || text[offset] > '9') {
        return false;
    }
    minutes = text[offset] - '0';
    ++offset;
    if (text[offset] >= '0' && text[offset] <= '9') {
        minutes = (minutes * decimal_base) + (text[offset] - '0');
        ++offset;
    }
    if (text[offset] != '\0' || minutes > time_zone_max_minute) {
        return false;
    }

    total_minutes = sign * ((hours * time_zone_minutes_per_hour) + minutes);
    if (total_minutes > time_zone_max_positive_offset_minutes ||
        total_minutes < -time_zone_max_negative_offset_minutes) {
        return false;
    }
    if (total_minutes == 0) {
        sign = 1;
    }

    written = snprintf(
        canonical,
        canonical_size,
        "%c%02d:%02d",
        sign < 0 ? '-' : '+',
        abs(total_minutes) / time_zone_minutes_per_hour,
        abs(total_minutes) % time_zone_minutes_per_hour
    );
    if (written < 0 || (size_t)written >= canonical_size) {
        return false;
    }
    *out_offset_minutes = total_minutes;
    return true;
}

static void set_unknown_or_incorrect_time_zone_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Unknown or incorrect time zone: '%s'",
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_or_incorrect_time_zone,
        "HY000",
        message
    );
}

static void set_time_zone_incorrect_argument_type_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_argument_type,
        "42000",
        "Incorrect argument type to variable 'time_zone'"
    );
}

static int parse_sql_mode_text(struct mylite_db *database, const char *text, uint64_t *out_modes) {
    const char *invalid_token = NULL;
    size_t invalid_token_length = 0U;
    int rc =
        mylite_execution_sql_mode_parse(text, out_modes, &invalid_token, &invalid_token_length);

    if (rc == MYLITE_MISUSE) {
        set_runtime_error(database, "invalid sql_mode value");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_ERROR) {
        set_invalid_sql_mode_error(database, invalid_token, invalid_token_length);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static bool sql_mode_token_matches(const char *text, size_t length, const char *expected) {
    return mylite_execution_sql_mode_token_matches(text, length, expected);
}

static void set_invalid_sql_mode_error(
    struct mylite_db *database,
    const char *text,
    size_t length
) {
    char value[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t copied = length < sizeof(value) ? length : sizeof(value) - 1U;
    int written = 0;

    if (text != NULL && copied > 0U) {
        memcpy(value, text, copied);
    }
    value[copied] = '\0';
    written = snprintf(
        message,
        sizeof(message),
        "Variable 'sql_mode' can't be set to the value of '%s'",
        value
    );
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_variable_cant_be_set,
        "42000",
        message
    );
}

static void set_system_variable_cant_be_set_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Variable '%s' can't be set to the value of '%s'",
        variable_name == NULL ? "unknown" : variable_name,
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_variable_cant_be_set,
        "42000",
        message
    );
}

static int append_set_sql_mode_warnings(struct mylite_db *database, uint64_t modes) {
    const uint64_t zero_group = MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE |
                                MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE |
                                MYLITE_SESSION_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO;
    bool has_strict = (modes & (MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES |
                                MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES)) != 0U;
    bool has_zero_mode = (modes & zero_group) != 0U;
    bool has_zero_group = (modes & zero_group) == zero_group;
    int rc = MYLITE_OK;

    if ((has_strict || has_zero_mode) && !(has_strict && has_zero_group)) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_sql_mode_should_be_used_with_strict,
            "HY000",
            "'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes "
            "should be used with strict mode. They will be merged with strict mode in a future "
            "release."
        );
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
            return rc;
        }
    }
    if ((modes & MYLITE_SESSION_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH) != 0U) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_sql_mode_pad_char_deprecated,
            "HY000",
            "Changing sql mode 'PAD_CHAR_TO_FULL_LENGTH' is deprecated. It will be removed in "
            "a future release."
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int set_session_sql_mode(struct mylite_db *database, uint64_t modes) {
    char text[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
    int rc = rebuild_sql_mode_text(database, modes, text, sizeof(text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sql_mode = modes;
    memcpy(database->session.sql_mode_text, text, strlen(text) + 1U);
    database->session.sql_mode_is_placeholder = false;
    return MYLITE_OK;
}

static int rebuild_sql_mode_text(
    struct mylite_db *database,
    uint64_t modes,
    char *buffer,
    size_t buffer_size
) {
    int rc = mylite_execution_sql_mode_format(modes, buffer, buffer_size);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static bool session_sql_mode_has(const struct mylite_session_state *session, uint64_t mode) {
    if (session == NULL) {
        return false;
    }
    if ((session->sql_mode & mode) != 0U) {
        return true;
    }
    return false;
}

static unsigned int lexer_modes_for_session_sql_mode(const struct mylite_session_state *session) {
    unsigned int modes = 0U;

    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_ANSI_QUOTES)) {
        modes |= MYLITE_SQL_MODE_ANSI_QUOTES;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        modes |= MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_IGNORE_SPACE)) {
        modes |= MYLITE_SQL_MODE_IGNORE_SPACE;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_PIPES_AS_CONCAT)) {
        modes |= MYLITE_SQL_MODE_PIPES_AS_CONCAT;
    }
    return modes;
}

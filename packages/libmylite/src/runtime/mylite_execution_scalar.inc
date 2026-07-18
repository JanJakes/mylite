static int session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = unwrap_any_value_function_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_PARAMETER:
        return copy_active_stmt_parameter_cell(database, expression, out_cell);
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        if (database->session.has_selected_schema) {
            out_cell->value = database->session.selected_schema;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        out_cell->value = database->session.client_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        out_cell->value = database->session.current_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        out_cell->value = "NONE";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.connection_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format CONNECTION_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ANY_VALUE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PI_FUNCTION:
        out_cell->value = scalar_pi_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
        return rand_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        return current_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "CURRENT_TIMESTAMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        return sysdate_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SYSDATE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
        return mylite_execution_scalar_unix_timestamp_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
        return mylite_execution_scalar_timestamp_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TIMESTAMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
        return mylite_execution_scalar_datediff_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "DATEDIFF");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
        return mylite_execution_scalar_timestampdiff_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
        return mylite_execution_scalar_timediff_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TIMEDIFF");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
        return mylite_execution_scalar_sec_to_time_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SEC_TO_TIME");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
        return mylite_execution_scalar_from_unixtime_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "FROM_UNIXTIME");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return mylite_execution_scalar_temporal_constructor_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
        return mylite_execution_scalar_period_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
        return mylite_execution_scalar_convert_tz_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "FROM_DAYS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "MAKEDATE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "MAKETIME");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "PERIOD_ADD");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "PERIOD_DIFF");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "CONVERT_TZ");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TIME_TO_SEC");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TO_DAYS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TO_SECONDS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        return current_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        if (mylite_execution_validate_temporal_fractional_precision(
                database,
                expression,
                (struct mylite_execution_temporal_fractional_precision_context){
                    .subject_name = "curtime",
                    .unsupported_message = "fractional CURRENT_TIME precision is not supported",
                }
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        return current_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        return utc_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        if (mylite_execution_validate_temporal_fractional_precision(
                database,
                expression,
                (struct mylite_execution_temporal_fractional_precision_context){
                    .subject_name = "utc_time",
                    .unsupported_message = "fractional UTC_TIME precision is not supported",
                }
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        return utc_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        if (mylite_execution_validate_temporal_fractional_precision(
                database,
                expression,
                (struct mylite_execution_temporal_fractional_precision_context){
                    .subject_name = "utc_timestamp",
                    .unsupported_message = "fractional UTC_TIMESTAMP precision is not supported",
                }
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        return utc_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
        return mylite_execution_scalar_string_length_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ASCII_FUNCTION:
    case MYLITE_SQL_AST_ORD_FUNCTION:
        return mylite_execution_scalar_string_codepoint_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ORD");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
        return mylite_execution_scalar_string_case_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
        return mylite_execution_scalar_string_trim_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_LEFT_FUNCTION:
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
        return mylite_execution_scalar_string_slice_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_LPAD_FUNCTION:
    case MYLITE_SQL_AST_RPAD_FUNCTION:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_SPACE_FUNCTION:
        return mylite_execution_scalar_string_padding_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_EXPORT_SET_FUNCTION:
    case MYLITE_SQL_AST_MAKE_SET_FUNCTION:
        return mylite_execution_scalar_string_bitmask_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "EXPORT_SET");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "MAKE_SET");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_LOCATE_FUNCTION:
    case MYLITE_SQL_AST_INSTR_FUNCTION:
    case MYLITE_SQL_AST_POSITION_FUNCTION:
        return mylite_execution_scalar_string_search_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FIND_IN_SET_FUNCTION:
        return mylite_execution_scalar_find_in_set_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_STRCMP_FUNCTION:
        return mylite_execution_scalar_strcmp_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION:
        return mylite_execution_scalar_substring_index_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION:
        return mylite_execution_scalar_regexp_like_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "REGEXP_LIKE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
        return mylite_execution_scalar_regexp_string_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(
            database,
            mylite_execution_regexp_string_function_argument_count_error_name(expression->kind)
        );
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
        return mylite_execution_scalar_json_valid_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_VALID");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
        return mylite_execution_scalar_json_extract_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
        return mylite_execution_scalar_json_value_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
        return mylite_execution_scalar_json_contains_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_CONTAINS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
        return mylite_execution_scalar_json_contains_path_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_CONTAINS_PATH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_SEARCH_FUNCTION:
        return mylite_execution_scalar_json_search_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_SEARCH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_SEARCH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_OVERLAPS_FUNCTION:
        return mylite_execution_scalar_json_overlaps_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_OVERLAPS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_OVERLAPS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_MEMBER_OF_FUNCTION:
        return mylite_execution_scalar_json_member_of_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
        return mylite_execution_scalar_json_length_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_LENGTH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_DEPTH_FUNCTION:
        return mylite_execution_scalar_json_depth_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_DEPTH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_DEPTH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_STORAGE_SIZE_FUNCTION:
        return mylite_execution_scalar_json_storage_size_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_STORAGE_SIZE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_STORAGE_SIZE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_STORAGE_FREE_FUNCTION:
        return mylite_execution_scalar_json_storage_free_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_STORAGE_FREE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_STORAGE_FREE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
        return mylite_execution_scalar_json_keys_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_KEYS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
        return mylite_execution_scalar_json_type_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_TYPE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_PRETTY_FUNCTION:
        return mylite_execution_scalar_json_pretty_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_PRETTY_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_PRETTY");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
        return mylite_execution_scalar_json_set_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_SET");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
        return mylite_execution_scalar_json_insert_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_INSERT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION:
        return mylite_execution_scalar_json_array_append_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_ARRAY_APPEND");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION:
        return mylite_execution_scalar_json_array_insert_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_ARRAY_INSERT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_MERGE_FUNCTION:
        return mylite_execution_scalar_json_merge_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_MERGE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_MERGE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION:
        return mylite_execution_scalar_json_merge_patch_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_MERGE_PATCH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_MERGE_PATCH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION:
        return mylite_execution_scalar_json_merge_preserve_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_JSON_MERGE_PRESERVE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_MERGE_PRESERVE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
        return mylite_execution_scalar_json_replace_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_REPLACE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
        return mylite_execution_scalar_json_remove_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_REMOVE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
        return mylite_execution_scalar_json_quote_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_QUOTE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return mylite_execution_scalar_json_unquote_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "JSON_UNQUOTE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
        return mylite_execution_scalar_json_array_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
        return mylite_execution_scalar_json_object_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
        return scalar_concat_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
        return mylite_execution_scalar_concat_ws_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
        return mylite_execution_scalar_string_replace_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_INSERT_STRING_FUNCTION:
        return mylite_execution_scalar_string_insert_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
        return mylite_execution_scalar_string_reverse_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
        return mylite_execution_scalar_soundex_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SOUNDEX");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_QUOTE_FUNCTION:
        return mylite_execution_scalar_quote_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "QUOTE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CHARSET_FUNCTION:
    case MYLITE_SQL_AST_COLLATION_FUNCTION:
    case MYLITE_SQL_AST_COERCIBILITY_FUNCTION:
        return mylite_execution_scalar_charset_collation_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            database->session.previous_row_count
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format ROW_COUNT() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.found_rows
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format FOUND_ROWS() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.last_insert_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format LAST_INSERT_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
        return last_insert_id_set_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_LITERAL:
        return literal_projection_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
        return row_constructor_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        if (is_row_constructor_projection_expression(expression)) {
            return row_constructor_scalar_value(database, expression, out_cell);
        }
        return session_unary_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return session_binary_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_NOT_PREDICATE:
        if (is_row_constructor_projection_expression(expression)) {
            return row_constructor_scalar_value(database, expression, out_cell);
        }
        return scalar_comparison_value(database, expression, out_cell);
    case MYLITE_SQL_AST_MOD_FUNCTION:
        return scalar_arithmetic_value(database, expression, out_cell);
    case MYLITE_SQL_AST_GENERIC_FUNCTION: {
        enum mylite_spatial_function_kind spatial_kind = MYLITE_SPATIAL_FUNCTION_NONE;

        if (generic_spatial_function_kind(expression, &spatial_kind)) {
            return spatial_function_session_scalar_value(database, expression, out_cell);
        }
        if (mylite_execution_scalar_aes_function_match(expression)) {
            return mylite_execution_scalar_aes_function_value(database, expression, out_cell);
        }
        return generic_system_function_value(database, expression, out_cell);
    }
    case MYLITE_SQL_AST_ABS_FUNCTION:
        return mylite_execution_scalar_abs_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SIGN_FUNCTION:
        return mylite_execution_scalar_sign_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CEIL_FUNCTION:
    case MYLITE_SQL_AST_CEILING_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_ROUND_FUNCTION:
        return mylite_execution_scalar_rounding_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SQRT_FUNCTION:
        return mylite_execution_scalar_sqrt_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
        return mylite_execution_scalar_angle_conversion_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_ASIN_FUNCTION:
        return mylite_execution_scalar_inverse_trig_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SIN_FUNCTION:
    case MYLITE_SQL_AST_COS_FUNCTION:
    case MYLITE_SQL_AST_TAN_FUNCTION:
    case MYLITE_SQL_AST_COT_FUNCTION:
        return mylite_execution_scalar_direct_trig_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ATAN_FUNCTION:
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
        return mylite_execution_scalar_atan_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_EXP_FUNCTION:
        return mylite_execution_scalar_exp_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_LN_FUNCTION:
    case MYLITE_SQL_AST_LOG_FUNCTION:
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
        return mylite_execution_scalar_logarithm_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POWER_FUNCTION:
        return mylite_execution_scalar_power_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_OCT_FUNCTION:
        return mylite_execution_scalar_base_conversion_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_CONV_FUNCTION:
        return mylite_execution_scalar_conv_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
        return mylite_execution_scalar_bit_count_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CRC32_FUNCTION:
        return mylite_execution_scalar_crc32_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
        return mylite_execution_scalar_digest_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "MD5");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SHA");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SHA1");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SHA2");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_HEX_FUNCTION:
        return mylite_execution_scalar_hex_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "HEX");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
        return mylite_execution_scalar_weight_string_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
        return mylite_execution_scalar_to_base64_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TO_BASE64");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
        return mylite_execution_scalar_from_base64_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "FROM_BASE64");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
        return mylite_execution_scalar_compress_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_COMPRESS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "COMPRESS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
        return mylite_execution_scalar_uncompress_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UNCOMPRESS_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UNCOMPRESS");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
        return mylite_execution_scalar_uncompressed_length_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UNCOMPRESSED_LENGTH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
        return mylite_execution_scalar_random_bytes_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_RANDOM_BYTES_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "RANDOM_BYTES");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
        return mylite_execution_scalar_ip_address_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_INET_ATON_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "INET_ATON");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_INET_NTOA_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "INET_NTOA");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
        return mylite_execution_scalar_unhex_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UNHEX");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_SHORT_FUNCTION:
        return mylite_execution_scalar_uuid_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UUID");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UUID_SHORT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UUID_SHORT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
        return mylite_execution_scalar_uuid_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "IS_UUID");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UUID_TO_BIN");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "BIN_TO_UUID");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CHAR_FUNCTION:
        return mylite_execution_scalar_char_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
        return mylite_execution_scalar_format_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED:
        set_unsupported_error(database, "FORMAT() locale arguments are not supported");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
        return mylite_execution_scalar_truncate_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
        return cast_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
        return cast_char_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
        return cast_signed_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
        return cast_unsigned_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        return convert_binary_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
        return convert_char_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
        return convert_signed_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
        return convert_unsigned_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return convert_using_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return convert_using_charset_value(database, expression, out_cell);
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
        return collate_expression_value(database, expression, out_cell);
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
        return mylite_execution_scalar_date_interval_second_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
        return mylite_execution_scalar_addtime_subtime_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ADDTIME");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SUBTIME");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "DATE_FORMAT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "TIME_FORMAT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "STR_TO_DATE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "DAYNAME");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "DAYOFMONTH");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "DAYOFWEEK");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "DAYOFYEAR");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "LAST_DAY");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "MONTHNAME");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "WEEKDAY");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "WEEKOFYEAR");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "YEARWEEK");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DATE_FUNCTION:
    case MYLITE_SQL_AST_TIME_FUNCTION:
    case MYLITE_SQL_AST_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_QUARTER_FUNCTION:
    case MYLITE_SQL_AST_WEEK_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
    case MYLITE_SQL_AST_YEAR_FUNCTION:
    case MYLITE_SQL_AST_MONTH_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
    case MYLITE_SQL_AST_DAY_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_DAYOFMONTH_FUNCTION:
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
    case MYLITE_SQL_AST_HOUR_FUNCTION:
    case MYLITE_SQL_AST_MINUTE_FUNCTION:
    case MYLITE_SQL_AST_SECOND_FUNCTION:
    case MYLITE_SQL_AST_MICROSECOND_FUNCTION:
    case MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION:
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
        return mylite_execution_scalar_temporal_extract_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "LOWER");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "LCASE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UPPER");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UCASE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "LTRIM");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "RTRIM");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "LOCATE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "INSTR");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
        return mylite_execution_scalar_date_format_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
        return mylite_execution_scalar_get_format_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_TIME_FORMAT_FUNCTION:
        return mylite_execution_scalar_time_format_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
        return mylite_execution_scalar_str_to_date_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ELT");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_ELT_FUNCTION:
        return elt_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_FIELD_FUNCTION:
        return field_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "GREATEST");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "LEAST");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
        return greatest_least_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_INTERVAL_FUNCTION:
        return interval_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "FIND_IN_SET");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "STRCMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SUBSTRING_INDEX");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_IF_FUNCTION:
        return if_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return ifnull_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return coalesce_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return nullif_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return isnull_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
        return case_expression_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
        return scalar_subquery_value(database, expression, out_cell);
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION:
        return user_variable_assignment_expression_value(database, expression, out_cell);
    case MYLITE_SQL_AST_USER_VARIABLE:
        return session_user_variable_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return system_variable_value(database, expression, out_cell);
    default:
        return MYLITE_OK;
    }
}

static int user_variable_assignment_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_sql_ast_node *unwrapped_value = NULL;
    struct session_scalar_cell value = {0};
    enum mylite_session_user_variable_value_kind value_kind =
        MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        expression->kind != MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(expression, 0U);
    value_node = child_at(expression, 1U);
    unwrapped_value = unwrap_parenthesized_expression(value_node);
    rc = session_scalar_value(database, value_node, &value);
    if (rc == MYLITE_OK) {
        value_kind = infer_user_variable_value_kind(value_node, &value);
        if (unwrapped_value != NULL && unwrapped_value->kind == MYLITE_SQL_AST_USER_VARIABLE) {
            rc = session_user_variable_value_kind(database, unwrapped_value, &value_kind);
        }
    }
    if (rc == MYLITE_OK) {
        rc = set_session_user_variable(database, target, &value, value_kind);
    }
    if (rc == MYLITE_OK) {
        rc = append_user_variable_assignment_deprecation_warning(database);
    }
    if (rc == MYLITE_OK) {
        rc = copy_user_variable_assignment_result_cell(database, out_cell, &value);
    }
    session_scalar_cell_deinit(&value);
    return rc;
}

static int copy_user_variable_assignment_result_cell(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell,
    const struct session_scalar_cell *value
) {
    size_t value_size = 0U;
    char *owned_text = NULL;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    copy_session_scalar_cell(out_cell, value);
    if (value == NULL || value->value == NULL) {
        return MYLITE_OK;
    }

    value_size = value->has_value_size ? value->value_size : strlen(value->value);
    if (value_size == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    owned_text = (char *)malloc(value_size + 1U);
    if (owned_text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(owned_text, value->value, value_size);
    owned_text[value_size] = '\0';

    out_cell->owned_text = owned_text;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = value_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int append_user_variable_assignment_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_user_variable_assignment_deprecated,
        "HY000",
        "Setting user variables within expressions is deprecated and will be removed in a future "
        "release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT "
        "expression(s) INTO variables(s)'."
    );
}

static int last_insert_id_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    uint64_t value = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_last_insert_id_argument_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = last_insert_id_set_argument_value(database, child_at(expression, 0U), &is_null, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_null) {
        database->session.last_insert_id = 0U;
        return MYLITE_OK;
    }

    rc = format_session_scalar_uint64_value(database, value, out_cell);
    if (rc == MYLITE_OK) {
        database->session.last_insert_id = value;
    }
    return rc;
}

static int last_insert_id_set_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool *out_is_null,
    uint64_t *out_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_is_null == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null = false;
    *out_value = 0U;

    argument = unwrap_parenthesized_expression(argument);
    if (argument == NULL) {
        set_last_insert_id_argument_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(argument, 0U));

        if ((operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
             operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) ||
            literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_last_insert_id_argument_unsupported_error(database);
            return MYLITE_ERROR;
        }
        return signed_last_insert_id_argument_value(
            database,
            literal,
            operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
            out_value
        );
    }
    if (argument->kind != MYLITE_SQL_AST_LITERAL) {
        set_last_insert_id_argument_unsupported_error(database);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(argument);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_last_insert_id_argument_unsupported_error(database);
        return MYLITE_ERROR;
    }

    if (parse_unsigned_integer_literal(&argument->span, out_value) != MYLITE_OK) {
        set_unsupported_error(database, "LAST_INSERT_ID() integer literal is out of range");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int signed_last_insert_id_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint64_t *out_value
) {
    static const uint64_t int64_negative_abs_max = 9223372036854775808ULL;
    uint64_t magnitude = 0U;

    if (literal == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_unsupported_error(database, "LAST_INSERT_ID() integer literal is out of range");
        return MYLITE_ERROR;
    }
    if (!is_negative) {
        *out_value = magnitude;
        return MYLITE_OK;
    }
    if (magnitude > int64_negative_abs_max) {
        set_unsupported_error(
            database,
            "LAST_INSERT_ID() negative integer literal is out of range"
        );
        return MYLITE_ERROR;
    }

    *out_value = 0U - magnitude;
    return MYLITE_OK;
}

static void set_last_insert_id_argument_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "LAST_INSERT_ID(expr) supports only integer, boolean, and NULL literal arguments"
    );
}

static int rand_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell seed_warnings = {0};
    size_t child_count = 0U;
    uint32_t seed = 0U;
    double value = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_RAND_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_RAND_SEED_FUNCTION)) {
        set_unsupported_error(database, "RAND() supports only RAND() and RAND(seed)");
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION && child_count == 0U) {
        if (database != NULL && database->session.rand_seed_state_initialized) {
            struct mylite_rand_state state = {0};

            mylite_rand_state_init_words(
                &state,
                database->session.rand_seed1,
                database->session.rand_seed2
            );
            value = mylite_rand_state_next_unit_double(&state);
            database->session.rand_seed1 = state.seed1;
            database->session.rand_seed2 = state.seed2;
        } else {
            value = mylite_rand_unseeded_unit_double();
        }
    } else if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION && child_count == 1U) {
        rc = rand_scalar_seed_value(database, child_at(expression, 0U), &seed, &seed_warnings);
        if (rc == MYLITE_OK) {
            value = mylite_rand_seeded_unit_double(seed);
        }
    } else {
        set_unsupported_error(database, "RAND() supports only RAND() and RAND(seed)");
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = mylite_execution_format_double_text(
            database,
            value,
            "RAND",
            out_cell->double_text,
            sizeof(out_cell->double_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->double_text;
        move_session_scalar_warning_state(out_cell, &seed_warnings);
    }
    session_scalar_cell_deinit(&seed_warnings);
    return rc;
}

static int rand_scalar_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    bool has_unary_sign = false;
    bool is_negative = false;

    if (out_seed == NULL || out_warnings == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seed = 0U;
    *out_warnings = (struct session_scalar_cell){0};
    if (literal == NULL) {
        set_unsupported_error(database, "RAND(seed) does not support this seed expression");
        return MYLITE_ERROR;
    }

    if (literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(literal);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_unsupported_error(database, "RAND(seed) does not support this seed expression");
            return MYLITE_ERROR;
        }
        has_unary_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
        return rand_scalar_seed_literal_value(
            database,
            literal,
            has_unary_sign,
            is_negative,
            out_seed,
            out_warnings
        );
    }
    if (has_unary_sign) {
        set_unsupported_error(database, "RAND(seed) does not support this seed expression");
        return MYLITE_ERROR;
    }
    return rand_seed_expression_value(database, literal, out_seed, out_warnings);
}

static int rand_scalar_seed_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool has_unary_sign,
    bool is_negative,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal == NULL || out_seed == NULL || out_warnings == NULL) {
        return MYLITE_MISUSE;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_seed = 0U;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        return rand_seed_from_magnitude(1U, is_negative, out_seed);
    case MYLITE_SQL_AST_LITERAL_FALSE:
        return rand_seed_from_magnitude(0U, false, out_seed);
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return rand_seed_literal_value(database, literal, is_negative, out_seed);
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return rand_seed_numeric_literal_value(database, literal, is_negative, out_seed);
    case MYLITE_SQL_AST_LITERAL_STRING:
        if (!has_unary_sign) {
            return rand_seed_string_literal_value(database, literal, out_seed, out_warnings);
        }
        break;
    default:
        break;
    }

    set_unsupported_error(database, "RAND(seed) does not support this seed expression");
    return MYLITE_ERROR;
}

static int rand_seed_numeric_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint32_t *out_seed
) {
    char *text = NULL;
    char *end = NULL;
    long double value = 0.0L;
    long double rounded = 0.0L;
    long double half = 0.0L;
    uint64_t magnitude = 0U;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal == NULL || literal->span.text == NULL || out_seed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seed = 0U;
    text = (char *)malloc(literal->span.length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, literal->span.text, literal->span.length);
    text[literal->span.length] = '\0';
    literal_kind = mylite_sql_ast_node_literal_kind(literal);

    errno = 0;
    value = mylite_numeric_parse_long_double(text, &end);
    if (end == text || end == NULL || *end != '\0' || errno == ERANGE || !isfinite(value) ||
        value < 0.0L) {
        free(text);
        set_unsupported_error(database, "RAND(seed) numeric seed is out of range");
        return MYLITE_ERROR;
    }
    half = (long double)decimal_round_half_digit / (long double)decimal_base;
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        long double floor_value = floorl(value);
        long double fraction = value - floor_value;

        rounded = floor_value;
        if (fraction > half || (fabsl(fraction - half) <= LDBL_EPSILON &&
                                fmodl(floor_value, (long double)integer_even_divisor) != 0.0L)) {
            rounded += decimal_round_integer_step;
        }
    } else {
        rounded = floorl(value + half);
    }
    if (rounded > (long double)UINT64_MAX) {
        free(text);
        set_unsupported_error(database, "RAND(seed) numeric seed is out of range");
        return MYLITE_ERROR;
    }
    magnitude = (uint64_t)rounded;
    free(text);
    return rand_seed_from_magnitude(magnitude, is_negative, out_seed);
}

static int rand_seed_string_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
) {
    struct session_scalar_cell string_cell = {0};
    int rc = scalar_text_conversion_input_value(
        database,
        literal,
        &(const struct scalar_text_conversion_messages){
            .unsupported = "RAND(seed) supports only string seed literals",
            .signed_value = "RAND(seed) supports only string seed literals",
            .string_unsupported = "RAND(seed) supports only string seed literals",
            .embedded_nul = "RAND(seed) string literals do not support embedded NUL bytes",
        },
        &string_cell
    );

    if (rc == MYLITE_OK) {
        rc = rand_seed_from_scalar_cell(database, &string_cell, out_seed, out_warnings);
    }
    session_scalar_cell_deinit(&string_cell);
    return rc;
}

static int rand_seed_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, "RAND(seed) does not support this seed expression");
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        rc = rand_seed_nullif_expression_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
        rc = cast_signed_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
        rc = cast_unsigned_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
        rc = convert_signed_type_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
        rc = convert_unsigned_type_value(database, expression, &cell);
        break;
    default:
        set_unsupported_error(database, "RAND(seed) does not support this seed expression");
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = rand_seed_from_scalar_cell(database, &cell, out_seed, out_warnings);
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int rand_seed_nullif_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell first_cell = {0};
    struct session_scalar_cell second_cell = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_NULLIF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    }

    rc = if_non_function_scalar_value(database, child_at(expression, 0U), "NULLIF", &first_cell);
    if (rc == MYLITE_OK) {
        rc = if_non_function_scalar_value(
            database,
            child_at(expression, 1U),
            "NULLIF",
            &second_cell
        );
    }
    if (rc == MYLITE_OK && first_cell.value != NULL &&
        (second_cell.value == NULL || strcmp(first_cell.value, second_cell.value) != 0)) {
        move_session_scalar_cell(out_cell, &first_cell);
    }
    session_scalar_cell_deinit(&first_cell);
    session_scalar_cell_deinit(&second_cell);
    return rc;
}

static int rand_seed_from_scalar_cell(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
) {
    if (cell == NULL || out_seed == NULL || out_warnings == NULL) {
        return MYLITE_MISUSE;
    }
    if (cell->owned_text != NULL && out_warnings->owned_text == NULL) {
        out_warnings->owned_text = cell->owned_text;
        cell->owned_text = NULL;
    }
    move_session_scalar_warning_state(out_warnings, cell);
    if (cell->value == NULL) {
        *out_seed = 0U;
        return MYLITE_OK;
    }
    return rand_seed_convert_string_value(database, cell->value, out_seed, out_warnings);
}

static int rand_seed_from_integer_text(
    struct mylite_db *database,
    const char *text,
    uint32_t *out_seed
) {
    struct mylite_sql_source_span span = {0};
    uint64_t magnitude = 0U;
    bool is_negative = false;

    if (text == NULL || out_seed == NULL) {
        return MYLITE_MISUSE;
    }
    if (*text == '-') {
        is_negative = true;
        ++text;
    }
    span.text = text;
    span.length = strlen(text);
    if (parse_unsigned_integer_literal(&span, &magnitude) != MYLITE_OK) {
        set_unsupported_error(database, "RAND(seed) numeric seed is out of range");
        return MYLITE_ERROR;
    }
    return rand_seed_from_magnitude(magnitude, is_negative, out_seed);
}

static int rand_seed_from_magnitude(uint64_t magnitude, bool is_negative, uint32_t *out_seed) {
    uint32_t truncated = 0U;

    if (out_seed == NULL) {
        return MYLITE_MISUSE;
    }
    truncated = (uint32_t)magnitude;
    if (is_negative) {
        *out_seed = (uint32_t)(0U - truncated);
    } else {
        *out_seed = truncated;
    }
    return MYLITE_OK;
}

static int rand_seed_convert_string_value(
    struct mylite_db *database,
    const char *text,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
) {
    struct session_scalar_cell cast_cell = {0};
    int rc =
        scalar_integer_cast_string_value(database, text, SCALAR_INTEGER_CAST_SIGNED, &cast_cell);

    if (rc == MYLITE_OK) {
        move_session_scalar_warning_state(out_warnings, &cast_cell);
        if (cast_cell.value == NULL) {
            *out_seed = 0U;
        } else {
            rc = rand_seed_from_integer_text(database, cast_cell.value, out_seed);
        }
    }
    session_scalar_cell_deinit(&cast_cell);
    return rc;
}

static void move_session_scalar_warning_state(
    struct session_scalar_cell *destination,
    struct session_scalar_cell *source
) {
    if (destination == NULL || source == NULL) {
        return;
    }

    if (source->has_staged_truncated_integer_warning) {
        destination->has_staged_truncated_integer_warning = true;
        if (source->staged_truncated_integer_text == source->owned_text &&
            source->owned_text != NULL) {
            free(destination->owned_text);
            destination->owned_text = source->owned_text;
            destination->staged_truncated_integer_text = destination->owned_text;
            source->owned_text = NULL;
        } else {
            destination->staged_truncated_integer_text = source->staged_truncated_integer_text;
        }
    }
    if (source->has_staged_truncated_decimal_warning) {
        destination->has_staged_truncated_decimal_warning = true;
        memcpy(
            destination->staged_truncated_decimal_text,
            source->staged_truncated_decimal_text,
            sizeof(destination->staged_truncated_decimal_text)
        );
    }
    if (source->has_staged_unhex_incorrect_string_warning) {
        destination->has_staged_unhex_incorrect_string_warning = true;
        memcpy(
            destination->staged_unhex_incorrect_string_text,
            source->staged_unhex_incorrect_string_text,
            sizeof(destination->staged_unhex_incorrect_string_text)
        );
    }
    destination->staged_signed_complement_warning_count +=
        source->staged_signed_complement_warning_count;
    destination->staged_unsigned_complement_warning_count +=
        source->staged_unsigned_complement_warning_count;
    destination->staged_division_by_zero_warning_count +=
        source->staged_division_by_zero_warning_count;
    destination->staged_invalid_logarithm_warning_count +=
        source->staged_invalid_logarithm_warning_count;
}

static int rand_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    bool has_unary_sign = false;
    bool is_negative = false;

    if (out_seed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seed = 0U;
    if (literal == NULL) {
        set_unsupported_error(
            database,
            "RAND(seed) supports only integer, boolean, and NULL seed literals"
        );
        return MYLITE_ERROR;
    }

    if (literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(literal);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_unsupported_error(
                database,
                "RAND(seed) supports only integer, boolean, and NULL seed literals"
            );
            return MYLITE_ERROR;
        }
        has_unary_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "RAND(seed) supports only integer, boolean, and NULL seed literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (has_unary_sign && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "RAND(seed) supports only integer, boolean, and NULL seed literals"
        );
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_seed = 0U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_seed = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_seed = 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "RAND(seed) supports only integer, boolean, and NULL seed literals"
        );
        return MYLITE_ERROR;
    }

    return rand_seed_literal_value(database, literal, is_negative, out_seed);
}

static int rand_seed_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint32_t *out_seed
) {
    uint64_t magnitude = 0U;
    uint32_t truncated = 0U;

    if (out_seed == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seed = 0U;
    if (literal == NULL ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_unsupported_error(database, "RAND(seed) integer literal is out of range");
        return MYLITE_ERROR;
    }

    truncated = (uint32_t)magnitude;
    if (is_negative) {
        *out_seed = (uint32_t)(0U - truncated);
    } else {
        *out_seed = truncated;
    }

    return MYLITE_OK;
}

static int current_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = format_current_timestamp_text(
        database,
        out_cell->datetime_text,
        sizeof(out_cell->datetime_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->datetime_text;
    }
    return rc;
}

static int sysdate_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    uint64_t precision = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = validate_supported_temporal_fractional_precision(
        database,
        expression,
        "sysdate",
        &precision
    );
    if (rc == MYLITE_OK) {
        rc = format_sysdate_text_with_precision(
            database,
            precision,
            precision == 0U ? out_cell->datetime_text : out_cell->fractional_datetime_text,
            precision == 0U ? sizeof(out_cell->datetime_text)
                            : sizeof(out_cell->fractional_datetime_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_cell->value =
            precision == 0U ? out_cell->datetime_text : out_cell->fractional_datetime_text;
    }
    return rc;
}

static int current_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = format_current_date_text(
        database,
        out_cell->datetime_text,
        sizeof(out_cell->datetime_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->datetime_text;
    }
    return rc;
}

static int current_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = format_current_time_text(
        database,
        out_cell->datetime_text,
        sizeof(out_cell->datetime_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->datetime_text;
    }
    return rc;
}

static int utc_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = format_utc_timestamp_text(
        database,
        out_cell->datetime_text,
        sizeof(out_cell->datetime_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->datetime_text;
    }
    return rc;
}

static int utc_date_scalar_value(struct mylite_db *database, struct session_scalar_cell *out_cell) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = format_utc_date_text(database, out_cell->datetime_text, sizeof(out_cell->datetime_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->datetime_text;
    }
    return rc;
}

static int utc_time_scalar_value(struct mylite_db *database, struct session_scalar_cell *out_cell) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = format_utc_time_text(database, out_cell->datetime_text, sizeof(out_cell->datetime_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->datetime_text;
    }
    return rc;
}

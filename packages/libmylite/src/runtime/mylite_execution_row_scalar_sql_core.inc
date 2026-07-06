static int append_row_scalar_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
        return mylite_dynamic_string_append(string, "NULL");
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_column_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
        return mylite_dynamic_string_append(string, "COUNT(*)");
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
        return append_row_scalar_sum_column_expression_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return append_row_scalar_window_function_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return append_row_scalar_integer_arithmetic_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
        return append_row_scalar_bitwise_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
        return append_row_scalar_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
        return append_row_scalar_concat_ws_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
        return append_row_scalar_field_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
        return append_row_scalar_elt_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return append_row_scalar_greatest_least_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
        return append_row_scalar_interval_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
        return append_row_scalar_date_format_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
        return append_row_scalar_date_format_numeric_comparison_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
        return append_row_scalar_date_interval_second_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
        return append_row_scalar_datediff_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
        return append_row_scalar_timediff_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
        return append_row_scalar_addtime_subtime_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
        return append_row_scalar_timestampdiff_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
        return append_row_scalar_timestamp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
        return append_row_scalar_string_length_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
        return append_row_scalar_string_codepoint_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
        return append_row_scalar_string_case_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
        return append_row_scalar_string_trim_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
        return append_row_scalar_unix_timestamp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        return append_row_scalar_temporal_extract_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
        return append_row_scalar_sec_to_time_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
        return append_row_scalar_from_unixtime_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
        return append_row_scalar_temporal_constructor_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_add",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_diff",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_convert_tz",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
        return append_row_scalar_string_slice_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
        return append_row_scalar_string_padding_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
        return append_row_scalar_string_bitmask_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
        return append_row_scalar_string_search_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
        return append_row_scalar_string_replace_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
        return append_row_scalar_string_insert_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
        return append_row_scalar_string_reverse_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
        return append_row_scalar_soundex_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
        return append_row_scalar_string_quote_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
        return append_row_scalar_substring_index_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
        return append_row_scalar_find_in_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
        return append_row_scalar_strcmp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
        return append_row_scalar_regexp_like_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
        return append_row_scalar_regexp_string_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
        return append_row_scalar_json_valid_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
        return append_row_scalar_json_schema_expression_sql(
            string,
            "_mylite_json_schema_valid",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
        return append_row_scalar_json_schema_expression_sql(
            string,
            "_mylite_json_schema_validation_report",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
        return append_row_scalar_json_search_expression_sql(
            string,
            "_mylite_json_contains",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
        return append_row_scalar_json_search_expression_sql(
            string,
            "_mylite_json_contains_path",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
        return append_row_scalar_json_search_expression_sql(
            string,
            "_mylite_json_search",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
        return append_row_scalar_json_search_expression_sql(
            string,
            "_mylite_json_overlaps",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
        return append_row_scalar_json_member_of_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
        return append_row_scalar_json_extract_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return append_row_scalar_json_value_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
        return append_row_scalar_json_unquote_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
        return append_row_scalar_json_quote_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
        return append_row_scalar_json_unquote_extract_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
        return append_row_scalar_json_length_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
        return append_row_scalar_json_depth_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
        return append_row_scalar_json_storage_expression_sql(
            string,
            "_mylite_json_storage_size",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
        return append_row_scalar_json_storage_expression_sql(
            string,
            "_mylite_json_storage_free",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
        return append_row_scalar_json_keys_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        return append_row_scalar_json_type_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
        return append_row_scalar_json_pretty_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return append_row_scalar_json_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
        return append_row_scalar_json_constructor_expression_sql(
            string,
            "_mylite_json_array",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        return append_row_scalar_json_constructor_expression_sql(
            string,
            "_mylite_json_object",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
        return append_row_scalar_hex_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
        return append_row_scalar_base64_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
        return append_row_scalar_unhex_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_compress",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_uncompress",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_uncompressed_length",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_random_bytes",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
        return append_row_scalar_spatial_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_inet_aton",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_inet_ntoa",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_last_insert_id_set",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_last_insert_id_get",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_get_lock",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_is_free_lock",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_is_used_lock",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_release_lock",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_release_all_locks",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_icu_version",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_benchmark",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_sleep",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_load_file",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_extract_value",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_update_xml",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_aes_encrypt",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_aes_decrypt",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION: {
        const char *sqlite_name =
            mylite_sys_function_sqlite_name(expression->sys_function_kind);

        if (sqlite_name == NULL) {
            return MYLITE_ERROR;
        }
        return append_row_scalar_registered_function_expression_sql(
            string,
            sqlite_name,
            expression,
            next_parameter
        );
    }
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
        return append_row_scalar_numeric_function_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
        return append_row_scalar_base_conversion_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
        return append_row_scalar_numeric_extra_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST: {
        const char *function_name = row_scalar_digest_sql_function_name(expression->digest_kind);

        if (function_name == NULL) {
            return MYLITE_ERROR;
        }
        return append_row_scalar_registered_function_expression_sql(
            string,
            function_name,
            expression,
            next_parameter
        );
    }
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string_binary",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return append_row_scalar_uuid_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
        return append_row_scalar_char_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
        return append_row_scalar_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        return append_row_scalar_rand_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return append_row_scalar_conversion_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
        return append_row_scalar_collate_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_collate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *sqlite_collation = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLLATE ||
        expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    sqlite_collation = row_scalar_sql_collation_name(expression->collation_name);
    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_expression_sql(string, &expression->arguments[0], next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " COLLATE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, sqlite_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_spatial_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION) {
        return MYLITE_ERROR;
    }
    function_name = mylite_spatial_function_name(expression->spatial_function_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    return append_row_scalar_registered_function_expression_sql(
        string,
        function_name,
        expression,
        next_parameter
    );
}

static const char *row_scalar_sql_collation_name(const char *collation_name) {
    if (row_scalar_mysql_collation_is_binary(collation_name)) {
        return "BINARY";
    }
    if (row_scalar_mysql_collation_is_case_sensitive(collation_name)) {
        return ascii_case_sensitive_collation_name;
    }
    return string_key_collation_name;
}

static bool row_scalar_mysql_collation_is_binary_sensitive(const char *collation_name) {
    return row_scalar_mysql_collation_is_binary(collation_name) ||
           row_scalar_mysql_collation_is_case_sensitive(collation_name);
}

static bool row_scalar_mysql_collation_is_binary(const char *collation_name) {
    if (collation_name == NULL) {
        return false;
    }
    return text_equals_ascii_case_insensitive(collation_name, "binary") ||
           text_has_ascii_case_insensitive_suffix(collation_name, "_bin");
}

static bool row_scalar_mysql_collation_is_case_sensitive(const char *collation_name) {
    if (collation_name == NULL) {
        return false;
    }
    return text_has_ascii_case_insensitive_suffix(collation_name, "_cs");
}

static bool row_scalar_expression_is_binary_sensitive_collate(
    const struct planned_row_scalar_expression *expression
) {
    return expression != NULL && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_COLLATE &&
           row_scalar_mysql_collation_is_binary_sensitive(expression->collation_name);
}

static bool text_has_ascii_case_insensitive_suffix(const char *text, const char *suffix) {
    size_t text_length = 0U;
    size_t suffix_length = 0U;

    if (text == NULL || suffix == NULL) {
        return false;
    }

    text_length = strlen(text);
    suffix_length = strlen(suffix);
    if (suffix_length > text_length) {
        return false;
    }
    return text_equals_ascii_case_insensitive(text + text_length - suffix_length, suffix);
}

static int append_row_scalar_sum_column_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
) {
    struct planned_row_scalar_expression column_expression = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN) {
        return MYLITE_ERROR;
    }

    column_expression.kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    column_expression.column = expression->column;
    column_expression.column_has_source_index = expression->column_has_source_index;
    column_expression.column_source_index = expression->column_source_index;

    rc = mylite_dynamic_string_append(string, "SUM(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_column_sql(string, &column_expression);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return MYLITE_ERROR;
    }
    if (expression->column_cast_as_integer) {
        rc = mylite_dynamic_string_append(string, "CAST(");
    }
    if (rc == MYLITE_OK && expression->column_has_source_index) {
        rc = append_select_source_alias(string, expression->column_source_index);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, expression->column.name);
    }
    if (rc == MYLITE_OK && expression->column_cast_as_integer) {
        rc = mylite_dynamic_string_append(string, " AS INTEGER)");
    }
    return rc;
}

static int append_row_scalar_window_function_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->window_function_kind == PLANNED_WINDOW_FUNCTION_AVG) {
        return append_row_scalar_avg_window_expression_sql(string, expression, next_parameter);
    }
    if (window_function_is_json_aggregate(expression->window_function_kind)) {
        return append_row_scalar_json_aggregate_window_expression_sql(
            string,
            expression,
            next_parameter
        );
    }
    function_name = window_function_sql_name(expression->window_function_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }

    return append_row_scalar_window_function_call_sql(
        string,
        expression,
        function_name,
        expression->window_function_kind == PLANNED_WINDOW_FUNCTION_COUNT_STAR,
        next_parameter
    );
}

static int append_row_scalar_avg_window_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_avg_window_result(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_window_function_call_sql(
            string,
            expression,
            "sum",
            false,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_window_function_call_sql(
            string,
            expression,
            "count",
            false,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_aggregate_window_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (string == NULL || expression == NULL || expression->arguments == NULL ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->window_function_kind == PLANNED_WINDOW_FUNCTION_JSON_ARRAYAGG &&
        expression->argument_count == 1U) {
        function_name = "_mylite_json_arrayagg";
    } else if (expression->window_function_kind == PLANNED_WINDOW_FUNCTION_JSON_OBJECTAGG &&
               expression->argument_count == 2U) {
        function_name = "_mylite_json_objectagg";
    } else {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_constructor_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_window_over_clause_sql(string, expression);
    }
    return rc;
}

static int append_row_scalar_window_function_call_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    const char *function_name,
    bool count_star,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (string == NULL || expression == NULL || function_name == NULL || next_parameter == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK && count_star) {
        rc = mylite_dynamic_string_append_char(string, '*');
    }
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && !count_star && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_window_function_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_window_over_clause_sql(string, expression);
    }
    return rc;
}

static int append_row_scalar_window_over_clause_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
) {
    int rc = MYLITE_OK;

    if (string == NULL || expression == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "OVER (");
    if (rc == MYLITE_OK && expression->window_has_partition) {
        rc = mylite_dynamic_string_append(string, "PARTITION BY ");
    }
    if (rc == MYLITE_OK && expression->window_has_partition) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &expression->window_partition_column,
            0U,
            false
        );
    }
    if (rc == MYLITE_OK && expression->window_has_partition && expression->window_has_order) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK && expression->window_has_order) {
        rc = mylite_dynamic_string_append(string, "ORDER BY ");
    }
    if (rc == MYLITE_OK && expression->window_has_order) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &expression->window_order_column,
            0U,
            false
        );
    }
    if (rc == MYLITE_OK && expression->window_has_order) {
        rc = append_select_order_direction_sql(string, expression->window_order_direction);
    }
    if (rc == MYLITE_OK && expression->window_has_frame &&
        (expression->window_has_partition || expression->window_has_order)) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK && expression->window_has_frame) {
        rc = append_row_scalar_window_frame_sql(string, expression);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_window_frame_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || !expression->window_has_frame) {
        return MYLITE_ERROR;
    }
    if (expression->window_frame_unit == PLANNED_WINDOW_FRAME_UNIT_ROWS) {
        rc = mylite_dynamic_string_append(string, "ROWS ");
    } else if (expression->window_frame_unit == PLANNED_WINDOW_FRAME_UNIT_RANGE) {
        rc = mylite_dynamic_string_append(string, "RANGE ");
    } else {
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && expression->window_frame_end.kind != PLANNED_WINDOW_FRAME_BOUND_NONE) {
        rc = mylite_dynamic_string_append(string, "BETWEEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_window_frame_bound_sql(string, &expression->window_frame_start);
    }
    if (rc == MYLITE_OK && expression->window_frame_end.kind != PLANNED_WINDOW_FRAME_BOUND_NONE) {
        rc = mylite_dynamic_string_append(string, " AND ");
    }
    if (rc == MYLITE_OK && expression->window_frame_end.kind != PLANNED_WINDOW_FRAME_BOUND_NONE) {
        rc = append_row_scalar_window_frame_bound_sql(string, &expression->window_frame_end);
    }
    return rc;
}

static int append_row_scalar_window_frame_bound_sql(
    struct mylite_dynamic_string *string,
    const struct planned_window_frame_bound *bound
) {
    enum { window_frame_bound_text_capacity = 32 };

    char buffer[window_frame_bound_text_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (bound == NULL) {
        return MYLITE_ERROR;
    }
    switch (bound->kind) {
    case PLANNED_WINDOW_FRAME_BOUND_UNBOUNDED_PRECEDING:
        return mylite_dynamic_string_append(string, "UNBOUNDED PRECEDING");
    case PLANNED_WINDOW_FRAME_BOUND_UNBOUNDED_FOLLOWING:
        return mylite_dynamic_string_append(string, "UNBOUNDED FOLLOWING");
    case PLANNED_WINDOW_FRAME_BOUND_CURRENT_ROW:
        return mylite_dynamic_string_append(string, "CURRENT ROW");
    case PLANNED_WINDOW_FRAME_BOUND_VALUE_PRECEDING:
    case PLANNED_WINDOW_FRAME_BOUND_VALUE_FOLLOWING:
        written = snprintf(buffer, sizeof(buffer), "%" PRIu64, bound->value);
        if (written < 0 || (size_t)written >= sizeof(buffer)) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append(string, buffer);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(
                string,
                bound->kind == PLANNED_WINDOW_FRAME_BOUND_VALUE_PRECEDING ? " PRECEDING"
                                                                          : " FOLLOWING"
            );
        }
        return rc;
    case PLANNED_WINDOW_FRAME_BOUND_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_window_function_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    if (argument == NULL || next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    return append_row_scalar_expression_sql(string, argument, next_parameter);
}

static const char *window_function_sql_name(enum planned_window_function_kind kind) {
    switch (kind) {
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
        return "row_number";
    case PLANNED_WINDOW_FUNCTION_RANK:
        return "rank";
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
        return "dense_rank";
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
        return "percent_rank";
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        return "cume_dist";
    case PLANNED_WINDOW_FUNCTION_NTILE:
        return "ntile";
    case PLANNED_WINDOW_FUNCTION_LAG:
        return "lag";
    case PLANNED_WINDOW_FUNCTION_LEAD:
        return "lead";
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
        return "first_value";
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
        return "last_value";
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return "nth_value";
    case PLANNED_WINDOW_FUNCTION_COUNT_STAR:
    case PLANNED_WINDOW_FUNCTION_COUNT_VALUE:
        return "count";
    case PLANNED_WINDOW_FUNCTION_SUM:
        return "sum";
    case PLANNED_WINDOW_FUNCTION_AVG:
        return "avg";
    case PLANNED_WINDOW_FUNCTION_MIN:
        return "min";
    case PLANNED_WINDOW_FUNCTION_MAX:
        return "max";
    case PLANNED_WINDOW_FUNCTION_BIT_AND:
        return "_mylite_bit_and";
    case PLANNED_WINDOW_FUNCTION_BIT_OR:
        return "_mylite_bit_or";
    case PLANNED_WINDOW_FUNCTION_BIT_XOR:
        return "_mylite_bit_xor";
    case PLANNED_WINDOW_FUNCTION_STD:
    case PLANNED_WINDOW_FUNCTION_STDDEV:
    case PLANNED_WINDOW_FUNCTION_STDDEV_POP:
        return "_mylite_stddev_pop";
    case PLANNED_WINDOW_FUNCTION_STDDEV_SAMP:
        return "_mylite_stddev_samp";
    case PLANNED_WINDOW_FUNCTION_VAR_POP:
    case PLANNED_WINDOW_FUNCTION_VARIANCE:
        return "_mylite_var_pop";
    case PLANNED_WINDOW_FUNCTION_VAR_SAMP:
        return "_mylite_var_samp";
    case PLANNED_WINDOW_FUNCTION_GROUP_CONCAT:
    case PLANNED_WINDOW_FUNCTION_JSON_ARRAYAGG:
    case PLANNED_WINDOW_FUNCTION_JSON_OBJECTAGG:
    case PLANNED_WINDOW_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_rand_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_RAND ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    if (!expression->has_rand_seed) {
        return mylite_dynamic_string_append(string, "_mylite_rand()");
    }
    if (expression->rand_seed_is_expression) {
        if (expression->arguments == NULL || expression->argument_count != 1U) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append(string, "_mylite_rand_seeded_once(");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_conversion_expression_sql(
                string,
                &expression->arguments[0],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_rand_seeded(");
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_conversion_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U ||
        expression->conversion_step_count == 0U) {
        return MYLITE_ERROR;
    }

    for (size_t step_index = 0U; step_index < expression->conversion_step_count; ++step_index) {
        const char *function_name =
            row_scalar_conversion_sql_function_name(&expression->conversion_steps[step_index]);

        if (function_name == NULL) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append(string, function_name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '(');
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    rc = append_row_scalar_conversion_base_expression_sql(
        string,
        &expression->arguments[0],
        next_parameter
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t step_index = 0U; step_index < expression->conversion_step_count; ++step_index) {
        rc = mylite_dynamic_string_append_char(string, ')');
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_row_scalar_conversion_base_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
        return mylite_dynamic_string_append(string, "NULL");
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_column_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
        return append_row_scalar_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

static const char *row_scalar_conversion_sql_function_name(
    const struct planned_row_scalar_conversion_step *step
) {
    if (step == NULL) {
        return NULL;
    }

    switch (step->kind) {
    case PLANNED_ROW_SCALAR_CONVERSION_SIGNED:
        return "_mylite_cast_signed";
    case PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED:
        return "_mylite_cast_unsigned";
    case PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET:
        if (step->ascii_only) {
            return "_mylite_cast_convert_text_ascii";
        }
        if (step->charset != NULL &&
            (strcmp(step->charset, "ascii") == 0 || strcmp(step->charset, "ujis") == 0 ||
             strcmp(step->charset, "utf8mb3") == 0 || strcmp(step->charset, "utf8mb4") == 0)) {
            return "_mylite_cast_convert_text";
        }
        if (step->charset != NULL && strcmp(step->charset, "big5") == 0) {
            return "_mylite_cast_convert_bytes_big5";
        }
        return "_mylite_cast_convert_bytes";
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
        return "_mylite_cast_convert_bytes";
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
        return "_mylite_cast_convert_text";
    case PLANNED_ROW_SCALAR_CONVERSION_NONE:
        break;
    }
    return NULL;
}

// Row-scalar SQL generation walks expression trees recursively through helper-specific nodes.
// NOLINTNEXTLINE(misc-no-recursion)
static int append_row_scalar_non_concat_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
        return append_row_scalar_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return append_row_scalar_greatest_least_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
        return append_row_scalar_collate_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
        return append_row_scalar_expression_sql(string, expression, next_parameter);
    }
    return append_row_scalar_non_control_flow_expression_sql(string, expression, next_parameter);
}

static int append_row_scalar_non_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
        return mylite_dynamic_string_append(string, "NULL");
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_column_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
        return append_row_scalar_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return append_row_scalar_greatest_least_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
        return append_row_scalar_numeric_function_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return append_row_scalar_integer_arithmetic_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
        return append_row_scalar_bitwise_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return append_row_scalar_conversion_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
        return append_row_scalar_collate_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return append_row_scalar_uuid_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_add",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_period_diff",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_convert_tz",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_compress",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_uncompress",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_uncompressed_length",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_random_bytes",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_aes_encrypt",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_aes_decrypt",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_inet_aton",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_inet_ntoa",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_last_insert_id_set",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_last_insert_id_get",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
        return append_row_scalar_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return append_row_scalar_registered_function_expression_sql(
            string,
            "_mylite_weight_string_binary",
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
        return append_row_scalar_char_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_integer_arithmetic_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    struct row_scalar_integer_arithmetic_append_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = row_scalar_integer_arithmetic_append_stack_push(
        &stack,
        (struct row_scalar_integer_arithmetic_append_frame){
            .expression = expression,
            .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER,
        }
    );

    while (rc == MYLITE_OK && stack.count != 0U) {
        struct row_scalar_integer_arithmetic_append_frame frame = stack.items[--stack.count];

        rc = append_row_scalar_integer_arithmetic_frame_sql(string, &stack, &frame, next_parameter);
    }
    row_scalar_integer_arithmetic_append_stack_deinit(&stack);
    return rc;
}

static int append_row_scalar_integer_arithmetic_frame_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct row_scalar_integer_arithmetic_append_frame *frame,
    size_t *next_parameter
) {
    if (frame == NULL) {
        return MYLITE_ERROR;
    }

    switch (frame->phase) {
    case ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_COMMA:
        return mylite_dynamic_string_append(string, ", ");
    case ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_CLOSE:
        return mylite_dynamic_string_append_char(string, ')');
    case ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER:
        return append_row_scalar_integer_arithmetic_enter_sql(
            string,
            stack,
            frame->expression,
            next_parameter
        );
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_integer_arithmetic_enter_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
        return mylite_dynamic_string_append(string, "NULL");
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_integer_arithmetic_column_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
        return append_row_scalar_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return append_row_scalar_integer_arithmetic_function_call_sql(string, stack, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
        return append_row_scalar_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
        return append_row_scalar_string_length_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
        return append_row_scalar_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
        return append_row_scalar_unix_timestamp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        rc = mylite_dynamic_string_append(string, "CAST(");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_temporal_extract_expression_sql(
                string,
                expression,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS INTEGER)");
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return MYLITE_ERROR;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_integer_arithmetic_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return MYLITE_ERROR;
    }
    if (expression->column_has_source_index) {
        rc = append_select_source_alias(string, expression->column_source_index);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, expression->column.name);
    }
    return rc;
}

static int append_row_scalar_integer_arithmetic_function_call_sql(
    struct mylite_dynamic_string *string,
    struct row_scalar_integer_arithmetic_append_stack *stack,
    const struct planned_row_scalar_expression *expression
) {
    const char *function_name = row_scalar_integer_arithmetic_function_name(expression);
    int rc = MYLITE_OK;

    if (function_name == NULL || expression->arguments == NULL ||
        expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = row_scalar_integer_arithmetic_append_stack_push(
        stack,
        (struct row_scalar_integer_arithmetic_append_frame){
            .expression = NULL,
            .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_CLOSE,
        }
    );
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_arithmetic_append_stack_push(
            stack,
            (struct row_scalar_integer_arithmetic_append_frame){
                .expression = &expression->arguments[1],
                .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_arithmetic_append_stack_push(
            stack,
            (struct row_scalar_integer_arithmetic_append_frame){
                .expression = NULL,
                .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_COMMA,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_arithmetic_append_stack_push(
            stack,
            (struct row_scalar_integer_arithmetic_append_frame){
                .expression = &expression->arguments[0],
                .phase = ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER,
            }
        );
    }
    return rc;
}

static const char *row_scalar_integer_arithmetic_function_name(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return NULL;
    }
    switch (expression->arithmetic_operator) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "_mylite_arithmetic_add";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "_mylite_arithmetic_sub";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "_mylite_arithmetic_mul";
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        return "_mylite_arithmetic_div";
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        return "_mylite_arithmetic_int_div";
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        return "_mylite_arithmetic_mod";
    default:
        return NULL;
    }
}

static void row_scalar_integer_arithmetic_append_stack_deinit(
    struct row_scalar_integer_arithmetic_append_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct row_scalar_integer_arithmetic_append_stack){0};
}

static int row_scalar_integer_arithmetic_append_stack_push(
    struct row_scalar_integer_arithmetic_append_stack *stack,
    struct row_scalar_integer_arithmetic_append_frame frame
) {
    struct row_scalar_integer_arithmetic_append_frame *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_MISUSE;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static int append_row_scalar_concat_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK && expression->argument_count == 1U) {
        rc = mylite_dynamic_string_append(string, "'' || ");
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, " || ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_row_scalar_concat_ws_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_concat_ws(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_row_scalar_field_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    bool use_string_collation = expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    int rc = MYLITE_OK;

    if (expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_field_operand_sql(
            string,
            &expression->arguments[0],
            next_parameter,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN 0");
    }
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        char position_text[integer_text_capacity];
        int written = snprintf(position_text, sizeof(position_text), "%zu", argument_index);

        if (written < 0 || (size_t)written >= sizeof(position_text)) {
            return MYLITE_NOMEM;
        }
        rc = mylite_dynamic_string_append(string, " WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_field_operand_sql(
                string,
                &expression->arguments[0],
                next_parameter,
                use_string_collation
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_field_operand_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter,
                use_string_collation
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " THEN ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, position_text);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE 0 END)");
    }

    return rc;
}

static int append_row_scalar_elt_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_expression_sql(string, &expression->arguments[0], next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN NULL");
    }
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        char position_text[integer_text_capacity];
        int written = snprintf(position_text, sizeof(position_text), "%zu", argument_index);

        if (written < 0 || (size_t)written >= sizeof(position_text)) {
            return MYLITE_NOMEM;
        }
        rc = mylite_dynamic_string_append(string, " WHEN ");
        if (rc == MYLITE_OK) {
            rc =
                append_row_scalar_expression_sql(string, &expression->arguments[0], next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, position_text);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " THEN ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE NULL END)");
    }

    return rc;
}

static int append_row_scalar_greatest_least_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *sqlite_function = NULL;
    bool use_string_collation = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_GREATEST) {
        sqlite_function = "max";
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_LEAST) {
        sqlite_function = "min";
    } else {
        return MYLITE_ERROR;
    }

    use_string_collation = expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    rc = mylite_dynamic_string_append(string, sqlite_function);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t offset = 0U; rc == MYLITE_OK && offset < expression->argument_count; ++offset) {
        size_t argument_index = expression->argument_count - 1U - offset;

        if (offset != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_field_operand_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter,
                use_string_collation
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_interval_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL ||
        expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN -1");
    }
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = mylite_dynamic_string_append(string, " WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[0],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " < ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " THEN ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_interval_result_sql(string, (int64_t)argument_index - 1);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_interval_result_sql(string, (int64_t)expression->argument_count - 1);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_interval_result_sql(
    struct mylite_dynamic_string *string,
    int64_t value
) {
    char text[integer_text_capacity];
    int written = snprintf(text, sizeof(text), "%" PRId64, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }
    return mylite_dynamic_string_append(string, text);
}

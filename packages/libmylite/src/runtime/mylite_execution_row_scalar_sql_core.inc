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
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
        return append_row_scalar_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
        return append_row_scalar_concat_ws_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
        return append_row_scalar_field_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return append_row_scalar_greatest_least_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
        return append_row_scalar_interval_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
        return append_row_scalar_date_format_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
        return append_row_scalar_date_format_numeric_equal_sql(string, expression, next_parameter);
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
        return append_row_scalar_json_keys_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        return append_row_scalar_json_type_expression_sql(string, expression, next_parameter);
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
        return append_row_scalar_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        return append_row_scalar_rand_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return append_row_scalar_conversion_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
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
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    function_name = window_function_sql_name(expression->window_function_kind);
    if (function_name == NULL) {
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
            rc = append_row_scalar_window_function_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") OVER (");
    }
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
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_window_function_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL || next_parameter == NULL) {
        return MYLITE_ERROR;
    }
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_descriptor_value_sql_for_source(string, &argument->column, 0U, false);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
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
            rc =
                append_row_scalar_expression_sql(string, &expression->arguments[0], next_parameter);
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
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_column_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        return append_row_scalar_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
        return "_mylite_cast_convert_text";
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
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
    int rc = MYLITE_OK;

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_column_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
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
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return append_row_scalar_conversion_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        return append_row_scalar_control_flow_expression_sql(string, expression, next_parameter);
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
        rc = mylite_dynamic_string_append(string, "CAST(");
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS INTEGER)");
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_column_sql(string, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return append_row_scalar_integer_arithmetic_function_call_sql(string, stack, expression);
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
        return append_row_scalar_string_length_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
        return append_row_scalar_unix_timestamp_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
        return "_mylite_i64_add";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "_mylite_i64_sub";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "_mylite_i64_mul";
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
            if (expression->arguments[argument_index].kind ==
                PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH) {
                rc = append_row_scalar_string_length_expression_sql(
                    string,
                    &expression->arguments[argument_index],
                    next_parameter
                );
            } else if (
                expression->arguments[argument_index].kind ==
                PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE
            ) {
                rc = append_row_scalar_string_slice_expression_sql(
                    string,
                    &expression->arguments[argument_index],
                    next_parameter
                );
            } else {
                rc = append_row_scalar_non_concat_expression_sql(
                    string,
                    &expression->arguments[argument_index],
                    next_parameter
                );
            }
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
            if (expression->arguments[argument_index].kind ==
                PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH) {
                rc = append_row_scalar_string_length_expression_sql(
                    string,
                    &expression->arguments[argument_index],
                    next_parameter
                );
            } else if (
                expression->arguments[argument_index].kind ==
                PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE
            ) {
                rc = append_row_scalar_string_slice_expression_sql(
                    string,
                    &expression->arguments[argument_index],
                    next_parameter
                );
            } else {
                rc = append_row_scalar_non_concat_expression_sql(
                    string,
                    &expression->arguments[argument_index],
                    next_parameter
                );
            }
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

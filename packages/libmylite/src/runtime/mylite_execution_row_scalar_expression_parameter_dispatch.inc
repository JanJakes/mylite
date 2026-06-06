
static int bind_row_scalar_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return bind_row_scalar_window_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return bind_row_scalar_integer_arithmetic_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
        for (size_t argument_index = 0U;
             rc == MYLITE_OK && argument_index < expression->argument_count;
             ++argument_index) {
            if (expression->arguments[argument_index].kind ==
                PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH) {
                rc = bind_row_scalar_string_length_expression_parameters(
                    statement,
                    &expression->arguments[argument_index],
                    parameter_index
                );
            } else if (
                expression->arguments[argument_index].kind ==
                PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE
            ) {
                rc = bind_row_scalar_string_slice_expression_parameters(
                    statement,
                    &expression->arguments[argument_index],
                    parameter_index
                );
            } else {
                rc = bind_row_scalar_non_concat_expression_parameters(
                    statement,
                    &expression->arguments[argument_index],
                    parameter_index
                );
            }
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
        return bind_row_scalar_field_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
        return bind_row_scalar_reversed_arguments_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
        return bind_row_scalar_interval_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
        return bind_row_scalar_date_format_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
        if (expression->argument_count != 2U) {
            return MYLITE_ERROR;
        }
        rc = bind_row_scalar_date_format_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[1],
                parameter_index
            );
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
        return bind_row_scalar_date_interval_second_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
        return bind_row_scalar_datediff_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
        return bind_row_scalar_timediff_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
        return bind_row_scalar_timestampdiff_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
        return bind_row_scalar_timestamp_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
        return bind_row_scalar_string_length_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
        return bind_row_scalar_string_codepoint_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
        return bind_row_scalar_string_case_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
        return bind_row_scalar_string_trim_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
        return bind_row_scalar_unix_timestamp_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        return bind_row_scalar_temporal_extract_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
        return bind_row_scalar_sec_to_time_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
        return bind_row_scalar_from_unixtime_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
        return bind_row_scalar_temporal_constructor_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        return bind_row_scalar_registered_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
        return bind_row_scalar_string_slice_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
        return bind_row_scalar_string_padding_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
        return bind_row_scalar_string_bitmask_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
        return bind_row_scalar_string_search_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
        return bind_row_scalar_string_replace_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
        return bind_row_scalar_string_insert_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
        return bind_row_scalar_string_reverse_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
        return bind_row_scalar_soundex_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
        return bind_row_scalar_string_quote_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
        return bind_row_scalar_substring_index_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
        return bind_row_scalar_find_in_set_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
        return bind_row_scalar_strcmp_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
        return bind_row_scalar_regexp_like_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
        return bind_row_scalar_regexp_string_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
        return bind_row_scalar_json_valid_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
        return bind_row_scalar_json_search_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
        return bind_row_scalar_json_extract_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return bind_row_scalar_json_value_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
        return bind_row_scalar_json_unquote_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
        return bind_row_scalar_json_quote_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
        return bind_row_scalar_json_length_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
        return bind_row_scalar_json_keys_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        return bind_row_scalar_json_type_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return bind_row_scalar_json_set_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        return bind_row_scalar_json_constructor_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
        return bind_row_scalar_hex_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
        return bind_row_scalar_base64_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
        return bind_row_scalar_unhex_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return bind_row_scalar_registered_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return bind_row_scalar_uuid_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
        return bind_row_scalar_char_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
        return bind_row_scalar_control_flow_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        return bind_row_scalar_rand_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return bind_row_scalar_conversion_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan) {
    int parameter_index = 1;

    return bind_select_parameters_at(statement, plan, &parameter_index);
}

static int bind_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    rc = bind_select_predicate_parameters(statement, &plan->predicate, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_select_order_parameters(statement, &plan->order, parameter_index);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
    }

    return rc;
}

static int bind_row_scalar_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan
) {
    int parameter_index = 1;

    return bind_row_scalar_select_parameters_at(statement, plan, &parameter_index);
}

static int bind_row_scalar_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &plan->items[item_index].expression,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, parameter_index);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = bind_select_order_parameters(statement, &plan->order, parameter_index);
    }
    if (rc == MYLITE_OK && plan->has_source && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && plan->has_source && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->tableless_filter.has_filter &&
        plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_predicate_parameters_without_exists(
            statement,
            &plan->tableless_filter.predicate,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->tableless_filter.has_filter &&
        !plan->tableless_filter.has_scalar_predicate) {
        rc = bind_select_exists_predicate_parameters(
            statement,
            &plan->tableless_filter.subquery,
            parameter_index
        );
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }
    if (rc == MYLITE_OK && !plan->has_source && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, *parameter_index, plan->limit.offset);
    }

    return rc;
}

static int bind_select_order_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_order *order,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (order == NULL || !order->has_order) {
        return MYLITE_OK;
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < order->item_count; ++item_index) {
        const struct planned_select_order_item *item = &order->items[item_index];

        if (item->kind == PLANNED_SELECT_ORDER_ITEM_FIELD ||
            item->kind == PLANNED_SELECT_ORDER_ITEM_RAND) {
            rc =
                bind_row_scalar_expression_parameters(statement, item->expression, parameter_index);
        }
    }

    return rc;
}

static int bind_insert_select_materialize_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return bind_insert_select_compound_parameters(statement, plan);
    }

    return bind_select_parameters(statement, &plan->source);
}

static int bind_insert_select_compound_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    for (size_t branch_index = 0U;
         rc == MYLITE_OK && branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR) {
            rc = bind_row_scalar_select_parameters_at(
                statement,
                &branch->row_source,
                &parameter_index
            );
        } else {
            rc = bind_select_parameters_at(statement, &branch->source, &parameter_index);
        }
    }

    return rc;
}

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
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
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
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
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

static int bind_row_scalar_window_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (statement == NULL || expression == NULL || parameter_index == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION) {
        return MYLITE_MISUSE;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_window_function_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_window_function_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (argument == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &argument->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_rand_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (statement == NULL || expression == NULL || parameter_index == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_RAND) {
        return MYLITE_MISUSE;
    }
    if (!expression->has_rand_seed) {
        return MYLITE_OK;
    }

    rc = bind_int64_parameter(statement, *parameter_index, (int64_t)expression->rand_seed);
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
    }
    return rc;
}

static int bind_row_scalar_conversion_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_conversion_base_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_conversion_base_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
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

// Row-scalar parameter binding follows the same recursive expression tree as SQL generation.
// NOLINTNEXTLINE(misc-no-recursion)
static int bind_row_scalar_non_concat_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
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
        return bind_row_scalar_conversion_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return bind_row_scalar_uuid_expression_parameters(statement, expression, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        return bind_row_scalar_registered_function_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_integer_arithmetic_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    struct row_scalar_integer_arithmetic_bind_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = row_scalar_integer_arithmetic_bind_stack_push(&stack, expression);
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct planned_row_scalar_expression *current = stack.items[--stack.count];

        rc = bind_row_scalar_integer_arithmetic_frame_parameters(
            statement,
            &stack,
            current,
            parameter_index
        );
    }
    row_scalar_integer_arithmetic_bind_stack_deinit(&stack);
    return rc;
}

static int bind_row_scalar_integer_arithmetic_frame_parameters(
    sqlite3_stmt *statement,
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        if (expression->arguments == NULL || expression->argument_count != 2U) {
            return MYLITE_ERROR;
        }
        rc = row_scalar_integer_arithmetic_bind_stack_push(stack, &expression->arguments[1]);
        if (rc == MYLITE_OK) {
            rc = row_scalar_integer_arithmetic_bind_stack_push(stack, &expression->arguments[0]);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return MYLITE_ERROR;
    }
    return MYLITE_ERROR;
}

static void row_scalar_integer_arithmetic_bind_stack_deinit(
    struct row_scalar_integer_arithmetic_bind_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free((void *)stack->items);
    *stack = (struct row_scalar_integer_arithmetic_bind_stack){0};
}

static int row_scalar_integer_arithmetic_bind_stack_push(
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression
) {
    const struct planned_row_scalar_expression **items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_MISUSE;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return MYLITE_NOMEM;
        }
        items = (const struct planned_row_scalar_expression **)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = expression;
    ++stack->count;
    return MYLITE_OK;
}

static int bind_row_scalar_field_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
    }

    return rc;
}

static int bind_row_scalar_reversed_arguments_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }
    for (size_t offset = 0U; rc == MYLITE_OK && offset < expression->argument_count; ++offset) {
        size_t argument_index = expression->argument_count - 1U - offset;

        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_interval_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL ||
        expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
    }
    return rc;
}

static int bind_row_scalar_date_format_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_date_interval_second_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != date_interval_planned_argument_count) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < date_interval_planned_argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_datediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_timediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_timestampdiff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        expression->argument_count != planned_row_scalar_timestampdiff_argument_count) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < planned_row_scalar_timestampdiff_argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_codepoint_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_case_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_trim_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_temporal_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_sec_to_time_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_from_unixtime_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_temporal_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_unix_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_slice_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return bind_row_scalar_substring_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    }
    return bind_row_scalar_left_right_expression_parameters(statement, expression, parameter_index);
}

static int bind_row_scalar_string_padding_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_bitmask_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_replace_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_insert_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_reverse_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_soundex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_substring_index_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_find_in_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_strcmp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_regexp_like_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[1],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_regexp_string_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[1],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
    }
    if (rc == MYLITE_OK && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[2],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_valid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_json_introspection_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_json_extract_document_argument_parameters(
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
}

static int bind_row_scalar_json_value_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_json_extract_document_argument_parameters(
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
}

static int bind_row_scalar_json_unquote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_json_introspection_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_extract_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
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
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_json_introspection_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return bind_row_scalar_json_extract_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_json_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_json_introspection_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_keys_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_json_introspection_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_json_type_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_json_introspection_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_json_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->json_mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        if (expression->argument_count < 2U) {
            return MYLITE_ERROR;
        }
    } else if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
        return MYLITE_ERROR;
    }

    rc = bind_row_scalar_json_set_document_argument_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    if (expression->json_mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        for (size_t argument_index = 1U;
             rc == MYLITE_OK && argument_index < expression->argument_count;
             ++argument_index) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
    } else {
        for (size_t argument_index = 1U;
             rc == MYLITE_OK && argument_index < expression->argument_count;
             argument_index += 2U) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
            if (rc != MYLITE_OK) {
                break;
            }
            rc = bind_row_scalar_json_set_value_argument_parameters(
                statement,
                &expression->arguments[argument_index + 1U],
                parameter_index
            );
        }
    }
    return rc;
}

static int bind_row_scalar_json_set_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_json_set_value_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        return bind_row_scalar_json_constructor_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    default:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    }
}

static int bind_row_scalar_json_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_control_flow_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_nested_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_control_flow_leaf_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_control_flow_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return bind_row_scalar_nested_control_flow_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_control_flow_leaf_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_non_concat_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_left_right_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_LEFT &&
        expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_RIGHT) {
        return MYLITE_ERROR;
    }
    value = &expression->arguments[0];
    length = &expression->arguments[1];

    rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    return rc;
}

static int bind_row_scalar_substring_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *position = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    bool has_length = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return MYLITE_ERROR;
    }

    value = &expression->arguments[0];
    position = &expression->arguments[1];
    has_length = expression->argument_count == 3U;
    if (has_length) {
        length = &expression->arguments[2];
    }

    rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, length, parameter_index);
    }
    return rc;
}

static int bind_row_scalar_hex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    const struct planned_row_scalar_expression *argument = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    argument = &expression->arguments[0];

    rc = bind_row_scalar_non_concat_expression_parameters(statement, argument, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(statement, argument, parameter_index);
    }
    return rc;
}

static int bind_row_scalar_unhex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_base64_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

// Registered scalar functions delegate argument binding back to the general row-scalar walker.
// NOLINTNEXTLINE(misc-no-recursion)
static int bind_row_scalar_registered_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_uuid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_uuid_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            argument_index,
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_uuid_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    size_t argument_index,
    int *parameter_index
) {
    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return bind_row_scalar_uuid_leaf_argument_parameters(statement, argument, parameter_index);
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        if (argument_index == 0U) {
            return bind_row_scalar_uuid_inner_expression_parameters(
                statement,
                argument,
                parameter_index
            );
        }
        break;
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
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_uuid_inner_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_uuid_leaf_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_uuid_leaf_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = bind_planned_value_parameter(statement, *parameter_index, &argument->value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return MYLITE_OK;
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
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static int bind_row_scalar_char_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

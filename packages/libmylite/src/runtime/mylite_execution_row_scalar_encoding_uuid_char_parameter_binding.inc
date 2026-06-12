
static int bind_row_scalar_string_slice_operand_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression != NULL &&
        expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC) {
        return bind_row_scalar_integer_arithmetic_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    }
    return bind_row_scalar_non_concat_expression_parameters(statement, expression, parameter_index);
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

    rc = bind_row_scalar_string_slice_operand_parameters(statement, value, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, length, parameter_index);
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

    rc = bind_row_scalar_string_slice_operand_parameters(statement, value, parameter_index);
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, length, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, value, parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, position, parameter_index);
    }
    if (rc == MYLITE_OK && has_length) {
        rc = bind_row_scalar_string_slice_operand_parameters(statement, length, parameter_index);
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
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
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

static int bind_row_scalar_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    return bind_row_scalar_control_flow_arguments(
        statement,
        expression,
        ROW_SCALAR_CONTROL_FLOW_BIND_ARGUMENT,
        parameter_index
    );
}

static int bind_row_scalar_control_flow_arguments(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    enum row_scalar_control_flow_bind_mode mode,
    int *parameter_index
) {
    struct row_scalar_control_flow_bind_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }

    rc = row_scalar_control_flow_bind_stack_push_arguments(&stack, expression, mode);
    while (rc == MYLITE_OK && stack.count != 0U) {
        struct row_scalar_control_flow_bind_frame frame = stack.items[--stack.count];

        rc = bind_row_scalar_control_flow_frame_parameters(
            statement,
            &stack,
            &frame,
            parameter_index
        );
    }

    row_scalar_control_flow_bind_stack_deinit(&stack);
    return rc;
}

static int bind_row_scalar_control_flow_frame_parameters(
    sqlite3_stmt *statement,
    struct row_scalar_control_flow_bind_stack *stack,
    const struct row_scalar_control_flow_bind_frame *frame,
    int *parameter_index
) {
    const struct planned_row_scalar_expression *expression = NULL;

    if (frame == NULL || frame->expression == NULL) {
        return MYLITE_ERROR;
    }
    expression = frame->expression;

    switch (frame->mode) {
    case ROW_SCALAR_CONTROL_FLOW_BIND_ARGUMENT:
        switch (expression->kind) {
        case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
            return bind_row_scalar_control_flow_value_or_column_parameters(
                statement,
                expression,
                parameter_index
            );
        case PLANNED_ROW_SCALAR_EXPRESSION_IF:
        case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
        case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
        case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
        case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
            return row_scalar_control_flow_bind_stack_push_arguments(
                stack,
                expression,
                ROW_SCALAR_CONTROL_FLOW_BIND_LEAF_ARGUMENT
            );
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
            break;
        }
        break;
    case ROW_SCALAR_CONTROL_FLOW_BIND_LEAF_ARGUMENT:
        switch (expression->kind) {
        case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
            return bind_row_scalar_control_flow_value_or_column_parameters(
                statement,
                expression,
                parameter_index
            );
        case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
            return row_scalar_control_flow_bind_stack_push_arguments(
                stack,
                expression,
                ROW_SCALAR_CONTROL_FLOW_BIND_LEAF_ARGUMENT
            );
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
            break;
        }
        break;
    }

    return MYLITE_ERROR;
}

static int bind_row_scalar_control_flow_value_or_column_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return MYLITE_OK;
    }
    if (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
        return MYLITE_ERROR;
    }

    rc = bind_planned_value_parameter(statement, *parameter_index, &expression->value);
    if (rc == MYLITE_OK) {
        ++(*parameter_index);
    }
    return rc;
}

static int row_scalar_control_flow_bind_stack_push_arguments(
    struct row_scalar_control_flow_bind_stack *stack,
    const struct planned_row_scalar_expression *expression,
    enum row_scalar_control_flow_bind_mode mode
) {
    if (expression == NULL || (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }

    for (size_t remaining = expression->argument_count; remaining != 0U; --remaining) {
        int rc = row_scalar_control_flow_bind_stack_push(
            stack,
            (struct row_scalar_control_flow_bind_frame){
                .expression = &expression->arguments[remaining - 1U],
                .mode = mode,
            }
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int row_scalar_control_flow_bind_stack_push(
    struct row_scalar_control_flow_bind_stack *stack,
    struct row_scalar_control_flow_bind_frame frame
) {
    struct row_scalar_control_flow_bind_frame *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_ERROR;
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

static void row_scalar_control_flow_bind_stack_deinit(
    struct row_scalar_control_flow_bind_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct row_scalar_control_flow_bind_stack){0};
}

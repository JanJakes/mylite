
static int if_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct if_eval_stack stack = {0};
    struct session_scalar_cell cell = {0};
    const struct mylite_sql_ast_node *current = expression;
    const char *function_name = if_function_name(expression);
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = validate_if_value_expression(database, expression, function_name);
    while (rc == MYLITE_OK) {
        if (current != NULL) {
            rc = if_eval_current_expression(
                database,
                &stack,
                current,
                function_name,
                &current,
                &cell
            );
            continue;
        }
        if (if_eval_completed_value(&stack, &cell, &current, out_cell)) {
            break;
        }
    }

    if_eval_stack_deinit(&stack);
    return rc;
}

static int if_eval_current_expression(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **next_expression,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IF_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 3U) {
            set_if_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        rc = if_eval_stack_push(
            database,
            stack,
            IF_EVAL_FRAME_IF,
            child_at(expression, 1U),
            child_at(expression, 2U)
        );
        if (rc == MYLITE_OK) {
            *next_expression = child_at(expression, 0U);
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_IFNULL_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            set_if_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        rc = if_eval_stack_push(
            database,
            stack,
            IF_EVAL_FRAME_IFNULL,
            child_at(expression, 1U),
            NULL
        );
        if (rc == MYLITE_OK) {
            *next_expression = child_at(expression, 0U);
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_COALESCE_FUNCTION) {
        const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
        const struct mylite_sql_ast_node *first_argument = NULL;

        if (mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
            arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
            mylite_sql_ast_node_child_count(arguments) == 0U) {
            set_if_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        first_argument = child_at(arguments, 0U);
        rc = if_eval_stack_push(
            database,
            stack,
            IF_EVAL_FRAME_COALESCE,
            first_argument == NULL ? NULL : first_argument->next_sibling,
            NULL
        );
        if (rc == MYLITE_OK) {
            *next_expression = first_argument;
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_NULLIF_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            set_if_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        rc = if_eval_stack_push(
            database,
            stack,
            IF_EVAL_FRAME_NULLIF,
            child_at(expression, 0U),
            child_at(expression, 1U)
        );
        if (rc == MYLITE_OK) {
            *next_expression = child_at(expression, 0U);
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_ISNULL_FUNCTION) {
        return if_eval_isnull_expression(
            database,
            stack,
            expression,
            function_name,
            next_expression
        );
    }

    rc = if_non_function_scalar_value(database, expression, function_name, out_cell);
    *next_expression = NULL;
    return rc;
}

static int if_eval_isnull_expression(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **next_expression
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = if_eval_stack_push(database, stack, IF_EVAL_FRAME_ISNULL, NULL, NULL);
    if (rc == MYLITE_OK) {
        *next_expression = child_at(expression, 0U);
    }
    return rc;
}

static bool if_eval_completed_value(
    struct if_eval_stack *stack,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression,
    struct session_scalar_cell *out_cell
) {
    struct if_eval_frame *frame = NULL;

    if (stack->count == 0U) {
        copy_session_scalar_cell(out_cell, cell);
        return true;
    }

    frame = &stack->items[stack->count - 1U];
    if (frame->kind == IF_EVAL_FRAME_COALESCE) {
        if_eval_complete_coalesce_frame(frame, cell, next_expression);
        if (*next_expression == NULL) {
            --stack->count;
        }
        return false;
    }
    if (frame->kind == IF_EVAL_FRAME_NULLIF) {
        if_eval_complete_nullif_frame(frame, cell, next_expression);
        if (*next_expression == NULL) {
            --stack->count;
        }
        return false;
    }
    if (frame->kind == IF_EVAL_FRAME_ISNULL) {
        --stack->count;
        if_eval_complete_isnull_frame(cell, next_expression);
        return false;
    }

    --stack->count;
    if (frame->kind == IF_EVAL_FRAME_IF) {
        if_eval_complete_if_frame(frame, cell, next_expression);
    } else {
        if_eval_complete_ifnull_frame(frame, cell, next_expression);
    }
    return false;
}

static void if_eval_complete_if_frame(
    const struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
) {
    if (if_scalar_condition_is_true(cell)) {
        *next_expression = frame->first_value;
    } else {
        *next_expression = frame->second_value;
    }
    *cell = (struct session_scalar_cell){0};
}

static void if_eval_complete_ifnull_frame(
    const struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
) {
    if (cell->value == NULL) {
        *next_expression = frame->first_value;
        *cell = (struct session_scalar_cell){0};
    } else {
        *next_expression = NULL;
    }
}

static void if_eval_complete_coalesce_frame(
    struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
) {
    if (cell->value == NULL && frame->first_value != NULL) {
        *next_expression = frame->first_value;
        frame->first_value = frame->first_value->next_sibling;
        *cell = (struct session_scalar_cell){0};
        return;
    }

    *next_expression = NULL;
}

static void if_eval_complete_nullif_frame(
    struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
) {
    if (frame->first_value != NULL) {
        copy_session_scalar_cell(&frame->first_cell, cell);
        frame->first_value = NULL;
        *cell = (struct session_scalar_cell){0};
        *next_expression = frame->second_value;
        return;
    }

    if (frame->first_cell.value == NULL ||
        (cell->value != NULL && strcmp(frame->first_cell.value, cell->value) == 0)) {
        *cell = (struct session_scalar_cell){0};
    } else {
        copy_session_scalar_cell(cell, &frame->first_cell);
    }
    *next_expression = NULL;
}

static void if_eval_complete_isnull_frame(
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
) {
    bool is_null = cell->value == NULL;

    *cell = (struct session_scalar_cell){0};
    if (is_null) {
        cell->value = "1";
    } else {
        cell->value = "0";
    }
    *next_expression = NULL;
}

static int if_non_function_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool has_sign = false;
    bool is_negative = false;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (generic_function_name_equals(expression, "GET_LOCK")) {
        out_cell->value = "1";
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        has_sign = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_if_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        literal = child_at(expression, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }

    switch (mylite_sql_ast_node_literal_kind(literal)) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        if (has_sign) {
            break;
        }
        out_cell->value = NULL;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        if (has_sign) {
            break;
        }
        out_cell->value = "1";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        if (has_sign) {
            break;
        }
        out_cell->value = "0";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return if_integer_literal_value(database, literal, is_negative, function_name, out_cell);
    default:
        break;
    }

    set_if_unsupported_error(database, function_name);
    return MYLITE_ERROR;
}

static int if_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *function_name,
    struct session_scalar_cell *out_cell
) {
    uint64_t magnitude = 0U;
    int64_t value = 0;
    int written = 0;

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        magnitude > (uint64_t)INT64_MAX) {
        char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
        int message_length = snprintf(
            message,
            sizeof(message),
            "SELECT %s() supports only signed 64-bit integer operands",
            function_name
        );

        if (message_length < 0 || (size_t)message_length >= sizeof(message)) {
            set_runtime_error(database, "failed to format scalar control-flow diagnostic");
        } else {
            set_unsupported_error(database, message);
        }
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude != 0U) {
        value = -(int64_t)magnitude;
    } else {
        value = (int64_t)magnitude;
    }
    written = snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, value);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format scalar control-flow integer value");
        return MYLITE_ERROR;
    }

    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int if_eval_stack_push(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    enum if_eval_frame_kind kind,
    const struct mylite_sql_ast_node *first_value,
    const struct mylite_sql_ast_node *second_value
) {
    struct if_eval_frame *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_MISUSE;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = (struct if_eval_frame *)realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = (struct if_eval_frame){
        .kind = kind,
        .first_value = first_value,
        .second_value = second_value,
    };
    ++stack->count;
    return MYLITE_OK;
}

static void if_eval_stack_deinit(struct if_eval_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct if_eval_stack){0};
}

static void copy_session_scalar_cell(
    struct session_scalar_cell *destination,
    const struct session_scalar_cell *source
) {
    *destination = (struct session_scalar_cell){0};
    if (source == NULL || source->value == NULL) {
        if (source != NULL) {
            destination->staged_division_by_zero_warning_count =
                source->staged_division_by_zero_warning_count;
            destination->staged_invalid_logarithm_warning_count =
                source->staged_invalid_logarithm_warning_count;
            destination->has_staged_truncated_integer_warning =
                source->has_staged_truncated_integer_warning;
            destination->staged_truncated_integer_text = source->staged_truncated_integer_text;
            destination->has_staged_truncated_decimal_warning =
                source->has_staged_truncated_decimal_warning;
            destination->has_value_size = source->has_value_size;
            destination->value_size = source->value_size;
            memcpy(
                destination->staged_truncated_decimal_text,
                source->staged_truncated_decimal_text,
                sizeof(destination->staged_truncated_decimal_text)
            );
            destination->has_staged_unhex_incorrect_string_warning =
                source->has_staged_unhex_incorrect_string_warning;
            memcpy(
                destination->staged_unhex_incorrect_string_text,
                source->staged_unhex_incorrect_string_text,
                sizeof(destination->staged_unhex_incorrect_string_text)
            );
            destination->staged_signed_complement_warning_count =
                source->staged_signed_complement_warning_count;
            destination->staged_unsigned_complement_warning_count =
                source->staged_unsigned_complement_warning_count;
        }
        return;
    }
    destination->staged_division_by_zero_warning_count =
        source->staged_division_by_zero_warning_count;
    destination->staged_invalid_logarithm_warning_count =
        source->staged_invalid_logarithm_warning_count;
    destination->has_staged_truncated_integer_warning =
        source->has_staged_truncated_integer_warning;
    destination->staged_truncated_integer_text = source->staged_truncated_integer_text;
    destination->has_staged_truncated_decimal_warning =
        source->has_staged_truncated_decimal_warning;
    destination->has_value_size = source->has_value_size;
    destination->value_size = source->value_size;
    memcpy(
        destination->staged_truncated_decimal_text,
        source->staged_truncated_decimal_text,
        sizeof(destination->staged_truncated_decimal_text)
    );
    destination->has_staged_unhex_incorrect_string_warning =
        source->has_staged_unhex_incorrect_string_warning;
    memcpy(
        destination->staged_unhex_incorrect_string_text,
        source->staged_unhex_incorrect_string_text,
        sizeof(destination->staged_unhex_incorrect_string_text)
    );
    destination->staged_signed_complement_warning_count =
        source->staged_signed_complement_warning_count;
    destination->staged_unsigned_complement_warning_count =
        source->staged_unsigned_complement_warning_count;
    if (source->value == source->integer_text) {
        memcpy(destination->integer_text, source->integer_text, sizeof(destination->integer_text));
        destination->value = destination->integer_text;
        return;
    }
    if (source->value == source->literal_text) {
        memcpy(destination->literal_text, source->literal_text, sizeof(destination->literal_text));
        destination->value = destination->literal_text;
        return;
    }
    if (source->value == source->datetime_text) {
        memcpy(
            destination->datetime_text,
            source->datetime_text,
            sizeof(destination->datetime_text)
        );
        destination->value = destination->datetime_text;
        return;
    }

    destination->value = source->value;
}

static void session_scalar_cell_deinit(struct session_scalar_cell *cell) {
    if (cell == NULL) {
        return;
    }

    free(cell->owned_text);
    *cell = (struct session_scalar_cell){0};
}

static void session_scalar_cell_array_deinit(struct session_scalar_cell *cells, size_t cell_count) {
    if (cells == NULL) {
        return;
    }

    for (size_t index = 0U; index < cell_count; ++index) {
        session_scalar_cell_deinit(&cells[index]);
    }
    free(cells);
}

static bool if_scalar_condition_is_true(const struct session_scalar_cell *cell) {
    if (cell == NULL || cell->value == NULL) {
        return false;
    }
    if (strcmp(cell->value, "0") == 0) {
        return false;
    }
    return true;
}

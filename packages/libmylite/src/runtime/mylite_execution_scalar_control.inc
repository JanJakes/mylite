static int if_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int ifnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int coalesce_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int nullif_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int isnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = validate_case_expression(database, expression);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (expression->kind == MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        return searched_case_expression_value(database, expression, out_cell);
    }
    return simple_case_expression_value(database, expression, out_cell);
}

static int searched_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *when_list = child_at(expression, 0U);
    const struct mylite_sql_ast_node *else_clause = child_at(expression, 1U);
    const struct mylite_sql_ast_node *when_clause = child_at(when_list, 0U);
    size_t staged_warning_count = 0U;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && when_clause != NULL) {
        struct scalar_arithmetic_value condition = {.is_null = false, .integer = 0};

        rc = evaluate_case_arithmetic_expression(database, child_at(when_clause, 0U), &condition);
        if (rc == MYLITE_OK) {
            rc = accumulate_staged_division_by_zero_warnings(
                database,
                condition.division_by_zero_warning_count,
                &staged_warning_count
            );
        }
        if (rc == MYLITE_OK && scalar_arithmetic_truth_value(&condition)) {
            struct session_scalar_cell result = {0};

            rc = session_scalar_value_without_case(database, child_at(when_clause, 1U), &result);
            if (rc == MYLITE_OK) {
                rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
            }
            return rc;
        }
        when_clause = when_clause->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (else_clause != NULL) {
        struct session_scalar_cell result = {0};

        rc = session_scalar_value_without_case(database, child_at(else_clause, 0U), &result);
        if (rc == MYLITE_OK) {
            rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
        }
        return rc;
    }

    *out_cell = (struct session_scalar_cell){0};
    out_cell->staged_division_by_zero_warning_count = staged_warning_count;
    return MYLITE_OK;
}

static int simple_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *case_value_node = child_at(expression, 0U);
    const struct mylite_sql_ast_node *when_list = child_at(expression, 1U);
    const struct mylite_sql_ast_node *else_clause = child_at(expression, 2U);
    const struct mylite_sql_ast_node *when_clause = child_at(when_list, 0U);
    struct scalar_arithmetic_value case_value = {.is_null = false, .integer = 0};
    size_t staged_warning_count = 0U;
    int rc = evaluate_case_arithmetic_expression(database, case_value_node, &case_value);

    if (rc == MYLITE_OK) {
        rc = accumulate_staged_division_by_zero_warnings(
            database,
            case_value.division_by_zero_warning_count,
            &staged_warning_count
        );
    }
    while (rc == MYLITE_OK && when_clause != NULL) {
        struct scalar_arithmetic_value compare_value = {.is_null = false, .integer = 0};

        rc = evaluate_case_arithmetic_expression(
            database,
            child_at(when_clause, 0U),
            &compare_value
        );
        if (rc == MYLITE_OK) {
            rc = accumulate_staged_division_by_zero_warnings(
                database,
                compare_value.division_by_zero_warning_count,
                &staged_warning_count
            );
        }
        if (rc == MYLITE_OK && case_arithmetic_values_are_equal(&case_value, &compare_value)) {
            struct session_scalar_cell result = {0};

            rc = session_scalar_value_without_case(database, child_at(when_clause, 1U), &result);
            if (rc == MYLITE_OK) {
                rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
            }
            return rc;
        }
        when_clause = when_clause->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (else_clause != NULL) {
        struct session_scalar_cell result = {0};

        rc = session_scalar_value_without_case(database, child_at(else_clause, 0U), &result);
        if (rc == MYLITE_OK) {
            rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
        }
        return rc;
    }

    *out_cell = (struct session_scalar_cell){0};
    out_cell->staged_division_by_zero_warning_count = staged_warning_count;
    return MYLITE_OK;
}

static int evaluate_case_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct session_scalar_cell cell = {0};
    int rc = session_scalar_value_without_case(database, expression, &cell);

    if (rc == MYLITE_OK) {
        rc = parse_scalar_arithmetic_operand(database, &cell, out_value);
    }
    return rc;
}

static int copy_case_result_cell(
    struct mylite_db *database,
    const struct session_scalar_cell *selected_cell,
    size_t previous_warning_count,
    struct session_scalar_cell *out_cell
) {
    size_t total_warning_count = previous_warning_count;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = accumulate_staged_division_by_zero_warnings(
        database,
        selected_cell == NULL ? 0U : selected_cell->staged_division_by_zero_warning_count,
        &total_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    copy_session_scalar_cell(out_cell, selected_cell);
    out_cell->staged_division_by_zero_warning_count = total_warning_count;
    return MYLITE_OK;
}

static bool case_arithmetic_values_are_equal(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
) {
    if (left == NULL || right == NULL || left->is_null || right->is_null) {
        return false;
    }
    return left->integer == right->integer;
}

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

static int literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool has_sign = false;
    bool is_negative = false;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, string, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        has_sign = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "SELECT literal projection supports only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(expression, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, string, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (has_sign && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only signed integer literals"
        );
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_cell->value = "1";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        int rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "SELECT literal projection supports only ordinary string literals",
            "SELECT literal projection does not support NUL bytes in string literals",
            true,
            &out_cell->owned_text,
            &out_cell->value_size
        );

        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->owned_text;
            out_cell->has_value_size = true;
        }
        return rc;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, string, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    int rc = normalize_decimal_integer_literal(
        database,
        &literal->span,
        is_negative,
        out_cell->literal_text,
        sizeof(out_cell->literal_text)
    );

    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->literal_text;
    }
    return rc;
}

static int normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
) {
    size_t first_significant = 0U;
    size_t significant_digit_count = 0U;
    size_t output_offset = 0U;
    size_t required_size = 0U;

    if (span == NULL || span->text == NULL || span->length == 0U || buffer == NULL ||
        buffer_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_significant = span->length;
    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];

        if (byte < '0' || byte > '9') {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (byte != '0' && first_significant == span->length) {
            first_significant = index;
        }
    }
    if (first_significant == span->length) {
        if (buffer_size < sizeof("0")) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return MYLITE_OK;
    }

    significant_digit_count = span->length - first_significant;
    if (significant_digit_count > literal_projection_max_significant_digits) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports at most 81 significant decimal digits"
        );
        return MYLITE_ERROR;
    }
    required_size = significant_digit_count + 1U;
    if (is_negative) {
        ++required_size;
    }
    if (required_size > buffer_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        buffer[output_offset] = '-';
        ++output_offset;
    }
    memcpy(&buffer[output_offset], &span->text[first_significant], significant_digit_count);
    output_offset += significant_digit_count;
    buffer[output_offset] = '\0';

    return MYLITE_OK;
}

static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum mylite_execution_system_variable_kind variable = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    uint64_t count = 0U;
    int rc = resolve_session_system_variable(database, expression, &variable);

    if (rc != MYLITE_OK) {
        return rc;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        out_cell->value = database->session.character_set_client;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
        out_cell->value = database->session.character_set_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        out_cell->value = database->session.character_set_results;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        out_cell->value = database->session.collation_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
        out_cell->value = "utf8mb4";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER:
        out_cell->value = "utf8mb4_0900_ai_ci";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
        return database_character_set_system_variable_value(
            database,
            system_variable_expression_has_global_scope(expression),
            out_cell->literal_text,
            sizeof(out_cell->literal_text),
            &out_cell->value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE:
        return database_collation_system_variable_value(
            database,
            system_variable_expression_has_global_scope(expression),
            out_cell->literal_text,
            sizeof(out_cell->literal_text),
            &out_cell->value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
        out_cell->value = "InnoDB";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
        out_cell->value = "YES";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
        out_cell->value = "utf8mb3";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
        out_cell->value = "binary";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE:
        if (system_variable_expression_has_global_scope(expression)) {
            out_cell->value = default_sql_mode_value();
        } else {
            out_cell->value = database->session.sql_mode_text;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE:
        out_cell->value = "UTC";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE:
        if (system_variable_expression_has_global_scope(expression)) {
            out_cell->value = "SYSTEM";
        } else {
            out_cell->value = database->session.time_zone;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE:
        out_cell->value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED:
        out_cell->value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
        out_cell->value = basedir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
        out_cell->value = datadir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
        out_cell->value = hostname_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
        out_cell->value = license_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
        out_cell->value = log_bin_basename_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
        out_cell->value = log_bin_index_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
        out_cell->value = pid_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
        out_cell->value = plugin_dir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
        out_cell->value = version_compile_machine_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
        out_cell->value = version_compile_os_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        out_cell->value = version_compile_zlib_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
        out_cell->value = server_uuid_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
        out_cell->value = socket_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        return format_session_scalar_uint64_value(
            database,
            auto_increment_step_system_variable_value(
                database,
                variable,
                system_variable_expression_has_global_scope(expression)
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        return format_session_scalar_uint64_value(
            database,
            timeout_system_variable_value(
                database,
                variable,
                system_variable_expression_has_global_scope(expression)
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        if (system_variable_expression_has_global_scope(expression)) {
            return format_session_scalar_uint64_value(
                database,
                group_concat_max_len_default_value,
                out_cell
            );
        }
        return format_session_scalar_uint64_value(
            database,
            database->session.group_concat_max_len,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        return format_session_scalar_uint64_value(
            database,
            information_schema_stats_expiry_system_variable_value(
                database,
                system_variable_expression_has_global_scope(expression)
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        return format_session_scalar_uint64_value(
            database,
            foreign_key_checks_system_variable_uint64_value(
                database,
                system_variable_expression_has_global_scope(expression)
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
        return format_session_scalar_uint64_value(
            database,
            sql_require_primary_key_system_variable_uint64_value(
                database,
                system_variable_expression_has_global_scope(expression)
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        return format_session_scalar_uint64_value(
            database,
            big_tables_system_variable_uint64_value(
                database,
                system_variable_expression_has_global_scope(expression)
            ),
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
        rc = format_uint64(database, 1U, out_cell->integer_text, sizeof(out_cell->integer_text));
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        rc = format_uint64(database, 0U, out_cell->integer_text, sizeof(out_cell->integer_text));
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        return format_session_scalar_uint64_value(
            database,
            server_id_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        return format_session_scalar_uint64_value(
            database,
            server_id_bits_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        return format_session_scalar_uint64_value(database, port_system_variable_value, out_cell);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        return format_session_scalar_uint64_value(
            database,
            protocol_version_system_variable_value,
            out_cell
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        rc = format_uint64(
            database,
            sql_select_limit_system_variable_value(
                database,
                system_variable_expression_has_global_scope(expression)
            ),
            out_cell->integer_text,
            sizeof(out_cell->integer_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET:
        rc = format_uint64(
            database,
            max_allowed_packet_default_value,
            out_cell->integer_text,
            sizeof(out_cell->integer_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
        out_cell->value = transaction_isolation_system_variable_value(database, expression);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
        out_cell->value = transaction_read_only_system_variable_value(database, expression);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP:
        rc = format_timestamp_system_variable_value(
            database,
            out_cell->literal_text,
            sizeof(out_cell->literal_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->literal_text;
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMMENT:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING;
        return MYLITE_OK;
    default:
        break;
    }

    rc = diagnostics_count_system_variable_value(
        system_variable_count_diagnostics(database),
        variable,
        &count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = format_uint64(database, count, out_cell->integer_text, sizeof(out_cell->integer_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

static int diagnostics_count_system_variable_value(
    const struct mylite_diagnostics *diagnostics,
    enum mylite_execution_system_variable_kind variable,
    uint64_t *out_count
) {
    uint64_t warning_count = 0U;
    uint64_t condition_count = 0U;
    int rc = MYLITE_OK;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }
    if (variable == MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT) {
        return previous_diagnostics_error_count(diagnostics, out_count);
    }

    rc = previous_diagnostics_condition_count(diagnostics, &condition_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
    if (condition_count > UINT64_MAX - warning_count) {
        return MYLITE_NOMEM;
    }

    *out_count = condition_count + warning_count;
    return MYLITE_OK;
}

static uint64_t information_schema_stats_expiry_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope) {
        return information_schema_stats_expiry_default_value;
    }
    return database->session.information_schema_stats_expiry;
}

static int database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_execution_catalog_builtin_schema *builtin_schema = NULL;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    if (global_scope || database == NULL || !database->session.has_selected_schema) {
        *out_value = MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
        return MYLITE_OK;
    }
    builtin_schema = find_builtin_schema_descriptor(database->session.selected_schema);
    if (builtin_schema != NULL) {
        *out_value = builtin_schema->default_charset;
        return MYLITE_OK;
    }

    rc = resolve_schema_name(database, database->session.selected_schema, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strlen(schema.default_charset) >= buffer_size) {
        set_runtime_error(database, "system variable buffer is too small");
        return MYLITE_ERROR;
    }

    memcpy(buffer, schema.default_charset, strlen(schema.default_charset) + 1U);
    *out_value = buffer;
    return MYLITE_OK;
}

static int database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_execution_catalog_builtin_schema *builtin_schema = NULL;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    if (global_scope || database == NULL || !database->session.has_selected_schema) {
        *out_value = MYLITE_CATALOG_DEFAULT_TABLE_COLLATION;
        return MYLITE_OK;
    }
    builtin_schema = find_builtin_schema_descriptor(database->session.selected_schema);
    if (builtin_schema != NULL) {
        *out_value = builtin_schema->default_collation;
        return MYLITE_OK;
    }

    rc = resolve_schema_name(database, database->session.selected_schema, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strlen(schema.default_collation) >= buffer_size) {
        set_runtime_error(database, "system variable buffer is too small");
        return MYLITE_ERROR;
    }

    memcpy(buffer, schema.default_collation, strlen(schema.default_collation) + 1U);
    *out_value = buffer;
    return MYLITE_OK;
}

static int format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_uint64(database, value, out_cell->integer_text, sizeof(out_cell->integer_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

static uint64_t auto_increment_step_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return 1U;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT) {
        return database->session.auto_increment_increment;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET) {
        return database->session.auto_increment_offset;
    }
    return 1U;
}

static uint64_t sql_select_limit_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return UINT64_MAX;
    }
    return database->session.sql_select_limit;
}

static uint64_t timeout_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return timeout_system_variable_default_value;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT) {
        return database->session.wait_timeout;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT) {
        return database->session.interactive_timeout;
    }
    return timeout_system_variable_default_value;
}

static const char *transaction_isolation_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    if (system_variable_expression_has_global_scope(expression)) {
        return transaction_isolation_value_text(MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ);
    }
    return transaction_isolation_value_text(database->session.session_transaction_isolation);
}

static const char *transaction_read_only_system_variable_value(
    const struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    if (system_variable_expression_has_global_scope(expression)) {
        return transaction_read_only_scalar_text(MYLITE_TRANSACTION_ACCESS_READ_WRITE);
    }
    return transaction_read_only_scalar_text(database->session.session_transaction_access_mode);
}

static bool system_variable_expression_has_global_scope(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    size_t offset = system_variable_body_offset;
    size_t scope_start = offset;
    size_t scope_length = 0U;

    if (span == NULL || span->text == NULL || span->length <= offset) {
        return false;
    }
    if (span->text[offset] == '`') {
        return false;
    }
    while (offset < span->length && span->text[offset] != '.') {
        ++offset;
    }
    if (offset == span->length) {
        return false;
    }

    scope_length = offset - scope_start;
    return sql_mode_token_matches(&span->text[scope_start], scope_length, "global");
}

static bool foreign_key_checks_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL) {
        return true;
    }
    return database->session.foreign_key_checks_enabled;
}

static uint64_t foreign_key_checks_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (foreign_key_checks_system_variable_value(database, global_scope)) {
        return 1U;
    }
    return 0U;
}

static const char *foreign_key_checks_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (foreign_key_checks_system_variable_value(database, global_scope)) {
        return "ON";
    }
    return "OFF";
}

static uint64_t sql_require_primary_key_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL || !database->session.sql_require_primary_key) {
        return 0U;
    }
    return 1U;
}

static const char *sql_require_primary_key_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (sql_require_primary_key_system_variable_uint64_value(database, global_scope) == 0U) {
        return "OFF";
    }
    return "ON";
}

static uint64_t big_tables_system_variable_uint64_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (global_scope || database == NULL || !database->session.big_tables) {
        return 0U;
    }
    return 1U;
}

static const char *big_tables_system_variable_show_value(
    const struct mylite_db *database,
    bool global_scope
) {
    if (big_tables_system_variable_uint64_value(database, global_scope) == 0U) {
        return "OFF";
    }
    return "ON";
}

static const char *default_sql_mode_value(void) {
    return MYLITE_SESSION_SQL_MODE_DEFAULT_TEXT;
}

static const struct mylite_diagnostics *system_variable_count_diagnostics(
    const struct mylite_db *database
) {
    if (database == NULL) {
        return NULL;
    }
    if (mylite_diagnostics_errcode(&database->diagnostics) != MYLITE_OK ||
        mylite_diagnostics_warning_count(&database->diagnostics) > 0U) {
        return &database->diagnostics;
    }
    return &database->previous_diagnostics;
}

static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    struct system_variable_component first = {0};
    struct system_variable_component second = {0};
    const struct system_variable_component *name = &first;
    size_t offset = 2U;
    bool has_scope = false;
    int rc = MYLITE_OK;

    *out_kind = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    if (span == NULL || span->text == NULL || span->length < 3U || span->text[0] != '@' ||
        span->text[1] != '@') {
        set_unknown_system_variable_error(database, expression);
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
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    if (!resolve_system_variable_kind(name, out_kind)) {
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    if (!has_scope) {
        return MYLITE_OK;
    }
    if (system_variable_component_equals(&first, "global")) {
        if (mylite_execution_system_variable_allows_global_scope(*out_kind)) {
            return MYLITE_OK;
        }
        set_session_variable_only_error(database, name->text);
        return MYLITE_ERROR;
    }
    if (system_variable_component_equals(&first, "session") ||
        system_variable_component_equals(&first, "local")) {
        if (!mylite_execution_system_variable_allows_session_scope(*out_kind)) {
            set_global_variable_only_error(database, name->text);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    set_unknown_system_variable_error(database, expression);
    return MYLITE_ERROR;
}

static bool resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum mylite_execution_system_variable_kind *out_kind
) {
    const struct mylite_execution_system_variable_descriptor *descriptor =
        mylite_execution_system_variable_descriptor_by_name(name == NULL ? NULL : name->text);

    if (descriptor != NULL && system_variable_component_equals(name, descriptor->name)) {
        *out_kind = descriptor->kind;
        return true;
    }

    return false;
}

static int show_system_variable_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
) {
    if (out_value == NULL || mylite_execution_system_variable_descriptor_by_kind(kind) == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES descriptor");
        return MYLITE_ERROR;
    }
    *out_value = NULL;

    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        *out_value = database->session.character_set_client;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
        *out_value = database->session.character_set_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        *out_value = database->session.character_set_results;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        *out_value = database->session.collation_connection;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
        *out_value = "utf8mb4";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
        return database_character_set_system_variable_value(
            database,
            global_scope,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER:
        *out_value = "utf8mb4_0900_ai_ci";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE:
        return database_collation_system_variable_value(
            database,
            global_scope,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
        *out_value = "InnoDB";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
        *out_value = "YES";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
        *out_value = "utf8mb3";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
        *out_value = "binary";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE:
        if (global_scope) {
            *out_value = default_sql_mode_value();
        } else {
            *out_value = database->session.sql_mode_text;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE:
        *out_value = "UTC";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE:
        if (global_scope) {
            *out_value = "SYSTEM";
        } else {
            *out_value = database->session.time_zone;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED:
        *out_value = "";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
        *out_value = basedir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
        *out_value = datadir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
        *out_value = hostname_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
        *out_value = license_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
        *out_value = log_bin_basename_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
        *out_value = log_bin_index_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
        *out_value = pid_file_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
        *out_value = plugin_dir_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
        *out_value = version_compile_machine_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
        *out_value = version_compile_os_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        *out_value = version_compile_zlib_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
        *out_value = server_uuid_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
        *out_value = socket_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        return format_show_system_variable_uint64_value(
            database,
            auto_increment_step_system_variable_value(database, kind, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        return format_show_system_variable_uint64_value(
            database,
            timeout_system_variable_value(database, kind, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        if (global_scope) {
            return format_show_system_variable_uint64_value(
                database,
                group_concat_max_len_default_value,
                integer_buffer,
                integer_buffer_size,
                out_value
            );
        }
        return format_show_system_variable_uint64_value(
            database,
            database->session.group_concat_max_len,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        return format_show_system_variable_uint64_value(
            database,
            information_schema_stats_expiry_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        *out_value = foreign_key_checks_system_variable_show_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
        *out_value = sql_require_primary_key_system_variable_show_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        *out_value = big_tables_system_variable_show_value(database, global_scope);
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
        *out_value = "ON";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
        *out_value = "OFF";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        *out_value = "0";
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        return format_show_system_variable_uint64_value(
            database,
            server_id_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        return format_show_system_variable_uint64_value(
            database,
            server_id_bits_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        return format_show_system_variable_uint64_value(
            database,
            port_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        return format_show_system_variable_uint64_value(
            database,
            protocol_version_system_variable_value,
            integer_buffer,
            integer_buffer_size,
            out_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET: {
        int rc = format_uint64(
            database,
            max_allowed_packet_default_value,
            integer_buffer,
            integer_buffer_size
        );
        if (rc == MYLITE_OK) {
            *out_value = integer_buffer;
        }
        return rc;
    }
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
        if (global_scope) {
            *out_value =
                transaction_isolation_value_text(MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ);
        } else {
            *out_value =
                transaction_isolation_value_text(database->session.session_transaction_isolation);
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
        if (global_scope) {
            *out_value = transaction_read_only_show_text(MYLITE_TRANSACTION_ACCESS_READ_WRITE);
        } else {
            *out_value =
                transaction_read_only_show_text(database->session.session_transaction_access_mode);
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT: {
        int rc = format_uint64(
            database,
            sql_select_limit_system_variable_value(database, global_scope),
            integer_buffer,
            integer_buffer_size
        );
        if (rc == MYLITE_OK) {
            *out_value = integer_buffer;
        }
        return rc;
    }
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP: {
        int rc =
            format_timestamp_system_variable_value(database, integer_buffer, integer_buffer_size);
        if (rc == MYLITE_OK) {
            *out_value = integer_buffer;
        }
        return rc;
    }
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION:
        *out_value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMMENT:
        *out_value = MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE:
        break;
    }

    set_runtime_error(database, "unsupported SHOW VARIABLES value");
    return MYLITE_ERROR;
}

static int format_show_system_variable_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
) {
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_uint64(database, value, integer_buffer, integer_buffer_size);
    if (rc == MYLITE_OK) {
        *out_value = integer_buffer;
    }
    return rc;
}

static int format_timestamp_system_variable_value(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid timestamp system variable buffer");
        return MYLITE_ERROR;
    }
    written =
        snprintf(buffer, buffer_size, "%" PRId64 ".000000", current_timestamp_epoch(database));
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format timestamp system variable");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@sql_slave_skip_counter' is deprecated and will be removed in a future release. "
            "Please use sql_replica_skip_counter instead."
        );
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@log_bin_trust_function_creators' is deprecated and will be removed in a future "
            "release."
        );
    }

    return MYLITE_OK;
}

static int parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
) {
    size_t component_length = 0U;

    *out_component = (struct system_variable_component){0};
    if (*offset >= span->length) {
        return MYLITE_OK;
    }

    if (span->text[*offset] == '`') {
        out_component->quoted = true;
        ++*offset;
        while (*offset < span->length) {
            char value = span->text[*offset];

            if (value == '`') {
                ++*offset;
                if (*offset < span->length && span->text[*offset] == '`') {
                    int rc = append_quoted_system_variable_byte(
                        database,
                        out_component,
                        &component_length,
                        '`'
                    );
                    if (rc != MYLITE_OK) {
                        return rc;
                    }
                    ++*offset;
                    continue;
                }
                return MYLITE_OK;
            }

            {
                int rc = append_quoted_system_variable_byte(
                    database,
                    out_component,
                    &component_length,
                    value
                );
                if (rc != MYLITE_OK) {
                    return rc;
                }
            }
            ++*offset;
        }

        set_unsupported_error(database, "unterminated system variable identifier");
        return MYLITE_ERROR;
    }

    while (*offset < span->length && span->text[*offset] != '.') {
        if (component_length + 1U >= sizeof(out_component->text)) {
            set_unknown_system_variable_error(database, NULL);
            return MYLITE_ERROR;
        }
        out_component->text[component_length] = span->text[*offset];
        ++component_length;
        ++*offset;
    }
    out_component->text[component_length] = '\0';

    return MYLITE_OK;
}

static int append_quoted_system_variable_byte(
    struct mylite_db *database,
    struct system_variable_component *component,
    size_t *component_length,
    char value
) {
    if (*component_length + 1U >= sizeof(component->text)) {
        set_unknown_system_variable_error(database, NULL);
        return MYLITE_ERROR;
    }

    component->text[*component_length] = value;
    ++*component_length;
    component->text[*component_length] = '\0';
    return MYLITE_OK;
}

static bool system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
) {
    return text_equals_ascii_case_insensitive(component == NULL ? NULL : component->text, expected);
}

static bool system_variable_component_is_empty(const struct system_variable_component *component) {
    if (component == NULL) {
        return true;
    }
    return component->text[0] == '\0';
}

static bool is_session_scalar_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_any_value_function_expression(expression);

    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_PI_FUNCTION:
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
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
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEK_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_USER_VARIABLE:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        return false;
    }
}

static int validate_if_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    struct if_validation_stack stack = {0};
    int rc = MYLITE_OK;

    rc = if_validation_stack_push(database, &stack, expression);
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current = NULL;

        --stack.count;
        current = stack.items[stack.count];
        rc = validate_if_value_node(database, &stack, current, function_name);
    }

    if_validation_stack_deinit(&stack);
    return rc;
}

static int validate_if_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IF_FUNCTION) {
        return validate_if_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_IFNULL_FUNCTION) {
        return validate_ifnull_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_COALESCE_FUNCTION) {
        return validate_coalesce_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_NULLIF_FUNCTION) {
        return validate_nullif_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_ISNULL_FUNCTION) {
        return validate_isnull_function_value_node(database, stack, expression, function_name);
    }
    if (!is_if_non_function_value_expression(expression)) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_if_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 3U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 0U));
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    }
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 2U));
    }
    return rc;
}

static int validate_ifnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 0U));
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    }
    return rc;
}

static int validate_coalesce_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
        mylite_sql_ast_node_child_count(arguments) == 0U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    argument = child_at(arguments, 0U);
    while (rc == MYLITE_OK && argument != NULL) {
        rc = if_validation_stack_push(database, stack, argument);
        argument = argument->next_sibling;
    }
    return rc;
}

static int validate_nullif_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 0U));
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    }
    return rc;
}

static int validate_isnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    return if_validation_stack_push(database, stack, child_at(expression, 0U));
}

static int validate_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *case_value = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_case_expression_kind(expression->kind)) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        if (child_count < 1U || child_count > 2U) {
            set_case_unsupported_error(database);
            return MYLITE_ERROR;
        }
        when_list = child_at(expression, 0U);
        else_clause = child_at(expression, 1U);
    } else {
        if (child_count < 2U || child_count > 3U) {
            set_case_unsupported_error(database);
            return MYLITE_ERROR;
        }
        case_value = child_at(expression, 0U);
        when_list = child_at(expression, 1U);
        else_clause = child_at(expression, 2U);
        rc = validate_case_value_expression(database, case_value);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST ||
        mylite_sql_ast_node_child_count(when_list) == 0U) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    when_clause = child_at(when_list, 0U);
    while (rc == MYLITE_OK && when_clause != NULL) {
        if (when_clause->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
            mylite_sql_ast_node_child_count(when_clause) != 2U) {
            set_case_unsupported_error(database);
            return MYLITE_ERROR;
        }
        rc = validate_case_value_expression(database, child_at(when_clause, 0U));
        if (rc == MYLITE_OK) {
            rc = validate_case_value_expression(database, child_at(when_clause, 1U));
        }
        when_clause = when_clause->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (else_clause == NULL) {
        return MYLITE_OK;
    }
    if (else_clause->kind != MYLITE_SQL_AST_CASE_ELSE_CLAUSE ||
        mylite_sql_ast_node_child_count(else_clause) != 1U) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return validate_case_value_expression(database, child_at(else_clause, 0U));
}

static int validate_case_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct if_validation_stack stack = {0};
    int rc = if_validation_stack_push(database, &stack, expression);

    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];

        rc = validate_case_value_node(database, &stack, current);
    }
    if_validation_stack_deinit(&stack);
    return rc;
}

static int validate_case_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    bool handled = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = validate_case_argument_count_error_node(database, expression, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (!case_value_expression_is_admitted(expression)) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    if (is_scalar_function_expression(expression)) {
        return validate_if_value_expression(database, expression, if_function_name(expression));
    }
    if (expression->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        return validate_case_mod_value_node(database, stack, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return validate_case_unary_value_node(database, stack, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return validate_case_binary_value_node(database, stack, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return validate_case_literal_value_node(database, expression);
    }

    return MYLITE_OK;
}

static int validate_case_argument_count_error_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;

    switch (expression->kind) {
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "IFNULL");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "NULLIF");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ISNULL");
        *out_handled = true;
        return MYLITE_ERROR;
    default:
        return MYLITE_OK;
    }
}

static int validate_case_mod_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    if (rc != MYLITE_OK) {
        return rc;
    }
    return if_validation_stack_push(database, stack, child_at(expression, 0U));
}

static int validate_case_unary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    const struct mylite_sql_ast_node *operand = child_at(expression, 0U);
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(operand);

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
            return validate_case_literal_value_node(database, expression);
        }
        return if_validation_stack_push(database, stack, operand);
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        return if_validation_stack_push(database, stack, operand);
    }
    set_case_unsupported_error(database);
    return MYLITE_ERROR;
}

static int validate_case_binary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    int rc = MYLITE_OK;

    if (!is_scalar_is_operator(operator_kind)) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return if_validation_stack_push(database, stack, child_at(expression, 0U));
}

static int validate_case_literal_value_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    if (is_if_non_function_value_expression(expression)) {
        return MYLITE_OK;
    }
    set_case_unsupported_error(database);
    return MYLITE_ERROR;
}

static bool case_value_expression_is_admitted(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || is_session_scalar_expression(expression)) {
        return false;
    }
    if (is_case_expression_kind(expression->kind)) {
        return false;
    }
    if (is_scalar_logical_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_comparison_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_arithmetic_projection_expression(expression)) {
        return true;
    }
    return is_scalar_value_projection_expression(expression);
}

static bool is_case_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_case_expression_kind(expression->kind)) {
        return false;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        if (child_count < 1U || child_count > 2U) {
            return false;
        }
        when_list = child_at(expression, 0U);
        else_clause = child_at(expression, 1U);
    } else {
        if (child_count < 2U || child_count > 3U || child_at(expression, 0U) == NULL) {
            return false;
        }
        when_list = child_at(expression, 1U);
        else_clause = child_at(expression, 2U);
    }
    if (!is_case_when_list_projection_expression(when_list)) {
        return false;
    }
    if (else_clause != NULL && !is_case_else_clause_projection_expression(else_clause)) {
        return false;
    }
    return true;
}

static bool is_case_when_list_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *when_clause = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CASE_WHEN_LIST ||
        mylite_sql_ast_node_child_count(expression) == 0U) {
        return false;
    }
    when_clause = child_at(expression, 0U);
    while (when_clause != NULL) {
        if (!is_case_when_clause_projection_expression(when_clause)) {
            return false;
        }
        when_clause = when_clause->next_sibling;
    }
    return true;
}

static bool is_case_when_clause_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }
    if (child_at(expression, 0U) == NULL) {
        return false;
    }
    return child_at(expression, 1U) != NULL;
}

static bool is_case_else_clause_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CASE_ELSE_CLAUSE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_case_expression_kind(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
        return true;
    default:
        return false;
    }
}

static void set_case_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT CASE supports only no-source and FROM DUAL signed 64-bit integer, boolean, "
        "NULL, scalar arithmetic, scalar comparison, scalar logical, scalar IS, "
        "and existing scalar IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() expressions"
    );
}

static void set_if_unsupported_error(struct mylite_db *database, const char *function_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int message_length = snprintf(
        message,
        sizeof(message),
        "SELECT %s() supports only signed 64-bit integer, boolean, NULL, and nested "
        "IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() arguments",
        function_name
    );

    if (message_length < 0 || (size_t)message_length >= sizeof(message)) {
        set_runtime_error(database, "failed to format scalar control-flow diagnostic");
        return;
    }
    set_unsupported_error(database, message);
}

static const char *if_function_name(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_IFNULL_FUNCTION) {
        return "IFNULL";
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_COALESCE_FUNCTION) {
        return "COALESCE";
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_NULLIF_FUNCTION) {
        return "NULLIF";
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_ISNULL_FUNCTION) {
        return "ISNULL";
    }
    return "IF";
}

static bool is_if_non_function_value_expression(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal = child_at(expression, 0U);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
            return false;
        }
        if (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return false;
        }
        return is_if_integer_literal_in_range(literal);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return is_if_integer_literal_in_range(expression);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return true;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        return true;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return true;
    }
    return false;
}

static bool is_if_integer_literal_in_range(const struct mylite_sql_ast_node *literal) {
    uint64_t magnitude = 0U;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        return false;
    }
    if (magnitude > (uint64_t)INT64_MAX) {
        return false;
    }
    return true;
}

static int if_validation_stack_push(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node **items = NULL;
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
        items = (const struct mylite_sql_ast_node **)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = expression;
    ++stack->count;
    return MYLITE_OK;
}

static void if_validation_stack_deinit(struct if_validation_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct if_validation_stack){0};
}
